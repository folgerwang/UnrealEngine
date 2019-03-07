// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConcertClientWorkspace.h"
#include "ConcertTransactionLedger.h"
#include "ConcertClientTransactionManager.h"
#include "IConcertClient.h"
#include "IConcertModule.h"
#include "IConcertSession.h"
#include "ConcertSyncSettings.h"
#include "ConcertSyncClientUtil.h"
#include "ConcertLogGlobal.h"
#include "ConcertPackageLedger.h"
#include "ConcertSandboxPlatformFile.h"
#include "ConcertClientActivityLedger.h"
#include "ConcertActivityEvents.h"
#include "ConcertWorkspaceData.h"
#include "ConcertClientDataStore.h"
#include "ConcertClientLiveTransactionAuthors.h"

#include "Containers/Ticker.h"
#include "Containers/ArrayBuilder.h"
#include "UObject/Package.h"
#include "UObject/Linker.h"
#include "UObject/LinkerLoad.h"
#include "UObject/StructOnScope.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/FeedbackContext.h"
#include "RenderingThread.h"
#include "Modules/ModuleManager.h"

#include "AssetRegistryModule.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

#if WITH_EDITOR
	#include "UnrealEdGlobals.h"
	#include "UnrealEdMisc.h"
	#include "Editor/EditorEngine.h"
	#include "Editor/UnrealEdEngine.h"
	#include "Editor/TransBuffer.h"
	#include "LevelEditor.h"
	#include "FileHelpers.h"
	#include "GameMapsSettings.h"
#endif

#define LOCTEXT_NAMESPACE "ConcertClientWorkspace"

namespace ConcertClientWorkspaceUtil
{
FString GetSandboxRootPath(const FString& InSessionWorkingDir)
{
	return InSessionWorkingDir / TEXT("Sandbox");
}

void FillPackageInfo(UPackage* InPackage, const EConcertPackageUpdateType PackageUpdateType, const uint64 InNextTransactionIndexWhenSaved, FConcertPackageInfo& OutPackageInfo)
{
	OutPackageInfo.PackageName = InPackage->GetFName();
	OutPackageInfo.PackageFileExtension = UWorld::FindWorldInPackage(InPackage) ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension();
	OutPackageInfo.PackageUpdateType = PackageUpdateType;
	OutPackageInfo.NextTransactionIndexWhenSaved = InNextTransactionIndexWhenSaved;
}
}

FConcertClientWorkspace::FConcertClientWorkspace(const TSharedRef<IConcertClientSession>& InSession)
{
	BindSession(InSession);
}

FConcertClientWorkspace::~FConcertClientWorkspace()
{
	UnbindSession();
}

TSharedPtr<IConcertClientSession> FConcertClientWorkspace::GetSession() const
{
	return Session;
}

FGuid FConcertClientWorkspace::GetWorkspaceLockId() const
{
	return Session->GetSessionClientEndpointId();
}

FGuid FConcertClientWorkspace::GetResourceLockId(const FName InResourceName) const
{
	return LockedResources.FindRef(InResourceName);
}

bool FConcertClientWorkspace::AreResourcesLockedBy(TArrayView<const FName> ResourceNames, const FGuid& ClientId)
{
	for (const FName& ResourceName : ResourceNames)
	{
		if (LockedResources.FindRef(ResourceName) != ClientId)
		{
			return false;
		}
	}
	return true;
}

TFuture<FConcertResourceLockResponse> FConcertClientWorkspace::LockResources(TArray<FName> InResourceNames)
{
	FConcertResourceLockRequest Request{ Session->GetSessionClientEndpointId(), MoveTemp(InResourceNames), EConcertResourceLockType::Lock };
	return Session->SendCustomRequest<FConcertResourceLockRequest, FConcertResourceLockResponse>(Request, Session->GetSessionServerEndpointId());
}

TFuture<FConcertResourceLockResponse> FConcertClientWorkspace::UnlockResources(TArray<FName> InResourceNames)
{
	FConcertResourceLockRequest Request{ Session->GetSessionClientEndpointId(), MoveTemp(InResourceNames), EConcertResourceLockType::Unlock };
	return Session->SendCustomRequest<FConcertResourceLockRequest, FConcertResourceLockResponse>(Request, Session->GetSessionServerEndpointId());
}

TArray<FString> FConcertClientWorkspace::GatherSessionChanges()
{
	TArray<FString> SessionChanges;
#if WITH_EDITOR
	// Save live transactions to packages so we can properly report those changes.
	SaveLiveTransactionsToPackages();

	// Persist the sandbox state over the real content directory
	// This will also check things out from source control and make them ready to be submitted
	if (SandboxPlatformFile.IsValid())
	{
		SessionChanges = SandboxPlatformFile->GatherSandboxChangedFilenames();
	}
#endif
	return SessionChanges;
}

bool FConcertClientWorkspace::PersistSessionChanges(TArrayView<const FString> InFilesToPersist, ISourceControlProvider* SourceControlProvider, TArray<FText>* OutFailureReasons)
{
#if WITH_EDITOR
	if (SandboxPlatformFile.IsValid())
	{
		return SandboxPlatformFile->PersistSandbox(MoveTemp(InFilesToPersist), SourceControlProvider, OutFailureReasons);
	}
#endif
	return false;
}


bool FConcertClientWorkspace::FindTransactionEvent(const uint64 TransactionIndex, FConcertTransactionFinalizedEvent& OutTransaction) const
{
	return TransactionManager->GetLedger().FindTypedTransaction(TransactionIndex, OutTransaction);
}

bool FConcertClientWorkspace::FindPackageEvent(const FName& PackageName, const uint32 Revision, FConcertPackageInfo& OutPackage) const
{
	return PackageLedger->FindPackage(PackageName, &OutPackage, nullptr, &Revision);
}

uint64 FConcertClientWorkspace::GetActivityCount() const
{
	return ActivityLedger->GetActivityCount();
}

uint64 FConcertClientWorkspace::GetLastActivities(const uint32 Limit, TArray<FStructOnScope>& OutActivities) const
{
	return ActivityLedger->GetLastActivities(Limit, OutActivities);
}

void FConcertClientWorkspace::GetActivities(const uint64 Offset, const uint32 Limit, TArray<FStructOnScope>& OutActivities) const
{
	return ActivityLedger->GetActivities(Offset, Limit, OutActivities);
}

FOnAddActivity& FConcertClientWorkspace::OnAddActivity()
{
	return ActivityLedger->OnAddActivity();
}

FOnWorkspaceSynchronized& FConcertClientWorkspace::OnWorkspaceSynchronized()
{
	return OnWorkspaceSyncedDelegate;
}

IConcertClientDataStore& FConcertClientWorkspace::GetDataStore()
{
	return *DataStore;
}

void FConcertClientWorkspace::BindSession(const TSharedRef<IConcertClientSession>& InSession)
{
	UnbindSession();
	Session = InSession;

	bIsSavingPackage = false;
	bHasSyncedWorkspace = false;
	bFinalizeWorkspaceSyncRequested = false;

#if WITH_EDITOR
	// Create Sandbox
	SandboxPlatformFile = MakeUnique<FConcertSandboxPlatformFile>(ConcertClientWorkspaceUtil::GetSandboxRootPath(Session->GetSessionWorkingDirectory()));
	SandboxPlatformFile->Initialize(&FPlatformFileManager::Get().GetPlatformFile(), TEXT(""));
#endif

	// Provide access to the data store (shared by session clients) maintained by the server.
	DataStore = MakeUnique<FConcertClientDataStore>(InSession);

	// Create Package Ledger
	PackageLedger = MakeUnique<FConcertPackageLedger>(EConcertPackageLedgerType::Transient, Session->GetSessionWorkingDirectory());

	// Create Transaction Manager
	TransactionManager = MakeUnique<FConcertClientTransactionManager>(InSession);

	// Create Activity Ledger
	ActivityLedger = MakeUnique<FConcertClientActivityLedger>(InSession);

	// Create the service tracking which clients have live transaction on which packages.
	LiveTransactionAuthors = MakeUnique<FConcertClientLiveTransactionAuthors>(InSession);

	// Register to Transaction ledger
	TransactionManager->GetMutableLedger().OnAddFinalizedTransaction().AddLambda([this](const FConcertTransactionFinalizedEvent& FinalizedEvent, uint64 TransactionIndex)
	{
		FConcertSessionClientInfo SessionClientInfo;
		if (Session->FindSessionClient(FinalizedEvent.TransactionEndpointId, SessionClientInfo))
		{
			ActivityLedger->RecordFinalizedTransaction(FinalizedEvent, TransactionIndex, SessionClientInfo.ClientInfo);
			LiveTransactionAuthors->AddLiveTransaction(FinalizedEvent.ModifiedPackages, SessionClientInfo.ClientInfo, TransactionIndex);
		}
		else
		{
			// When the transaction originated from our client
			const FConcertClientInfo& ClientInfo = Session->GetLocalClientInfo();
			ActivityLedger->RecordFinalizedTransaction(FinalizedEvent, TransactionIndex, ClientInfo);
			LiveTransactionAuthors->AddLiveTransaction(FinalizedEvent.ModifiedPackages, ClientInfo, TransactionIndex);
		}
	});

	TransactionManager->GetMutableLedger().OnLiveTransactionsTrimmed().AddLambda([this](const FName& PackageName, uint64 UpToIndex)
	{
		LiveTransactionAuthors->TrimLiveTransactions(PackageName, UpToIndex);
	});

	// Get the live transactions from the transaction ledger, match live transactions to their authors using the activity ledger and populate the live transaction author tracker.
	ResolveLiveTransactionAuthors(TransactionManager->GetLedger(), *ActivityLedger, *LiveTransactionAuthors);

	// Register Session events
	SessionConnectedHandle = Session->OnConnectionChanged().AddRaw(this, &FConcertClientWorkspace::HandleConnectionChanged);

#if WITH_EDITOR
	if (GIsEditor)
	{
		// Back up 'package ok to save delegate' and install ours
		OkToSaveBackupDelegate = FCoreUObjectDelegates::IsPackageOKToSaveDelegate;
		FCoreUObjectDelegates::IsPackageOKToSaveDelegate.BindRaw(this, &FConcertClientWorkspace::CanSavePackage);

		// Register Package Saved Events
		UPackage::PackageSavedEvent.AddRaw(this, &FConcertClientWorkspace::HandlePackageSaved);

		// Register Asset Registry Events
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		AssetRegistryModule.Get().OnInMemoryAssetCreated().AddRaw(this, &FConcertClientWorkspace::HandleAssetAdded);
		AssetRegistryModule.Get().OnInMemoryAssetDeleted().AddRaw(this, &FConcertClientWorkspace::HandleAssetDeleted);
		AssetRegistryModule.Get().OnAssetRenamed().AddRaw(this, &FConcertClientWorkspace::HandleAssetRenamed);
	}

	// Register Asset Load Events
	FCoreUObjectDelegates::OnAssetLoaded.AddRaw(this, &FConcertClientWorkspace::HandleAssetLoaded);

	// Register PIE/SIE Events
	PostPIEStartedHandle = FEditorDelegates::PostPIEStarted.AddRaw(this, &FConcertClientWorkspace::HandlePostPIEStarted);
	SwitchBeginPIEAndSIEHandle = FEditorDelegates::OnSwitchBeginPIEAndSIE.AddRaw(this, &FConcertClientWorkspace::HandleSwitchBeginPIEAndSIE);
	EndPIEHandle = FEditorDelegates::EndPIE.AddRaw(this, &FConcertClientWorkspace::HandleEndPIE);

	// Register Object Transaction events
	if (GUnrealEd)
	{
		if (UTransBuffer* TransBuffer = Cast<UTransBuffer>(GUnrealEd->Trans))
		{
			TransactionStateChangedHandle = TransBuffer->OnTransactionStateChanged().AddRaw(this, &FConcertClientWorkspace::HandleTransactionStateChanged);
		}
	}
	ObjectTransactedHandle = FCoreUObjectDelegates::OnObjectTransacted.AddRaw(this, &FConcertClientWorkspace::HandleObjectTransacted);
#endif

	// Register OnEndFrame events
	OnEndFrameHandle = FCoreDelegates::OnEndFrame.AddRaw(this, &FConcertClientWorkspace::OnEndFrame);

	// Register workspace event
	Session->RegisterCustomEventHandler<FConcertWorkspaceSyncTransactionEvent>(this, &FConcertClientWorkspace::HandleWorkspaceSyncTransactionEvent);
	Session->RegisterCustomEventHandler<FConcertWorkspaceSyncPackageEvent>(this, &FConcertClientWorkspace::HandleWorkspaceSyncPackageEvent);
	Session->RegisterCustomEventHandler<FConcertWorkspaceSyncLockEvent>(this, &FConcertClientWorkspace::HandleWorkspaceSyncLockEvent);
	Session->RegisterCustomEventHandler<FConcertWorkspaceInitialSyncCompletedEvent>(this, &FConcertClientWorkspace::HandleWorkspaceInitialSyncCompletedEvent);

	Session->RegisterCustomEventHandler<FConcertResourceLockEvent>(this, &FConcertClientWorkspace::HandleResourceLockEvent);
}

void FConcertClientWorkspace::UnbindSession()
{
	if (Session.IsValid())
	{
#if WITH_EDITOR
		// Discard Sandbox and gather packages to be reloaded/purged
		SandboxPlatformFile->DiscardSandbox(PackagesPendingHotReload, PackagesPendingPurge);
		SandboxPlatformFile.Reset();

		// Gather file with live transactions that also need to be reloaded, overlaps from the sandbox are filtered directly in ReloadPackages
		for (const FName PackageNameWithLiveTransactions : TransactionManager->GetLedger().GetPackagesNamesWithLiveTransactions())
		{
			if (!PackagesPendingPurge.Contains(PackageNameWithLiveTransactions))
			{
				PackagesPendingHotReload.Add(PackageNameWithLiveTransactions);
			}
		}
#endif

		// Destroy Transaction Manager
		TransactionManager.Reset();

		// Destroy Package Ledger
		PackageLedger.Reset();

		// Destroy Activity ledger
		ActivityLedger.Reset();

		// Destroy the object tracking the live transaction authors.
		LiveTransactionAuthors.Reset();

		// Unregister Session events
		if (SessionConnectedHandle.IsValid())
		{
			Session->OnConnectionChanged().Remove(SessionConnectedHandle);
			SessionConnectedHandle.Reset();
		}

#if WITH_EDITOR
		// Restore 'is ok to save package' delegate
		if (OkToSaveBackupDelegate.IsBound())
		{
			FCoreUObjectDelegates::IsPackageOKToSaveDelegate = OkToSaveBackupDelegate;
			OkToSaveBackupDelegate.Unbind();
		}

		// Unregister Package Events
		UPackage::PackageSavedEvent.RemoveAll(this);

		// Unregister Asset Registry Events
		if (FAssetRegistryModule* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry"))
		{
			AssetRegistryModule->Get().OnInMemoryAssetCreated().RemoveAll(this);
			AssetRegistryModule->Get().OnInMemoryAssetDeleted().RemoveAll(this);
			AssetRegistryModule->Get().OnAssetRenamed().RemoveAll(this);
		}

		// Unregister Asset Load Events
		FCoreUObjectDelegates::OnAssetLoaded.RemoveAll(this);

		// Unregister PIE/SIE Events
		if (PostPIEStartedHandle.IsValid())
		{
			FEditorDelegates::PostPIEStarted.Remove(PostPIEStartedHandle);
			PostPIEStartedHandle.Reset();
		}
		if (SwitchBeginPIEAndSIEHandle.IsValid())
		{
			FEditorDelegates::OnSwitchBeginPIEAndSIE.Remove(SwitchBeginPIEAndSIEHandle);
			SwitchBeginPIEAndSIEHandle.Reset();
		}
		if (EndPIEHandle.IsValid())
		{
			FEditorDelegates::EndPIE.Remove(EndPIEHandle);
			EndPIEHandle.Reset();
		}

		// Unregister Object Transaction events
		if (GUnrealEd && TransactionStateChangedHandle.IsValid())
		{
			if (UTransBuffer* TransBuffer = Cast<UTransBuffer>(GUnrealEd->Trans))
			{
				TransBuffer->OnTransactionStateChanged().Remove(TransactionStateChangedHandle);
			}
			TransactionStateChangedHandle.Reset();
		}
		if (ObjectTransactedHandle.IsValid())
		{
			FCoreUObjectDelegates::OnObjectTransacted.Remove(ObjectTransactedHandle);
			ObjectTransactedHandle.Reset();
		}

		if (!GIsRequestingExit)
		{
			// Hot reload after unregistering from most delegates to prevent events triggered by hot-reloading (such as asset deleted) to be recorded as transaction.
			HotReloadPendingPackages();

			// Get the current world edited.
			if (UWorld* World = GEditor->GetEditorWorldContext().World())
			{
				// If the current world package is scheduled to be purged (it doesn't exist outside the session).
				if (PackagesPendingPurge.Contains(World->GetOutermost()->GetFName()))
				{
					// Replace the current world because it doesn't exist outside the session (it cannot be saved anymore, even with 'Save Current As').
					FString StartupMapPackage = GetDefault<UGameMapsSettings>()->EditorStartupMap.GetLongPackageName();
					if (FPackageName::DoesPackageExist(StartupMapPackage))
					{
						UEditorLoadingAndSavingUtils::NewMapFromTemplate(StartupMapPackage, /*bSaveExistingMap*/ false);
					}
					else
					{
						UEditorLoadingAndSavingUtils::NewBlankMap(/*bSaveExistingMap*/ false);
					}
				}

				PurgePendingPackages();
			}
		}
#endif

		// Unregister OnEndFrame events
		if (OnEndFrameHandle.IsValid())
		{
			FCoreDelegates::OnEndFrame.Remove(OnEndFrameHandle);
			OnEndFrameHandle.Reset();
		}

		// Unregister workspace event
		Session->UnregisterCustomEventHandler<FConcertWorkspaceSyncTransactionEvent>();
		Session->UnregisterCustomEventHandler<FConcertWorkspaceSyncPackageEvent>();
		Session->UnregisterCustomEventHandler<FConcertWorkspaceSyncLockEvent>();
		Session->UnregisterCustomEventHandler<FConcertWorkspaceInitialSyncCompletedEvent>();

		Session->UnregisterCustomEventHandler<FConcertResourceLockEvent>();

		DataStore.Reset();
		Session.Reset();
	}
}

void FConcertClientWorkspace::HandleConnectionChanged(IConcertClientSession& InSession, EConcertConnectionStatus Status)
{
	check(Session.Get() == &InSession);

	if (Status == EConcertConnectionStatus::Connected)
	{
		bHasSyncedWorkspace = false;
		bFinalizeWorkspaceSyncRequested = false;
		InitialSyncSlowTask = MakeUnique<FScopedSlowTask>(1.0f, LOCTEXT("SynchronizingWorkspace", "Synchronizing Workspace..."));
		InitialSyncSlowTask->MakeDialog();

#if WITH_EDITOR
		if (GUnrealEd)
		{
			if (FWorldContext* PIEWorldContext = GUnrealEd->GetPIEWorldContext())
			{
				if (UWorld* PIEWorld = PIEWorldContext->World())
				{
					// Track open PIE/SIE sessions so the server can discard them once everyone leaves
					FConcertPlaySessionEvent PlaySessionEvent;
					PlaySessionEvent.EventType = EConcertPlaySessionEventType::BeginPlay;
					PlaySessionEvent.PlayEndpointId = Session->GetSessionClientEndpointId();
					PlaySessionEvent.PlayPackageName = PIEWorld->GetOutermost()->GetFName();
					PlaySessionEvent.bIsSimulating = GUnrealEd->bIsSimulatingInEditor;
					Session->SendCustomEvent(PlaySessionEvent, Session->GetSessionServerEndpointId(), EConcertMessageFlags::ReliableOrdered);
				}
			}
		}
#endif
	}
	else if (Status == EConcertConnectionStatus::Disconnected)
	{
		bHasSyncedWorkspace = false;
		bFinalizeWorkspaceSyncRequested = false;
		InitialSyncSlowTask.Reset();
	}
}

#if WITH_EDITOR

void FConcertClientWorkspace::SaveLiveTransactionsToPackages()
{
	// Save any packages that have live transactions, filtering them from being sent to other clients (which should already be synced)
	if (GEditor)
	{
		const uint64 NextTransactionIndexWhenSaved = TransactionManager->GetLedger().GetNextTransactionIndex();
		for (const FName PackageName : TransactionManager->GetLedger().GetPackagesNamesWithLiveTransactions())
		{
			const FString PackageNameStr = PackageName.ToString();
			UPackage* Package = LoadPackage(nullptr, *PackageNameStr, LOAD_None);
			if (Package)
			{
				TGuardValue<bool> IsSavingPackageScope(bIsSavingPackage, true);

				UWorld* World = UWorld::FindWorldInPackage(Package);
				FString PackageFilename;
				if (!FPackageName::DoesPackageExist(PackageNameStr, nullptr, &PackageFilename))
				{
					PackageFilename = FPackageName::LongPackageNameToFilename(PackageNameStr, World ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension());
				}

				if (GEditor->SavePackage(Package, World, RF_Standalone, *PackageFilename, GWarn))
				{
					// The bIsSavingPackage check prevents HandlePackageSaved trimming the ledger, so we do it here instead
					TransactionManager->GetMutableLedger().TrimLiveTransactions(NextTransactionIndexWhenSaved, PackageName);
				}
				else
				{
					UE_LOG(LogConcert, Warning, TEXT("Failed to save package '%s' when persiting sandbox state!"), *PackageNameStr);
				}
			}
		}
	}
}

bool FConcertClientWorkspace::CanSavePackage(UPackage* InPackage, const FString& InFilename, FOutputDevice* ErrorLog)
{
	FGuid LockOwner = LockedResources.FindRef(InPackage->GetFName());
	if (LockOwner.IsValid() && LockOwner != GetWorkspaceLockId())
	{
		ErrorLog->Log(TEXT("LogConcert"), ELogVerbosity::Warning, FString::Printf(TEXT("Package %s currently locked by another user."), *InPackage->GetFName().ToString()));
		return false;
	}
	return true;
}

void FConcertClientWorkspace::HandlePackageSaved(const FString& PackageFilename, UObject* Outer)
{
	UPackage* Package = CastChecked<UPackage>(Outer);

	// Ignore Auto saves
	if (bIsSavingPackage || GEngine->IsAutosaving())
	{
		return;
	}

	// if we end up here, the package should be either unlocked or locked by this client, the server will resend the latest revision if it wasn't the case.
	FName NewPackageName;
	PackagesBeingRenamed.RemoveAndCopyValue(Package->GetFName(), NewPackageName);

	FConcertPackageUpdateEvent Event;
	ConcertClientWorkspaceUtil::FillPackageInfo(Package, NewPackageName.IsNone() ? EConcertPackageUpdateType::Saved : EConcertPackageUpdateType::Renamed, TransactionManager->GetLedger().GetNextTransactionIndex(), Event.Package.Info);
	Event.Package.Info.NewPackageName = NewPackageName;

	if (FFileHelper::LoadFileToArray(Event.Package.PackageData, *PackageFilename))
	{
		PackageLedger->AddPackage(Event.Package);
		TransactionManager->GetMutableLedger().TrimLiveTransactions(Event.Package.Info.NextTransactionIndexWhenSaved, Event.Package.Info.PackageName);
		Session->SendCustomEvent(Event, Session->GetSessionServerEndpointId(), EConcertMessageFlags::ReliableOrdered);
	}

	UE_LOG(LogConcert, Verbose, TEXT("Asset Saved: %s"), *Package->GetName());
}

void FConcertClientWorkspace::HandleAssetAdded(UObject *Object)
{
	UPackage* Package = Object->GetOutermost();

	// Skip packages that are in the process of being renamed as they are always saved after being added
	if (PackagesBeingRenamed.Contains(Package->GetFName()))
	{
		return;
	}

	// Save this package to the sandbox at its proper location immediately so we can send it since it won't exist on disk
	{
		TGuardValue<bool> IsSavingPackageScope(bIsSavingPackage, true);
		UWorld* World = UWorld::FindWorldInPackage(Package);
		
		FString PackageFilename;
		FPackageName::TryConvertLongPackageNameToFilename(Package->GetFName().ToString(), PackageFilename, World != nullptr ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension());
		if (UPackage::SavePackage(Package, World, RF_Standalone, *PackageFilename, GWarn, nullptr, false, false, SAVE_NoError | SAVE_KeepDirty))
		{
			FConcertPackageUpdateEvent Event;
			ConcertClientWorkspaceUtil::FillPackageInfo(Package, EConcertPackageUpdateType::Added, TransactionManager->GetLedger().GetNextTransactionIndex(), Event.Package.Info);

			if (FFileHelper::LoadFileToArray(Event.Package.PackageData, *PackageFilename))
			{
				PackageLedger->AddPackage(Event.Package);
				Session->SendCustomEvent(Event, Session->GetSessionServerEndpointId(), EConcertMessageFlags::ReliableOrdered);
			}
		}
	}

	UE_LOG(LogConcert, Verbose, TEXT("Asset Added: %s"), *Package->GetName());
}

void FConcertClientWorkspace::HandleAssetDeleted(UObject *Object)
{
	UPackage* Package = Object->GetOutermost();
	
	FConcertPackageUpdateEvent Event;
	ConcertClientWorkspaceUtil::FillPackageInfo(Package, EConcertPackageUpdateType::Deleted, TransactionManager->GetLedger().GetNextTransactionIndex(), Event.Package.Info);
	PackageLedger->AddPackage(Event.Package);
	Session->SendCustomEvent(Event, Session->GetSessionServerEndpointId(), EConcertMessageFlags::ReliableOrdered);

	UE_LOG(LogConcert, Verbose, TEXT("Asset Deleted: %s"), *Package->GetName());
}

void FConcertClientWorkspace::HandleAssetRenamed(const FAssetData& Data, const FString& OldName)
{
	// A rename operation comes through as:
	//	1) Asset renamed (this notification)
	//	2) Asset added (old asset, which we'll ignore)
	//	3) Asset saved (new asset)
	//	4) Asset saved (old asset, as a redirector)
	const FName OldPackageName = *FPackageName::ObjectPathToPackageName(OldName);
	PackagesBeingRenamed.Add(OldPackageName, Data.PackageName);

	UE_LOG(LogConcert, Verbose, TEXT("Asset Renamed: %s -> %s"), *OldPackageName.ToString(), *Data.PackageName.ToString());
}

void FConcertClientWorkspace::HandleAssetLoaded(UObject* InAsset)
{
	if (TransactionManager.IsValid() && bHasSyncedWorkspace)
	{
		const FName LoadedPackageName = InAsset->GetOutermost()->GetFName();
		TransactionManager->ReplayTransactions(LoadedPackageName);
	}
}

void FConcertClientWorkspace::HandlePostPIEStarted(const bool InIsSimulating)
{
	if (FWorldContext* PIEWorldContext = GUnrealEd->GetPIEWorldContext())
	{
		if (UWorld* PIEWorld = PIEWorldContext->World())
		{
			// Track open PIE/SIE sessions so the server can discard them once everyone leaves
			FConcertPlaySessionEvent PlaySessionEvent;
			PlaySessionEvent.EventType = EConcertPlaySessionEventType::BeginPlay;
			PlaySessionEvent.PlayEndpointId = Session->GetSessionClientEndpointId();
			PlaySessionEvent.PlayPackageName = PIEWorld->GetOutermost()->GetFName();
			PlaySessionEvent.bIsSimulating = InIsSimulating;
			Session->SendCustomEvent(PlaySessionEvent, Session->GetSessionServerEndpointId(), EConcertMessageFlags::ReliableOrdered);

			// Apply transactions to the PIE/SIE world
			HandleAssetLoaded(PIEWorld);
		}
	}
}

void FConcertClientWorkspace::HandleSwitchBeginPIEAndSIE(const bool InIsSimulating)
{
	if (FWorldContext* PIEWorldContext = GUnrealEd->GetPIEWorldContext())
	{
		if (UWorld* PIEWorld = PIEWorldContext->World())
		{
			// Track open PIE/SIE sessions so the server can discard them once everyone leaves
			FConcertPlaySessionEvent PlaySessionEvent;
			PlaySessionEvent.EventType = EConcertPlaySessionEventType::SwitchPlay;
			PlaySessionEvent.PlayEndpointId = Session->GetSessionClientEndpointId();
			PlaySessionEvent.PlayPackageName = PIEWorld->GetOutermost()->GetFName();
			PlaySessionEvent.bIsSimulating = InIsSimulating;
			Session->SendCustomEvent(PlaySessionEvent, Session->GetSessionServerEndpointId(), EConcertMessageFlags::ReliableOrdered);
		}
	}
}

void FConcertClientWorkspace::HandleEndPIE(const bool InIsSimulating)
{
	if (FWorldContext* PIEWorldContext = GUnrealEd->GetPIEWorldContext())
	{
		if (UWorld* PIEWorld = PIEWorldContext->World())
		{
			// Track open PIE/SIE sessions so the server can discard them once everyone leaves
			FConcertPlaySessionEvent PlaySessionEvent;
			PlaySessionEvent.EventType = EConcertPlaySessionEventType::EndPlay;
			PlaySessionEvent.PlayEndpointId = Session->GetSessionClientEndpointId();
			PlaySessionEvent.PlayPackageName = PIEWorld->GetOutermost()->GetFName();
			PlaySessionEvent.bIsSimulating = InIsSimulating;
			Session->SendCustomEvent(PlaySessionEvent, Session->GetSessionServerEndpointId(), EConcertMessageFlags::ReliableOrdered);
		}
	}
}

void FConcertClientWorkspace::HandleTransactionStateChanged(const FTransactionContext& InTransactionContext, const ETransactionStateEventType InTransactionState)
{
	if (TransactionManager.IsValid())
	{
		TransactionManager->HandleTransactionStateChanged(InTransactionContext, InTransactionState);
	}
}

void FConcertClientWorkspace::HandleObjectTransacted(UObject* InObject, const FTransactionObjectEvent& InTransactionEvent)
{
	if (TransactionManager.IsValid())
	{
		TransactionManager->HandleObjectTransacted(InObject, InTransactionEvent);
	}
}

#endif	// WITH_EDITOR

void FConcertClientWorkspace::OnEndFrame()
{
	if (bFinalizeWorkspaceSyncRequested)
	{
		bFinalizeWorkspaceSyncRequested = false;

		// Make sure any new packages are loaded
		if (InitialSyncSlowTask.IsValid())
		{
			InitialSyncSlowTask->EnterProgressFrame(0.0f, LOCTEXT("ApplyingSynchronizedPackages", "Applying Synchronized Packages..."));
		}
		HotReloadPendingPackages();
		PurgePendingPackages();

		// Replay any "live" transactions
		if (InitialSyncSlowTask.IsValid())
		{
			InitialSyncSlowTask->EnterProgressFrame(0.0f, LOCTEXT("ApplyingSynchronizedTransactions", "Applying Synchronized Transactions..."));
		}
		TransactionManager->ReplayAllTransactions();

		// We process all pending transactions we just replayed before finalizing the sync to prevent package being loaded as a result to trigger replaying transactions again
		TransactionManager->ProcessPending();

		// Finalize the sync
		bHasSyncedWorkspace = true;
		InitialSyncSlowTask.Reset();
	}

	if (bHasSyncedWorkspace)
	{
		HotReloadPendingPackages();
		PurgePendingPackages();

		if (TransactionManager.IsValid())
		{
			TransactionManager->ProcessPending();
		}
	}
}

void FConcertClientWorkspace::HandleWorkspaceSyncTransactionEvent(const FConcertSessionContext& Context, const FConcertWorkspaceSyncTransactionEvent& Event)
{
	// Update slow task dialog
	if (InitialSyncSlowTask.IsValid())
	{
		InitialSyncSlowTask->TotalAmountOfWork = InitialSyncSlowTask->CompletedWork + Event.RemainingWork + 1;
		InitialSyncSlowTask->EnterProgressFrame(FMath::Min<float>(Event.RemainingWork, 1.0f), FText::Format(LOCTEXT("SynchronizedTransactionFmt", "Synchronized Transaction {0}..."), Event.TransactionIndex));
	}

	// Apply transaction to ledger
	TransactionManager->HandleRemoteTransaction(Event.TransactionIndex, Event.TransactionData, bHasSyncedWorkspace);
}

void FConcertClientWorkspace::HandleWorkspaceSyncPackageEvent(const FConcertSessionContext& Context, const FConcertWorkspaceSyncPackageEvent& Event)
{
	// Update slow task dialog
	if (InitialSyncSlowTask.IsValid())
	{
		InitialSyncSlowTask->TotalAmountOfWork = InitialSyncSlowTask->CompletedWork + Event.RemainingWork + 1;
		InitialSyncSlowTask->EnterProgressFrame(FMath::Min<float>(Event.RemainingWork, 1.0f), FText::Format(LOCTEXT("SynchronizedPackageFmt", "Synchronized Package {0}..."), FText::FromName(Event.Package.Info.PackageName)));
	}

	switch (Event.Package.Info.PackageUpdateType)
	{
	case EConcertPackageUpdateType::Added:
	case EConcertPackageUpdateType::Saved:
		if (Event.Package.PackageData.Num() > 0)
		{
			SavePackageFile(Event.Package);
		}
		break;

	case EConcertPackageUpdateType::Renamed:
		DeletePackageFile(Event.Package);
		if (Event.Package.PackageData.Num() > 0)
		{
			SavePackageFile(Event.Package);
		}
		break;

	case EConcertPackageUpdateType::Deleted:
		DeletePackageFile(Event.Package);
		break;

	default:
		break;
	}

	PackageLedger->AddPackage(Event.PackageRevision, Event.Package);

	TransactionManager->GetMutableLedger().TrimLiveTransactions(Event.Package.Info.NextTransactionIndexWhenSaved, Event.Package.Info.PackageName);
}

void FConcertClientWorkspace::HandleWorkspaceSyncLockEvent(const FConcertSessionContext& Context, const FConcertWorkspaceSyncLockEvent& Event)
{
	// Initial sync of the locked resources
	LockedResources = Event.LockedResources;
}

void FConcertClientWorkspace::HandleWorkspaceInitialSyncCompletedEvent(const FConcertSessionContext& Context, const FConcertWorkspaceInitialSyncCompletedEvent& Event)
{
	// Request the sync to finalize at the end of the next frame
	bFinalizeWorkspaceSyncRequested = true;
	OnWorkspaceSyncedDelegate.Broadcast();
}

void FConcertClientWorkspace::HandleResourceLockEvent(const FConcertSessionContext& Context, const FConcertResourceLockEvent& Event)
{
	switch (Event.LockType)
	{
	case EConcertResourceLockType::Lock:
		for (const FName& ResourceName : Event.ResourceNames)
		{
			LockedResources.FindOrAdd(ResourceName) = Event.ClientId;
		}
		break;
	case EConcertResourceLockType::Unlock:
		for (const FName& ResourceName : Event.ResourceNames)
		{
			LockedResources.Remove(ResourceName);
		}
		break;
	default:
		// no-op
		break;
	}
}

void FConcertClientWorkspace::SavePackageFile(const FConcertPackage& Package)
{
	// This path should only be taken for non-cooked targets for now
	check(!FPlatformProperties::RequiresCookedData());

	FString PackageName = Package.Info.PackageName.ToString();
	ConcertSyncClientUtil::FlushPackageLoading(PackageName);

	// Convert long package name to filename
	FString PackageFilename;
	bool bSuccess = FPackageName::TryConvertLongPackageNameToFilename(PackageName, PackageFilename, Package.Info.PackageFileExtension);
	if (bSuccess)
	{
		// Overwrite the file on disk
		FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*PackageFilename, false);
		bSuccess = FFileHelper::SaveArrayToFile(Package.PackageData, *PackageFilename);
	}

	if (bSuccess)
	{
		PackagesPendingHotReload.Add(Package.Info.PackageName);
		PackagesPendingPurge.Remove(Package.Info.PackageName);
	}
}

void FConcertClientWorkspace::DeletePackageFile(const FConcertPackage& Package)
{
	// This path should only be taken for non-cooked targets for now
	check(!FPlatformProperties::RequiresCookedData());

	FString PackageName = Package.Info.PackageName.ToString();
	ConcertSyncClientUtil::FlushPackageLoading(PackageName);

	// Convert long package name to filename
	FString PackageFilenameWildcard;
	bool bSuccess = FPackageName::TryConvertLongPackageNameToFilename(PackageName, PackageFilenameWildcard, TEXT(".*"));
	if (bSuccess)
	{
		// Delete the file on disk
		// We delete any files associated with this package as it may have changed extension type during the session
		TArray<FString> FoundPackageFilenames;
		IFileManager::Get().FindFiles(FoundPackageFilenames, *PackageFilenameWildcard, /*Files*/true, /*Directories*/false);
		const FString PackageDirectory = FPaths::GetPath(PackageFilenameWildcard);
		for (const FString& FoundPackageFilename : FoundPackageFilenames)
		{
			bSuccess |= IFileManager::Get().Delete(*(PackageDirectory / FoundPackageFilename), false, true, true);
		}
	}

	if (bSuccess)
	{
		PackagesPendingPurge.Add(Package.Info.PackageName);
		PackagesPendingHotReload.Remove(Package.Info.PackageName);
	}
}

bool FConcertClientWorkspace::CanHotReloadOrPurge() const
{
	return ConcertSyncClientUtil::CanPerformBlockingAction() && !Session->IsSuspended();
}

void FConcertClientWorkspace::HotReloadPendingPackages()
{
	if (CanHotReloadOrPurge())
	{
		ConcertSyncClientUtil::HotReloadPackages(PackagesPendingHotReload);
		PackagesPendingHotReload.Reset();
	}
}

void FConcertClientWorkspace::PurgePendingPackages()
{
	if (CanHotReloadOrPurge())
	{
		ConcertSyncClientUtil::PurgePackages(PackagesPendingPurge);
		PackagesPendingPurge.Reset();
	}
}

bool FConcertClientWorkspace::IsAssetModifiedByOtherClients(const FName& AssetName, int* OutOtherClientsWithModifNum, TArray<FConcertClientInfo>* OutOtherClientsWithModifInfo, int OtherClientsWithModifMaxFetchNum) const
{
	return LiveTransactionAuthors->IsPackageAuthoredByOtherClients(AssetName, OutOtherClientsWithModifNum, OutOtherClientsWithModifInfo, OtherClientsWithModifMaxFetchNum);
}

#undef LOCTEXT_NAMESPACE
