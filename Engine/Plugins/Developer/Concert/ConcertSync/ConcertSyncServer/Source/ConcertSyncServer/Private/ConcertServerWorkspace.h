// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IConcertSessionHandler.h"
#include "ConcertWorkspaceMessages.h"

class FConcertServerActivityLedger;
class IConcertServerSession;
class FConcertServerSyncCommandQueue;
class FConcertPackageLedger;
class FConcertTransactionLedger;
struct FConcertTransactionFinalizedEvent;
struct FConcertTransactionSnapshotEvent;
class FConcertServerDataStore;

enum class EConcertLockFlags : uint8
{
	None		= 0,
	Explicit	= 1 << 0,
	Force		= 1 << 1,
};
ENUM_CLASS_FLAGS(EConcertLockFlags);

class FConcertServerWorkspace
{
public:
	FConcertServerWorkspace(const TSharedRef<IConcertServerSession>& InSession);
	~FConcertServerWorkspace();

private:
	/** Bind the workspace to this session. */
	void BindSession(const TSharedRef<IConcertServerSession>& InSession);

	/** Unbind the workspace to its bound session. */
	void UnbindSession();

	/** */
	void HandleTick(IConcertServerSession& InSession, float InDeltaTime);

	/** */
	void HandleSessionClientChanged(IConcertServerSession& InSession, EConcertClientStatus InClientStatus, const FConcertSessionClientInfo& InClientInfo);

	/** */
	void HandlePackageUpdateEvent(const FConcertSessionContext& Context, const FConcertPackageUpdateEvent& Event);

	/** */
	void HandleTransactionFinalizedEvent(const FConcertSessionContext& InEventContext, const FConcertTransactionFinalizedEvent& InEvent);

	/** */
	void HandleTransactionSnapshotEvent(const FConcertSessionContext& InEventContext, const FConcertTransactionSnapshotEvent& InEvent);

	/** */
	void HandlePlaySessionEvent(const FConcertSessionContext& Context, const FConcertPlaySessionEvent& Event);

	/** */
	EConcertSessionResponseCode HandleResourceLockRequest(const FConcertSessionContext& Context, const FConcertResourceLockRequest& Request, FConcertResourceLockResponse& Response);

	/** Invoked when the client corresponding to the specified endpoint begins to "Play" in a mode such as PIE or SIE. */
	void HandleBeginPlaySession(const FName InPlayPackageName, const FGuid& InEndpointId, bool bIsSimulating);

	/** Invoked when the client corresponding to the specified endpoint exits a "Play" mode such as PIE or SIE. */
	void HandleEndPlaySession(const FName InPlayPackageName, const FGuid& InEndpointId);

	/** Invoked when the client corresponding to specified endpoint toggles between PIE and SIE play mode. */
	void HandleSwitchPlaySession(const FName InPlayPackageName, const FGuid& InEndpointId);

	/** Invoked when the cient corresponding to the specified endpoint exits a "Play" mode such as PIE or SIE. */
	void HandleEndPlaySessions(const FGuid& InEndpointId);

	/** Returns the package name being played (PIE/SIE) by the specified client endpoint if that endpoint is in such play mode, otherwise, returns an empty name. */
	FName FindPlaySession(const FGuid& InEndpointId);

	/**
	 * Attempt to lock the given resource to the given endpoint.
	 * @note Passing force will always assign the lock to the given endpoint, even if currently locked by another.
	 * @return True if the resource was locked (or already locked by the given endpoint), false otherwise.
	 */
	bool LockWorkspaceResource(const FName InResourceName, const FGuid& InLockEndpointId, EConcertLockFlags InLockFlags);

	/**
	 * Attempt to lock a list of resources to the given endpoint.
	 * @param InResourceNames The list of resource to lock
	 * @param InLockEndpointId The client id trying to acquire the lock
	 * @param InExplicit mark the lock as explicit
	 * @param InForce steal locks if true
	 * @param OutFailedResources Pointer to an array to gather resources on which acquiring the lock failed.
	 * @return true if the lock was successfully acquired on all InResourceNames
	 */
	bool LockWorkspaceResources(const TArray<FName>& InResourceNames, const FGuid& InLockEndpointId, EConcertLockFlags InLockFlags, TMap<FName, FGuid>* OutFailedRessources = nullptr);

	/**
	 * Attempt to unlock the given resource from the given endpoint.
	 * @note Passing force will always clear, even if currently locked by another endpoint.
	 * @return True if the resource was unlocked, false otherwise.
	 */
	bool UnlockWorkspaceResource(const FName InResourceName, const FGuid& InLockEndpointId, EConcertLockFlags InLockFlags);

	/**
	 * Attempt to unlock a list of resources from the given endpoint.
	 * @param InResourceNames The list of resource to unlock
	 * @param InLockEndpointId The client id trying to releasing the lock
	 * @param InExplicit mark the unlock as explicit, implicit unlock won't unlock explicit lock
	 * @param InForce release locks even of not owned if true
	 * @param OutFailedResources Pointer to an array to gather resources on which releasing the lock failed.
	 * @return true if the lock was successfully released on all InResourceNames
	 */
	bool UnlockWorkspaceResources(const TArray<FName>& InResourceNames, const FGuid& InLockEndpointId, EConcertLockFlags InLockFlags, TMap<FName, FGuid>* OutFailedRessources = nullptr);

	/**
	 * Unlock all resource locks held by a client.
	 * @param InLockEndpointId the client endpoint id releasing the lock on resources.
	 */
	void UnlockAllWorkspaceResources(const FGuid& InLockEndpointId);

	/**
	 * Check to see if the given resource is locked by the given endpoint.
	 */
	bool IsWorkspaceResourceLocked(const FName InResourceName, const FGuid& InLockEndpointId) const;

	/**
	 * Load the working session data from the disk
	 */
	void LoadWorkingSessionData();

	/** Server Session tracked by this workspace */
	TSharedPtr<IConcertServerSession> Session;

	/** Array of newly connected endpoints that haven't completed a full sync yet */
	TArray<FGuid> UnsyncedEndpoints;

	/** */
	TSharedPtr<FConcertServerSyncCommandQueue> SyncCommandQueue;

	/** */
	TUniquePtr<FConcertPackageLedger> PackageLedger;

	/** Persistent ledger of transactions for this session. */
	TUniquePtr<FConcertTransactionLedger> TransactionLedger;

	/** */
	TUniquePtr<FConcertServerActivityLedger> ActivityLedger;

	/** Contains the play state (PIE/SIE) of a client endpoint. */
	struct FPlaySessionInfo
	{
		FGuid EndpointId;
		bool bIsSimulating;
		bool operator==(const FPlaySessionInfo& Other) const { return EndpointId == Other.EndpointId && bIsSimulating == Other.bIsSimulating; }
	};

	/** Tracks endpoints that are in a play session (package name -> {endpoint IDs, bSimulating}) */
	TMap<FName, TArray<FPlaySessionInfo>> ActivePlaySessions;

	/** Tracks locked transaction resources (resource ID -> Lock owner) */
	struct FLockOwner
	{
		FGuid EndpointId;
		bool bExplicit;
	};
	TMap<FName, FLockOwner> LockedResources;


	/** Delegate handle for the ticker function. */
	FDelegateHandle TickHandle;

	/** Delegate handle for the client changed event. */
	FDelegateHandle SessionClientChangedHandle;

	/** The data store shared by all clients connected to the server tracked by this workspace. */
	TUniquePtr<FConcertServerDataStore> DataStore;
};
