// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleInterface.h"

#include "ISourceControlModule.h"
#include "SourceControlOperations.h"

#include "IConcertSyncClientModule.h"
#include "IConcertModule.h"
#include "IConcertClient.h"
#include "IConcertSession.h"
#include "ConcertClientWorkspace.h"
#include "ConcertClientSequencerManager.h"
#include "ConcertClientPresenceManager.h"
#include "ConcertSourceControlProxy.h"

#define LOCTEXT_NAMESPACE "ConcertSyncClient"

/**
 * Connection task used to validate that the workspace has no local changes (according to source control).
 */
class FConcertClientConnectionValidationTask : public IConcertClientConnectionTask
{
public:
	FConcertClientConnectionValidationTask()
		: SharedState(MakeShared<FSharedAsyncState, ESPMode::ThreadSafe>())
	{
	}

	virtual void Execute() override
	{
		check(SharedState && SharedState->Result == EConcertResponseCode::Pending);

		ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
		ISourceControlProvider& SourceControlProvider = SourceControlModule.GetProvider();

		// Query source control to make sure we don't have any local changes before allowing us to join a remote session
		if (SourceControlModule.IsEnabled() && SourceControlProvider.IsAvailable())
		{
			// Query all content paths
			TArray<FString> RootPaths;
			FPackageName::QueryRootContentPaths(RootPaths);
			for (const FString& RootPath : RootPaths)
			{
				const FString RootPathOnDisk = FPackageName::LongPackageNameToFilename(RootPath);
				SharedState->ContentPaths.Add(FPaths::ConvertRelativePathToFull(RootPathOnDisk));
			}

			UpdateStatusOperation = ISourceControlOperation::Create<FUpdateStatus>();
			UpdateStatusOperation->SetUpdateModifiedState(true);
			SourceControlProvider.Execute(UpdateStatusOperation.ToSharedRef(), SharedState->ContentPaths, EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateStatic(&FConcertClientConnectionValidationTask::HandleAsyncResult, SharedState.ToSharedRef()));
		}
		else
		{
			SharedState->Result = EConcertResponseCode::Success;
		}
	}

	virtual void Abort() override
	{
		Cancel();
		SharedState.Reset(); // Always abandon the result
	}

	virtual void Tick(const bool bShouldCancel) override
	{
		if (bShouldCancel)
		{
			Cancel();
		}
	}

	virtual bool CanCancel() const override
	{
		// Always report we can be canceled (if we haven't been aborted) as even if the source control 
		// provider doesn't natively support cancellation, we just let it finish but disown the result
		return SharedState.IsValid();
	}

	virtual EConcertResponseCode GetStatus() const override
	{
		return SharedState ? SharedState->Result : EConcertResponseCode::Failed;
	}

	virtual FText GetError() const override
	{
		return SharedState ? SharedState->ErrorText : LOCTEXT("ValidatingWorkspace_Aborted", "The workspace validation request was aborted.");
	}

	virtual FText GetDescription() const override
	{
		return LOCTEXT("ValidatingWorkspace", "Validating Workspace...");
	}

private:
	struct FSharedAsyncState
	{
		TArray<FString> ContentPaths;
		EConcertResponseCode Result = EConcertResponseCode::Pending;
		FText ErrorText;
	};

	/** Callback for the source control result - deliberately not a member function as 'this' may be deleted while the request is in flight, so the shared state is used as a safe bridge */
	static void HandleAsyncResult(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FSharedAsyncState, ESPMode::ThreadSafe> InSharedState)
	{
		switch (InResult)
		{
		case ECommandResult::Succeeded:
		{
			ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
			ISourceControlProvider& SourceControlProvider = SourceControlModule.GetProvider();
			if (ensure(SourceControlModule.IsEnabled() && SourceControlProvider.IsAvailable()))
			{
				bool bHasLocalChanges = false;
				for (const FString& ContentPath : InSharedState->ContentPaths)
				{
					IFileManager::Get().IterateDirectoryRecursively(*ContentPath, [&bHasLocalChanges, &SourceControlProvider](const TCHAR* InFilename, bool InIsDirectory) -> bool
					{
						const FString FilenameStr = InFilename;
						if (!InIsDirectory && FPackageName::IsPackageFilename(FilenameStr))
						{
							FSourceControlStatePtr FileState = SourceControlProvider.GetState(FilenameStr, EStateCacheUsage::Use);
							if (FileState.IsValid() && (FileState->IsAdded() || FileState->IsDeleted() || FileState->IsModified() || (SourceControlProvider.UsesCheckout() && FileState->IsCheckedOut()))) // TODO: Include unversioned files?
							{
								bHasLocalChanges = true;
								return false; // end iteration
							}
						}
						return true; // continue iteration
					});

					if (bHasLocalChanges)
					{
						break;
					}
				}
				if (bHasLocalChanges)
				{
					InSharedState->Result = EConcertResponseCode::Failed;
					InSharedState->ErrorText = LOCTEXT("ValidatingWorkspace_LocalChanges", "This workspace has local changes. Please submit or revert these changes before attempting to connect.");
				}
				else
				{
					InSharedState->Result = EConcertResponseCode::Success;
				}
			}
		}
		break;

		case ECommandResult::Cancelled:
			InSharedState->Result = EConcertResponseCode::Failed;
			InSharedState->ErrorText = LOCTEXT("ValidatingWorkspace_Canceled", "The workspace validation request was canceled.");
			break;

		default:
			InSharedState->Result = EConcertResponseCode::Failed;
			InSharedState->ErrorText = LOCTEXT("ValidatingWorkspace_Failed", "The workspace validation request failed. Please check your source control settings.");
			break;
		}
	}

	void Cancel()
	{
		if (SharedState && UpdateStatusOperation)
		{
			ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
			ISourceControlProvider& SourceControlProvider = SourceControlModule.GetProvider();
			if (ensure(SourceControlModule.IsEnabled() && SourceControlProvider.IsAvailable()))
			{
				// Gracefully cancel the operation if we're able to
				// Otherwise just abort it by disowning the result
				if (SourceControlProvider.CanCancelOperation(UpdateStatusOperation.ToSharedRef()))
				{
					SourceControlProvider.CancelOperation(UpdateStatusOperation.ToSharedRef());
				}
				else
				{
					SharedState.Reset();
				}
			}
			UpdateStatusOperation.Reset();
		}
	}

	TSharedPtr<FUpdateStatus, ESPMode::ThreadSafe> UpdateStatusOperation;
	TSharedPtr<FSharedAsyncState, ESPMode::ThreadSafe> SharedState;
};

/**
 * Implements the Concert Sync module for Event synchronization
 */
class FConcertSyncClientModule : public IConcertSyncClientModule
{
public:
	FConcertSyncClientModule() {}
	virtual ~FConcertSyncClientModule() {}

	virtual void StartupModule() override
	{
		IConcertClientPtr ConcertClient = IConcertModule::Get().GetClientInstance();
		if (ConcertClient.IsValid())
		{
			OnSessionStartupHandle = ConcertClient->OnSessionStartup().AddRaw(this, &FConcertSyncClientModule::RegisterConcertSyncHandlers);
			OnSessionShutdownHandle = ConcertClient->OnSessionShutdown().AddRaw(this, &FConcertSyncClientModule::UnregisterConcertSyncHandlers);
			OnGetPreConnectionTasksHandle = ConcertClient->OnGetPreConnectionTasks().AddRaw(this, &FConcertSyncClientModule::GetPreConnectionTasks);
		}

		AppPreExitDelegateHandle = FCoreDelegates::OnPreExit.AddRaw(this, &FConcertSyncClientModule::HandleAppPreExit);

		ParseSettingOverrides();

		// Boot the client instance
		const UConcertClientConfig* ClientConfig = GetDefault<UConcertClientConfig>();
		ConcertClient->Configure(ClientConfig);
		ConcertClient->Startup();

		// if auto connection, start auto-connection routine
		if (ClientConfig->bAutoConnect)
		{
			ConcertClient->DefaultConnect();
		}
	}

	virtual void ShutdownModule() override
	{
		// Unhook AppPreExit and call it
		if (AppPreExitDelegateHandle.IsValid())
		{
			FCoreDelegates::OnPreExit.Remove(AppPreExitDelegateHandle);
			AppPreExitDelegateHandle.Reset();
		}
		HandleAppPreExit();
	}

	// Module shutdown is dependent on the UObject system which is currently shutdown on AppExit
	void HandleAppPreExit()
	{
		// if UObject system isn't initialized, skip shutdown
		if (!UObjectInitialized())
		{
			return;
		}

		IConcertClientPtr ConcertClient = IConcertModule::Get().GetClientInstance();
		if (ConcertClient.IsValid())
		{
			TSharedPtr<IConcertClientSession> ConcertClientSession = ConcertClient->GetCurrentSession();
			if (ConcertClientSession.IsValid())
			{
				UnregisterConcertSyncHandlers(ConcertClientSession.ToSharedRef());
			}

			ConcertClient->OnSessionStartup().Remove(OnSessionStartupHandle);
			OnSessionStartupHandle.Reset();

			ConcertClient->OnSessionShutdown().Remove(OnSessionShutdownHandle);
			OnSessionShutdownHandle.Reset();

			ConcertClient->OnGetPreConnectionTasks().Remove(OnGetPreConnectionTasksHandle);
			OnGetPreConnectionTasksHandle.Reset();
		}
	}

	virtual TSharedPtr<IConcertClientWorkspace> GetWorkspace()
	{
		return Workspace;
	}

	virtual FOnConcertClientWorkspaceStartupOrShutdown& OnWorkspaceStartup()
	{
		return OnWorkspaceStartupDelegate;
	}

	virtual FOnConcertClientWorkspaceStartupOrShutdown& OnWorkspaceShutdown()
	{
		return OnWorkspaceShutdownDelegate;
	}

	virtual void SetPresenceEnabled(const bool IsEnabled) override
	{
#if WITH_EDITOR
		if (PresenceManager.IsValid())
		{
			PresenceManager->SetPresenceEnabled(IsEnabled);
		}
#endif
	}

	virtual void SetPresenceVisibility(const FString& DisplayName, bool Visibility, bool PropagateToAll) override
	{
#if WITH_EDITOR
		if (PresenceManager.IsValid())
		{
			PresenceManager->SetPresenceVisibility(DisplayName, Visibility, PropagateToAll);
		}
#endif
	}

	virtual void JumpToPresence(const FGuid OtherEndpointId) override
	{
#if WITH_EDITOR
		if (PresenceManager.IsValid())
		{
			PresenceManager->InitiateJumpToPresence(OtherEndpointId);
		}
#endif
	}

	virtual FString GetPresenceWorldPath(const FGuid EndpointId) override
	{
#if WITH_EDITOR
		if (PresenceManager.IsValid())
		{
			return PresenceManager->GetClientWorldPath(EndpointId);
		}
#endif
		return FString();
	}

	virtual void PersistSessionChanges() override
	{
#if WITH_EDITOR
		if (Workspace.IsValid())
		{
			TArray<FString> SessionChanges = Workspace->GatherSessionChanges();
			Workspace->PersistSessionChanges(SessionChanges, &SourceControlProxy);
		}
#endif
	}

private:
#if WITH_EDITOR
	void CreatePresenceManager(const TSharedRef<IConcertClientSession>& InSession)
	{
		DestroyPresenceManager();
		PresenceManager = MakeShared<FConcertClientPresenceManager>(InSession);
	}

	void DestroyPresenceManager()
	{
		PresenceManager.Reset();
	}
#endif

	void CreateWorkspace(const TSharedRef<IConcertClientSession>& InSession)
	{
		DestroyWorkspace();
		Workspace = MakeShared<FConcertClientWorkspace>(InSession/*TODO Some config maybe*/);
		OnWorkspaceStartupDelegate.Broadcast(Workspace);
#if WITH_EDITOR
		if (GIsEditor)
		{
			SourceControlProxy.SetWorkspace(Workspace);
		}
#endif
	}

	void DestroyWorkspace()
	{
#if WITH_EDITOR
		if (GIsEditor)
		{
			SourceControlProxy.SetWorkspace(nullptr);
		}
#endif
		OnWorkspaceShutdownDelegate.Broadcast(Workspace);
		Workspace.Reset();
	}

	void RegisterConcertSyncHandlers(TSharedRef<IConcertClientSession> InSession)
	{
		CreateWorkspace(InSession);
#if WITH_EDITOR
		if (GIsEditor)
		{
			CreatePresenceManager(InSession);
		}
		SequencerEventClient.Register(InSession);
#endif
	}

	void UnregisterConcertSyncHandlers(TSharedRef<IConcertClientSession> InSession)
	{
#if WITH_EDITOR
		SequencerEventClient.Unregister(InSession);
		DestroyPresenceManager();
#endif
		DestroyWorkspace();
	}

	void ParseSettingOverrides()
	{
		UConcertClientConfig* ClientConfig = GetMutableDefault<UConcertClientConfig>();

		// CONCERTAUTOCONNECT
		{
			if (FParse::Param(FCommandLine::Get(), TEXT("CONCERTAUTOCONNECT")))
			{
				ClientConfig->bAutoConnect = true;
			}

			bool bAutoConnect = false;
			if (FParse::Bool(FCommandLine::Get(), TEXT("-CONCERTAUTOCONNECT="), bAutoConnect))
			{
				ClientConfig->bAutoConnect = bAutoConnect;
			}
		}

		// CONCERTSERVER
		{
			FString DefaultServerURL;
			if (FParse::Value(FCommandLine::Get(), TEXT("-CONCERTSERVER="), DefaultServerURL))
			{
				ClientConfig->DefaultServerURL = DefaultServerURL;
			}
		}

		// CONCERTSESSION
		{
			FString DefaultSessionName;
			if (FParse::Value(FCommandLine::Get(), TEXT("-CONCERTSESSION="), DefaultSessionName))
			{
				ClientConfig->DefaultSessionName = DefaultSessionName;
			}
		}

		// CONCERTSESSIONLOADDATA
		{
			FString DefaultSessionToRestore;
			if (FParse::Value(FCommandLine::Get(), TEXT("-CONCERTSESSIONTORESTORE="), DefaultSessionToRestore))
			{
				ClientConfig->DefaultSessionToRestore = DefaultSessionToRestore;
			}
		}

		// CONCERTSESSIONSAVEDATA
		{
			FString DefaultSaveSessionAs;
			if (FParse::Value(FCommandLine::Get(), TEXT("-CONCERTSAVESESSIONAS="), DefaultSaveSessionAs))
			{
				ClientConfig->DefaultSaveSessionAs = DefaultSaveSessionAs;
			}
		}

		// CONCERTDISPLAYNAME
		{
			FString DefaultDisplayName;
			if (FParse::Value(FCommandLine::Get(), TEXT("-CONCERTDISPLAYNAME="), DefaultDisplayName))
			{
				ClientConfig->ClientSettings.DisplayName = DefaultDisplayName;
			}
		}
	}

	void GetPreConnectionTasks(const IConcertClient& InClient, TArray<TUniquePtr<IConcertClientConnectionTask>>& OutTasks)
	{
		OutTasks.Emplace(MakeUnique<FConcertClientConnectionValidationTask>());
	}

	/** Delegate Handle for the PreExit callback, needed to execute UObject related shutdowns */
	FDelegateHandle AppPreExitDelegateHandle;

	/** Delegate handle for a the callback when a session starts up */
	FDelegateHandle OnSessionStartupHandle;

	/** Delegate handle for a the callback when a session shuts down */
	FDelegateHandle OnSessionShutdownHandle;

	/** Delegate handle for the callback to get pre-connection tasks */
	FDelegateHandle OnGetPreConnectionTasksHandle;

	/** Client Workspace for the current session. */
	TSharedPtr<FConcertClientWorkspace> Workspace;

	/** Delegate called on every workspace startup. */
	FOnConcertClientWorkspaceStartupOrShutdown OnWorkspaceStartupDelegate;

	/** Delegate called on every workspace shutdown. */
	FOnConcertClientWorkspaceStartupOrShutdown OnWorkspaceShutdownDelegate;

#if WITH_EDITOR
	/** Presence manager for the current session. */
	TSharedPtr<FConcertClientPresenceManager> PresenceManager;

	/** Sequencer event manager for Concert session. */
	FSequencerEventClient SequencerEventClient;

	/** Source Control Provider Proxy for Concert session. */
	FConcertSourceControlProxy SourceControlProxy;
#endif
};

IMPLEMENT_MODULE(FConcertSyncClientModule, ConcertSyncClient);

#undef LOCTEXT_NAMESPACE
