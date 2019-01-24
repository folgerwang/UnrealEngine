// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConcertServerWorkspace.h"

#include "IConcertSession.h"
#include "ConcertLogGlobal.h"
#include "ConcertServerSyncCommandQueue.h"
#include "ConcertTransactionLedger.h"
#include "ConcertPackageLedger.h"
#include "ConcertServerActivityLedger.h"
#include "ConcertTransactionEvents.h"
#include "ConcertServerDataStore.h"
#include "Algo/Transform.h"

FConcertServerWorkspace::FConcertServerWorkspace(const TSharedRef<IConcertServerSession>& InSession)
{
	BindSession(InSession);
}

FConcertServerWorkspace::~FConcertServerWorkspace()
{
	UnbindSession();
}

void FConcertServerWorkspace::BindSession(const TSharedRef<IConcertServerSession>& InSession)
{
	UnbindSession();
	Session = InSession;

	// Create Sync Command Queue
	SyncCommandQueue = MakeShared<FConcertServerSyncCommandQueue>();

	// Create Package Ledger
	PackageLedger = MakeUnique<FConcertPackageLedger>(EConcertPackageLedgerType::Persistent, Session->GetSessionWorkingDirectory());

	// Create Transaction Ledger
	TransactionLedger = MakeUnique<FConcertTransactionLedger>(EConcertTransactionLedgerType::Persistent, Session->GetSessionWorkingDirectory());

	// Create a ActivityLedger
	ActivityLedger = MakeUnique<FConcertServerActivityLedger>(Session.ToSharedRef(), SyncCommandQueue.ToSharedRef());

	// Try to restore old session data
	LoadWorkingSessionData();

	// Register to Transaction ledger
	TransactionLedger->OnAddFinalizedTransaction().AddLambda([this](const FConcertTransactionFinalizedEvent& FinalizedEvent, uint64 TransactionIndex)
		{
			FConcertSessionClientInfo SessionClientInfo;
			if (Session->FindSessionClient(FinalizedEvent.TransactionEndpointId, SessionClientInfo))
			{
				ActivityLedger->RecordFinalizedTransaction(FinalizedEvent, TransactionIndex, SessionClientInfo.ClientInfo);
			}
		});

	// Register Tick events
	TickHandle = Session->OnTick().AddRaw(this, &FConcertServerWorkspace::HandleTick);

	// Register Client Change events
	SessionClientChangedHandle = Session->OnSessionClientChanged().AddRaw(this, &FConcertServerWorkspace::HandleSessionClientChanged);

	Session->RegisterCustomEventHandler<FConcertPackageUpdateEvent>(this, &FConcertServerWorkspace::HandlePackageUpdateEvent);
	Session->RegisterCustomEventHandler<FConcertPlaySessionEvent>(this, &FConcertServerWorkspace::HandlePlaySessionEvent);

	Session->RegisterCustomEventHandler<FConcertTransactionFinalizedEvent>(this, &FConcertServerWorkspace::HandleTransactionFinalizedEvent);
	Session->RegisterCustomEventHandler<FConcertTransactionSnapshotEvent>(this, &FConcertServerWorkspace::HandleTransactionSnapshotEvent);

	Session->RegisterCustomRequestHandler<FConcertResourceLockRequest, FConcertResourceLockResponse>(this, &FConcertServerWorkspace::HandleResourceLockRequest);

	DataStore = MakeUnique<FConcertServerDataStore>(Session);
}

void FConcertServerWorkspace::UnbindSession()
{
	if (Session.IsValid())
	{
		// Destroy Sync Command Queue
		SyncCommandQueue.Reset();
		
		// Destroy Transaction Ledger
		TransactionLedger.Reset();

		// Destroy Package Ledger
		PackageLedger.Reset();

		// Destroy the Activity Ledger
		ActivityLedger.Reset();

		// Unregister Tick events
		if (TickHandle.IsValid())
		{
			Session->OnTick().Remove(TickHandle);
			TickHandle.Reset();
		}

		// Unregister Client Change events
		if (SessionClientChangedHandle.IsValid())
		{
			Session->OnSessionClientChanged().Remove(SessionClientChangedHandle);
			SessionClientChangedHandle.Reset();
		}

		Session->UnregisterCustomEventHandler<FConcertPackageUpdateEvent>();
		Session->UnregisterCustomEventHandler<FConcertPlaySessionEvent>();

		Session->UnregisterCustomEventHandler<FConcertTransactionFinalizedEvent>();
		Session->UnregisterCustomEventHandler<FConcertTransactionSnapshotEvent>();

		Session->UnregisterCustomRequestHandler<FConcertResourceLockRequest>();
		Session.Reset();

		DataStore.Reset();
	}
}

void FConcertServerWorkspace::HandleTick(IConcertServerSession& InSession, float InDeltaTime)
{
	check(Session.Get() == &InSession);

	static const double SyncFrameLimitSeconds = 1.0 / 60;
	SyncCommandQueue->ProcessQueue(SyncFrameLimitSeconds);

	for (auto UnsyncedEndpointIter = UnsyncedEndpoints.CreateIterator(); UnsyncedEndpointIter; ++UnsyncedEndpointIter)
	{
		if (SyncCommandQueue->IsQueueEmpty(*UnsyncedEndpointIter))
		{
			Session->SendCustomEvent(FConcertWorkspaceInitialSyncCompletedEvent(), *UnsyncedEndpointIter, EConcertMessageFlags::ReliableOrdered);
			SyncCommandQueue->SetCommandProcessingMethod(*UnsyncedEndpointIter, FConcertServerSyncCommandQueue::ESyncCommandProcessingMethod::ProcessAll);
			UnsyncedEndpointIter.RemoveCurrent();
		}
	}
}

void FConcertServerWorkspace::HandleSessionClientChanged(IConcertServerSession& InSession, EConcertClientStatus InClientStatus, const FConcertSessionClientInfo& InClientInfo)
{
	check(Session.Get() == &InSession);

	if (InClientStatus == EConcertClientStatus::Connected)
	{
		// Newly connected clients will be time-sliced until they've finished their initial sync
		UnsyncedEndpoints.AddUnique(InClientInfo.ClientEndpointId);
		SyncCommandQueue->SetCommandProcessingMethod(InClientInfo.ClientEndpointId, FConcertServerSyncCommandQueue::ESyncCommandProcessingMethod::ProcessTimeSliced);

		// Transactions
		for (uint64 TransactionIndex = 0; TransactionIndex < TransactionLedger->GetNextTransactionIndex(); ++TransactionIndex)
		{
			SyncCommandQueue->QueueCommand(InClientInfo.ClientEndpointId, [this, TransactionIndex](const FConcertServerSyncCommandQueue::FSyncCommandContext& InSyncCommandContext, const FGuid& InEndpointId)
			{
				FConcertWorkspaceSyncTransactionEvent SyncEvent;
				SyncEvent.RemainingWork = InSyncCommandContext.GetNumRemainingCommands();
				SyncEvent.TransactionIndex = TransactionIndex;
				if (TransactionLedger->FindSerializedTransaction(TransactionIndex, SyncEvent.TransactionData))
				{
					Session->SendCustomEvent(SyncEvent, InEndpointId, EConcertMessageFlags::ReliableOrdered);
				}
			});
		}
		// Packages
		for (const FName PackageName : PackageLedger->GetAllPackageNames())
		{
			SyncCommandQueue->QueueCommand(InClientInfo.ClientEndpointId, [this, PackageName](const FConcertServerSyncCommandQueue::FSyncCommandContext& InSyncCommandContext, const FGuid& InEndpointId)
			{
				uint32 HeadRevision = 0;
				if (PackageLedger->GetPackageHeadRevision(PackageName, HeadRevision))
				{
					for (uint32 Revision = 0; Revision <= HeadRevision; ++Revision)
					{
						// Only the head revision sends the full package data, as that's the only one that needs writing to disk
						// Other revisions just send the meta-data for the package revision
						if (Revision < HeadRevision)
						{
							// TODO: We probably want to flag these items as being partial so that the full information can be requested later if the client needs it (eg, for diffing)
							FConcertWorkspaceSyncPackageEvent SyncEvent;
							SyncEvent.PackageRevision = Revision;
							SyncEvent.RemainingWork = InSyncCommandContext.GetNumRemainingCommands();
							if (PackageLedger->FindPackage(PackageName, &SyncEvent.Package.Info, nullptr, &Revision))
							{
								Session->SendCustomEvent(SyncEvent, InEndpointId, EConcertMessageFlags::ReliableOrdered);
							}
						}
						else
						{
							// TODO: If the head revision is a "dummy" entry, should be send the package data from the previous revision instead?
							FConcertWorkspaceSyncPackageEvent SyncEvent;
							SyncEvent.PackageRevision = Revision;
							SyncEvent.RemainingWork = InSyncCommandContext.GetNumRemainingCommands();
							if (PackageLedger->FindPackage(PackageName, SyncEvent.Package, &Revision))
							{
								Session->SendCustomEvent(SyncEvent, InEndpointId, EConcertMessageFlags::ReliableOrdered);
							}
						}
					}
				}
			});
		}
		// Resource Locks
		SyncCommandQueue->QueueCommand(InClientInfo.ClientEndpointId, [this](const FConcertServerSyncCommandQueue::FSyncCommandContext& InSyncCommandContext, const FGuid& InEndpointId)
		{
			FConcertWorkspaceSyncLockEvent SyncEvent;
			SyncEvent.RemainingWork = InSyncCommandContext.GetNumRemainingCommands();
			Algo::Transform(LockedResources, SyncEvent.LockedResources, [](const TPair<FName, FLockOwner>& Pair)
			{
				return TPair<FName, FGuid>{ Pair.Key, Pair.Value.EndpointId };
			});
			Session->SendCustomEvent(SyncEvent, InEndpointId, EConcertMessageFlags::ReliableOrdered);
		});
		// PIE/SIE play state.
		SyncCommandQueue->QueueCommand(InClientInfo.ClientEndpointId, [this](const FConcertServerSyncCommandQueue::FSyncCommandContext& InSyncCommandContext, const FGuid& InEndpointId)
		{
			for (const TPair<FName, TArray<FPlaySessionInfo>>& PlayInfoPair : ActivePlaySessions)
			{
				for (const FPlaySessionInfo& PlayInfo: PlayInfoPair.Value)
				{
					Session->SendCustomEvent(FConcertPlaySessionEvent{EConcertPlaySessionEventType::BeginPlay, PlayInfo.EndpointId, PlayInfoPair.Key, PlayInfo.bIsSimulating}, InEndpointId, EConcertMessageFlags::ReliableOrdered);
				}
			}
		});

		ActivityLedger->RecordClientConectionStatusChanged(EConcertClientStatus::Connected, InClientInfo.ClientInfo);
		ActivityLedger->DoInitialSync(InClientInfo.ClientEndpointId);
	}
	else if (InClientStatus == EConcertClientStatus::Disconnected)
	{
		UnlockAllWorkspaceResources(InClientInfo.ClientEndpointId);
		HandleEndPlaySessions(InClientInfo.ClientEndpointId);
		UnsyncedEndpoints.Remove(InClientInfo.ClientEndpointId);
		SyncCommandQueue->ClearQueue(InClientInfo.ClientEndpointId);
		ActivityLedger->RecordClientConectionStatusChanged(EConcertClientStatus::Disconnected, InClientInfo.ClientInfo);
	}
}

void FConcertServerWorkspace::HandlePackageUpdateEvent(const FConcertSessionContext& Context, const FConcertPackageUpdateEvent& Event)
{
	TArray<FGuid> QueueClientEndpointIds;
	uint32 Revision = 0;
	
	// Consider acquiring lock on asset saving an explicit lock
	const bool bLockOwned = LockWorkspaceResource(Event.Package.Info.PackageName, Context.SourceEndpointId, EConcertLockFlags::Explicit);
	if (bLockOwned)
	{
		// if the client own the lock, queue this package update back to every endpoint but the one that sent it
		QueueClientEndpointIds = Session->GetSessionClientEndpointIds();
		QueueClientEndpointIds.Remove(Context.SourceEndpointId);

		// Add the package and trim associated live transactions
		Revision = PackageLedger->AddPackage(Event.Package);
		TransactionLedger->TrimLiveTransactions(Event.Package.Info.NextTransactionIndexWhenSaved, Event.Package.Info.PackageName);
	}
	else if (PackageLedger->GetPackageHeadRevision(Event.Package.Info.PackageName, Revision))
	{
		// if the client does not possess the lock, queue the latest revision back to the it
		QueueClientEndpointIds.Add(Context.SourceEndpointId);
	}

	if (QueueClientEndpointIds.Num() > 0)
	{
		SyncCommandQueue->QueueCommand(QueueClientEndpointIds, [this, Revision, PackageName = Event.Package.Info.PackageName](const FConcertServerSyncCommandQueue::FSyncCommandContext& InSyncCommandContext, const FGuid& InEndpointId)
		{
			FConcertWorkspaceSyncPackageEvent SyncEvent;
			SyncEvent.PackageRevision = Revision;
			SyncEvent.RemainingWork = InSyncCommandContext.GetNumRemainingCommands();
			if (PackageLedger->FindPackage(PackageName, SyncEvent.Package, &Revision))
			{
				Session->SendCustomEvent(SyncEvent, InEndpointId, EConcertMessageFlags::ReliableOrdered);
			}
		});
		// if the lock was owned and we are deleting the package also unlock the resource (TODO: should use sync queue?)
		if (Event.Package.Info.PackageUpdateType == EConcertPackageUpdateType::Deleted && bLockOwned)
		{
			UnlockWorkspaceResource(Event.Package.Info.PackageName, Context.SourceEndpointId, EConcertLockFlags::Explicit);
		}
	}
	
	FConcertSessionClientInfo SessionClientInfo;
	Session->FindSessionClient(Context.SourceEndpointId, SessionClientInfo);
	ActivityLedger->RecordPackageUpdate(Revision, Event.Package.Info, SessionClientInfo.ClientInfo);
}

void FConcertServerWorkspace::HandleTransactionFinalizedEvent(const FConcertSessionContext& InEventContext, const FConcertTransactionFinalizedEvent& InEvent)
{
	// Implicitly acquire locks for object in the transaction
	TArray<FName> ResourceNames;
	Algo::Transform(InEvent.ExportedObjects, ResourceNames, [](const FConcertExportedObject& InObject)
	{
		FString PathString = InObject.ObjectId.ObjectOuterPathName.ToString();
		PathString.AppendChar('.');
		InObject.ObjectId.ObjectName.AppendString(PathString);
		return FName(*PathString);
	});
	bool bLockOwned = LockWorkspaceResources(ResourceNames, InEventContext.SourceEndpointId, EConcertLockFlags::None);

	if (bLockOwned)
	{
		const uint64 TransactionIndex = TransactionLedger->AddTransaction(InEvent);

		SyncCommandQueue->QueueCommand(Session->GetSessionClientEndpointIds(), [this, TransactionIndex](const FConcertServerSyncCommandQueue::FSyncCommandContext& InSyncCommandContext, const FGuid& InEndpointId)
		{
			FConcertWorkspaceSyncTransactionEvent SyncEvent;
			SyncEvent.RemainingWork = InSyncCommandContext.GetNumRemainingCommands();
			SyncEvent.TransactionIndex = TransactionIndex;
			if (TransactionLedger->FindSerializedTransaction(TransactionIndex, SyncEvent.TransactionData))
			{
				Session->SendCustomEvent(SyncEvent, InEndpointId, EConcertMessageFlags::ReliableOrdered);
			}
		});

		// Implicitly unlock resources in the transaction (TODO: should use sync queue?)
		bool bUnlocked = UnlockWorkspaceResources(ResourceNames, InEventContext.SourceEndpointId, EConcertLockFlags::None);
		check(bUnlocked);
	}
	// if the lock isn't owned, request an undo for that transaction
	else
	{
		FConcertTransactionRejectedEvent UndoEvent;
		UndoEvent.TransactionId = InEvent.TransactionId;
		Session->SendCustomEvent(UndoEvent, InEventContext.SourceEndpointId, EConcertMessageFlags::ReliableOrdered);
	}

}

void FConcertServerWorkspace::HandleTransactionSnapshotEvent(const FConcertSessionContext& InEventContext, const FConcertTransactionSnapshotEvent& InEvent)
{
	// Implicitly acquire locks for objects in the transaction
	TArray<FName> ResourceNames;
	Algo::Transform(InEvent.ExportedObjects, ResourceNames, [](const FConcertExportedObject& InObject)
	{
		FString PathString = InObject.ObjectId.ObjectOuterPathName.ToString();
		PathString.AppendChar('.');
		InObject.ObjectId.ObjectName.AppendString(PathString);
		return FName(*PathString);
	});
	bool bLockOwned = LockWorkspaceResources(ResourceNames, InEventContext.SourceEndpointId, EConcertLockFlags::None);

	// if the lock was acquired, forward the snapshot
	if (bLockOwned)
	{
		TArray<FGuid> QueueClientEndpointIds = Session->GetSessionClientEndpointIds();
		QueueClientEndpointIds.Remove(InEventContext.SourceEndpointId);

		Session->SendCustomEvent(InEvent, QueueClientEndpointIds, EConcertMessageFlags::None);
	}
	// otherwise do nothing, we will undo the finalized transaction (TODO: send notification back?)
}

void FConcertServerWorkspace::HandlePlaySessionEvent(const FConcertSessionContext& Context, const FConcertPlaySessionEvent& Event)
{
	// Forward this notification onto all clients except the one that entered the play session
	{
		TArray<FGuid> NotifyEndpointIds = Session->GetSessionClientEndpointIds();
		NotifyEndpointIds.Remove(Context.SourceEndpointId);
		Session->SendCustomEvent(Event, NotifyEndpointIds, EConcertMessageFlags::ReliableOrdered);
	}

	if (Event.EventType == EConcertPlaySessionEventType::BeginPlay)
	{
		HandleBeginPlaySession(Event.PlayPackageName, Event.PlayEndpointId, Event.bIsSimulating);
	}
	else if (Event.EventType == EConcertPlaySessionEventType::EndPlay)
	{
		HandleEndPlaySession(Event.PlayPackageName, Event.PlayEndpointId);
	}
	else
	{
		check(Event.EventType == EConcertPlaySessionEventType::SwitchPlay);
		HandleSwitchPlaySession(Event.PlayPackageName, Event.PlayEndpointId);
	}
}

EConcertSessionResponseCode FConcertServerWorkspace::HandleResourceLockRequest(const FConcertSessionContext& Context, const FConcertResourceLockRequest& Request, FConcertResourceLockResponse& Response)
{
	check(Context.SourceEndpointId == Request.ClientId);
	Response.LockType = Request.LockType;

	switch (Response.LockType)
	{
	case EConcertResourceLockType::Lock:
		LockWorkspaceResources(Request.ResourceNames, Request.ClientId, EConcertLockFlags::Explicit, &Response.FailedResources);
		break;
	case EConcertResourceLockType::Unlock:
		UnlockWorkspaceResources(Request.ResourceNames, Request.ClientId, EConcertLockFlags::Explicit, &Response.FailedResources);
		break;
	default:
		return EConcertSessionResponseCode::InvalidRequest;
		break;
	}
	return EConcertSessionResponseCode::Success;
}

void FConcertServerWorkspace::HandleBeginPlaySession(const FName InPlayPackageName, const FGuid& InEndpointId, bool bIsSimulating)
{
	TArray<FPlaySessionInfo>& PlaySessionEndpoints = ActivePlaySessions.FindOrAdd(InPlayPackageName);
	PlaySessionEndpoints.AddUnique({InEndpointId, bIsSimulating});
}

void FConcertServerWorkspace::HandleSwitchPlaySession(const FName InPlayPackageName, const FGuid& InEndpointId)
{
	// The client has toggled between PIE/SIE play type.
	if (TArray<FPlaySessionInfo>* PlaySessionInfo = ActivePlaySessions.Find(InPlayPackageName))
	{
		if (FPlaySessionInfo* PlayInfo = PlaySessionInfo->FindByPredicate([InEndpointId](const FPlaySessionInfo& Info) { return InEndpointId == Info.EndpointId; }))
		{
			PlayInfo->bIsSimulating = !PlayInfo->bIsSimulating; // Toggle the status.
		}
	}
}

void FConcertServerWorkspace::HandleEndPlaySession(const FName InPlayPackageName, const FGuid& InEndpointId)
{
	bool bDiscardPackage = false;
	if (TArray<FPlaySessionInfo>* PlaySessionInfo = ActivePlaySessions.Find(InPlayPackageName))
	{
		PlaySessionInfo->RemoveAll([InEndpointId](const FPlaySessionInfo& Info) {return Info.EndpointId == InEndpointId; });
		if (PlaySessionInfo->Num() == 0)
		{
			bDiscardPackage = true;
			ActivePlaySessions.Remove(InPlayPackageName);
		}
	}

	if (bDiscardPackage)
	{
		FConcertPackage DummyPackage;
		DummyPackage.Info.PackageName = InPlayPackageName;
		DummyPackage.Info.NextTransactionIndexWhenSaved = TransactionLedger->GetNextTransactionIndex();

		// Save a dummy package in the ledger to discard the live transactions for the previous play world
		const uint32 Revision = PackageLedger->AddPackage(DummyPackage);
		TransactionLedger->TrimLiveTransactions(DummyPackage.Info.NextTransactionIndexWhenSaved, DummyPackage.Info.PackageName);

		// Queue this package discard back to every endpoint
		SyncCommandQueue->QueueCommand(Session->GetSessionClientEndpointIds(), [this, Revision, PackageName = DummyPackage.Info.PackageName](const FConcertServerSyncCommandQueue::FSyncCommandContext& SyncCommandContext, const FGuid& EndpointId)
		{
			FConcertWorkspaceSyncPackageEvent SyncEvent;
			SyncEvent.PackageRevision = Revision;
			SyncEvent.RemainingWork = SyncCommandContext.GetNumRemainingCommands();
			if (PackageLedger->FindPackage(PackageName, SyncEvent.Package, &Revision))
			{
				Session->SendCustomEvent(SyncEvent, EndpointId, EConcertMessageFlags::ReliableOrdered);
			}
		});
	}
}

void FConcertServerWorkspace::HandleEndPlaySessions(const FGuid& InEndpointId)
{
	FName PlayPackageName = FindPlaySession(InEndpointId);
	if (!PlayPackageName.IsNone())
	{
		HandleEndPlaySession(PlayPackageName, InEndpointId);

#if DO_CHECK
		// Verify that there are no sessions left using this endpoint (they should only ever end up in a single session)
		PlayPackageName = FindPlaySession(InEndpointId);
		ensureAlwaysMsgf(PlayPackageName.IsNone(), TEXT("Endpoint '%s' has in multiple play sessions!"), *InEndpointId.ToString());
#endif
	}
}

FName FConcertServerWorkspace::FindPlaySession(const FGuid& InEndpointId)
{
	for (const auto& ActivePlaySessionPair : ActivePlaySessions)
	{
		if (ActivePlaySessionPair.Value.ContainsByPredicate([InEndpointId](const FPlaySessionInfo& Info){ return Info.EndpointId == InEndpointId; }))
		{
			return ActivePlaySessionPair.Key;
		}
	}
	return FName();
}

bool FConcertServerWorkspace::LockWorkspaceResource(const FName InResourceName, const FGuid& InLockEndpointId, EConcertLockFlags InLockFlags)
{
	FLockOwner& Owner = LockedResources.FindOrAdd(InResourceName);
	if (!Owner.EndpointId.IsValid() || EnumHasAnyFlags(InLockFlags, EConcertLockFlags::Force))
	{
		Owner.EndpointId = InLockEndpointId;
		Owner.bExplicit = EnumHasAnyFlags(InLockFlags, EConcertLockFlags::Explicit);

		FConcertResourceLockEvent LockEvent{ InLockEndpointId, {InResourceName}, EConcertResourceLockType::Lock };
		Session->SendCustomEvent(LockEvent, Session->GetSessionClientEndpointIds(), EConcertMessageFlags::ReliableOrdered);
	}
	return Owner.EndpointId == InLockEndpointId;
}

bool FConcertServerWorkspace::LockWorkspaceResources(const TArray<FName>& InResourceNames, const FGuid& InLockEndpointId, EConcertLockFlags InLockFlags, TMap<FName, FGuid>* OutFailedRessources)
{
	int32 AcquiredLockCount = 0;
	bool bForce = EnumHasAnyFlags(InLockFlags, EConcertLockFlags::Force);
	bool bExplicit = EnumHasAnyFlags(InLockFlags, EConcertLockFlags::Explicit);
	FConcertResourceLockEvent LockEvent{ InLockEndpointId, {}, EConcertResourceLockType::Lock };

	for (const FName& ResourceName : InResourceNames)
	{
		FLockOwner* Owner = LockedResources.Find(ResourceName);
		if (Owner == nullptr || bForce)
		{
			LockEvent.ResourceNames.Add(ResourceName);
			++AcquiredLockCount;
		}
		else if (Owner->EndpointId == InLockEndpointId)
		{
			++AcquiredLockCount;
		}
		else if (OutFailedRessources != nullptr)
		{
			OutFailedRessources->Add(ResourceName, Owner->EndpointId);
		}
	}

	bool bSuccess = AcquiredLockCount == InResourceNames.Num();
	// if the operation was successful and any new locks were acquired, add them and send an update
	if (bSuccess && LockEvent.ResourceNames.Num() > 0)
	{
		for (const FName& ResourceName : LockEvent.ResourceNames)
		{
			LockedResources.Add(ResourceName, { InLockEndpointId, bExplicit });
		}
		Session->SendCustomEvent(LockEvent, Session->GetSessionClientEndpointIds(), EConcertMessageFlags::ReliableOrdered);
	}
	return bSuccess;
}

bool FConcertServerWorkspace::UnlockWorkspaceResource(const FName InResourceName, const FGuid& InLockEndpointId, EConcertLockFlags InLockFlags)
{
	bool bForce = EnumHasAnyFlags(InLockFlags, EConcertLockFlags::Force);
	bool bExplicit = EnumHasAnyFlags(InLockFlags, EConcertLockFlags::Explicit);

	if (const FLockOwner* Owner = LockedResources.Find(InResourceName))
	{
		if (Owner->EndpointId == InLockEndpointId || bForce)
		{
			if (Owner->bExplicit == bExplicit || bForce)
			{
				LockedResources.Remove(InResourceName);
				FConcertResourceLockEvent LockEvent{ InLockEndpointId, {InResourceName}, EConcertResourceLockType::Unlock };
				Session->SendCustomEvent(LockEvent, Session->GetSessionClientEndpointIds(), EConcertMessageFlags::ReliableOrdered);
			}
			return true;
		}
	}
	return false;
}

bool FConcertServerWorkspace::UnlockWorkspaceResources(const TArray<FName>& InResourceNames, const FGuid& InLockEndpointId, EConcertLockFlags InLockFlags, TMap<FName, FGuid>* OutFailedRessources)
{
	int32 ReleasedLockCount = 0;
	bool bForce = EnumHasAnyFlags(InLockFlags, EConcertLockFlags::Force);
	bool bExplicit = EnumHasAnyFlags(InLockFlags, EConcertLockFlags::Explicit);
	FConcertResourceLockEvent LockEvent{ InLockEndpointId, {}, EConcertResourceLockType::Unlock };

	for (const FName& ResourceName : InResourceNames)
	{
		const FLockOwner Owner = LockedResources.FindRef(ResourceName);
		if (Owner.EndpointId == InLockEndpointId || bForce)
		{
			if (Owner.bExplicit == bExplicit || bForce)
			{
				LockedResources.Remove(ResourceName);
				LockEvent.ResourceNames.Add(ResourceName);
			}
			++ReleasedLockCount;
		}
		else if (OutFailedRessources != nullptr)
		{
			OutFailedRessources->Add(ResourceName, Owner.EndpointId);
		}
	}

	// if any lock were successfully released, send an update
	if (LockEvent.ResourceNames.Num() > 0)
	{
		Session->SendCustomEvent(LockEvent, Session->GetSessionClientEndpointIds(), EConcertMessageFlags::ReliableOrdered);
	}

	return ReleasedLockCount == InResourceNames.Num();;
}

void FConcertServerWorkspace::UnlockAllWorkspaceResources(const FGuid& InLockEndpointId)
{
	FConcertResourceLockEvent LockEvent;
	for (auto It = LockedResources.CreateIterator(); It; ++It)
	{
		if (It->Value.EndpointId == InLockEndpointId)
		{
			LockEvent.ResourceNames.Add(It->Key);
			It.RemoveCurrent();
		}
	}
	// Notify lock state change
	if (LockEvent.ResourceNames.Num() > 0)
	{
		LockEvent.ClientId = InLockEndpointId;
		LockEvent.LockType = EConcertResourceLockType::Unlock;
		Session->SendCustomEvent(LockEvent, Session->GetSessionClientEndpointIds(), EConcertMessageFlags::ReliableOrdered);
	}
}

bool FConcertServerWorkspace::IsWorkspaceResourceLocked(const FName InResourceName, const FGuid& InLockEndpointId) const
{
	const FLockOwner* Owner = LockedResources.Find(InResourceName);
	return Owner && Owner->EndpointId == InLockEndpointId;
}

void FConcertServerWorkspace::LoadWorkingSessionData()
{
	if (PackageLedger->LoadLedger())
	{
		UE_LOG(LogConcert, Warning, TEXT("Session '%s' packages were restored."), *Session->GetName());
	}
	
	if (TransactionLedger->LoadLedger())
	{
		UE_LOG(LogConcert, Warning, TEXT("Session '%s' transactions were restored."), *Session->GetName());
	}

	if (ActivityLedger->LoadLedger())
	{
		UE_LOG(LogConcert, Warning, TEXT("Session '%s' activities were restored."), *Session->GetName());
	}
}
