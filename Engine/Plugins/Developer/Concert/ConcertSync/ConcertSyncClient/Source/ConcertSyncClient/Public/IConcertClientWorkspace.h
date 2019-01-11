// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "ConcertWorkspaceMessages.h"
#include "ConcertActivityLedger.h"

class ISourceControlProvider;
class IConcertClientSession;
class IConcertClientDataStore;
struct FConcertTransactionEventBase;
struct FConcertPackageInfo;

DECLARE_MULTICAST_DELEGATE(FOnWorkspaceSynchronized);

class IConcertClientWorkspace
{
public:
	/**
	 * Get the associated session.
	 */
	virtual TSharedPtr<IConcertClientSession> GetSession() const = 0;

	/**
	 * @return the client id this workspace uses to lock resources.
	 */
	virtual FGuid GetWorkspaceLockId() const = 0;

	/**
	 * @return a valid client id of the owner of this resource lock or an invalid id if unlocked
	 */
	virtual FGuid GetResourceLockId(const FName InResourceName) const = 0;

	/**
	 * Verify if resources are locked by a particular client
	 * @param ResourceNames list of resources path to verify
	 * @param ClientId the client id to verify
	 * @return true if all resources in ResourceNames are locked by ClientId
	 * @note passing an invalid client id will return true if all resources are unlocked
	 */
	virtual bool AreResourcesLockedBy(TArrayView<const FName> ResourceNames, const FGuid& ClientId) = 0;

	/**
	 * Attempt to lock the given resource.
	 * @note Passing force will always assign the lock to the given endpoint, even if currently locked by another.
	 * @return True if the resource was locked (or already locked by the given endpoint), false otherwise.
	 */
	virtual TFuture<FConcertResourceLockResponse> LockResources(TArray<FName> InResourceName) = 0;

	/**
	 * Attempt to unlock the given resource.
	 * @note Passing force will always clear, even if currently locked by another endpoint.
	 * @return True if the resource was unlocked, false otherwise.
	 */
	virtual TFuture<FConcertResourceLockResponse> UnlockResources(TArray<FName> InResourceName) = 0;

	/**
	 * Gather assets changes that happened on the workspace in this session.
	 * @return a list of asset files that were modified during the session.
	 */
	virtual TArray<FString> GatherSessionChanges() = 0;

	/** Persist the session changes from the file list and prepare it for source control submission */
	virtual bool PersistSessionChanges(TArrayView<const FString> InFilesToPersist, ISourceControlProvider* SourceControlProvider, TArray<FText>* OutFailureReasonMap = nullptr) = 0;

	/**
	 * Get the number of activities in the workspace ledger.
	 */
	virtual uint64 GetActivityCount() const = 0;

	/**
	 * Get the last activities from the ledger.
	 * @param Limit the maximum number of activities returned.
	 * @param OutActivities the activities fetched from the ledger.
	 * @return the index of the first fetched activity.
	 */
	virtual uint64 GetLastActivities(uint32 Limit, TArray<FStructOnScope>& OutActivities) const = 0;

	/**
	 * Get Activities from the ledger.
	 * @param Offset the index at which to start fetching activities.
	 * @param Limit the maximum number of activities returned.
	 * @param OutActivities the activities fetched from the ledger.
	 */
	virtual void GetActivities(uint64 Offset, uint32 Limit, TArray<FStructOnScope>& OutActivities) const = 0;

	/**
	 * @return the delegate called every time a new activity is added to the workspace ledger.
	 */
	virtual FOnAddActivity& OnAddActivity() = 0;

	/**
	 * @param TransactionIndex index of the transaction to look for.
	 * @return whether or not the transaction event was found.
	 */
	virtual bool FindTransactionEvent(uint64 TransactionIndex, FConcertTransactionFinalizedEvent& OutTransaction) const = 0;

	/**
	 * @param PackageName name of the package to look for.
	 * @return whether or not the package event was found.
	 */
	virtual bool FindPackageEvent(const FName& PackageName, const uint32 Revision, FConcertPackageInfo& OutPackage) const = 0;

	/**
	 * @return the delegate called every time the workspace is synced.
	 */
	virtual FOnWorkspaceSynchronized& OnWorkspaceSynchronized() = 0;
	
	/**
	 * @return the key/value store shared by all clients.
	 */
	virtual IConcertClientDataStore& GetDataStore() = 0;
};
