// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ChaosSolversModule.h"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#if INCLUDE_CHAOS

#include "ChaosStats.h"
#include "ChaosLog.h"
#include "HAL/PlatformProcess.h"
#include "Framework/PersistentTask.h"
#include "Misc/CoreDelegates.h"
#include "HAL/IConsoleManager.h"
#include "PBDRigidsSolver.h"

TAutoConsoleVariable<int32> CVarChaosThreadEnabled(
	TEXT("p.Chaos.DedicatedThreadEnabled"),
	1,
	TEXT("Enables a dedicated physics task/thread for Chaos tasks.")
	TEXT("0: Disabled")
	TEXT("1: Enabled"));

TAutoConsoleVariable<float> CVarDedicatedThreadDesiredHz(
	TEXT("p.Chaos.Thread.DesiredHz"),
	60.0f,
	TEXT("Desired update rate of the dedicated physics thread in Hz/FPS (Default 60.0f)"));

TAutoConsoleVariable<int32> CVarDedicatedThreadSyncThreshold(
	TEXT("p.Chaos.Thread.WaitThreshold"),
	16,
	TEXT("Desired wait time in ms before the game thread stops waiting to sync physics and just takes the last result. (default 16ms)")
);

static FAutoConsoleVariableSink CVarChaosModuleSink(FConsoleCommandDelegate::CreateStatic(&FChaosConsoleSinks::OnCVarsChanged));

void FChaosConsoleSinks::OnCVarsChanged()
{
	// #BG TODO - Currently this isn't dynamic, should be made to be switchable.
	FChaosSolversModule* ChaosModule = FModuleManager::Get().GetModulePtr<FChaosSolversModule>("ChaosSolvers");

	if(ChaosModule)
	{
		bool bCurrentlyRunning = ChaosModule->IsPersistentTaskRunning();
		bool bShouldBeRunning = CVarChaosThreadEnabled.GetValueOnGameThread() != 0;

		if(bCurrentlyRunning != bShouldBeRunning)
		{
			if(bShouldBeRunning)
			{
				// Spin up the threaded system. As part of doing this the solver storage member will be duplicated over to the physics thread
				ChaosModule->StartPhysicsTask();
			}
			else
			{
				// Spin down the physics thread. Our solver storages should be in sync here and the GT scenes should pick up ticking
				ChaosModule->EndPhysicsTask();
			}
		}

		if(ChaosModule->IsPersistentTaskRunning())
		{
			float NewHz = CVarDedicatedThreadDesiredHz.GetValueOnGameThread();
			ChaosModule->GetDispatcher()->EnqueueCommand([NewHz](Chaos::FPersistentPhysicsTask* Thread)
			{
				if(Thread)
				{
					Thread->SetTargetDt(1.0f / NewHz);
				}
			});
		}
	}
}

FSolverStateStorage::FSolverStateStorage()
	: Solver(nullptr)
{

}

FChaosSolversModule* FChaosSolversModule::GetModule()
{
	return FModuleManager::Get().GetModulePtr<FChaosSolversModule>("ChaosSolvers");
}

FChaosSolversModule::FChaosSolversModule()
	: bPersistentTaskSpawned(false)
	, PhysicsAsyncTask(nullptr)
	, PhysicsInnerTask(nullptr)
	, Dispatcher(nullptr)
{

}

void FChaosSolversModule::StartupModule()
{
	if(IsPersistentTaskEnabled())
	{
		StartPhysicsTask();
	}
	else
	{
		Dispatcher = new Chaos::Dispatcher<Chaos::DispatcherMode::SingleThread>(this);
	}
}

void FChaosSolversModule::ShutdownModule()
{
	EndPhysicsTask();

	FCoreDelegates::OnPreExit.RemoveAll(this);
}

bool FChaosSolversModule::IsPersistentTaskEnabled() const
{
	return CVarChaosThreadEnabled.GetValueOnGameThread() == 1;
}

bool FChaosSolversModule::IsPersistentTaskRunning() const
{
	return bPersistentTaskSpawned;
}

void FChaosSolversModule::StartPhysicsTask()
{
	// Create the dispatcher
	if(Dispatcher)
	{
		delete Dispatcher;
		Dispatcher = nullptr;
	}

	Dispatcher = new Chaos::Dispatcher<Chaos::DispatcherMode::DedicatedThread>(this);

	// Setup the physics thread (Cast the dispatcher out to the correct type for threaded work)
	const float SafeFps = FMath::Clamp(CVarDedicatedThreadDesiredHz.GetValueOnGameThread(), 5.0f, 1000.0f);
	PhysicsAsyncTask = new FAsyncTask<Chaos::FPersistentPhysicsTask>(1.0f / SafeFps, false, (Chaos::Dispatcher<Chaos::DispatcherMode::DedicatedThread>*)Dispatcher);
	PhysicsInnerTask = &PhysicsAsyncTask->GetTask();
	PhysicsAsyncTask->StartBackgroundTask();
	bPersistentTaskSpawned = true;

	PreExitHandle = FCoreDelegates::OnPreExit.AddRaw(this, &FChaosSolversModule::EndPhysicsTask);
}

void FChaosSolversModule::EndPhysicsTask()
{
	// Pull down the thread if it exists
	if(PhysicsInnerTask)
	{
		// Ask the physics thread to stop
		PhysicsInnerTask->RequestShutdown();
		// Wait for the stop
		PhysicsInnerTask->GetShutdownEvent()->Wait();
		PhysicsInnerTask = nullptr;
		// Wait for the actual task to complete so we can get rid of it, then delete
		PhysicsAsyncTask->EnsureCompletion(false);
		delete PhysicsAsyncTask;
		PhysicsAsyncTask = nullptr;

		bPersistentTaskSpawned = false;

		FCoreDelegates::OnPreExit.Remove(PreExitHandle);
	}

	// Destroy the dispatcher
	if(Dispatcher)
	{
		delete Dispatcher;
		Dispatcher = nullptr;
	}

	Dispatcher = new Chaos::Dispatcher<Chaos::DispatcherMode::SingleThread>(this);
}

Chaos::IDispatcher* FChaosSolversModule::GetDispatcher() const
{
	return Dispatcher;
}

Chaos::FPersistentPhysicsTask* FChaosSolversModule::GetDedicatedTask() const
{
	if(PhysicsAsyncTask)
	{
		return &PhysicsAsyncTask->GetTask();
	}

	return nullptr;
}

void FChaosSolversModule::SyncTask(bool bForceBlockingSync /*= false*/)
{
	// Hard lock the physics thread before syncing our data
	FChaosScopedPhysicsThreadLock ScopeLock(bForceBlockingSync ? MAX_uint32 : (uint32)(CVarDedicatedThreadSyncThreshold.GetValueOnGameThread()));

	// This will either get the results because physics finished, or fall back on whatever physics last gave us
	// to allow the game thread to continue on without stalling.
	PhysicsInnerTask->SyncProxiesFromCache(ScopeLock.DidGetLock());
}

FSolverStateStorage* FChaosSolversModule::CreateSolverState()
{
	SolverStorage.Add(new FSolverStateStorage());
	FSolverStateStorage* Storage = SolverStorage.Last();

	Storage->Solver = new Chaos::PBDRigidsSolver();

	if(IsPersistentTaskRunning() && Dispatcher)
	{
		// Need to let the thread know there's a new storage to care about
		Dispatcher->EnqueueCommand([Storage](Chaos::FPersistentPhysicsTask* PhysThread)
		{
			PhysThread->AddSolver(Storage);
		});
	}

	return Storage;
}

void FChaosSolversModule::DestroySolverState(FSolverStateStorage* InState)
{
	if(SolverStorage.Remove(InState) > 0)
	{
		delete InState;
	}
	else if(InState)
	{
		UE_LOG(LogChaosGeneral, Warning, TEXT("Passed valid solver state to DestroySolverState but it wasn't in the solver storage list! Make sure it was created using the Chaos module."));
	}
}

const TArray<FSolverStateStorage*>& FChaosSolversModule::GetSolverStorage() const
{
	return SolverStorage;
}

FChaosScopedPhysicsThreadLock::FChaosScopedPhysicsThreadLock()
	: FChaosScopedPhysicsThreadLock(MAX_uint32)
{

}

FChaosScopedPhysicsThreadLock::FChaosScopedPhysicsThreadLock(uint32 InMsToWait)
	: CompleteEvent(nullptr)
	, PTStallEvent(nullptr)
	, Module(nullptr)
	, bGotLock(false)
{
	Module = FChaosSolversModule::GetModule();
	checkSlow(Module && Module->GetDispatcher());

	Chaos::IDispatcher* PhysDispatcher = Module->GetDispatcher();
	if(PhysDispatcher->GetMode() == Chaos::DispatcherMode::DedicatedThread)
	{
		CompleteEvent = FPlatformProcess::GetSynchEventFromPool(false);
		PTStallEvent = FPlatformProcess::GetSynchEventFromPool(false);

		// Request a halt on the physics thread
		PhysDispatcher->EnqueueCommand([PTStall = PTStallEvent, GTSync = CompleteEvent](Chaos::FPersistentPhysicsTask* PhysThread)
		{
			PTStall->Trigger();
			GTSync->Wait();

			FPlatformProcess::ReturnSynchEventToPool(GTSync);
			FPlatformProcess::ReturnSynchEventToPool(PTStall);
		});

		{
			SCOPE_CYCLE_COUNTER(STAT_LockWaits);
			// Wait for the physics thread to actually stall
			bGotLock = PTStallEvent->Wait(InMsToWait);
		}

		if(!bGotLock)
		{
			// Trigger this if we didn't get a lock to avoid blocking the physics thread
			CompleteEvent->Trigger();
		}
	}
	else
	{
		CompleteEvent = nullptr;
		PTStallEvent = nullptr;
	}
}

FChaosScopedPhysicsThreadLock::~FChaosScopedPhysicsThreadLock()
{
	if(CompleteEvent && PTStallEvent && bGotLock)
	{
		CompleteEvent->Trigger();
	}

	// Can't return these here until the physics thread wakes up,
	// the physics thread will return these events (see FChaosScopedPhysicsLock::FChaosScopedPhysicsLock)
	CompleteEvent = nullptr;
	PTStallEvent = nullptr;
	Module = nullptr;
}

bool FChaosScopedPhysicsThreadLock::DidGetLock() const
{
	return bGotLock;
}

IMPLEMENT_MODULE(FChaosSolversModule, ChaosSolvers);

#else

// Workaround for module not having any exported symbols
CHAOSSOLVERS_API int ChaosSolversExportedSymbol = 0;

IMPLEMENT_MODULE(FDefaultModuleImpl, ChaosSolvers);

#endif
