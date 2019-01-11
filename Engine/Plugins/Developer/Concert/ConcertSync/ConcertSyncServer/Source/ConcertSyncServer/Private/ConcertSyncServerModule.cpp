// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Misc/CoreDelegates.h"

#include "IConcertModule.h"
#include "IConcertServer.h"
#include "ConcertServerWorkspace.h"
#include "ConcertServerSequencerManager.h"

/**
 * 
 */
class FConcertSyncServerModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		IConcertServerPtr ConcertServer = IConcertModule::Get().GetServerInstance();
		if (ConcertServer.IsValid())
		{
			OnSessionStartupHandle = ConcertServer->OnSessionStartup().AddRaw(this, &FConcertSyncServerModule::RegisterConcertSyncHandlers);
			OnSessionShutdownHandle = ConcertServer->OnSessionShutdown().AddRaw(this, &FConcertSyncServerModule::UnregisterConcertSyncHandlers);
		}

		AppPreExitDelegateHandle = FCoreDelegates::OnPreExit.AddRaw(this, &FConcertSyncServerModule::HandleAppPreExit);

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

		IConcertServerPtr ConcertServer = IConcertModule::Get().GetServerInstance();
		if (ConcertServer.IsValid())
		{
			TArray<TSharedPtr<IConcertServerSession>> ServerSessions = ConcertServer->GetSessions();
			for (const TSharedPtr<IConcertServerSession>& ServerSession : ServerSessions)
			{
				UnregisterConcertSyncHandlers(ServerSession.ToSharedRef());
			}

			if (OnSessionStartupHandle.IsValid())
			{
				ConcertServer->OnSessionStartup().Remove(OnSessionStartupHandle);
				OnSessionStartupHandle.Reset();
			}

			if (OnSessionShutdownHandle.IsValid())
			{
				ConcertServer->OnSessionShutdown().Remove(OnSessionShutdownHandle);
				OnSessionShutdownHandle.Reset();
			}
		}
	}

private:
	void CreateWorkspace(const TSharedRef<IConcertServerSession>& InSession)
	{
		DestroyWorkspace(InSession);
		Workspaces.Add(*InSession->GetName(), MakeShared<FConcertServerWorkspace>(InSession/*TODO Some config maybe*/));
	}

	void DestroyWorkspace(const TSharedRef<IConcertServerSession>& InSession)
	{
		// TODO : some shutdown?
		Workspaces.Remove(*InSession->GetName());
	}

	void CreateSequencerManager(const TSharedRef<IConcertServerSession>& InSession)
	{
		DestroySequencerManager(InSession);
		SequencerManagers.Add(*InSession->GetName(), MakeShared<FConcertServerSequencerManager>(InSession));
	}

	void DestroySequencerManager(const TSharedRef<IConcertServerSession>& InSession)
	{
		SequencerManagers.Remove(*InSession->GetName());
	}

	void RegisterConcertSyncHandlers(TSharedRef<IConcertServerSession> InSession)
	{
		CreateWorkspace(InSession);
		CreateSequencerManager(InSession);
	}

	void UnregisterConcertSyncHandlers(TSharedRef<IConcertServerSession> InSession)
	{
		DestroyWorkspace(InSession);
		DestroySequencerManager(InSession);
	}

	/** Map of Session Name to their associated workspaces */
	TMap<FName, TSharedPtr<FConcertServerWorkspace>> Workspaces;

	/** Map of Session Name to their associated workspaces */
	TMap<FName, TSharedPtr<FConcertServerSequencerManager>> SequencerManagers;

	/** Delegate Handle for the PreExit callback, needed to execute UObject related shutdowns */
	FDelegateHandle AppPreExitDelegateHandle;

	/** Delegate handle for a the callback when a session starts up */
	FDelegateHandle OnSessionStartupHandle;

	/** Delegate handle for a the callback when a session shuts down */
	FDelegateHandle OnSessionShutdownHandle;
};

IMPLEMENT_MODULE(FConcertSyncServerModule, ConcertSyncServer);
