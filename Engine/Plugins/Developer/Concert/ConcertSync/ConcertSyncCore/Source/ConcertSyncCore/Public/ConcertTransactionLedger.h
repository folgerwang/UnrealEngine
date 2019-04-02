// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConcertTransactionEvents.h"

class FStructOnScope;
class FConcertFileCache;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAddFinalizedTransaction, const FConcertTransactionFinalizedEvent& /** FinalizedEvent */, uint64 /** TransactionIndex */);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLiveTransactionsTrimmed, const FName& /** PackageName */, uint64 /*UpToIndex*/);

enum class EConcertTransactionLedgerType : uint8
{
	/** This is a persistent ledger (eg, belonging to a server session) */
	Persistent,
	/** This is a transient ledger (eg, belonging to a client session) */
	Transient,
};

/**
 * In-memory index of a transaction ledger, which references on-disk files that contain the bulk of the transaction data.
 */
class CONCERTSYNCCORE_API FConcertTransactionLedger
{
public:
	/**
	* Create a new ledger
	* @note The ledger path must not be empty.
	*/
	FConcertTransactionLedger(const EConcertTransactionLedgerType InLedgerType, const FString& InLedgerPath);

	~FConcertTransactionLedger();

	/**
	 * Non-copyable
	 */
	FConcertTransactionLedger(const FConcertTransactionLedger&) = delete;
	FConcertTransactionLedger& operator=(const FConcertTransactionLedger&) = delete;

	/**
	 * Get the path to this ledger on-disk.
	 */
	const FString& GetLedgerPath() const;

	/**
	 * Get the file extension of ledger entries on-disk.
	 */
	const FString& GetLedgerEntryExtension() const;

	/**
	 * Get the index of the next transaction to be added to the ledger.
	 */
	uint64 GetNextTransactionIndex() const;

	/**
	 * Load this ledger from the existing content on-disk.
	 * @return true if a transaction was loaded
	 */
	bool LoadLedger();

	/**
	 * Clear this ledger, removing any content on-disk.
	 * @note Happens automatically when destroying a transient ledger.
	 */
	void ClearLedger();

	/**
	 * @return the delegate that is triggered each time a finalized transaction is about to be added to the ledger
	 */
	FOnAddFinalizedTransaction& OnAddFinalizedTransaction();

	/**
	 * @return the delegate that is triggered each time live transactions for a given packages are trimmed, which means the package was saved on disk.
	 */
	FOnLiveTransactionsTrimmed& OnLiveTransactionsTrimmed();

	/**
	 * Add the given transaction with this ledger.
	 * @return The index of the transaction within the ledger.
	 */
	template <typename TransactionType>
	uint64 AddTransaction(const TransactionType& InTransaction)
	{
		static_assert(TIsDerivedFrom<TransactionType, FConcertTransactionEventBase>::IsDerived, "AddTransaction can only be used with types deriving from FConcertTransactionEventBase");
		return AddTransaction(TransactionType::StaticStruct(), &InTransaction);
	}

	/**
	 * Add the given transaction with this ledger.
	 * @return The index of the transaction within the ledger.
	 */
	uint64 AddTransaction(const UScriptStruct* InTransactionType, const void* InTransactionData);

	/**
	 * Add a transaction to this ledger from its serialized data.
	 * @return The index of the transaction within the ledger.
	 */
	uint64 AddSerializedTransaction(const TArray<uint8>& InTransactionData);

	/**
	 * Add the given transaction with this ledger using the given index.
	 * @note Will clobber any existing transaction with that index!
	 */
	template <typename TransactionType>
	uint64 AddTypedTransaction(const uint64 InIndex, const TransactionType& InTransaction)
	{
		static_assert(TIsDerivedFrom<TransactionType, FConcertTransactionEventBase>::IsDerived, "AddTransaction can only be used with types deriving from FConcertTransactionEventBase");
		return AddTransaction(InIndex, TransactionType::StaticStruct(), &InTransaction);
	}

	/**
	 * Add the given transaction with this ledger using the given index.
	 * @note Will clobber any existing transaction with that index!
	 */
	void AddTransaction(const uint64 InIndex, const UScriptStruct* InTransactionType, const void* InTransactionData);

	/**
	 * Add a transaction to this ledger from its serialized data using the given index.
	 * @note Will clobber any existing transaction with that index!
	 */
	void AddSerializedTransaction(const uint64 InIndex, const TArray<uint8>& InTransactionData);

	/**
	 * Find the transaction with the given index from this ledger.
	 * @return True if the transaction was found, false otherwise.
	 */
	template <typename TransactionType>
	bool FindTypedTransaction(const uint64 InIndex, TransactionType& OutTransaction) const
	{
		static_assert(TIsDerivedFrom<TransactionType, FConcertTransactionEventBase>::IsDerived, "FindTransaction can only be used with types deriving from FConcertTransactionEventBase");
		return FindTransaction(InIndex, TransactionType::StaticStruct(), &OutTransaction);
	}

	/**
	 * Find the transaction with the given index from this ledger.
	 * @return True if the transaction was found, false otherwise.
	 */
	bool FindTransaction(const uint64 InIndex, const UScriptStruct* InTransactionType, void* OutTransactionData) const;

	/**
	 * Find the transaction with the given index from this ledger.
	 * @return True if the transaction was found, false otherwise.
	 */
	bool FindTransaction(const uint64 InIndex, FStructOnScope& OutTransaction) const;

	/**
	 * Find the serialized data for the transaction with the given index from this ledger.
	 * @return True if the transaction was found, false otherwise.
	 */
	bool FindSerializedTransaction(const uint64 InIndex, TArray<uint8>& OutTransactionData) const;

	/**
	 * Get the transaction indices of the "live" transactions for all packages.
	 */
	TArray<uint64> GetAllLiveTransactions() const;

	/**
	 * Get the transaction indices of the "live" transactions for the given package.
	 */
	TArray<uint64> GetLiveTransactions(const FName InPackageName) const;

	/**
	 * Get the packages that have "live" transactions.
	 */
	TArray<FName> GetPackagesNamesWithLiveTransactions() const;

	/**
	 * Called when a package is saved to trim the "live" transactions for that package.
	 * This function should be given the next transaction index when the package was saved, and will clear up-to that value.
	 */
	void TrimLiveTransactions(const uint64 InIndex, const FName InPackageName);

private:
	friend class FConcertTransactionVisitor;

	/**
	 * Track the modified packages of the given transaction as being associated with the given live transaction index.
	 */
	void TrackLiveTransaction(const uint64 InIndex, const FConcertTransactionEventBase* InTransactionEvent);

	/**
	 * Save and cache the given transaction with the given filename.
	 */
	bool SaveTransaction(const FString& InTransactionFilename, const FStructOnScope& InTransaction) const;

	/**
	 * Load and cache the given transaction from the given filename.
	 */
	bool LoadTransaction(const FString& InTransactionFilename, FStructOnScope& OutTransaction) const;

	/** The type of this ledger */
	EConcertTransactionLedgerType LedgerType;

	/** Path to this ledger on-disk */
	FString LedgerPath;

	/** Index to give the next transaction added to the ledger */
	uint64 NextTransactionIndex;

	/** Mapping from a package name to its current "live" transactions (those that should be replayed when the package is loaded) */
	TMap<FName, TArray<uint64>> LivePackageTransactions;

	/** In-memory cache of on-disk ledger entries */
	TUniquePtr<FConcertFileCache> LedgerFileCache;

	/** Delegate called every time a finalized transaction is added. */
	FOnAddFinalizedTransaction OnAddFinalizedTransactionDelegate;

	FOnLiveTransactionsTrimmed OnLiveTransactionsTrimmedDelegate;
};
