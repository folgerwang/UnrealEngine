// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/ITransaction.h"
#include "UObject/Class.h"
#include "ConcertMessages.h"
#include "IConcertSessionHandler.h"
#include "ConcertTransactionEvents.h"
#include "IdentifierTable/ConcertIdentifierTable.h"
#include "UObject/StructOnScope.h"

class IConcertClientSession;
class FConcertTransactionLedger;

class FConcertClientTransactionManager
{
public:
	FConcertClientTransactionManager(TSharedRef<IConcertClientSession> InSession);
	~FConcertClientTransactionManager();

	/**
	 * Get the transient ledger of transactions for this session.
	 */
	const FConcertTransactionLedger& GetLedger() const;

	/**
	 * Get the transient ledger of transactions for this session.
	 */
	FConcertTransactionLedger& GetMutableLedger();

	/**
	 * Called to replay any live transactions for all packages.
	 */
	void ReplayAllTransactions();

	/**
	 * Called to replay live transactions for the given package.
	 */
	void ReplayTransactions(const FName InPackageName);

	/**
	 * Called to handle a remote transaction being received.
	 */
	void HandleRemoteTransaction(const uint64 InTransactionIndex, const TArray<uint8>& InTransactionData, const bool bApply);

	/**
	 * Called to handle a transaction state change.
	 */
	void HandleTransactionStateChanged(const FTransactionContext& InTransactionContext, const ETransactionStateEventType InTransactionState);

	/**
	 * Called to handle an object being transacted.
	 */
	void HandleObjectTransacted(UObject* InObject, const FTransactionObjectEvent& InTransactionEvent);

	/**
	 * Called to process any pending transaction events (sending or receiving).
	 */
	void ProcessPending();

private:
	/**
	 * Context object for transactions that are to be processed.
	 */
	struct FPendingTransactionToProcessContext
	{
		FPendingTransactionToProcessContext()
			: bIsRequired(false)
		{
		}

		/** Is this transaction required? */
		bool bIsRequired;
		
		/** Optional list of packages to process transactions for, or empty to process transactions for all packages */
		TArray<FName> PackagesToProcess;
	};

	/**
	 * A received pending transaction event that was queued for processing later.
	 */
	struct FPendingTransactionToProcess
	{
		FPendingTransactionToProcess(const FPendingTransactionToProcessContext& InContext, const UScriptStruct* InEventStruct, const void* InEventData)
			: Context(InContext)
			, EventData(InEventStruct)
		{
			InEventStruct->CopyScriptStruct(EventData.GetStructMemory(), InEventData);
		}

		FPendingTransactionToProcess(const FPendingTransactionToProcessContext& InContext, FStructOnScope&& InEvent)
			: Context(InContext)
			, EventData(MoveTemp(InEvent))
		{
			check(EventData.OwnsStructMemory());
		}

		FPendingTransactionToProcessContext Context;
		FStructOnScope EventData;
	};

	/**
	 * A pending transaction that may be sent in the future (when finalized).
	 */
	struct FPendingTransactionToSend
	{
		FPendingTransactionToSend(const FGuid& InTransactionId, const FGuid& InOperationId, UObject* InPrimaryObject)
			: TransactionId(InTransactionId)
			, OperationId(InOperationId)
			, PrimaryObject(InPrimaryObject)
			, LastSnapshotTimeSeconds(0)
			, bIsFinalized(false)
			, bIsExcluded(false)
		{}

		FGuid TransactionId;
		FGuid OperationId;
		FWeakObjectPtr PrimaryObject;
		double LastSnapshotTimeSeconds;
		bool bIsFinalized;
		bool bIsExcluded;
		TArray<FConcertObjectId> ExcludedObjectUpdates;
		TArray<FName> ModifiedPackages;
		FConcertLocalIdentifierTable FinalizedLocalIdentifierTable;
		TArray<FConcertExportedObject> FinalizedObjectUpdates;
		TArray<FConcertExportedObject> SnapshotObjectUpdates;
		FText Title;
	};

	/**
	 * Transaction filter result
	 */
	enum class ETransactionFilterResult : uint8
	{
		IncludeObject,		// Include the object in the Concert Transaction
		ExcludeObject,		// Filter the object from the Concert Transaction
		ExcludeTransaction	// Filter the entire transaction and prevent propagation
	};

	/**
	 * Handle a transaction event by queueing it for processing at the end of the current frame.
	 */
	template <typename EventType>
	void HandleTransactionEvent(const FConcertSessionContext& InEventContext, const EventType& InEvent);

	/**
	 * Handle a rejected transaction event, those are sent by the server when a transaction is refused.
	 */
	void HandleTransactionRejectedEvent(const FConcertSessionContext& InEventContext, const FConcertTransactionRejectedEvent& InEvent);

	/**
	 * Can we currently process transaction events?
	 * True if we are neither suspended nor unable to perform a blocking action, false otherwise.
	 */
	bool CanProcessTransactionEvent() const;

	/**
	 * Process a transaction event.
	 */
	void ProcessTransactionEvent(const FPendingTransactionToProcessContext& InContext, const FStructOnScope& InEvent);

	/**
	 * Process a transaction finalized event.
	 */
	void ProcessTransactionFinalizedEvent(const FPendingTransactionToProcessContext& InContext, const FConcertTransactionFinalizedEvent& InEvent);

	/**
	 * Process a transaction snapshot event.
	 */
	void ProcessTransactionSnapshotEvent(const FPendingTransactionToProcessContext& InContext, const FConcertTransactionSnapshotEvent& InEvent);

	/**
	 * Send a transaction finalized event.
	 */
	void SendTransactionFinalizedEvent(const FGuid& InTransactionId, const FGuid& InOperationId, UObject* InPrimaryObject, const TArray<FName>& InModifiedPackages, const TArray<FConcertExportedObject>& InObjectUpdates, const FConcertLocalIdentifierTable& InLocalIdentifierTable, const FText& InTitle);

	/**
	 * Send a transaction snapshot event.
	 */
	void SendTransactionSnapshotEvent(const FGuid& InTransactionId, const FGuid& InOperationId, UObject* InPrimaryObject, const TArray<FName>& InModifiedPackages, const TArray<FConcertExportedObject>& InObjectUpdates);

	/**
	 * Send any pending transaction events that qualify.
	 */
	void SendPendingTransactionEvents();

	/**
	 * Should process this transaction event?
	 */
	bool ShouldProcessTransactionEvent(const FConcertTransactionEventBase& InEvent, const bool InIsRequired) const;

	/**
	 * Fill in the transaction event based on the given GUID.
	 */
	void FillTransactionEvent(const FGuid& InTransactionId, const FGuid& InOperationId, const TArray<FName>& InModifiedPackages, FConcertTransactionEventBase& OutEvent) const;

	/**
	 * Filter transaction object 
	 * @param InObject object to test the filter against.
	 * @param InChangedPackage outer package the object is from.
	 * @return a transaction filter result which tell how to handle the object or the full transaction
	 */
	ETransactionFilterResult ApplyTransactionFilters(UObject* InObject, UPackage* InChangedPackage);

	/**
	 * Run an array of Transaction Class Filter on an object.
	 * @param InFilters object to test the filter against.
	 * @param InObject the object to run the filters on.
	 * @return true if the object matched at least one of the filters.
	 */
	bool RunTransactionFilters(const TArray<struct FTransactionClassFilter>& InFilters, UObject* InObject);

	/**
	 * Array of pending transaction events in the order they were received.
	 * Events are queued in this array while the session is suspended or the user is interacting, 
	 * and any queued transactions will be processed on the next Tick.
	 */
	TArray<FPendingTransactionToProcess> PendingTransactionsToProcess;

	/**
	 * Array of transaction IDs in the order they should be sent (maps to PendingTransactionsToSend, although canceled transactions may be missing from the map).
	 */
	TArray<FGuid> PendingTransactionsToSendOrder;

	/**
	 * Map of transaction IDs to the pending transaction that may be sent in the future (when finalized).
	 */
	TMap<FGuid, FPendingTransactionToSend> PendingTransactionsToSend;

	/**
	 * Transient ledger of transactions for this session.
	 */
	TUniquePtr<FConcertTransactionLedger> TransactionLedger;

	/**
	 * Session instance this transaction manager was created for.
	 */
	TSharedPtr<IConcertClientSession> Session;

	/**
	 * Flag to ignore transaction state change event, used when we do not want to record transaction we generate ourselves
	 */
	bool bIgnoreTransaction;
};
