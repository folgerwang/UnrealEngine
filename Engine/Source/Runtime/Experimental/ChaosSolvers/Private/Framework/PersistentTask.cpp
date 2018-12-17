// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if INCLUDE_CHAOS

#include "Framework/PersistentTask.h"

#include "Modules/ModuleManager.h"

#include "ChaosLog.h"
#include "ChaosStats.h"
#include "ChaosSolversModule.h"

#include "Chaos/Framework/Parallel.h"
#include "Framework/PhysicsProxy.h"
#include "PBDRigidsSolver.h"
#include "Field/FieldSystem.h"

namespace Chaos
{
	FPersistentPhysicsTask::FPersistentPhysicsTask(float InTargetDt, bool bInAvoidSpiral, Dispatcher<DispatcherMode::DedicatedThread>* InDispatcher)
		: TargetDt(InTargetDt)
		, CommandDispatcher(InDispatcher)
	{
		ShutdownEvent = FPlatformProcess::GetSynchEventFromPool(true);
	}

	FPersistentPhysicsTask::~FPersistentPhysicsTask()
	{
		FPlatformProcess::ReturnSynchEventToPool(ShutdownEvent);
	}

	void FPersistentPhysicsTask::DoWork()
	{
		// Capture solver states from the module by copying the current state. The module
		// will inject any new solvers with a command.
		FChaosSolversModule& ChaosModule = FModuleManager::Get().GetModuleChecked<FChaosSolversModule>("ChaosSolvers");
		SolverEntries = ChaosModule.GetSolverStorage();

		bRunning = true;
		ShutdownEvent->Reset();

		double LastTime = FPlatformTime::Seconds();
		double CurrTime = 0.0;
		while(bRunning)
		{
			SCOPE_CYCLE_COUNTER(STAT_PhysicsAdvance);

			// Run global and task commands
			{
				SCOPE_CYCLE_COUNTER(STAT_PhysCommands);
				TFunction<void()> GlobalCommand;
				while(CommandDispatcher->GlobalCommandQueue.Dequeue(GlobalCommand))
				{
					GlobalCommand();
				}
			}

			{
				SCOPE_CYCLE_COUNTER(STAT_TaskCommands);
				TFunction<void(FPersistentPhysicsTask*)> TaskCommand;
				while(CommandDispatcher->TaskCommandQueue.Dequeue(TaskCommand))
				{
					TaskCommand(this);
				}
			}

			// Go wide if possible on the solvers
			const int32 NumSolverEntries = SolverEntries.Num();
			PhysicsParallelFor(NumSolverEntries, [&](int32 Index)
			{
				SCOPE_CYCLE_COUNTER(STAT_SolverAdvance);
				FSolverStateStorage* Entry = SolverEntries[Index];

				HandleSolverCommands(Entry->Solver);

				if(Entry->ActiveProxies.Num() > 0)
				{
					AdvanceSolver(Entry->Solver);

					{
						FRWScopeLock(CacheLock, SLT_ReadOnly);
						for(FPhysicsProxy* Proxy : Entry->ActiveProxies)
						{
							Proxy->CacheResults();
						}
					}

					{
						FRWScopeLock(CacheLock, SLT_Write);
						for(FPhysicsProxy* Proxy : Entry->ActiveProxies)
						{
							Proxy->FlipCache();
						}
					}
				}
			});

			// Record our time and sync up our target update rate
			CurrTime = FPlatformTime::Seconds();
			double ActualDt = CurrTime - LastTime;

			if((float)ActualDt > TargetDt)
			{
				// Warn, we've gone over
				UE_LOG(LogChaosDebug, Log, TEXT("PhysAdvance: Exceeded requested Dt of %.3f (%.2fFPS). Ran for %.3f"), TargetDt, 1.0f / TargetDt, ActualDt);
			}
			else
			{
				// #BG TODO need some way to handle abandonning this when the gamethread requests a sync
				// Or just running more commands in general otherwise this is dead time.

				UE_LOG(LogChaosDebug, Verbose, TEXT("PhysAdvance: Advance took %.3f, sleeping for %.3f to reach target Dt of %.3f (%.2fFPS)"), ActualDt, TargetDt - ActualDt, TargetDt, 1.0f / TargetDt);
				FPlatformProcess::Sleep((float)(TargetDt - ActualDt));
			}

			LastTime = FPlatformTime::Seconds();
		}

		ShutdownEvent->Trigger();
	}

	void FPersistentPhysicsTask::AddSolver(FSolverStateStorage* InSolverState)
	{
		SolverEntries.Add(InSolverState);
	}

	void FPersistentPhysicsTask::RemoveSolver(FSolverStateStorage* InSolverState)
	{
		if(InSolverState)
		{
			if(InSolverState->ActiveProxies.Num() > 0)
			{
				// Proxies still exist, warn user
				UE_LOG(LogChaosGeneral, Warning, TEXT("Removing a solver from physics async task but it still has proxies. Remove the proxies before the scene shuts down."));
			}
		}

		SolverEntries.RemoveAll([InSolverState](const FSolverStateStorage* Test)
		{
			return Test == InSolverState;
		});
	}

	void FPersistentPhysicsTask::AddProxy(FPhysicsProxy* InProxy)
	{
		FSolverStateStorage* Entry = GetSolverEntry(InProxy->Solver);
		if(Entry)
		{
			Entry->ActiveProxies.Add(InProxy);
		}
	}

	void FPersistentPhysicsTask::RemoveProxy(FPhysicsProxy* InProxy)
	{
		FSolverStateStorage* Entry = GetSolverEntry(InProxy->Solver);
		if(Entry)
		{
			Entry->ActiveProxies.Remove(InProxy);
			Entry->Solver->UnregisterCallbacks(InProxy->GetCallbacks());
			RemovedProxies.Add(InProxy);
		}
	}

	void FPersistentPhysicsTask::AddFieldProxy(FPhysicsProxy* InProxy)
	{
		FSolverStateStorage* Entry = GetSolverEntry(InProxy->Solver);
		if(Entry)
		{
			Entry->ActiveProxies.Add(InProxy);
		}
	}

	void FPersistentPhysicsTask::RemoveFieldProxy(FPhysicsProxy* InProxy)
	{
		FSolverStateStorage* Entry = GetSolverEntry(InProxy->Solver);
		if(Entry)
		{
			Entry->ActiveProxies.Remove(InProxy);
			Entry->Solver->UnregisterFieldCallbacks(static_cast<FSolverFieldCallbacks*>(InProxy->GetCallbacks()));
			RemovedProxies.Add(InProxy);
		}
	}

	void FPersistentPhysicsTask::SyncProxiesFromCache(bool bFullSync /*= false*/)
	{
		// "Read" lock the cachelock here. Write is for flipping. Acquiring read here prevents a flip happening
		// on the physics thread (Sync called from game thread).
		FRWScopeLock(CacheLock, SLT_ReadOnly);

		for(FSolverStateStorage* Entry : SolverEntries)
		{
			for(FPhysicsProxy* Proxy : Entry->ActiveProxies)
			{
				Proxy->SyncToCache();
			}
		}

		if(bFullSync)
		{
			for(FPhysicsProxy* Proxy : RemovedProxies)
			{
				Proxy->SyncBeforeDestroy();
				delete Proxy;
			}

			RemovedProxies.Reset();
		}
	}

	void FPersistentPhysicsTask::RequestShutdown()
	{
		bRunning = false;
	}

	FEvent* FPersistentPhysicsTask::GetShutdownEvent()
	{
		return ShutdownEvent;
	}

	void FPersistentPhysicsTask::SetTargetDt(float InNewDt)
	{
		TargetDt = InNewDt;
	}

	FSolverStateStorage* FPersistentPhysicsTask::GetSolverEntry(PBDRigidsSolver* InSolver)
	{
		FSolverStateStorage** Entry = SolverEntries.FindByPredicate([&](const FSolverStateStorage* Item)
		{
			return Item->Solver == InSolver;
		});

		return Entry ? *Entry : nullptr;
	}

	void FPersistentPhysicsTask::HandleSolverCommands(PBDRigidsSolver* InSolver)
	{
		SCOPE_CYCLE_COUNTER(STAT_HandleSolverCommands);

		check(InSolver);
		TQueue<TFunction<void(PBDRigidsSolver*)>, EQueueMode::Mpsc>& Queue = InSolver->CommandQueue;
		TFunction<void(PBDRigidsSolver*)> Command;
		while(Queue.Dequeue(Command))
		{
			Command(InSolver);
		}
	}

	void FPersistentPhysicsTask::AdvanceSolver(PBDRigidsSolver* InSolver)
	{
		SCOPE_CYCLE_COUNTER(STAT_IntegrateSolver);

		check(InSolver);
		InSolver->AdvanceSolverBy(TargetDt);
	}
}

#endif
