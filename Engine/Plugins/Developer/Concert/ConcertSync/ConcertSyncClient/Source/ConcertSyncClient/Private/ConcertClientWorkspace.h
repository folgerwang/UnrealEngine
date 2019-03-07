// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/ITransaction.h"
#include "IConcertClientWorkspace.h"
#include "IConcertSessionHandler.h"
#include "ConcertWorkspaceMessages.h"

class IConcertClientSession;
class FConcertPackageLedger;
class FConcertSandboxPlatformFile;
class FConcertClientTransactionManager;
class ISourceControlProvider;
class FConcertClientActivityLedger;
class FConcertClientDataStore;
class FConcertClientLiveTransactionAuthors;

class FOutputDevice;
struct FAssetData;
struct FScopedSlowTask;

enum class EMapChangeType : uint8;

class FConcertClientWorkspace : public IConcertClientWorkspace
{
public:
	FConcertClientWorkspace(const TSharedRef<IConcertClientSession>& InSession);
	virtual ~FConcertClientWorkspace();

	// IConcertClientWorkspace interface
	virtual TSharedPtr<IConcertClientSession> GetSession() const override;
	virtual FGuid GetWorkspaceLockId() const override;
	virtual FGuid GetResourceLockId(const FName InResourceName) const override;
	virtual bool AreResourcesLockedBy(TArrayView<const FName> ResourceNames, const FGuid& ClientId) override;
	virtual TFuture<FConcertResourceLockResponse> LockResources(TArray<FName> InResourceNames) override;
	virtual TFuture<FConcertResourceLockResponse> UnlockResources(TArray<FName> InResourceNames) override;
	virtual TArray<FString> GatherSessionChanges() override;
	virtual bool PersistSessionChanges(TArrayView<const FString> InFilesToPersist, ISourceControlProvider* SourceControlProvider, TArray<FText>* OutFailureReasons = nullptr) override;
	virtual bool FindTransactionEvent(const uint64 TransactionIndex, FConcertTransactionFinalizedEvent& OutTransaction) const override;
	virtual bool FindPackageEvent(const FName& PackageName, const uint32 Revision, FConcertPackageInfo& OutPackage) const override;
	virtual uint64 GetActivityCount() const override;
	virtual uint64 GetLastActivities(uint32 Limit, TArray<FStructOnScope>& OutActivities) const override;
	virtual void GetActivities(uint64 Offset, uint32 Limit, TArray<FStructOnScope>& OutActivities) const override;
	virtual FOnAddActivity& OnAddActivity() override;
	virtual FOnWorkspaceSynchronized& OnWorkspaceSynchronized() override;
	virtual IConcertClientDataStore& GetDataStore() override;
	virtual bool IsAssetModifiedByOtherClients(const FName& AssetName, int* OutOtherClientsWithModifNum, TArray<FConcertClientInfo>* OutOtherClientsWithModifInfo, int OtherClientsWithModifMaxFetchNum) const override;

private:
	/** Bind the workspace to this session. */
	void BindSession(const TSharedRef<IConcertClientSession>& InSession);

	/** Unbind the workspace to its bound session. */
	void UnbindSession();

	/** */
	void HandleConnectionChanged(IConcertClientSession& InSession, EConcertConnectionStatus Status);

#if WITH_EDITOR
	/** */
	void SaveLiveTransactionsToPackages();

	/** */
	bool CanSavePackage(UPackage* InPackage, const FString& InFilename, FOutputDevice* ErrorLog);

	/** */
	void HandlePackageSaved(const FString& PackageFilename, UObject* Outer);

	/** */
	void HandleAssetAdded(UObject *Object);

	/** */
	void HandleAssetDeleted(UObject *Object);

	/** */
	void HandleAssetRenamed(const FAssetData& Data, const FString& OldName);

	/** */
	void HandleAssetLoaded(UObject* InAsset);

	/** */
	void HandlePostPIEStarted(const bool InIsSimulating);

	/** */
	void HandleSwitchBeginPIEAndSIE(const bool InIsSimulating);

	/** */
	void HandleEndPIE(const bool InIsSimulating);

	/** */
	void HandleTransactionStateChanged(const FTransactionContext& InTransactionContext, const ETransactionStateEventType InTransactionState);

	/** */
	void HandleObjectTransacted(UObject* InObject, const FTransactionObjectEvent& InTransactionEvent);
#endif	// WITH_EDITOR

	/** */
	void OnEndFrame();

	/** */
	void HandleWorkspaceSyncTransactionEvent(const FConcertSessionContext& Context, const FConcertWorkspaceSyncTransactionEvent& Event);

	/** */
	void HandleWorkspaceSyncPackageEvent(const FConcertSessionContext& Context, const FConcertWorkspaceSyncPackageEvent& Event);

	/** */
	void HandleWorkspaceSyncLockEvent(const FConcertSessionContext& Context, const FConcertWorkspaceSyncLockEvent& Event);

	/** */
	void HandleWorkspaceInitialSyncCompletedEvent(const FConcertSessionContext& Context, const FConcertWorkspaceInitialSyncCompletedEvent& Event);

	/** */
	void HandleResourceLockEvent(const FConcertSessionContext& Context, const FConcertResourceLockEvent& Event);

	/** */
	void SavePackageFile(const FConcertPackage& Package);

	/** */
	void DeletePackageFile(const FConcertPackage& Package);

	/**
	 * Can we currently perform content hot-reloads or purges?
	 * True if we are neither suspended nor unable to perform a blocking action, false otherwise.
	 */
	bool CanHotReloadOrPurge() const;

	/** */
	void HotReloadPendingPackages();

	/** */
	void PurgePendingPackages();

#if WITH_EDITOR
	/** */
	TUniquePtr<FConcertSandboxPlatformFile> SandboxPlatformFile; // TODO: Will need to ensure the sandbox also works on cooked clients
#endif

	/** */
	TUniquePtr<FConcertPackageLedger> PackageLedger;

	/** */
	TUniquePtr<FConcertClientTransactionManager> TransactionManager;

	/** Tracks locked resources from the server (resource ID -> endpoint ID) */
	TMap<FName, FGuid> LockedResources;

	/** Holds the client activity ledger. */
	TUniquePtr<FConcertClientActivityLedger> ActivityLedger;

	/** */
	TSharedPtr<IConcertClientSession> Session;

	/** */
	FDelegateHandle SessionConnectedHandle;

#if WITH_EDITOR
	/** */
	FCoreUObjectDelegates::FIsPackageOKToSaveDelegate OkToSaveBackupDelegate;

	/** */
	FDelegateHandle PostPIEStartedHandle;

	/** */
	FDelegateHandle SwitchBeginPIEAndSIEHandle;

	/** */
	FDelegateHandle EndPIEHandle;

	/** */
	FDelegateHandle TransactionStateChangedHandle;
	
	/** */
	FDelegateHandle ObjectTransactedHandle;
#endif	// WITH_EDITOR

	/** */
	FDelegateHandle OnEndFrameHandle;

	/** Map of packages that are in the process of being renamed */
	TMap<FName, FName> PackagesBeingRenamed;

	/** Array of package names that are pending a content hot-reload */
	TArray<FName> PackagesPendingHotReload;

	/** Array of package names that are pending an in-memory purge */
	TArray<FName> PackagesPendingPurge;

	/** True if we are currently saving a package */
	bool bIsSavingPackage;

	/** True if this client has performed its initial sync with the server session */
	bool bHasSyncedWorkspace;

	/** True if a request to finalize a workspace sync has been requested */
	bool bFinalizeWorkspaceSyncRequested;

	/** Slow task used during the initial sync of this workspace */
	TUniquePtr<FScopedSlowTask> InitialSyncSlowTask;

	/** The delegate called every time the workspace is synced. */
	FOnWorkspaceSynchronized OnWorkspaceSyncedDelegate;
	
	/** The session key/value store proxy. The real store is held by the server and shared across all clients. */
	TUniquePtr<FConcertClientDataStore> DataStore;

	/** Tracks the clients that have live transactions on any given packages. */
	TUniquePtr<FConcertClientLiveTransactionAuthors> LiveTransactionAuthors;
};
