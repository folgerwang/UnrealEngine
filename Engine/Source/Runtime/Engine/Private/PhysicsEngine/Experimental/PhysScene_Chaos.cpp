// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if INCLUDE_CHAOS

#include "Physics/Experimental/PhysScene_Chaos.h"
#include "CoreMinimal.h"
#include "Async/AsyncWork.h"
#include "Async/ParallelFor.h"
#include "Misc/ScopeLock.h"
#include "PBDRigidsSolver.h"
#include "ChaosSolversModule.h"
#include "Framework/Dispatcher.h"
#include "Framework/PersistentTask.h"
#include "ChaosLog.h"
#include "Framework/PhysicsProxy.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Engine/Engine.h"
#include "Field/FieldSystem.h"
#include "GameDelegates.h"
#include "Misc/CoreDelegates.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogFPhysScene_ChaosSolver, Log, All);

class FPhysicsThreadSyncCaller : public FTickableGameObject
{
public:

	FPhysicsThreadSyncCaller()
	{
		ChaosModule = FModuleManager::Get().GetModulePtr<FChaosSolversModule>("ChaosSolvers");
		check(ChaosModule);

		WorldCleanupHandle = FWorldDelegates::OnPostWorldCleanup.AddRaw(this, &FPhysicsThreadSyncCaller::OnWorldDestroyed);
	}

	~FPhysicsThreadSyncCaller()
	{
		if(WorldCleanupHandle.IsValid())
		{
			FWorldDelegates::OnPostWorldCleanup.Remove(WorldCleanupHandle);
		}
	}

	virtual void Tick(float DeltaTime) override
	{
		if(ChaosModule->IsPersistentTaskRunning())
		{
			ChaosModule->SyncTask();
		}
	}

	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(PhysicsThreadSync, STATGROUP_Tickables);
	}

	virtual bool IsTickableInEditor() const override
	{
		return false;
	}

private:

	void OnWorldDestroyed(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources)
	{
		// This should really only sync if it's the right world, but for now always sync on world destroy.
		if(ChaosModule->IsPersistentTaskRunning())
		{
			ChaosModule->SyncTask(true);
		}
	}

	FChaosSolversModule* ChaosModule;
	FDelegateHandle WorldCleanupHandle;
};
static FPhysicsThreadSyncCaller* SyncCaller;

FPhysScene_Chaos::FPhysScene_Chaos()
	: ChaosModule(nullptr)
	, SolverStorage(nullptr)
{
	ChaosModule = FModuleManager::Get().GetModulePtr<FChaosSolversModule>("ChaosSolvers");
	check(ChaosModule);

	SolverStorage = ChaosModule->CreateSolverState();
	check(SolverStorage);

	// If we're running the physics thread, hand over the solver to it - we are no longer
	// able to access the solver on the game thread and should only use commands
	if(ChaosModule->IsPersistentTaskEnabled())
	{
		// Should find a better way to spawn this. Engine module has no apeiron singleton right now.
		// this caller will tick after all worlds have ticked and tell the apeiron module to sync
		// all of the active proxies it has from the physics thread
		if(!SyncCaller)
		{
			SyncCaller = new FPhysicsThreadSyncCaller();
		}
	}

	// #BGallagher Temporary while we're using the global scene singleton. Shouldn't be required
	// once we have a better lifecycle for the scenes.
	FCoreDelegates::OnPreExit.AddRaw(this, &FPhysScene_Chaos::Shutdown);

#if WITH_EDITOR
	FGameDelegates::Get().GetEndPlayMapDelegate().AddRaw(this, &FPhysScene_Chaos::OnWorldEndPlay);
#endif
}

FPhysScene_Chaos::~FPhysScene_Chaos()
{
	Shutdown();
	
	FCoreDelegates::OnPreExit.RemoveAll(this);

#if WITH_EDITOR
	FGameDelegates::Get().GetEndPlayMapDelegate().RemoveAll(this);
#endif
}

TSharedPtr<FPhysScene_Chaos> FPhysScene_Chaos::GetInstance()
{
	UE_LOG(LogFPhysScene_ChaosSolver, Verbose, TEXT("PBDRigidsSolver::GetInstance()"));
	static TSharedPtr<FPhysScene_Chaos> Instance(new FPhysScene_Chaos);
	return Instance;
}

bool FPhysScene_Chaos::IsTickable() const
{
	const bool bDedicatedThread = ChaosModule->IsPersistentTaskEnabled();

	return !bDedicatedThread && GetSolver()->Enabled();
}

void FPhysScene_Chaos::Tick(float DeltaTime)
{
	float SafeDelta = FMath::Clamp(DeltaTime, 0.0f, UPhysicsSettings::Get()->MaxPhysicsDeltaTime);

	UE_LOG(LogFPhysScene_ChaosSolver, Verbose, TEXT("FPhysScene_Chaos::Tick(%3.5f)"), SafeDelta);
	GetSolver()->AdvanceSolverBy(SafeDelta);

	// Sync proxies after simulation
	for(Chaos::FPhysicsProxy* Proxy : SolverStorage->ActiveProxies)
	{
		// #BGallagher TODO Just use one side of the buffer for single-thread tick
		Proxy->CacheResults();
		Proxy->FlipCache();
		Proxy->SyncToCache();
	}
}

Chaos::PBDRigidsSolver* FPhysScene_Chaos::GetSolver() const
{
	return SolverStorage ? SolverStorage->Solver : nullptr;
}

Chaos::IDispatcher* FPhysScene_Chaos::GetDispatcher() const
{
	return ChaosModule ? ChaosModule->GetDispatcher() : nullptr;
}

void FPhysScene_Chaos::AddProxy(Chaos::FPhysicsProxy* InProxy)
{
	check(IsInGameThread());
	const bool bDedicatedThread = ChaosModule->IsPersistentTaskEnabled();

	InProxy->SetSolver(GetSolver());

	Chaos::IDispatcher* PhysDispatcher = GetDispatcher();

	if(bDedicatedThread && PhysDispatcher)
	{
		// Ensure that if we need to create the callbacks it's done on the main thread so UObjects etc. can be queried
		FSolverCallbacks* CreatedCallbacks = InProxy->GetCallbacks();

		// Pass the proxy off to the physics thread
		PhysDispatcher->EnqueueCommand([InProxy](Chaos::FPersistentPhysicsTask* PhysThread)
		{
			PhysThread->AddProxy(InProxy);
		});

		// Pass the callbacks off to the physics thread
		PhysDispatcher->EnqueueCommand(GetSolver(), [CreatedCallbacks](Chaos::PBDRigidsSolver* InSolver)
		{
			InSolver->RegisterCallbacks(CreatedCallbacks);
		});
	}
	else
	{
		SolverStorage->ActiveProxies.Add(InProxy);
		GetSolver()->RegisterCallbacks(InProxy->GetCallbacks());
	}
}


void FPhysScene_Chaos::AddFieldProxy(Chaos::FPhysicsProxy* InProxy)
{
	check(IsInGameThread());
	const bool bDedicatedThread = ChaosModule->IsPersistentTaskEnabled();

	InProxy->SetSolver(GetSolver());

	Chaos::IDispatcher* PhysDispatcher = GetDispatcher();

	if (bDedicatedThread && PhysDispatcher)
	{
		// Ensure that if we need to create the callbacks it's done on the main thread so UObjects etc. can be queried
		FSolverFieldCallbacks* CreatedCallbacks = static_cast<FSolverFieldCallbacks*>(InProxy->GetCallbacks());

		// Pass the proxy off to the physics thread
		PhysDispatcher->EnqueueCommand([InProxy, CreatedCallbacks, CurrSolver = GetSolver()](Chaos::FPersistentPhysicsTask* PhysThread)
		{
			PhysThread->AddFieldProxy(InProxy);
			CurrSolver->RegisterFieldCallbacks(CreatedCallbacks);
		});
	}
	else
	{
		SolverStorage->ActiveProxies.Add(InProxy);
		GetSolver()->RegisterFieldCallbacks(static_cast<FSolverFieldCallbacks*>(InProxy->GetCallbacks()));
	}
}

void FPhysScene_Chaos::RemoveProxy(Chaos::FPhysicsProxy* InProxy)
{
	check(IsInGameThread());
	const bool bDedicatedThread = ChaosModule->IsPersistentTaskEnabled();

	Chaos::IDispatcher* PhysDispatcher = GetDispatcher();

	if(PhysDispatcher && GetSolver())
	{
		PhysDispatcher->EnqueueCommand([InProxy, InSolver = GetSolver()](Chaos::FPersistentPhysicsTask* PhysThread)
		{
			// If we're multithreaded, remove from the thread proxy list
			if(PhysThread)
			{
				PhysThread->RemoveProxy(InProxy);
			}

			// Cleanup
			InProxy->OnRemoveFromScene();
			InSolver->UnregisterCallbacks(InProxy->GetCallbacks());
			InProxy->DestroyCallbacks();
		});

		// #BG TODO better storage for proxies so this can be done all in one command
		if(!bDedicatedThread)
		{
			// Finish up before destroying
			InProxy->SyncBeforeDestroy();
			SolverStorage->ActiveProxies.Remove(InProxy);
			delete InProxy;
		}
	}
}



void FPhysScene_Chaos::RemoveFieldProxy(Chaos::FPhysicsProxy* InProxy)
{
	check(IsInGameThread());
	const bool bDedicatedThread = ChaosModule->IsPersistentTaskEnabled();

	Chaos::IDispatcher* PhysDispatcher = GetDispatcher();

	if (PhysDispatcher && GetSolver())
	{
		PhysDispatcher->EnqueueCommand([InProxy, InSolver = GetSolver()](Chaos::FPersistentPhysicsTask* PhysThread)
		{
			// If we're multithreaded, remove from the thread proxy list
			if (PhysThread)
			{
				PhysThread->RemoveFieldProxy(InProxy);
			}

			// Cleanup
			InProxy->OnRemoveFromScene();
			InSolver->UnregisterFieldCallbacks(static_cast<FSolverFieldCallbacks*>(InProxy->GetCallbacks()));
			InProxy->DestroyCallbacks();
		});

		// #BG TODO better storage for proxies so this can be done all in one command
		if (!bDedicatedThread)
		{
			// Finish up before destroying
			InProxy->SyncBeforeDestroy();
			SolverStorage->ActiveProxies.Remove(InProxy);
			delete InProxy;
		}
	}
}

void FPhysScene_Chaos::Shutdown()
{
	if(ChaosModule)
	{
		// Destroy our solver
		Chaos::IDispatcher* CurrDispatcher = ChaosModule->GetDispatcher();
		if(CurrDispatcher)
		{
			CurrDispatcher->EnqueueCommand([InnerSolver = SolverStorage, OwnerModule = ChaosModule](Chaos::FPersistentPhysicsTask* PhysThread)
			{
				if(PhysThread)
				{
					PhysThread->RemoveSolver(InnerSolver);
				}

				OwnerModule->DestroySolverState(InnerSolver);
			});
		}
	}

	ChaosModule = nullptr;
	SolverStorage = nullptr;
}

void FPhysScene_Chaos::AddReferencedObjects(FReferenceCollector& Collector)
{
#if WITH_EDITOR
	for(UObject* Obj : PieModifiedObjects)
	{
		Collector.AddReferencedObject(Obj);
	}
#endif
}

#if WITH_EDITOR
void FPhysScene_Chaos::OnWorldEndPlay()
{
	// Mark PIE modified objects dirty - couldn't do this during the run because
	// it's silently ignored
	for(UObject* Obj : PieModifiedObjects)
	{
		Obj->Modify();
	}

	PieModifiedObjects.Reset();
}

void FPhysScene_Chaos::AddPieModifiedObject(UObject* InObj)
{
	if(GIsPlayInEditorWorld)
	{
		PieModifiedObjects.AddUnique(InObj);
	}
}
#endif

#endif
