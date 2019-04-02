// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConcertClientTransactionManager.h"
#include "IConcertSession.h"
#include "ConcertLogGlobal.h"
#include "ConcertSyncSettings.h"
#include "ConcertSyncArchives.h"
#include "ConcertSyncClientUtil.h"
#include "ConcertTransactionLedger.h"
#include "Scratchpad/ConcertScratchpad.h"

#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "AssetRegistryModule.h"

#include "Engine/World.h"
#include "Engine/Level.h"
#include "RenderingThread.h"
#include "Misc/ScopedSlowTask.h"

#if WITH_EDITOR
	#include "Editor.h"
	#include "UnrealEdGlobals.h"
	#include "Editor/UnrealEdEngine.h"
	#include "Editor/TransBuffer.h"
	#include "Layers/ILayers.h"
#endif

#define LOCTEXT_NAMESPACE "ConcertClientTransactionManager"

FConcertClientTransactionManager::FConcertClientTransactionManager(TSharedRef<IConcertClientSession> InSession)
	: Session(InSession)
	, bIgnoreTransaction(false)
{
	TransactionLedger = MakeUnique<FConcertTransactionLedger>(EConcertTransactionLedgerType::Transient, InSession->GetSessionWorkingDirectory());

	// snapshot event are handled directly, finalized event however are handled by the workspace
	Session->RegisterCustomEventHandler<FConcertTransactionSnapshotEvent>(this, &FConcertClientTransactionManager::HandleTransactionEvent<FConcertTransactionSnapshotEvent>);
	Session->RegisterCustomEventHandler<FConcertTransactionRejectedEvent>(this, &FConcertClientTransactionManager::HandleTransactionRejectedEvent);


#if WITH_EDITOR
	// if the manager is created while a transaction is ongoing, add it as pending
	if (GUndo != nullptr)
	{
		// Start a new pending transaction
		HandleTransactionStateChanged(GUndo->GetContext(), ETransactionStateEventType::TransactionStarted);
	}
#endif
}

FConcertClientTransactionManager::~FConcertClientTransactionManager()
{
	TransactionLedger.Reset();

	Session->UnregisterCustomEventHandler<FConcertTransactionSnapshotEvent>();
	Session->UnregisterCustomEventHandler<FConcertTransactionRejectedEvent>();

}

const FConcertTransactionLedger& FConcertClientTransactionManager::GetLedger() const
{
	return *TransactionLedger;
}

FConcertTransactionLedger& FConcertClientTransactionManager::GetMutableLedger()
{
	return *TransactionLedger;
}

void FConcertClientTransactionManager::ReplayAllTransactions()
{
	const TArray<uint64> TransactionIndices = TransactionLedger->GetAllLiveTransactions();
	if (TransactionIndices.Num() > 0)
	{
		FScopedSlowTask SlowTask(TransactionIndices.Num(), LOCTEXT("ReplayingTransactions", "Replaying Transactions..."));
		SlowTask.MakeDialogDelayed(1.0f);

		FPendingTransactionToProcessContext TransactionContext;
		TransactionContext.bIsRequired = true;

		for (uint64 TransactionIndex : TransactionIndices)
		{
			SlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("ReplayingTransactionFmt", "Replaying Transaction {0}"), TransactionIndex));

			FStructOnScope Transaction;
			if (TransactionLedger->FindTransaction(TransactionIndex, Transaction))
			{
				PendingTransactionsToProcess.Emplace(TransactionContext, MoveTemp(Transaction));
			}
		}
	}
}

void FConcertClientTransactionManager::ReplayTransactions(const FName InPackageName)
{
	const TArray<uint64> TransactionIndices = TransactionLedger->GetLiveTransactions(InPackageName);
	if (TransactionIndices.Num() > 0)
	{
		FScopedSlowTask SlowTask(TransactionIndices.Num(), FText::Format(LOCTEXT("ReplayingTransactionsForPackageFmt", "Replaying Transactions for {0}..."), FText::FromName(InPackageName)));
		SlowTask.MakeDialogDelayed(1.0f);

		FPendingTransactionToProcessContext TransactionContext;
		TransactionContext.bIsRequired = true;
		TransactionContext.PackagesToProcess.Add(InPackageName);

		for (uint64 TransactionIndex : TransactionIndices)
		{
			SlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("ReplayingTransactionForPackageFmt", "Replaying Transaction {0} for {1}"), TransactionIndex, FText::FromName(InPackageName)));

			FStructOnScope Transaction;
			if (TransactionLedger->FindTransaction(TransactionIndex, Transaction))
			{
				PendingTransactionsToProcess.Emplace(TransactionContext, MoveTemp(Transaction));
			}
		}
	}
}

void FConcertClientTransactionManager::HandleRemoteTransaction(const uint64 InTransactionIndex, const TArray<uint8>& InTransactionData, const bool bApply)
{
	TransactionLedger->AddSerializedTransaction(InTransactionIndex, InTransactionData);

	if (bApply)
	{
		FStructOnScope Transaction;
		if (TransactionLedger->FindTransaction(InTransactionIndex, Transaction))
		{
			checkf(Transaction.GetStruct()->IsChildOf(FConcertTransactionEventBase::StaticStruct()), TEXT("HandleRemoteTransaction can only be used with types deriving from FConcertTransactionEventBase"));

			// Ignore this transaction if we generated it
			const FConcertTransactionEventBase* InTransactionEvent = (const FConcertTransactionEventBase*)Transaction.GetStructMemory();
			if (InTransactionEvent->TransactionEndpointId != Session->GetSessionClientEndpointId())
			{
				FPendingTransactionToProcessContext TransactionContext;
				TransactionContext.bIsRequired = true;

				PendingTransactionsToProcess.Emplace(TransactionContext, MoveTemp(Transaction));
			}
		}
	}
}

void FConcertClientTransactionManager::HandleTransactionStateChanged(const FTransactionContext& InTransactionContext, const ETransactionStateEventType InTransactionState)
{
	if (bIgnoreTransaction)
	{
		return;
	}

	{
		const TCHAR* TransactionStateString = TEXT("");
		switch (InTransactionState)
		{
#define ENUM_TO_STRING(ENUM)						\
		case ETransactionStateEventType::ENUM:		\
			TransactionStateString = TEXT(#ENUM);	\
				break;
		ENUM_TO_STRING(TransactionStarted)
		ENUM_TO_STRING(TransactionCanceled)
		ENUM_TO_STRING(TransactionFinalized)
		ENUM_TO_STRING(UndoRedoStarted)
		ENUM_TO_STRING(UndoRedoFinalized)
#undef ENUM_TO_STRING
		default:
			break;
		}

		UE_LOG(LogConcert, VeryVerbose, TEXT("Transaction %s (%s): %s"), *InTransactionContext.TransactionId.ToString(), *InTransactionContext.OperationId.ToString(), TransactionStateString);
	}

	// Create, finalize, or remove a pending transaction
	if (InTransactionState == ETransactionStateEventType::TransactionStarted || InTransactionState == ETransactionStateEventType::UndoRedoStarted)
	{
		// Start a new pending transaction
		check(!PendingTransactionsToSend.Contains(InTransactionContext.OperationId));
		PendingTransactionsToSendOrder.Add(InTransactionContext.OperationId);
		PendingTransactionsToSend.Add(InTransactionContext.OperationId, FPendingTransactionToSend(InTransactionContext.TransactionId, InTransactionContext.OperationId, InTransactionContext.PrimaryObject));
	}
	else if (InTransactionState == ETransactionStateEventType::TransactionFinalized || InTransactionState == ETransactionStateEventType::UndoRedoFinalized)
	{
		// Finalize an existing pending transaction so it can be sent
		FPendingTransactionToSend& PendingTransaction = PendingTransactionsToSend.FindChecked(InTransactionContext.OperationId);
		PendingTransaction.PrimaryObject = InTransactionContext.PrimaryObject;
		PendingTransaction.bIsFinalized = true;
		PendingTransaction.Title = InTransactionContext.Title;
	}
	else if (InTransactionState == ETransactionStateEventType::TransactionCanceled)
	{
		// We receive an object undo event before a transaction is canceled to restore the object to its original state
		// We need to send this update if we sent any snapshot updates for this transaction (to undo the snapshot changes), otherwise we can just drop this transaction as no changes have propagated
		FPendingTransactionToSend& PendingTransaction = PendingTransactionsToSend.FindChecked(InTransactionContext.OperationId);
		if (PendingTransaction.LastSnapshotTimeSeconds == 0)
		{
			// Note: We don't remove this from PendingTransactionsToSendOrder as we just skip transactions missing from the map (assuming they've been canceled).
			PendingTransactionsToSend.Remove(InTransactionContext.OperationId);
		}
		else
		{
			// Finalize the transaction so it can be sent
			PendingTransaction.PrimaryObject = InTransactionContext.PrimaryObject;
			PendingTransaction.bIsFinalized = true;
		}
	}
}

void FConcertClientTransactionManager::HandleObjectTransacted(UObject* InObject, const FTransactionObjectEvent& InTransactionEvent)
{
	if (bIgnoreTransaction)
	{
		return;
	}

	UPackage* ChangedPackage = InObject->GetOutermost();
	ETransactionFilterResult FilterResult = ApplyTransactionFilters(InObject, ChangedPackage);
	
	// TODO: This needs to send both editor-only and non-editor-only payload data to the server, which will forward only the correct part to cooked and non-cooked clients
	bool bIncludeEditorOnlyProperties = true;

	{
		const TCHAR* ObjectEventString = TEXT("");
		switch (InTransactionEvent.GetEventType())
		{
#define ENUM_TO_STRING(ENUM)						\
		case ETransactionObjectEventType::ENUM:		\
			ObjectEventString = TEXT(#ENUM);		\
				break;
		ENUM_TO_STRING(UndoRedo)
		ENUM_TO_STRING(Finalized)
		ENUM_TO_STRING(Snapshot)
#undef ENUM_TO_STRING
		default:
			break;
		}

		UE_LOG(LogConcert, VeryVerbose,
			TEXT("Transaction %s (%s, %s):%s %s:%s (%s property changes, %s object changes)"), 
			*InTransactionEvent.GetTransactionId().ToString(),
			*InTransactionEvent.GetOperationId().ToString(),
			ObjectEventString,
			(FilterResult == ETransactionFilterResult::ExcludeObject ? TEXT(" FILTERED OBJECT: ") : TEXT("")),
			*InObject->GetClass()->GetName(),
			*InObject->GetPathName(), 
			(InTransactionEvent.HasPropertyChanges() ? TEXT("has") : TEXT("no")), 
			(InTransactionEvent.HasNonPropertyChanges() ? TEXT("has") : TEXT("no"))
			);
	}

	const FConcertObjectId ObjectId = FConcertObjectId(*InObject->GetClass()->GetPathName(), InTransactionEvent.GetOriginalObjectOuterPathName(), InTransactionEvent.GetOriginalObjectName(), InObject->GetFlags());
	FPendingTransactionToSend& PendingTransaction = PendingTransactionsToSend.FindChecked(InTransactionEvent.GetOperationId());

	// If the object is excluded or exclude the whole transaction add it to the excluded list
	if (FilterResult != ETransactionFilterResult::IncludeObject)
	{
		PendingTransaction.bIsExcluded |= FilterResult == ETransactionFilterResult::ExcludeTransaction;
		PendingTransaction.ExcludedObjectUpdates.Add(ObjectId);
		return;
	}

	const FName NewObjectName = InTransactionEvent.HasNameChange() ? InObject->GetFName() : FName();
	const FName NewObjectOuterPathName = (InTransactionEvent.HasOuterChange() && InObject->GetOuter()) ? FName(*InObject->GetOuter()->GetPathName()) : FName();
	const TArray<FName> RootPropertyNames = ConcertSyncClientUtil::GetRootProperties(InTransactionEvent.GetChangedProperties());
	TSharedPtr<ITransactionObjectAnnotation> TransactionAnnotation = InTransactionEvent.GetAnnotation();

	auto ObjectIdsMatch = [](const FConcertObjectId& One, const FConcertObjectId& Two) -> bool
	{
		return One.ObjectClassPathName == Two.ObjectClassPathName
			&& One.ObjectOuterPathName == Two.ObjectOuterPathName
			&& One.ObjectName == Two.ObjectName;
	};

	auto GetObjectPathDepth = [](UObject* InObjToTest) -> int32
	{
		int32 Depth = 0;
		for (UObject* Outer = InObjToTest; Outer; Outer = Outer->GetOuter())
		{
			++Depth;
		}
		return Depth;
	};

	// Track which packages were changed
	PendingTransaction.ModifiedPackages.AddUnique(ChangedPackage->GetFName());

	// Add this object change to its pending transaction
	if (InTransactionEvent.GetEventType() == ETransactionObjectEventType::Snapshot)
	{
		// Merge the snapshot property changes into pending snapshot list
		if (InTransactionEvent.HasPropertyChanges() || TransactionAnnotation.IsValid())
		{
			// Find or add an entry for this object
			FConcertExportedObject* ObjectUpdatePtr = nullptr;
			for (FConcertExportedObject& ObjectUpdate : PendingTransaction.SnapshotObjectUpdates)
			{
				if (ObjectIdsMatch(ObjectId, ObjectUpdate.ObjectId))
				{
					ObjectUpdatePtr = &ObjectUpdate;
					break;
				}
			}
			if (!ObjectUpdatePtr)
			{
				ObjectUpdatePtr = &PendingTransaction.SnapshotObjectUpdates.AddDefaulted_GetRef();
				ObjectUpdatePtr->ObjectId = ObjectId;
				ObjectUpdatePtr->ObjectPathDepth = GetObjectPathDepth(InObject);
				ObjectUpdatePtr->ObjectData.bAllowCreate = false;
				ObjectUpdatePtr->ObjectData.bIsPendingKill = InObject->IsPendingKill();
			}

			if (TransactionAnnotation.IsValid())
			{
				ObjectUpdatePtr->SerializedAnnotationData.Reset();
				FConcertSyncObjectWriter AnnotationWriter(nullptr, InObject, ObjectUpdatePtr->SerializedAnnotationData, bIncludeEditorOnlyProperties, true);
				TransactionAnnotation->Serialize(AnnotationWriter);
			}

			// Find or add an update for each property
			for (const FName& RootPropertyName : RootPropertyNames)
			{
				UProperty* RootProperty = ConcertSyncClientUtil::GetExportedProperty(InObject->GetClass(), RootPropertyName, bIncludeEditorOnlyProperties);
				if (!RootProperty)
				{
					continue;
				}

				FConcertSerializedPropertyData* PropertyDataPtr = nullptr;
				for (FConcertSerializedPropertyData& PropertyData : ObjectUpdatePtr->PropertyDatas)
				{
					if (RootPropertyName == PropertyData.PropertyName)
					{
						PropertyDataPtr = &PropertyData;
					}
				}
				if (!PropertyDataPtr)
				{
					PropertyDataPtr = &ObjectUpdatePtr->PropertyDatas.AddDefaulted_GetRef();
					PropertyDataPtr->PropertyName = RootPropertyName;
				}

				PropertyDataPtr->SerializedData.Reset();
				ConcertSyncClientUtil::SerializeProperty(nullptr, InObject, RootProperty, bIncludeEditorOnlyProperties, PropertyDataPtr->SerializedData);
			}
		}
	}
	else
	{
		FConcertExportedObject& ObjectUpdate = PendingTransaction.FinalizedObjectUpdates.AddDefaulted_GetRef();
		ObjectUpdate.ObjectId = ObjectId;
		ObjectUpdate.ObjectPathDepth = GetObjectPathDepth(InObject);
		ObjectUpdate.ObjectData.bAllowCreate = InTransactionEvent.HasPendingKillChange() && !InObject->IsPendingKill();
		ObjectUpdate.ObjectData.bIsPendingKill = InObject->IsPendingKill();
		ObjectUpdate.ObjectData.NewName = NewObjectName;
		ObjectUpdate.ObjectData.NewOuterPathName = NewObjectOuterPathName;

		if (TransactionAnnotation.IsValid())
		{
			FConcertSyncObjectWriter AnnotationWriter(&PendingTransaction.FinalizedLocalIdentifierTable, InObject, ObjectUpdate.SerializedAnnotationData, bIncludeEditorOnlyProperties, false);
			TransactionAnnotation->Serialize(AnnotationWriter);
		}

		// If this object changed from being pending kill to not being pending kill, we have to send a full object update (including all properties), rather than attempt a delta-update
		const bool bForceFullObjectUpdate = InTransactionEvent.HasPendingKillChange() && !InObject->IsPendingKill();

		if (bForceFullObjectUpdate || InTransactionEvent.HasNonPropertyChanges(/*SerializationOnly*/true))
		{
			ConcertSyncClientUtil::SerializeObject(&PendingTransaction.FinalizedLocalIdentifierTable, InObject, bForceFullObjectUpdate ? nullptr : &RootPropertyNames, bIncludeEditorOnlyProperties, ObjectUpdate.ObjectData.SerializedData);
		}
		else
		{
			for (const FName& RootPropertyName : RootPropertyNames)
			{
				UProperty* RootProperty = ConcertSyncClientUtil::GetExportedProperty(InObject->GetClass(), RootPropertyName, bIncludeEditorOnlyProperties);
				if (RootProperty)
				{
					FConcertSerializedPropertyData& PropertyData = ObjectUpdate.PropertyDatas.AddDefaulted_GetRef();
					PropertyData.PropertyName = RootPropertyName;
					ConcertSyncClientUtil::SerializeProperty(&PendingTransaction.FinalizedLocalIdentifierTable, InObject, RootProperty, bIncludeEditorOnlyProperties, PropertyData.SerializedData);
				}
			}
		}
	}
}

void FConcertClientTransactionManager::ProcessPending()
{
	if (PendingTransactionsToProcess.Num() > 0)
	{
		if (CanProcessTransactionEvent())
		{
			for (auto It = PendingTransactionsToProcess.CreateIterator(); It; ++It)
			{
				// PendingTransaction is moved out of the array since `ProcessTransactionEvent` can add more PendingTransactions through loading packages which would make a reference dangle
				FPendingTransactionToProcess PendingTransaction = MoveTemp(*It);
				ProcessTransactionEvent(PendingTransaction.Context, PendingTransaction.EventData);
				It.RemoveCurrent();
			}
		}
		else
		{
			PendingTransactionsToProcess.RemoveAll([](const FPendingTransactionToProcess& PendingTransaction)
				{
					return !PendingTransaction.Context.bIsRequired;
				});
		}
	}

	SendPendingTransactionEvents();
}

template <typename EventType>
void FConcertClientTransactionManager::HandleTransactionEvent(const FConcertSessionContext& InEventContext, const EventType& InEvent)
{
	static_assert(TIsDerivedFrom<EventType, FConcertTransactionEventBase>::IsDerived, "HandleTransactionEvent can only be used with types deriving from FConcertTransactionEventBase");

	FPendingTransactionToProcessContext TransactionContext;
	TransactionContext.bIsRequired = EnumHasAnyFlags(InEventContext.MessageFlags, EConcertMessageFlags::ReliableOrdered);

	PendingTransactionsToProcess.Emplace(TransactionContext, EventType::StaticStruct(), &InEvent);
}

void FConcertClientTransactionManager::HandleTransactionRejectedEvent(const FConcertSessionContext& InEventContext, const FConcertTransactionRejectedEvent& InEvent)
{
#if WITH_EDITOR
	UTransBuffer* TransBuffer = GUnrealEd ? Cast<UTransBuffer>(GUnrealEd->Trans) : nullptr;
	if (TransBuffer == nullptr)
	{
		return;
	}

	// For this undo operation, squelch the notification, also prevent us from recording
	TGuardValue<bool> IgnoreTransactionScope(bIgnoreTransaction, true);
	bool bOrigSquelchTransactionNotification = GEditor && GEditor->bSquelchTransactionNotification;
	if (GEditor)
	{
		GEditor->bSquelchTransactionNotification = true;
	}
	
	// if the transaction to undo is the current one, end it.
	if (GUndo && GUndo->GetContext().TransactionId == InEvent.TransactionId)
	{
		// Cancel doesn't entirely do what we want here as it will just, remove the current transaction without restoring object state
		// This shouldn't happen however, since we only undo finalized transaction
		ensureMsgf(false, TEXT("Received a Concert undo request for an ongoing transaction."));
		TransBuffer->End();
		TransBuffer->Undo(false);
	}
	// Otherwise undo operations until the requested transaction has been undone.
	else
	{		
		int32 ReversedQueueIndex = TransBuffer->FindTransactionIndex(InEvent.TransactionId);
		if (ReversedQueueIndex != INDEX_NONE)
		{
			ReversedQueueIndex = TransBuffer->GetQueueLength() - TransBuffer->GetUndoCount() - ReversedQueueIndex;
			int32 UndoCount = 0;

			// if we get a positive number, then we need to undo
			if (ReversedQueueIndex > 0)
			{
				while (UndoCount < ReversedQueueIndex)
				{
					TransBuffer->Undo();
					++UndoCount;
				}
			}
			// Otherwise we need to redo, as the transaction has already been undone
			else
			{
				ReversedQueueIndex = -ReversedQueueIndex + 1;
				while (UndoCount < ReversedQueueIndex)
				{
					TransBuffer->Redo();
					++UndoCount;
				}
			}
		}
	}

	if (GEditor)
	{
		GEditor->bSquelchTransactionNotification = bOrigSquelchTransactionNotification;
	}
#endif
}

bool FConcertClientTransactionManager::CanProcessTransactionEvent() const
{
	return ConcertSyncClientUtil::CanPerformBlockingAction() && !Session->IsSuspended();
}

void FConcertClientTransactionManager::ProcessTransactionEvent(const FPendingTransactionToProcessContext& InContext, const FStructOnScope& InEvent)
{
	const FConcertTransactionEventBase& TransactionEvent = *(const FConcertTransactionEventBase*)InEvent.GetStructMemory();
	if (!ShouldProcessTransactionEvent(TransactionEvent, InContext.bIsRequired))
	{
		UE_LOG(LogConcert, VeryVerbose, TEXT("Dropping transaction for '%s' (index %d) as it arrived out-of-order"), *TransactionEvent.TransactionId.ToString(), TransactionEvent.TransactionUpdateIndex);
		return;
	}

#define PROCESS_OBJECT_UPDATE_EVENT(EventName)																\
	if (InEvent.GetStruct() == FConcert##EventName::StaticStruct())											\
	{																										\
		return Process##EventName(InContext, static_cast<const FConcert##EventName&>(TransactionEvent));	\
	}

	PROCESS_OBJECT_UPDATE_EVENT(TransactionFinalizedEvent)
	PROCESS_OBJECT_UPDATE_EVENT(TransactionSnapshotEvent)

#undef PROCESS_OBJECT_UPDATE_EVENT
}

namespace ProcessTransactionEventUtil
{

#if WITH_EDITOR
/** Utility struct to suppress editor transaction notifications and fire the correct delegates */
struct FEditorTransactionNotification
{
	FEditorTransactionNotification(FTransactionContext&& InTransactionContext)
		: TransactionContext(MoveTemp(InTransactionContext))
		, TransBuffer(GUnrealEd ? Cast<UTransBuffer>(GUnrealEd->Trans) : nullptr)
		, bOrigSquelchTransactionNotification(GEditor && GEditor->bSquelchTransactionNotification)
		, bOrigNotifyUndoRedoSelectionChange(GEditor && GEditor->bNotifyUndoRedoSelectionChange)
	{
	}

	void PreUndo()
	{
		if (GEditor)
		{
			GEditor->bSquelchTransactionNotification = true;
			GEditor->bNotifyUndoRedoSelectionChange = false;
			if (TransBuffer)
			{
				TransBuffer->OnBeforeRedoUndo().Broadcast(TransactionContext);
			}
		}
	}

	void PostUndo()
	{
		if (GEditor)
		{
			if (TransBuffer)
			{
				TransBuffer->OnRedo().Broadcast(TransactionContext, true);
			}
			GEditor->bSquelchTransactionNotification = bOrigSquelchTransactionNotification;
			GEditor->bNotifyUndoRedoSelectionChange = bOrigNotifyUndoRedoSelectionChange;
		}
	}

	void HandleObjectTransacted(UObject* InTransactionObject, const FConcertExportedObject& InObjectUpdate, const TSharedPtr<ITransactionObjectAnnotation>& InTransactionAnnotation)
	{
		if (GUnrealEd)
		{
			FTransactionObjectEvent TransactionObjectEvent;
			{
				FTransactionObjectDeltaChange DeltaChange;
				DeltaChange.bHasNameChange = !InObjectUpdate.ObjectData.NewName.IsNone();
				DeltaChange.bHasOuterChange = !InObjectUpdate.ObjectData.NewOuterPathName.IsNone();
				DeltaChange.bHasPendingKillChange = InObjectUpdate.ObjectData.bIsPendingKill != InTransactionObject->IsPendingKill();
				DeltaChange.bHasNonPropertyChanges = InObjectUpdate.ObjectData.SerializedData.Num() > 0;
				for (const FConcertSerializedPropertyData& PropertyData : InObjectUpdate.PropertyDatas)
				{
					DeltaChange.ChangedProperties.Add(PropertyData.PropertyName);
				}
				TransactionObjectEvent = FTransactionObjectEvent(TransactionContext.TransactionId, TransactionContext.OperationId, ETransactionObjectEventType::UndoRedo, DeltaChange, InTransactionAnnotation, InTransactionObject->GetFName(), *InTransactionObject->GetPathName(), InObjectUpdate.ObjectId.ObjectOuterPathName, FName(*InTransactionObject->GetClass()->GetPathName()));
			}
			GUnrealEd->HandleObjectTransacted(InTransactionObject, TransactionObjectEvent);
		}
	}

	FTransactionContext TransactionContext;
	UTransBuffer* TransBuffer;
	bool bOrigSquelchTransactionNotification;
	bool bOrigNotifyUndoRedoSelectionChange;
};
#endif

void ProcessTransactionEvent(const FConcertTransactionEventBase& InEvent, const TArray<FName>& InPackagesToProcess, FConcertLocalIdentifierTable* InLocalIdentifierTablePtr, const bool bIsSnapshot)
{
	// Transactions are applied in multiple-phases...
	//	1) Find or create all objects in the transaction (to handle object-interdependencies in the serialized data)
	//	2) Notify all objects that they are about to be changed (via PreEditUndo)
	//	3) Update the state of all objects
	//	4) Notify all objects that they were changed (via PostEditUndo) - also finish spawning any new actors now that they have the correct state

	// --------------------------------------------------------------------------------------------------------------------
	// Phase 1
	// --------------------------------------------------------------------------------------------------------------------
	bool bObjectsDeleted = false;
	TArray<ConcertSyncClientUtil::FGetObjectResult, TInlineAllocator<8>> TransactionObjects;
	TransactionObjects.AddDefaulted(InEvent.ExportedObjects.Num());
	{
		// Sort the object list so that outers will be created before their child objects
		typedef TTuple<int32, const FConcertExportedObject*> FConcertExportedIndexAndObject;
		TArray<FConcertExportedIndexAndObject, TInlineAllocator<8>> SortedExportedObjects;
		SortedExportedObjects.Reserve(InEvent.ExportedObjects.Num());
		for (int32 ObjectIndex = 0; ObjectIndex < InEvent.ExportedObjects.Num(); ++ObjectIndex)
		{
			SortedExportedObjects.Add(MakeTuple(ObjectIndex, &InEvent.ExportedObjects[ObjectIndex]));
		}

		SortedExportedObjects.StableSort([](const FConcertExportedIndexAndObject& One, const FConcertExportedIndexAndObject& Two) -> bool
		{
			const FConcertExportedObject& OneObjectUpdate = *One.Value;
			const FConcertExportedObject& TwoObjectUpdate = *Two.Value;
			return OneObjectUpdate.ObjectPathDepth < TwoObjectUpdate.ObjectPathDepth;
		});

		// Find or create each object, populating TransactionObjects in the original order (not the sorted order)
		for (const FConcertExportedIndexAndObject& SortedExportedObjectsPair : SortedExportedObjects)
		{
			const int32 ObjectUpdateIndex = SortedExportedObjectsPair.Key;
			const FConcertExportedObject& ObjectUpdate = *SortedExportedObjectsPair.Value;
			ConcertSyncClientUtil::FGetObjectResult& TransactionObjectRef = TransactionObjects[ObjectUpdateIndex];

			// Is this object excluded? We exclude certain packages when re-applying live transactions on a package load
			if (InPackagesToProcess.Num() > 0)
			{
				const FName ObjectOuterPathName = ObjectUpdate.ObjectData.NewOuterPathName.IsNone() ? ObjectUpdate.ObjectId.ObjectOuterPathName : ObjectUpdate.ObjectData.NewOuterPathName;
				const FName ObjectPackageName = *FPackageName::ObjectPathToPackageName(ObjectOuterPathName.ToString());
				if (!InPackagesToProcess.Contains(ObjectPackageName))
				{
					continue;
				}
			}

			// Find or create the object
			TransactionObjectRef = ConcertSyncClientUtil::GetObject(ObjectUpdate.ObjectId, ObjectUpdate.ObjectData.NewName, ObjectUpdate.ObjectData.NewOuterPathName, ObjectUpdate.ObjectData.bAllowCreate);
			bObjectsDeleted |= (ObjectUpdate.ObjectData.bIsPendingKill || TransactionObjectRef.NeedsGC());
		}
	}

#if WITH_EDITOR
	UObject* PrimaryObject = InEvent.PrimaryObjectId.ObjectName.IsNone() ? nullptr : ConcertSyncClientUtil::GetObject(InEvent.PrimaryObjectId, FName(), FName(), /*bAllowCreate*/false).Obj;
	FEditorTransactionNotification EditorTransactionNotification(FTransactionContext(InEvent.TransactionId, InEvent.OperationId, LOCTEXT("ConcertTransactionEvent", "Concert Transaction Event"), TEXT("Concert Transaction Event"), PrimaryObject));
	if (!bIsSnapshot)
	{
		EditorTransactionNotification.PreUndo();
	}
#endif

	// --------------------------------------------------------------------------------------------------------------------
	// Phase 2
	// --------------------------------------------------------------------------------------------------------------------
#if WITH_EDITOR
	TArray<TSharedPtr<ITransactionObjectAnnotation>, TInlineAllocator<8>> TransactionAnnotations;
	TransactionAnnotations.AddDefaulted(InEvent.ExportedObjects.Num());
	for (int32 ObjectIndex = 0; ObjectIndex < TransactionObjects.Num(); ++ObjectIndex)
	{
		const ConcertSyncClientUtil::FGetObjectResult& TransactionObjectRef = TransactionObjects[ObjectIndex];
		const FConcertExportedObject& ObjectUpdate = InEvent.ExportedObjects[ObjectIndex];

		UObject* TransactionObject = TransactionObjectRef.Obj;
		if (!TransactionObject)
		{
			continue;
		}

		// Restore its annotation data
		TSharedPtr<ITransactionObjectAnnotation>& TransactionAnnotation = TransactionAnnotations[ObjectIndex];
		if (ObjectUpdate.SerializedAnnotationData.Num() > 0)
		{
			FConcertSyncObjectReader AnnotationReader(InLocalIdentifierTablePtr, FConcertSyncWorldRemapper(), TransactionObject, ObjectUpdate.SerializedAnnotationData);
			TransactionAnnotation = TransactionObject->CreateAndRestoreTransactionAnnotation(AnnotationReader);
			UE_CLOG(!TransactionAnnotation.IsValid(), LogConcert, Warning, TEXT("Object '%s' had transaction annotation data that failed to restore!"), *TransactionObject->GetPathName());
		}

		// Notify before changing anything
		if (!bIsSnapshot || TransactionAnnotation)
		{
			// Transaction annotations require us to invoke the redo flow (even for snapshots!) as that's the only thing that can apply the annotation
			TransactionObject->PreEditUndo();
		}

		// We need to manually call OnPreObjectPropertyChanged as PreEditUndo calls the PreEditChange version that skips it, but we have things that rely on it being called
		// For snapshot events this also triggers PreEditChange directly since we can skip the call to PreEditUndo
		for (const FConcertSerializedPropertyData& PropertyData : ObjectUpdate.PropertyDatas)
		{
			UProperty* TransactionProp = FindField<UProperty>(TransactionObject->GetClass(), PropertyData.PropertyName);
			if (TransactionProp)
			{
				if (bIsSnapshot)
				{
					TransactionObject->PreEditChange(TransactionProp);
				}

				FEditPropertyChain PropertyChain;
				PropertyChain.AddHead(TransactionProp);
				FCoreUObjectDelegates::OnPreObjectPropertyChanged.Broadcast(TransactionObject, PropertyChain);
			}
		}
	}
#endif

	// --------------------------------------------------------------------------------------------------------------------
	// Phase 3
	// --------------------------------------------------------------------------------------------------------------------
	for (int32 ObjectIndex = 0; ObjectIndex < TransactionObjects.Num(); ++ObjectIndex)
	{
		const ConcertSyncClientUtil::FGetObjectResult& TransactionObjectRef = TransactionObjects[ObjectIndex];
		const FConcertExportedObject& ObjectUpdate = InEvent.ExportedObjects[ObjectIndex];

		UObject* TransactionObject = TransactionObjectRef.Obj;
		if (!TransactionObject)
		{
			continue;
		}

		// Update the pending kill state
		ConcertSyncClientUtil::UpdatePendingKillState(TransactionObject, ObjectUpdate.ObjectData.bIsPendingKill);

		// Apply the new data
		if (ObjectUpdate.ObjectData.SerializedData.Num() > 0)
		{
			FConcertSyncObjectReader ObjectReader(InLocalIdentifierTablePtr, FConcertSyncWorldRemapper(), TransactionObject, ObjectUpdate.ObjectData.SerializedData);
			ObjectReader.SerializeObject(TransactionObject);
		}
		else
		{
			for (const FConcertSerializedPropertyData& PropertyData : ObjectUpdate.PropertyDatas)
			{
				UProperty* TransactionProp = FindField<UProperty>(TransactionObject->GetClass(), PropertyData.PropertyName);
				if (TransactionProp)
				{
					FConcertSyncObjectReader ObjectReader(InLocalIdentifierTablePtr, FConcertSyncWorldRemapper(), TransactionObject, PropertyData.SerializedData);
					ObjectReader.SerializeProperty(TransactionProp, TransactionObject);
				}
			}
		}
	}

	// --------------------------------------------------------------------------------------------------------------------
	// Phase 4
	// --------------------------------------------------------------------------------------------------------------------
	for (int32 ObjectIndex = 0; ObjectIndex < TransactionObjects.Num(); ++ObjectIndex)
	{
		const ConcertSyncClientUtil::FGetObjectResult& TransactionObjectRef = TransactionObjects[ObjectIndex];
		const FConcertExportedObject& ObjectUpdate = InEvent.ExportedObjects[ObjectIndex];

		UObject* TransactionObject = TransactionObjectRef.Obj;
		if (!TransactionObject)
		{
			continue;
		}

		// Finish spawning any newly created actors
		if (TransactionObjectRef.NeedsPostSpawn())
		{
			AActor* TransactionActor = CastChecked<AActor>(TransactionObject);
			TransactionActor->FinishSpawning(FTransform(), true);
		}

#if WITH_EDITOR
		// We need to manually call OnObjectPropertyChanged as PostEditUndo calls the PostEditChange version that skips it, but we have things that rely on it being called
		// For snapshot events this also triggers PostEditChange directly since we can skip the call to PostEditUndo
		for (const FConcertSerializedPropertyData& PropertyData : ObjectUpdate.PropertyDatas)
		{
			UProperty* TransactionProp = FindField<UProperty>(TransactionObject->GetClass(), PropertyData.PropertyName);
			if (TransactionProp)
			{
				if (bIsSnapshot)
				{
					TransactionObject->PostEditChange();
				}

				FPropertyChangedEvent PropertyChangedEvent(TransactionProp, bIsSnapshot ? EPropertyChangeType::Interactive : EPropertyChangeType::Unspecified);
				FCoreUObjectDelegates::OnObjectPropertyChanged.Broadcast(TransactionObject, PropertyChangedEvent);
			}
		}

		// Notify after changing everything
		const TSharedPtr<ITransactionObjectAnnotation>& TransactionAnnotation = TransactionAnnotations[ObjectIndex];
		if (TransactionAnnotation)
		{
			// Transaction annotations require us to invoke the redo flow (even for snapshots!) as that's the only thing that can apply the annotation
			TransactionObject->PostEditUndo(TransactionAnnotation);
		}
		else if (!bIsSnapshot)
		{
			TransactionObject->PostEditUndo();
		}

		// Notify the editor that a transaction happened, as some things rely on this being called
		// We need to call this ourselves as we aren't actually going through the full transaction redo that the editor hooks in to to generate these notifications
		if (!bIsSnapshot)
		{
			EditorTransactionNotification.HandleObjectTransacted(TransactionObject, ObjectUpdate, TransactionAnnotation);
		}
#endif
	}

#if WITH_EDITOR
	if (!bIsSnapshot)
	{
		EditorTransactionNotification.PostUndo();
	}
#endif

	// TODO: This can sometimes cause deadlocks - need to investigate why
	if (bObjectsDeleted)
	{
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, false);
	}
}

} // namespace ProcessTransactionEventUtil

void FConcertClientTransactionManager::ProcessTransactionFinalizedEvent(const FPendingTransactionToProcessContext& InContext, const FConcertTransactionFinalizedEvent& InEvent)
{
	FConcertLocalIdentifierTable LocalIdentifierTable(InEvent.LocalIdentifierState);
	ProcessTransactionEventUtil::ProcessTransactionEvent(InEvent, InContext.PackagesToProcess, &LocalIdentifierTable, /*bIsSnapshot*/false);
}

void FConcertClientTransactionManager::ProcessTransactionSnapshotEvent(const FPendingTransactionToProcessContext& InContext, const FConcertTransactionSnapshotEvent& InEvent)
{
	ProcessTransactionEventUtil::ProcessTransactionEvent(InEvent, InContext.PackagesToProcess, nullptr, /*bIsSnapshot*/true);

#if WITH_EDITOR
	if (GUnrealEd)
	{
		GUnrealEd->UpdatePivotLocationForSelection();
	}
#endif
}

void FConcertClientTransactionManager::SendTransactionFinalizedEvent(const FGuid& InTransactionId, const FGuid& InOperationId, UObject* InPrimaryObject, const TArray<FName>& InModifiedPackages, const TArray<FConcertExportedObject>& InObjectUpdates, const FConcertLocalIdentifierTable& InLocalIdentifierTable, const FText& InTitle)
{
	FConcertTransactionFinalizedEvent TransactionFinalizedEvent;
	FillTransactionEvent(InTransactionId, InOperationId, InModifiedPackages, TransactionFinalizedEvent);
	TransactionFinalizedEvent.PrimaryObjectId = InPrimaryObject ? FConcertObjectId(InPrimaryObject) : FConcertObjectId();
	TransactionFinalizedEvent.ExportedObjects = InObjectUpdates;
	InLocalIdentifierTable.GetState(TransactionFinalizedEvent.LocalIdentifierState);
	TransactionFinalizedEvent.Title = InTitle;

	Session->SendCustomEvent(TransactionFinalizedEvent, Session->GetSessionServerEndpointId(), EConcertMessageFlags::ReliableOrdered);
}

void FConcertClientTransactionManager::SendTransactionSnapshotEvent(const FGuid& InTransactionId, const FGuid& InOperationId, UObject* InPrimaryObject, const TArray<FName>& InModifiedPackages, const TArray<FConcertExportedObject>& InObjectUpdates)
{
	FConcertTransactionSnapshotEvent TransactionSnapshotEvent;
	FillTransactionEvent(InTransactionId, InOperationId, InModifiedPackages, TransactionSnapshotEvent);
	TransactionSnapshotEvent.PrimaryObjectId = InPrimaryObject ? FConcertObjectId(InPrimaryObject) : FConcertObjectId();
	TransactionSnapshotEvent.ExportedObjects = InObjectUpdates;

	Session->SendCustomEvent(TransactionSnapshotEvent, Session->GetSessionServerEndpointId(), EConcertMessageFlags::None);
}

void FConcertClientTransactionManager::SendPendingTransactionEvents()
{
	const double SnapshotEventDelaySeconds = 1.0 / FMath::Max(GetDefault<UConcertSyncConfig>()->SnapshotTransactionsPerSecond, KINDA_SMALL_NUMBER);

	const double CurrentTimeSeconds = FPlatformTime::Seconds();

	for (auto PendingTransactionsToSendOrderIter = PendingTransactionsToSendOrder.CreateIterator(); PendingTransactionsToSendOrderIter; ++PendingTransactionsToSendOrderIter)
	{
		FPendingTransactionToSend* PendingTransactionPtr = PendingTransactionsToSend.Find(*PendingTransactionsToSendOrderIter);
		if (!PendingTransactionPtr)
		{
			// Missing transaction, must have been canceled...
			PendingTransactionsToSendOrderIter.RemoveCurrent();
			continue;
		}

		// if the transaction isn't excluded, send updates
		if (!PendingTransactionPtr->bIsExcluded)
		{
			UObject* PrimaryObject = PendingTransactionPtr->PrimaryObject.Get(/*bEvenIfPendingKill*/true);
			if (PendingTransactionPtr->bIsFinalized)
			{
				// Process this transaction
				if (PendingTransactionPtr->FinalizedObjectUpdates.Num() > 0)
				{
					SendTransactionFinalizedEvent(PendingTransactionPtr->TransactionId, PendingTransactionPtr->OperationId, PrimaryObject, PendingTransactionPtr->ModifiedPackages, PendingTransactionPtr->FinalizedObjectUpdates, PendingTransactionPtr->FinalizedLocalIdentifierTable, PendingTransactionPtr->Title);
				}
				// TODO: Warn about excluded objects?

				PendingTransactionsToSend.Remove(PendingTransactionPtr->TransactionId);
				PendingTransactionsToSendOrderIter.RemoveCurrent();
				continue;
			}
			else if (PendingTransactionPtr->SnapshotObjectUpdates.Num() > 0 && CurrentTimeSeconds > PendingTransactionPtr->LastSnapshotTimeSeconds + SnapshotEventDelaySeconds)
			{
				// Process this snapshot
				SendTransactionSnapshotEvent(PendingTransactionPtr->TransactionId, PendingTransactionPtr->OperationId, PrimaryObject, PendingTransactionPtr->ModifiedPackages, PendingTransactionPtr->SnapshotObjectUpdates);

				PendingTransactionPtr->SnapshotObjectUpdates.Reset();
				PendingTransactionPtr->LastSnapshotTimeSeconds = CurrentTimeSeconds;
			}
		}
		// Once the excluded transaction is finalized, broadcast and remove it.
		else if (PendingTransactionPtr->bIsFinalized)
		{
			// TODO: Broadcast delegate

			PendingTransactionsToSend.Remove(PendingTransactionPtr->TransactionId);
			PendingTransactionsToSendOrderIter.RemoveCurrent();
			continue;
		}
	}
}

bool FConcertClientTransactionManager::ShouldProcessTransactionEvent(const FConcertTransactionEventBase& InEvent, const bool InIsRequired) const
{
	const FName TransactionKey = *FString::Printf(TEXT("TransactionManager.TransactionId:%s"), *InEvent.TransactionId.ToString());

	FConcertScratchpadPtr SenderScratchpad = Session->GetClientScratchpad(InEvent.TransactionEndpointId);
	if (SenderScratchpad.IsValid())
	{
		// If the event is required then we have to process it (it may have been received after a newer non-required transaction update, which is why we skip the update order check)
		if (InIsRequired)
		{
			SenderScratchpad->SetValue<uint8>(TransactionKey, InEvent.TransactionUpdateIndex);
			return true;
		}
		
		// If the event isn't required, then we can drop it if its update index is older than the last update we processed
		if (uint8* TransactionUpdateIndexPtr = Session->GetScratchpad()->GetValue<uint8>(TransactionKey))
		{
			uint8& TransactionUpdateIndex = *TransactionUpdateIndexPtr;
			const bool bShouldProcess = InEvent.TransactionUpdateIndex >= TransactionUpdateIndex + 1; // Note: We +1 before doing the check to handle overflow
			TransactionUpdateIndex = InEvent.TransactionUpdateIndex;
			return bShouldProcess;
		}

		// First update for this transaction, just process it
		SenderScratchpad->SetValue<uint8>(TransactionKey, InEvent.TransactionUpdateIndex);
		return true;
	}

	return true;
}

void FConcertClientTransactionManager::FillTransactionEvent(const FGuid& InTransactionId, const FGuid& InOperationId, const TArray<FName>& InModifiedPackages, FConcertTransactionEventBase& OutEvent) const
{
	const FName TransactionKey = *FString::Printf(TEXT("TransactionManager.TransactionId:%s"), *InTransactionId.ToString());

	OutEvent.TransactionId = InTransactionId;
	OutEvent.OperationId = InOperationId;
	OutEvent.TransactionEndpointId = Session->GetSessionClientEndpointId();
	OutEvent.TransactionUpdateIndex = 0;
	OutEvent.ModifiedPackages = InModifiedPackages;

	if (uint8* TransactionUpdateIndexPtr = Session->GetScratchpad()->GetValue<uint8>(TransactionKey))
	{
		uint8& TransactionUpdateIndex = *TransactionUpdateIndexPtr;
		OutEvent.TransactionUpdateIndex = TransactionUpdateIndex++;
	}
	else
	{
		Session->GetScratchpad()->SetValue<uint8>(TransactionKey, OutEvent.TransactionUpdateIndex);
	}
}

FConcertClientTransactionManager::ETransactionFilterResult FConcertClientTransactionManager::ApplyTransactionFilters(UObject* InObject, UPackage* InChangedPackage)
{
	// Ignore transient packages and objects
	if (!InChangedPackage || InChangedPackage == GetTransientPackage() || InChangedPackage->HasAnyFlags(RF_Transient) || InObject->HasAnyFlags(RF_Transient))
	{
		return ETransactionFilterResult::ExcludeObject;
	}

	// Ignore packages outside of known root paths (we ignore read-only roots here to skip things like unsaved worlds)
	if (!FPackageName::IsValidLongPackageName(InChangedPackage->GetName()))
	{
		return ETransactionFilterResult::ExcludeObject;
	}

	const UConcertSyncConfig* SyncConfig = GetDefault<UConcertSyncConfig>();

	// Run our exclude transaction filters: if a filter is matched on an object the whole transaction is excluded.
	if (SyncConfig->ExcludeTransactionClassFilters.Num() > 0 && RunTransactionFilters(SyncConfig->ExcludeTransactionClassFilters, InObject))
	{
		return ETransactionFilterResult::ExcludeTransaction;
	}

	// Run our include object filters: if the list is empty all objects are included, otherwise a filter needs to be matched.
	if (SyncConfig->IncludeObjectClassFilters.Num() == 0 || RunTransactionFilters(SyncConfig->IncludeObjectClassFilters, InObject))
	{
		return ETransactionFilterResult::IncludeObject;
	}

	// Otherwise the object is excluded from the transaction
	return ETransactionFilterResult::ExcludeObject;
}

bool FConcertClientTransactionManager::RunTransactionFilters(const TArray<FTransactionClassFilter>& InFilters, UObject* InObject)
{
	bool bMatchFilter = false;
	for (const FTransactionClassFilter& TransactionFilter : InFilters)
	{
		UClass* TransactionClass = TransactionFilter.ObjectClass.TryLoadClass<UObject>();
		if (TransactionClass && InObject->IsA(TransactionClass))
		{
			if (!TransactionFilter.ObjectOuterClass.IsValid())
			{
				bMatchFilter = true;
			}
			else if (UClass* TransactionOuterClass = TransactionFilter.ObjectOuterClass.TryLoadClass<UObject>())
			{
				for (UObject* Outer = InObject->GetOuter(); Outer != nullptr; Outer = Outer->GetOuter())
				{
					// if one of the outer pass the filter, break out of the check
					if (Outer->IsA(TransactionOuterClass))
					{
						bMatchFilter = true;
						break;
					}
				}
			}

			// the object passed a filter, break out of future filter
			if (bMatchFilter)
			{
				break;
			}
		}
	}
	return bMatchFilter;
}

#undef LOCTEXT_NAMESPACE
