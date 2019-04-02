// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConcertMessages.h"
#include "ConcertActivityEvents.h"
#include "ConcertWorkspaceData.h"
#include "ConcertLogGlobal.h"
#include "UObject/StructOnScope.h"

class FConcertFileCache;
class IConcertSession;

struct FConcertSessionClientInfo;
struct FConcertTransactionFinalizedEvent;
struct FConcertPackageInfo;
struct FConcertClientInfo;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAddActivity, const FStructOnScope& /** ActivityEvent */, uint64 /** ActivityIndex */);

enum class EConcertActivityLedgerType : uint8
{
	/** This is a persistent ledger (eg, belonging to a server session) */
	Persistent,
	/** This is a transient ledger (eg, belonging to a client session) */
	Transient,
};

/**
 * The FConcertActivityLedger records all the users activity for a given session
 */
class CONCERTSYNCCORE_API FConcertActivityLedger
{

public:
	friend class FConcertActivityVisitor;

	FConcertActivityLedger(EConcertActivityLedgerType LedgerType, const FString& InLedgerPath);

	FConcertActivityLedger(const FConcertActivityLedger&) = delete;
	FConcertActivityLedger& operator=(const FConcertActivityLedger&) = delete;

	virtual ~FConcertActivityLedger();

	/**
	 * Load this ledger from the existing content on-disk.
	 * @note Happens automatically when creating a persistent ledger.
	 * @return true if an activity was loaded.
	 */
	bool LoadLedger();

	/**
	 * Clear this ledger, removing any content on-disk.
	 * @note Happens only with transient ledgers.
	 */
	void ClearLedger();

	/**
	 * Get the number of activities in the ledger.
	 */
	uint64 GetActivityCount() const
	{
		return ActivityCount;
	}

	/**
	 * Get the path of the ledger on disk.
	 */
	const FString& GetLedgerPath() const
	{
		return LedgerPath;
	}

	/**
	 * Find the activity with the given index from this ledger.
	 * @return True if the activity was found, false otherwise.
	 */
	template<typename ActivityType>
	bool FindTypedActivity(const uint64 ActivityIndex, ActivityType& OutActivity) const
	{
		static_assert(TIsDerivedFrom<ActivityType, FConcertActivityEvent>::IsDerived, "FindTypedActivity can only be used with types deriving from FConcertActivityEvent");
		FStructOnScope Activity(ActivityType::StaticStruct(), (uint8*)OutActivity);
		return FindActivity(ActivityIndex, Activity);
	}

	/**
	 * Find the activity with the given index from this ledger.
	 * @return True if the activity was found, false otherwise.
	 */
	bool FindActivity(const uint64 ActivityIndex, FStructOnScope& OutActivity) const;

	/**
	 * Get the last activities.
	 * @param Limit the maximum number of activities to fetch.
	 * @param OutActivities the fetched activities.
	 * @return the index of the first fetched activity.
	 */
	uint64 GetLastActivities(uint32 Limit, TArray<FStructOnScope>& OutActivities) const;

	/**
	 * Get activities from the ledger.
	 * @param Offset the index at which to start fetching activities.
	 * @param Limit the maximum number of activities returned.
	 * @param OutActivities the fetched activities.
	 */
	void GetActivities(uint64 Offset, int32 Limit, TArray<FStructOnScope>& OutActivities) const;

	/**
	 * @return the delegate that is triggered each time a an activity is about to be added to the ledger.
	 */
	FOnAddActivity& OnAddActivity()
	{
		return OnAddActivityDelegate;
	}

 	/**
	 * Update the activity ledger when a client quit or join a session.
	 * @param ClientStatus - The status of the connection of the client.
	 * @param InClientInfo - The client info.
	 */
	virtual void RecordClientConectionStatusChanged(EConcertClientStatus ClientStatus, const FConcertClientInfo& InClientInfo);

	/**
	 * Update the activity ledger when a transaction has been applied.
	 * @param InTransactionFinalizedEvent - The event of the transaction.
	 * @param TransactionIndex - The index of the transaction in the transaction ledger.
	 * @param InClientInfo - The client info.
	 */
	virtual void RecordFinalizedTransaction(const FConcertTransactionFinalizedEvent& InTransactionFinalizedEvent, uint64 TransactionIndex, const FConcertClientInfo& InClientInfo);

	/**
	 * Update the activity ledger when a package has been updated.
	 * @param Revision - The revision of the package.
	 * @param InPackageInfo - The package info of the updated package.
	 * @param InClientInfo - The client info.
	 */
	virtual void RecordPackageUpdate(const uint32 Revision, const FConcertPackageInfo& InPackageInfo, const FConcertClientInfo& InClientInfo);

protected:

	/**
	 * Add the given transaction with this ledger.
	 * @return The index of the activity within the ledger.
	 */
	template <typename ActivityType>
	uint64 AddActivity(const ActivityType& InActivity)
	{
		static_assert(TIsDerivedFrom<ActivityType, FConcertActivityEvent>::IsDerived, "AddActivity can only be used with types deriving from FConcertActivityEvent");
		ensureAlways(AddActivity(ActivityType::StaticStruct(), &InActivity));
		AddActivityCallback(ActivityType::StaticStruct(), &InActivity);
		return ActivityCount - 1;
	}

private:

	bool AddActivity(const UScriptStruct* InActivityType, const void* InActivityData);

	bool LoadActivity(const FString& InActivityFilename, FStructOnScope& OutActivity) const;

	/**
	 * Does nothing by default but it allows child classes to react when a activity is added to the ledger.
	 * @param ActivityType - The type of the activity that was added to the ledger.
	 * @param ActivityData - A pointer the start of the activity struct.
	 */
	virtual void AddActivityCallback(UScriptStruct* InActivityType, const void* ActivityData)
	{
	}

	/** The type of this ledger. */
	EConcertActivityLedgerType LedgerType;

	/** Path to the ledger folder on-disk. */
	FString LedgerPath;

	/** The total number of activity in this ledger. */
	uint64 ActivityCount;

	/** In-memory cache of on-disk ledger entries. */
	TUniquePtr<FConcertFileCache> LedgerFileCache;

	/** Delegate called every time an activity is added. */
	FOnAddActivity OnAddActivityDelegate;
};


