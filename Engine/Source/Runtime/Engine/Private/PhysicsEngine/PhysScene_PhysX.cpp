// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#if !WITH_APEIRON && !WITH_IMMEDIATE_PHYSX && !PHYSICS_INTERFACE_LLIMMEDIATE

#include "Physics/PhysScene_PhysX.h"
#include "Misc/CommandLine.h"
#include "Stats/Stats.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "HAL/IConsoleManager.h"
#include "Async/TaskGraphInterfaces.h"
#include "EngineDefines.h"
#include "Engine/EngineTypes.h"
#include "PhysxUserData.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/RigidBodyIndexPair.h"
#include "PhysicsPublic.h"
#include "CustomPhysXPayload.h"
#include "HAL/LowLevelMemTracker.h"

#if WITH_PHYSX
#include "PhysXPublic.h"
#include "PhysicsEngine/PhysXSupport.h"
#endif

#include "PhysicsEngine/PhysSubstepTasks.h"
#include "PhysicsEngine/PhysicsCollisionHandler.h"
#include "Physics/PhysicsInterfaceUtils.h"
#include "Components/LineBatchComponent.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsReplication.h"
#include "ProfilingDebugging/CsvProfiler.h"

/** Physics stats **/

DEFINE_STAT(STAT_TotalPhysicsTime);
DEFINE_STAT(STAT_NumCloths);
DEFINE_STAT(STAT_NumClothVerts);


CSV_DECLARE_CATEGORY_MODULE_EXTERN(CORE_API, Basic);

DECLARE_CYCLE_STAT(TEXT("Start Physics Time (sync)"), STAT_PhysicsKickOffDynamicsTime, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Fetch Results Time (sync)"), STAT_PhysicsFetchDynamicsTime, STATGROUP_Physics);

DECLARE_CYCLE_STAT(TEXT("Start Physics Time (async)"), STAT_PhysicsKickOffDynamicsTime_Async, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Fetch Results Time (async)"), STAT_PhysicsFetchDynamicsTime_Async, STATGROUP_Physics);

DECLARE_CYCLE_STAT(TEXT("Update Kinematics On Deferred SkelMeshes"), STAT_UpdateKinematicsOnDeferredSkelMeshes, STATGROUP_Physics);

DECLARE_CYCLE_STAT(TEXT("Phys Events Time"), STAT_PhysicsEventTime, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("SyncComponentsToBodies (sync)"), STAT_SyncComponentsToBodies, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("SyncComponentsToBodies (async)"), STAT_SyncComponentsToBodies_Async, STATGROUP_Physics);

DECLARE_DWORD_COUNTER_STAT(TEXT("Broadphase Adds"), STAT_NumBroadphaseAdds, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Broadphase Removes"), STAT_NumBroadphaseRemoves, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Active Constraints"), STAT_NumActiveConstraints, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Active Simulated Bodies"), STAT_NumActiveSimulatedBodies, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Active Kinematic Bodies"), STAT_NumActiveKinematicBodies, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Mobile Bodies"), STAT_NumMobileBodies, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Static Bodies"), STAT_NumStaticBodies, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Shapes"), STAT_NumShapes, STATGROUP_Physics);

DECLARE_DWORD_COUNTER_STAT(TEXT("(ASync) Broadphase Adds"), STAT_NumBroadphaseAddsAsync, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("(ASync) Broadphase Removes"), STAT_NumBroadphaseRemovesAsync, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("(ASync) Active Constraints"), STAT_NumActiveConstraintsAsync, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("(ASync) Active Simulated Bodies"), STAT_NumActiveSimulatedBodiesAsync, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("(ASync) Active Kinematic Bodies"), STAT_NumActiveKinematicBodiesAsync, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("(ASync) Mobile Bodies"), STAT_NumMobileBodiesAsync, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("(ASync) Static Bodies"), STAT_NumStaticBodiesAsync, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("(ASync) Shapes"), STAT_NumShapesAsync, STATGROUP_Physics);

EPhysicsSceneType FPhysScene_PhysX::SceneType_AssumesLocked(const FBodyInstance* BodyInstance) const
{
#if WITH_PHYSX
	//This is a helper function for dynamic actors - static actors are in both scenes
	return HasAsyncScene() && BodyInstance->bUseAsyncScene ? PST_Async : PST_Sync;
#endif

	return PST_Sync;
}

/**
* Return true if we should be running in single threaded mode, ala dedicated server
**/

/**
* Return true if we should lag the async scene a frame
**/
FORCEINLINE static bool FrameLagAsync()
{
	if (IsRunningDedicatedServer())
	{
		return false;
	}
	return true;
}

#if WITH_PHYSX

FAutoConsoleTaskPriority CPrio_FPhysXTask(
	TEXT("TaskGraph.TaskPriorities.PhysXTask"),
	TEXT("Task and thread priority for FPhysXTask."),
	ENamedThreads::HighThreadPriority, // if we have high priority task threads, then use them...
	ENamedThreads::NormalTaskPriority, // .. at normal task priority
	ENamedThreads::HighTaskPriority // if we don't have hi pri threads, then use normal priority threads at high task priority instead
);


int32 GPhysXOverrideMbpNumSubdivisions_Client = 0;
int32 GPhysXOverrideMbpNumSubdivisions_Server = 0;
int32 GPhysXForceMbp_Client = 0;
int32 GPhysXForceMbp_Server = 0;
int32 GPhysXForceNoKinematicStaticPairs = 0;
int32 GPhysXForceNoKinematicKinematicPairs = 0;

FAutoConsoleVariableRef CVarOverrideMbpNumSubdivisionsClient(TEXT("p.OverrideMbpNumSubdivisionsClient"), GPhysXOverrideMbpNumSubdivisions_Client, TEXT("Override for number of subdivisions to perform when building MBP regions on a client, note regions are only generated when a scene is created - this will not update the scene if it's already running (0 = No override, 1>16 - Override number)"), ECVF_Default);
FAutoConsoleVariableRef CVarOverrideMbpNumSubdivisionsServer(TEXT("p.OverrideMbpNumSubdivisionsServer"), GPhysXOverrideMbpNumSubdivisions_Server, TEXT("Override for number of subdivisions to perform when building MBP regions on a server, note regions are only generated when a scene is created - this will not update the scene if it's already running (0 = No override, 1>16 - Override number)"), ECVF_Default);
FAutoConsoleVariableRef CVarForceMbpClient(TEXT("p.ForceMbpClient"), GPhysXForceMbp_Client, TEXT("Forces all created scenes to use MBP on client builds"), ECVF_Default);
FAutoConsoleVariableRef CVarForceMbpServer(TEXT("p.ForceMbpServer"), GPhysXForceMbp_Server, TEXT("Forces all created scenes to use MBP on server builds"), ECVF_Default);
FAutoConsoleVariableRef CVarForceNoKSPairs(TEXT("p.ForceNoKSPairs"), GPhysXForceNoKinematicStaticPairs, TEXT("Disables kinematic-static pairs. This makes converting from static to dynamic a little slower - but provides better broadphase performance because we early reject those pairs."), ECVF_Default);
FAutoConsoleVariableRef CVarForceNoKKPairs(TEXT("p.ForceNoKKPairs"), GPhysXForceNoKinematicKinematicPairs, TEXT("Disables kinematic-kinematic pairs. This is required when using APEX destruction to correctly generate chunk pairs - when not using destruction this speeds up the broadphase by early rejecting KK pairs."), ECVF_Default);


DECLARE_STATS_GROUP(TEXT("PhysXTasks"), STATGROUP_PhysXTasks, STATCAT_Advanced);

struct FPhysXRingBuffer
{
	static const int32 Size = 16;

	PxBaseTask* Buffer[Size];
	int32 Start;
	int32 End;
	int32 Num;
};

int32 GBatchPhysXTasksSize = 3;	//NOTE: FPhysXRingBuffer::Size should be twice as big as this value
TAutoConsoleVariable<int32> CVarBatchPhysXTasksSize(TEXT("p.BatchPhysXTasksSize"), GBatchPhysXTasksSize, TEXT("Number of tasks to batch together (max 8). 1 will go as wide as possible, but more overhead on small tasks"), ECVF_Default);

struct FBatchPhysXTasks
{
	static void SetPhysXTasksSinkFunc()
	{
		GBatchPhysXTasksSize = FMath::Max(1, FMath::Min(FPhysXRingBuffer::Size / 2, CVarBatchPhysXTasksSize.GetValueOnGameThread()));
	}
};

struct FPhysTaskScopedNamedEvent
{
	FPhysTaskScopedNamedEvent() = delete;
	FPhysTaskScopedNamedEvent(const FPhysTaskScopedNamedEvent& InOther) = delete;
	FPhysTaskScopedNamedEvent& operator=(const FPhysTaskScopedNamedEvent& InOther) = delete;

	FPhysTaskScopedNamedEvent(PxBaseTask* InTask)
	{
#if ENABLE_STATNAMEDEVENTS
		check(InTask);
		const char* TaskName = InTask->getName();

		bEmittedEvent = GCycleStatsShouldEmitNamedEvents != 0;

		if(bEmittedEvent)
		{
			FPlatformMisc::BeginNamedEvent(FColor::Green, TaskName);
		}
#endif
	}

	~FPhysTaskScopedNamedEvent()
	{
#if ENABLE_STATNAMEDEVENTS
		if(bEmittedEvent)
		{
			FPlatformMisc::EndNamedEvent();
		}
#endif
	}

private:

	bool bEmittedEvent;
};

static FAutoConsoleVariableSink CVarBatchPhysXTasks(FConsoleCommandDelegate::CreateStatic(&FBatchPhysXTasks::SetPhysXTasksSinkFunc));

namespace DynamicStatsHelper
{
	struct FStatLookup
	{
		const char* StatName;
		TStatId Stat;
	} Stats[100];

	int NumStats = 0;
	FCriticalSection CS;

	TStatId FindOrCreateStatId(const char* StatName)
	{
#if STATS
		for (int StatIdx = 0; StatIdx < NumStats; ++StatIdx)
		{
			FStatLookup& Lookup = Stats[StatIdx];
			if (Lookup.StatName == StatName)
			{
				return Lookup.Stat;
			}
		}

		if (ensureMsgf(NumStats < sizeof(Stats) / sizeof(Stats[0]), TEXT("Too many different physx task stats. This will make the stat search slow")))
		{
			FScopeLock ScopeLock(&CS);

			//Do the search again in case another thread added
			for (int StatIdx = 0; StatIdx < NumStats; ++StatIdx)
			{
				FStatLookup& Lookup = Stats[StatIdx];
				if (Lookup.StatName == StatName)
				{
					return Lookup.Stat;
				}
			}

			FStatLookup& NewStat = Stats[NumStats];
			NewStat.StatName = StatName;
			NewStat.Stat = FDynamicStats::CreateStatId<FStatGroup_STATGROUP_PhysXTasks>(FName(StatName));
			FPlatformMisc::MemoryBarrier();
			++NumStats;	//make sure to do this at the end in case another thread is currently iterating
			return NewStat.Stat;
		}
#endif // STATS
		return TStatId();
	}
}

struct FPhysXCPUDispatcher;

class FPhysXTask
{
public:
	FPhysXTask(PxBaseTask& InTask, FPhysXCPUDispatcher& InDispatcher);
	FPhysXTask(FPhysXRingBuffer& RingBuffer, FPhysXCPUDispatcher& InDispatcher);
	~FPhysXTask();

	static FORCEINLINE TStatId GetStatId()
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FPhysXTask, STATGROUP_Physics);
	}
	static FORCEINLINE ENamedThreads::Type GetDesiredThread()
	{
		return CPrio_FPhysXTask.Get();
	}
	static FORCEINLINE ESubsequentsMode::Type GetSubsequentsMode()
	{
		return ESubsequentsMode::TrackSubsequents;
	}

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);

	FPhysXRingBuffer RingBuffer;
	FPhysXCPUDispatcher& Dispatcher;
};

/** Used to dispatch physx tasks to task graph */
struct FPhysXCPUDispatcher : public PxCpuDispatcher
{
	FPhysXCPUDispatcher()
	{
		check(IsInGameThread());
		TLSKey = FPlatformTLS::AllocTlsSlot();
	}

	~FPhysXCPUDispatcher()
	{
		check(IsInGameThread());
		FPlatformTLS::FreeTlsSlot(TLSKey);
	}

	virtual void submitTask(PxBaseTask& Task) override
	{
		if (IsInGameThread())
		{
			//Game thread enqueues on task graph
			TGraphTask<FPhysXTask>::CreateTask(NULL).ConstructAndDispatchWhenReady(Task, *this);
		}
		else
		{
			//See if we can use local queue
			FPhysXRingBuffer& RingBuffer = *(FPhysXRingBuffer*)FPlatformTLS::GetTlsValue(TLSKey);
			RingBuffer.Buffer[RingBuffer.End] = &Task;
			RingBuffer.End = (RingBuffer.End + 1) % FPhysXRingBuffer::Size;
			RingBuffer.Num++;

			if (RingBuffer.Num >= GBatchPhysXTasksSize * 2)
			{
				TGraphTask<FPhysXTask>::CreateTask(NULL).ConstructAndDispatchWhenReady(RingBuffer, *this);
			}
		}
	}

	virtual PxU32 getWorkerCount() const override
	{
		return FTaskGraphInterface::Get().GetNumWorkerThreads();
	}

	uint32 TLSKey;
};

FPhysXTask::FPhysXTask(PxBaseTask& Task, FPhysXCPUDispatcher& InDispatcher)
	: Dispatcher(InDispatcher)
{
	RingBuffer.Buffer[0] = &Task;
	RingBuffer.Start = 0;
	RingBuffer.End = 1;
	RingBuffer.Num = 1;
}

FPhysXTask::FPhysXTask(FPhysXRingBuffer& InRingBuffer, FPhysXCPUDispatcher& InDispatcher)
	: Dispatcher(InDispatcher)
{
	int32 NumToSteal = InRingBuffer.Num / 2;
	ensureMsgf(NumToSteal > 0, TEXT("Trying to steal 0 items"));

	const int32 StartPos = (InRingBuffer.Start + NumToSteal);
	for (int32 Count = 0; Count < NumToSteal; ++Count)
	{
		RingBuffer.Buffer[Count] = InRingBuffer.Buffer[(StartPos + Count) % FPhysXRingBuffer::Size];
	}

	RingBuffer.Start = 0;
	RingBuffer.End = NumToSteal;
	RingBuffer.Num = NumToSteal;


	InRingBuffer.Num -= NumToSteal;
	InRingBuffer.End = (StartPos) % FPhysXRingBuffer::Size;
}

FPhysXTask::~FPhysXTask()
{
	FPlatformTLS::SetTlsValue(Dispatcher.TLSKey, nullptr);
}

void FPhysXTask::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	FPlatformTLS::SetTlsValue(Dispatcher.TLSKey, &RingBuffer);

	while (RingBuffer.Num)
	{
		PxBaseTask* Task = RingBuffer.Buffer[RingBuffer.Start];

#if ENABLE_STATNAMEDEVENTS || STATS
		FPhysTaskScopedNamedEvent TaskEvent(Task);
#endif

#if STATS
		const char* TaskName = Task->getName();
		FScopeCycleCounter CycleCounter(DynamicStatsHelper::FindOrCreateStatId(TaskName));
#endif

		Task->run();
		Task->release();

		RingBuffer.Start = (RingBuffer.Start + 1) % RingBuffer.Size;
		--RingBuffer.Num;
	}
}


DECLARE_CYCLE_STAT(TEXT("PhysX Single Thread Task"), STAT_PhysXSingleThread, STATGROUP_Physics);

/** Used to dispatch physx tasks to the game thread */
class FPhysXCPUDispatcherSingleThread : public PxCpuDispatcher
{
	TArray<PxBaseTask*> TaskStack;

	virtual void submitTask(PxBaseTask& Task) override
	{
		SCOPE_CYCLE_COUNTER(STAT_PhysXSingleThread);

		TaskStack.Push(&Task);
		if (TaskStack.Num() > 1)
		{
			return;
		}

		{
#if ENABLE_STATNAMEDEVENTS || STATS
			FPhysTaskScopedNamedEvent TaskEvent(&Task);
#endif

#if STATS
			const char* TaskName = Task.getName();
			FScopeCycleCounter CycleCounter(DynamicStatsHelper::FindOrCreateStatId(TaskName));
#endif

			Task.run();
			Task.release();
		}

		while (TaskStack.Num() > 1)
		{
			PxBaseTask& ChildTask = *TaskStack.Pop();
			{

#if ENABLE_STATNAMEDEVENTS || STATS
				FPhysTaskScopedNamedEvent TaskEvent(&ChildTask);
#endif

#if STATS
				const char* ChildTaskName = ChildTask.getName();
				FScopeCycleCounter CycleCounter(DynamicStatsHelper::FindOrCreateStatId(ChildTaskName));
#endif
				ChildTask.run();
				ChildTask.release();
			}
		}
		verify(&Task == TaskStack.Pop() && !TaskStack.Num());
	}

	virtual PxU32 getWorkerCount() const override
	{
		return 1;
	}
};

TSharedPtr<ISimEventCallbackFactory> FPhysScene_PhysX::SimEventCallbackFactory;
TSharedPtr<IContactModifyCallbackFactory> FPhysScene_PhysX::ContactModifyCallbackFactory;
TSharedPtr<ICCDContactModifyCallbackFactory> FPhysScene_PhysX::CCDContactModifyCallbackFactory;

#endif // WITH_PHYSX

TSharedPtr<IPhysicsReplicationFactory> FPhysScene_PhysX::PhysicsReplicationFactory;

static void StaticSetPhysXTreeRebuildRate(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() > 0)
	{
		const int32 NewRate = FCString::Atoi(*Args[0]);
		if(World && World->GetPhysicsScene())
		{
			World->GetPhysicsScene()->SetPhysXTreeRebuildRate(NewRate);
		}
	}
	else
	{
		UE_LOG(LogPhysics, Warning, TEXT("Usage: p.PhysXTreeRebuildRate <num_frames>"));
	}
}

static FAutoConsoleCommandWithWorldAndArgs GSetPhysXTreeRebuildRate(TEXT("p.PhysXTreeRebuildRate"), TEXT("Utility function to change PhysXTreeRebuildRate, useful when profiling fetchResults vs scene queries."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StaticSetPhysXTreeRebuildRate)
);


/** Exposes creation of physics-engine scene outside Engine (for use with Physics Asset Editor for example). */
FPhysScene_PhysX::FPhysScene_PhysX(const AWorldSettings* Settings)
{
	LineBatcher = NULL;
	OwningWorld = NULL;
#if WITH_PHYSX
	PhysxUserData = FPhysxUserData(this);
#endif	//#if WITH_PHYSX

	UPhysicsSettings * PhysSetting = UPhysicsSettings::Get();
	FMemory::Memzero(FrameTimeSmoothingFactor);
	FrameTimeSmoothingFactor[PST_Sync] = PhysSetting->SyncSceneSmoothingFactor;
	FrameTimeSmoothingFactor[PST_Async] = PhysSetting->AsyncSceneSmoothingFactor;

	bSubstepping = PhysSetting->bSubstepping;
	bSubsteppingAsync = PhysSetting->bSubsteppingAsync;
	bAsyncSceneEnabled = PhysSetting->bEnableAsyncScene;
	NumPhysScenes = bAsyncSceneEnabled ? PST_Async + 1 : PST_Sync + 1;

	PhysXTreeRebuildRate = PhysSetting->PhysXTreeRebuildRate;

	// Create scenes of all scene types
	for (uint32 SceneType = 0; SceneType < NumPhysScenes; ++SceneType)
	{
		// Create the physics scene
		InitPhysScene(SceneType, Settings);

		// Also initialize scene data
		bPhysXSceneExecuting[SceneType] = false;

		// Initialize to a value which would be acceptable if FrameTimeSmoothingFactor[i] = 1.0f, i.e. constant simulation substeps
		AveragedFrameTime[SceneType] = PhysSetting->InitialAverageFrameRate;

		// gets from console variable, and clamp to [0, 1] - 1 should be fixed time as 30 fps
		FrameTimeSmoothingFactor[SceneType] = FMath::Clamp<float>(FrameTimeSmoothingFactor[SceneType], 0.f, 1.f);
	}

	// Create replication manager
	PhysicsReplication = PhysicsReplicationFactory.IsValid() ? PhysicsReplicationFactory->Create(this) : new FPhysicsReplication(this);

	if (!bAsyncSceneEnabled)
	{
		PhysXScenes[PST_Async] = nullptr;
	}

	PreGarbageCollectDelegateHandle = FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddRaw(this, &FPhysScene_PhysX::WaitPhysScenes);

#if WITH_PHYSX
	// Initialise PhysX scratch buffers (only if size > 0)
	int32 SceneScratchBufferSize = PhysSetting->SimulateScratchMemorySize;
	if(SceneScratchBufferSize > 0)
	{
		// Make sure that SceneScratchBufferSize is a multiple of 16K as requested by PhysX.
		SceneScratchBufferSize = FMath::DivideAndRoundUp<int32>(SceneScratchBufferSize, SimScratchBufferBoundary) * SimScratchBufferBoundary;

		for(uint32 SceneType = 0; SceneType < PST_MAX; ++SceneType)
		{
			if(SceneType < NumPhysScenes)
			{
				PxScene* Scene = GetPxScene(SceneType);
				if(Scene)
				{
					// We have a valid scene, so allocate the buffer for it
					SimScratchBuffers[SceneType].Buffer = (uint8*)FMemory::Malloc(SceneScratchBufferSize, 16);
					SimScratchBuffers[SceneType].BufferSize = SceneScratchBufferSize;
				}
			}
		}
	}
#endif
}

////helper function for TermBody to avoid code duplication between scenes
//void TermBodyHelper(FPhysScene_PhysX* PhysScene, PxRigidActor*& PRigidActor, int32 SceneType)
//{
//	if (PRigidActor)
//	{
//		// #Phys2 fixed hitting the check below because body scene was null, check was invalid in this case.
//		PxScene* PScene = PhysScene->GetPxScene(SceneType);
//		PxScene* BodyPScene = PRigidActor->getScene();
//		if (PScene && BodyPScene)
//		{
//			checkSlow(PhysScene->GetPxScene(SceneType) == PRigidActor->getScene());
//
//			// Enable scene lock
//			SCOPED_SCENE_WRITE_LOCK(PScene);
//
//			// Let FPhysScene know
//			FBodyInstance* BodyInst = FPhysxUserData::Get<FBodyInstance>(PRigidActor->userData);
//			if (BodyInst)
//			{
//				PhysScene->RemoveBodyInstanceFromPendingLists_AssumesLocked(BodyInst, SceneType);
//			}
//
//			PRigidActor->release();
//			//we must do this within the lock because we use it in the sub-stepping thread to determine that RigidActor is still valid
//			PRigidActor = nullptr;
//		}
//		else
//		{
//			PRigidActor->release();
//			PRigidActor = nullptr;
//		}
//	}
//
//	checkSlow(PRigidActor == NULL);
//}
//
//void FPhysScene_PhysX::ReleaseActor(FPhysicsActorReference& InActor)
//{
//	TermBodyHelper(this, InActor.SyncActor, PST_Sync);
//	TermBodyHelper(this, InActor.AsyncActor, PST_Async);
//}


void FPhysScene_PhysX::AddActorsToPhysXScene_AssumesLocked(int32 SceneType, const TArray<FPhysicsActorHandle>& InActors)
{
	// Check we have a sync scene
	PxScene* PScene = GetPxScene(SceneType);
	if (PScene != nullptr)
	{
		// If not simulating at the moment, can use batch add
		if (!bIsSceneSimulating[SceneType])
		{
			TArray<PxActor*> PActors;
			for (const FPhysicsActorHandle& ActorRef : InActors)
			{
				if (SceneType == PST_Sync && ActorRef.SyncActor != nullptr)
				{
					PActors.Add(ActorRef.SyncActor);
				}
				else if (SceneType == PST_Async && ActorRef.AsyncActor != nullptr)
				{
					PActors.Add(ActorRef.AsyncActor);
				}
			}

			PScene->addActors(PActors.GetData(), PActors.Num());
		}
		// If we are simulating, add one at a time
		else
		{
			for (const FPhysicsActorHandle& ActorRef : InActors)
			{
				if (SceneType == PST_Sync && ActorRef.SyncActor != nullptr)
				{
					PScene->addActor(*ActorRef.SyncActor);
				}
				else if (SceneType == PST_Async && ActorRef.AsyncActor != nullptr)
				{
					PScene->addActor(*ActorRef.AsyncActor);
				}
			}
		}
	}
}

void FPhysScene_PhysX::AddActorsToScene_AssumesLocked(const TArray<FPhysicsActorHandle>& InActors)
{
	AddActorsToPhysXScene_AssumesLocked(PST_Sync, InActors);
	AddActorsToPhysXScene_AssumesLocked(PST_Async, InActors);
}

void FPhysScene_PhysX::AddAggregateToScene(const FPhysicsAggregateHandle& InAggregate, bool bUseAsyncScene)
{
	PxScene* PScene = GetPxScene(bUseAsyncScene ? PST_Async : PST_Sync);
	if (PScene)
	{
		SCOPED_SCENE_WRITE_LOCK(PScene);
		// add Aggregate into the scene
		if (InAggregate.IsValid() && InAggregate.Aggregate->getNbActors() > 0)
		{
			PScene->addAggregate(*InAggregate.Aggregate);
		}
	}
}


void FPhysScene_PhysX::SetOwningWorld(UWorld* InOwningWorld)
{
	OwningWorld = InOwningWorld;
}

/** Exposes destruction of physics-engine scene outside Engine. */
FPhysScene_PhysX::~FPhysScene_PhysX()
{
	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().Remove(PreGarbageCollectDelegateHandle);
	// Make sure no scenes are left simulating (no-ops if not simulating)
	WaitPhysScenes();

	if (IPhysicsReplicationFactory* RawReplicationFactory = PhysicsReplicationFactory.Get())
	{
		RawReplicationFactory->Destroy(PhysicsReplication);
	}
	else
	{
		delete PhysicsReplication;
	}

	// Loop through scene types to get all scenes
	for (uint32 SceneType = 0; SceneType < NumPhysScenes; ++SceneType)
	{

		// Destroy the physics scene
		TermPhysScene(SceneType);

#if WITH_PHYSX
		GPhysCommandHandler->DeferredDeleteCPUDispathcer(CPUDispatcher[SceneType]);
#endif	//#if WITH_PHYSX
	}

#if WITH_PHYSX
	// Free the scratch buffers
	for(uint32 SceneType = 0; SceneType < PST_MAX; ++SceneType)
	{
		if(SimScratchBuffers[SceneType].Buffer != nullptr)
		{
			FMemory::Free(SimScratchBuffers[SceneType].Buffer);
			SimScratchBuffers[SceneType].Buffer = nullptr;
			SimScratchBuffers[SceneType].BufferSize = 0;
		}
	}
#endif
}

namespace
{

	bool UseSyncTime(uint32 SceneType)
	{
		return (FrameLagAsync() && SceneType == PST_Async);
	}

}

bool FPhysScene_PhysX::GetKinematicTarget_AssumesLocked(const FBodyInstance* BodyInstance, FTransform& OutTM) const
{
#if WITH_PHYSX
	if (PxRigidDynamic * PRigidDynamic = FPhysicsInterface_PhysX::GetPxRigidDynamic_AssumesLocked(BodyInstance->GetPhysicsActorHandle()))
	{
		uint32 BodySceneType = SceneType_AssumesLocked(BodyInstance);
		if (IsSubstepping(BodySceneType))
		{
			FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[BodySceneType];
			return PhysSubStepper->GetKinematicTarget_AssumesLocked(BodyInstance, OutTM);
		}
		else
		{
			PxTransform POutTM;
			bool validTM = PRigidDynamic->getKinematicTarget(POutTM);
			if (validTM)
			{
				OutTM = P2UTransform(POutTM);
				return true;
			}
		}
	}
#endif

	return false;
}

void FPhysScene_PhysX::SetKinematicTarget_AssumesLocked(FBodyInstance* BodyInstance, const FTransform& TargetTransform, bool bAllowSubstepping)
{
	TargetTransform.DiagnosticCheck_IsValid();

#if WITH_PHYSX
	if (PxRigidDynamic * PRigidDynamic = FPhysicsInterface_PhysX::GetPxRigidDynamic_AssumesLocked(BodyInstance->GetPhysicsActorHandle()))
	{
		const bool bIsKinematicTarget = IsRigidBodyKinematicAndInSimulationScene_AssumesLocked(PRigidDynamic);
		if(bIsKinematicTarget)
		{
			uint32 BodySceneType = SceneType_AssumesLocked(BodyInstance);
			if (bAllowSubstepping && IsSubstepping(BodySceneType))
			{
				FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[BodySceneType];
				PhysSubStepper->SetKinematicTarget_AssumesLocked(BodyInstance, TargetTransform);
			}

			const PxTransform PNewPose = U2PTransform(TargetTransform);
			PRigidDynamic->setKinematicTarget(PNewPose);	//If we interpolate, we will end up setting the kinematic target once per sub-step. However, for the sake of scene queries we should do this right away
		}
		else
		{
			const PxTransform PNewPose = U2PTransform(TargetTransform);
			PRigidDynamic->setGlobalPose(PNewPose);
		}
	}
#endif
}

void FPhysScene_PhysX::AddCustomPhysics_AssumesLocked(FBodyInstance* BodyInstance, FCalculateCustomPhysics& CalculateCustomPhysics)
{
#if WITH_PHYSX
	uint32 BodySceneType = SceneType_AssumesLocked(BodyInstance);
	if (IsSubstepping(BodySceneType))
	{
		FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[SceneType_AssumesLocked(BodyInstance)];
		PhysSubStepper->AddCustomPhysics_AssumesLocked(BodyInstance, CalculateCustomPhysics);
	}
	else
	{
		// Since physics frame is set up before "pre-physics" tick group is called, can just fetch delta time from there
		CalculateCustomPhysics.ExecuteIfBound(this->DeltaSeconds, BodyInstance);
	}
#endif
}

void FPhysScene_PhysX::AddForce_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Force, bool bAllowSubstepping, bool bAccelChange)
{
#if WITH_PHYSX

	if (PxRigidBody * PRigidBody = FPhysicsInterface_PhysX::GetPxRigidBody_AssumesLocked(BodyInstance->GetPhysicsActorHandle()))
	{
		uint32 BodySceneType = SceneType_AssumesLocked(BodyInstance);
		if (bAllowSubstepping && IsSubstepping(BodySceneType))
		{
			FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[BodySceneType];
			PhysSubStepper->AddForce_AssumesLocked(BodyInstance, Force, bAccelChange);
		}
		else
		{
			PRigidBody->addForce(U2PVector(Force), bAccelChange ? PxForceMode::eACCELERATION : PxForceMode::eFORCE, true);
		}
	}
#endif
}

void FPhysScene_PhysX::AddForceAtPosition_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Force, const FVector& Position, bool bAllowSubstepping, bool bIsLocalForce)
{
#if WITH_PHYSX

	if (PxRigidBody * PRigidBody = FPhysicsInterface_PhysX::GetPxRigidBody_AssumesLocked(BodyInstance->GetPhysicsActorHandle()))
	{
		uint32 BodySceneType = SceneType_AssumesLocked(BodyInstance);
		if (bAllowSubstepping && IsSubstepping(BodySceneType))
		{
			FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[BodySceneType];
			PhysSubStepper->AddForceAtPosition_AssumesLocked(BodyInstance, Force, Position, bIsLocalForce);
		}
		else if (!bIsLocalForce)
		{
			PxRigidBodyExt::addForceAtPos(*PRigidBody, U2PVector(Force), U2PVector(Position), PxForceMode::eFORCE, true);
		}
		else
		{
			PxRigidBodyExt::addLocalForceAtLocalPos(*PRigidBody, U2PVector(Force), U2PVector(Position), PxForceMode::eFORCE, true);
		}
	}
#endif
}

void FPhysScene_PhysX::AddRadialForceToBody_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Origin, const float Radius, const float Strength, const uint8 Falloff, bool bAccelChange, bool bAllowSubstepping)
{
#if WITH_PHYSX

	if (PxRigidBody * PRigidBody = FPhysicsInterface_PhysX::GetPxRigidBody_AssumesLocked(BodyInstance->GetPhysicsActorHandle()))
	{
		uint32 BodySceneType = SceneType_AssumesLocked(BodyInstance);
		if (bAllowSubstepping && IsSubstepping(BodySceneType))
		{
			FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[BodySceneType];
			PhysSubStepper->AddRadialForceToBody_AssumesLocked(BodyInstance, Origin, Radius, Strength, Falloff, bAccelChange);
		}
		else
		{
			AddRadialForceToPxRigidBody_AssumesLocked(*PRigidBody, Origin, Radius, Strength, Falloff, bAccelChange);
		}
	}
#endif
}

void FPhysScene_PhysX::ClearForces_AssumesLocked(FBodyInstance* BodyInstance, bool bAllowSubstepping)
{
#if WITH_PHYSX
	if (PxRigidBody * PRigidBody = FPhysicsInterface_PhysX::GetPxRigidBody_AssumesLocked(BodyInstance->GetPhysicsActorHandle()))
	{
		PRigidBody->clearForce();
		uint32 BodySceneType = SceneType_AssumesLocked(BodyInstance);
		if (bAllowSubstepping && IsSubstepping(BodySceneType))
		{
			FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[BodySceneType];
			PhysSubStepper->ClearForces_AssumesLocked(BodyInstance);
		}
	}
#endif
}


void FPhysScene_PhysX::AddTorque_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Torque, bool bAllowSubstepping, bool bAccelChange)
{
#if WITH_PHYSX

	if (PxRigidBody * PRigidBody = FPhysicsInterface_PhysX::GetPxRigidBody_AssumesLocked(BodyInstance->GetPhysicsActorHandle()))
	{
		uint32 BodySceneType = SceneType_AssumesLocked(BodyInstance);
		if (bAllowSubstepping && IsSubstepping(BodySceneType))
		{
			FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[BodySceneType];
			PhysSubStepper->AddTorque_AssumesLocked(BodyInstance, Torque, bAccelChange);
		}
		else
		{
			PRigidBody->addTorque(U2PVector(Torque), bAccelChange ? PxForceMode::eACCELERATION : PxForceMode::eFORCE, true);
		}
	}
#endif
}

void FPhysScene_PhysX::ClearTorques_AssumesLocked(FBodyInstance* BodyInstance, bool bAllowSubstepping)
{
#if WITH_PHYSX
	if (PxRigidBody * PRigidBody = FPhysicsInterface_PhysX::GetPxRigidBody_AssumesLocked(BodyInstance->GetPhysicsActorHandle()))
	{
		PRigidBody->clearTorque();
		uint32 BodySceneType = SceneType_AssumesLocked(BodyInstance);
		if (bAllowSubstepping && IsSubstepping(BodySceneType))
{
			FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[BodySceneType];
			PhysSubStepper->ClearTorques_AssumesLocked(BodyInstance);
		}
}
#endif
}

void FPhysScene_PhysX::RemoveBodyInstanceFromPendingLists_AssumesLocked(FBodyInstance* BodyInstance, int32 SceneType)
{
#if WITH_PHYSX
	if (FPhysicsInterface_PhysX::IsRigidBody(BodyInstance->GetPhysicsActorHandle()))
	{
		FPhysSubstepTask* PhysSubStepper = PhysSubSteppers[SceneType];
		PhysSubStepper->RemoveBodyInstance_AssumesLocked(BodyInstance);
	}

	PendingSleepEvents[SceneType].Remove(BodyInstance);
#endif // WITH_PHYSX
}

FAutoConsoleTaskPriority CPrio_PhysXStepSimulation(
	TEXT("TaskGraph.TaskPriorities.PhysXStepSimulation"),
	TEXT("Task and thread priority for FPhysSubstepTask::StepSimulation."),
	ENamedThreads::HighThreadPriority, // if we have high priority task threads, then use them...
	ENamedThreads::NormalTaskPriority, // .. at normal task priority
	ENamedThreads::HighTaskPriority // if we don't have hi pri threads, then use normal priority threads at high task priority instead
);

bool FPhysScene_PhysX::SubstepSimulation(uint32 SceneType, FGraphEventRef &InOutCompletionEvent)
{
#if WITH_PHYSX
	float UseDelta = UseSyncTime(SceneType)? SyncDeltaSeconds : DeltaSeconds;
	float SubTime = PhysSubSteppers[SceneType]->UpdateTime(UseDelta);
	PxScene* PScene = GetPxScene(SceneType);
	if(SubTime <= 0.f)
	{
		return false;
	}else
	{
		//we have valid scene and subtime so enqueue task
		PhysXCompletionTask* Task = new PhysXCompletionTask(InOutCompletionEvent, SceneType, PScene->getTaskManager(), &SimScratchBuffers[SceneType]);
		ENamedThreads::Type NamedThread = PhysSingleThreadedMode() ? ENamedThreads::GameThread : ENamedThreads::SetTaskPriority(ENamedThreads::GameThread, ENamedThreads::HighTaskPriority);

		DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.SubstepSimulationImp"),
		STAT_FSimpleDelegateGraphTask_SubstepSimulationImp,
			STATGROUP_TaskGraphTasks);

		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateRaw(PhysSubSteppers[SceneType], &FPhysSubstepTask::StepSimulation, Task),
			GET_STATID(STAT_FSimpleDelegateGraphTask_SubstepSimulationImp), NULL, NamedThread
		);
		return true;
	}
#else
	return false;
#endif

}


/** Adds to queue of skelmesh we want to add to collision disable table */
void FPhysScene_PhysX::DeferredAddCollisionDisableTable(uint32 SkelMeshCompID, TMap<struct FRigidBodyIndexPair, bool> * CollisionDisableTable)
{
	check(IsInGameThread());

	FPendingCollisionDisableTable PendingCollisionDisableTable;
	PendingCollisionDisableTable.SkelMeshCompID = SkelMeshCompID;
	PendingCollisionDisableTable.CollisionDisableTable = CollisionDisableTable;

	DeferredCollisionDisableTableQueue.Add(PendingCollisionDisableTable);
}

/** Adds to queue of skelmesh we want to remove from collision disable table */
void FPhysScene_PhysX::DeferredRemoveCollisionDisableTable(uint32 SkelMeshCompID)
{
	check(IsInGameThread());

	FPendingCollisionDisableTable PendingDisableCollisionTable;
	PendingDisableCollisionTable.SkelMeshCompID = SkelMeshCompID;
	PendingDisableCollisionTable.CollisionDisableTable = NULL;

	DeferredCollisionDisableTableQueue.Add(PendingDisableCollisionTable);
}

void FPhysScene_PhysX::FlushDeferredCollisionDisableTableQueue()
{
	check(IsInGameThread());
	for (int32 i = 0; i < DeferredCollisionDisableTableQueue.Num(); ++i)
	{
		FPendingCollisionDisableTable & PendingCollisionDisableTable = DeferredCollisionDisableTableQueue[i];

		if (PendingCollisionDisableTable.CollisionDisableTable)
		{
			CollisionDisableTableLookup.Add(PendingCollisionDisableTable.SkelMeshCompID, PendingCollisionDisableTable.CollisionDisableTable);
		}
		else
		{
			CollisionDisableTableLookup.Remove(PendingCollisionDisableTable.SkelMeshCompID);
		}
	}

	DeferredCollisionDisableTableQueue.Empty();
}

#if WITH_PHYSX

void GatherPhysXStats_AssumesLocked(PxScene* PSyncScene, PxScene* PAsyncScene)
{
	/** Gather PhysX stats */
	if (PSyncScene)
	{
		PxSimulationStatistics SimStats;
		PSyncScene->getSimulationStatistics(SimStats);

		SET_DWORD_STAT(STAT_NumActiveConstraints, SimStats.nbActiveConstraints);
		SET_DWORD_STAT(STAT_NumActiveSimulatedBodies, SimStats.nbActiveDynamicBodies);
		SET_DWORD_STAT(STAT_NumActiveKinematicBodies, SimStats.nbActiveKinematicBodies);
		SET_DWORD_STAT(STAT_NumStaticBodies, SimStats.nbStaticBodies);
		SET_DWORD_STAT(STAT_NumMobileBodies, SimStats.nbDynamicBodies);

		//SET_DWORD_STAT(STAT_NumBroadphaseAdds, SimStats.getNbBroadPhaseAdds(PxSimulationStatistics::VolumeType::eRIGID_BODY));	//TODO: These do not seem to work
		//SET_DWORD_STAT(STAT_NumBroadphaseRemoves, SimStats.getNbBroadPhaseRemoves(PxSimulationStatistics::VolumeType::eRIGID_BODY));

		uint32 NumShapes = 0;
		for (int32 GeomType = 0; GeomType < PxGeometryType::eGEOMETRY_COUNT; ++GeomType)
		{
			NumShapes += SimStats.nbShapes[GeomType];
		}

		SET_DWORD_STAT(STAT_NumShapes, NumShapes);

	}

	if(PAsyncScene)
	{
		//Having to duplicate because of macros. In theory we can fix this but need to get this quickly
		PxSimulationStatistics SimStats;
		PAsyncScene->getSimulationStatistics(SimStats);

		SET_DWORD_STAT(STAT_NumActiveConstraintsAsync, SimStats.nbActiveConstraints);
		SET_DWORD_STAT(STAT_NumActiveSimulatedBodiesAsync, SimStats.nbActiveDynamicBodies);
		SET_DWORD_STAT(STAT_NumActiveKinematicBodiesAsync, SimStats.nbActiveKinematicBodies);
		SET_DWORD_STAT(STAT_NumStaticBodiesAsync, SimStats.nbStaticBodies);
		SET_DWORD_STAT(STAT_NumMobileBodiesAsync, SimStats.nbDynamicBodies);

		//SET_DWORD_STAT(STAT_NumBroadphaseAddsAsync, SimStats.getNbBroadPhaseAdds(PxSimulationStatistics::VolumeType::eRIGID_BODY)); //TODO: These do not seem to work
		//SET_DWORD_STAT(STAT_NumBroadphaseRemovesAsync, SimStats.getNbBroadPhaseRemoves(PxSimulationStatistics::VolumeType::eRIGID_BODY));

		uint32 NumShapes = 0;
		for (int32 GeomType = 0; GeomType < PxGeometryType::eGEOMETRY_COUNT; ++GeomType)
		{
			NumShapes += SimStats.nbShapes[GeomType];
		}

		SET_DWORD_STAT(STAT_NumShapesAsync, NumShapes);
	}
}
#endif // WITH_PHYSX

DECLARE_FLOAT_COUNTER_STAT(TEXT("Sync Sim Time (ms)"), STAT_PhysSyncSim, STATGROUP_Physics);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Async Sim Time (ms)"), STAT_PhysAsyncSim, STATGROUP_Physics);

double GSimStartTime[PST_MAX] = {0.f, 0.f};

void FinishSceneStat(uint32 Scene)
{
	if (Scene < PST_MAX)	//PST_MAX used when we don't care
	{
		float SceneTime = float(FPlatformTime::Seconds() - GSimStartTime[Scene]) * 1000.0f;
		switch(Scene)
		{
			case PST_Sync:
				INC_FLOAT_STAT_BY(STAT_PhysSyncSim, SceneTime); break;
			case PST_Async:
				INC_FLOAT_STAT_BY(STAT_PhysAsyncSim, SceneTime); break;
		}
	}
}

void GatherClothingStats(const UWorld* World)
{
#if WITH_PHYSX
#if STATS
	QUICK_SCOPE_CYCLE_COUNTER(STAT_GatherApexStats);

	SET_DWORD_STAT(STAT_NumCloths, 0);
	SET_DWORD_STAT(STAT_NumClothVerts, 0);

	if ( FThreadStats::IsCollectingData(GET_STATID(STAT_NumCloths)) ||  FThreadStats::IsCollectingData(GET_STATID(STAT_NumClothVerts)) )
	{
		for (TObjectIterator<USkeletalMeshComponent> Itr; Itr; ++Itr)
		{
			if (Itr->GetWorld() != World) { continue; }

			if(const IClothingSimulation* Simulation = Itr->GetClothingSimulation())
			{
				Simulation->GatherStats();
			}
		}
	}
#endif
#endif // WITH_PHYSX
}

/** Exposes ticking of physics-engine scene outside Engine. */
void FPhysScene_PhysX::TickPhysScene(uint32 SceneType, FGraphEventRef& InOutCompletionEvent)
{
	SCOPE_CYCLE_COUNTER(STAT_TotalPhysicsTime);
	CSV_SCOPED_TIMING_STAT(Basic, UWorld_Tick_TotalPhysicsTime);

	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_PhysicsKickOffDynamicsTime, SceneType == PST_Sync);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_PhysicsKickOffDynamicsTime_Async, SceneType == PST_Async);

	check(SceneType < NumPhysScenes && SceneType < PST_MAX);

	GSimStartTime[SceneType] = FPlatformTime::Seconds();

	if (bPhysXSceneExecuting[SceneType] != 0)
	{
		// Already executing this scene, must call WaitPhysScene before calling this function again.
		UE_LOG(LogPhysics, Log, TEXT("TickPhysScene: Already executing scene (%d) - aborting."), SceneType);
		return;
	}

	/**
	* clamp down... if this happens we are simming physics slower than real-time, so be careful with it.
	* it can improve framerate dramatically (really, it is the same as scaling all velocities down and
	* enlarging all timesteps) but at the same time, it will screw with networking (client and server will
	* diverge a lot more.)
	*/

	float UseDelta = FMath::Min(UseSyncTime(SceneType) ? SyncDeltaSeconds : DeltaSeconds, MaxPhysicsDeltaTime);

	// Only simulate a positive time step.
	if (UseDelta <= 0.f)
	{
		if (UseDelta < 0.f)
		{
			// only do this if negative. Otherwise, whenever we pause, this will come up
			UE_LOG(LogPhysics, Warning, TEXT("TickPhysScene: Negative timestep (%f) - aborting."), UseDelta);
		}
		return;
	}

	/**
	* Weight frame time according to PhysScene settings.
	*/
	AveragedFrameTime[SceneType] *= FrameTimeSmoothingFactor[SceneType];
	AveragedFrameTime[SceneType] += (1.0f - FrameTimeSmoothingFactor[SceneType])*UseDelta;

	// Set execution flag
	bPhysXSceneExecuting[SceneType] = true;

	check(!InOutCompletionEvent.GetReference()); // these should be gone because nothing is outstanding
	InOutCompletionEvent = FGraphEvent::CreateGraphEvent();
	bool bTaskOutstanding = false;

#if !WITH_PHYSX
	const bool bSimulateScene = false;
#else
#if !WITH_APEX
	PxScene* PScene = GetPxScene(SceneType);
	const bool bSimulateScene = PScene && (UseDelta > 0.f);
#else
	apex::Scene* ApexScene = GetApexScene(SceneType);
	const bool bSimulateScene = ApexScene && UseDelta > 0.f;
#endif
#endif

	// Replicate physics
#if WITH_PHYSX
	if (bSimulateScene && PhysicsReplication)
	{
		PhysicsReplication->Tick(AveragedFrameTime[SceneType]);
	}
#endif

	// Replicate physics
#if WITH_PHYSX
	if (bSimulateScene && PhysicsReplication)
	{
		PhysicsReplication->Tick(AveragedFrameTime[SceneType]);
	}
#endif

	float PreTickTime = IsSubstepping(SceneType) ? UseDelta : AveragedFrameTime[SceneType];

	// Broadcast 'pre tick' delegate
	OnPhysScenePreTick.Broadcast(this, SceneType, PreTickTime);

	// If not substepping, call this delegate here. Otherwise we call it in FPhysSubstepTask::SubstepSimulationStart
	if (IsSubstepping(SceneType) == false)
	{
		OnPhysSceneStep.Broadcast(this, SceneType, PreTickTime);
	}
	else
	{
		//We're about to start stepping so swap buffers. Might want to find a better place for this?
		PhysSubSteppers[SceneType]->SwapBuffers();
	}

#if WITH_PHYSX
	bIsSceneSimulating[SceneType] = true;

	if (bSimulateScene)
	{
		if(IsSubstepping(SceneType)) //we don't bother sub-stepping cloth
		{
			bTaskOutstanding = SubstepSimulation(SceneType, InOutCompletionEvent);
		}
		else
		{
#if !WITH_APEX
			PhysXCompletionTask* Task = new PhysXCompletionTask(InOutCompletionEvent, SceneType, PScene->getTaskManager());
			PScene->lockWrite();
			PScene->simulate(AveragedFrameTime[SceneType], Task, SimScratchBuffers[SceneType].Buffer, SimScratchBuffers[SceneType].BufferSize);
			PScene->unlockWrite();
			Task->removeReference();
			bTaskOutstanding = true;
#else
			PhysXCompletionTask* Task = new PhysXCompletionTask(InOutCompletionEvent, SceneType, ApexScene->getTaskManager());
			ApexScene->simulate(AveragedFrameTime[SceneType], true, Task, SimScratchBuffers[SceneType].Buffer, SimScratchBuffers[SceneType].BufferSize);
			Task->removeReference();
			bTaskOutstanding = true;
#endif
		}
	}

#endif // WITH_PHYSX

	if (!bTaskOutstanding)
	{
		TArray<FBaseGraphTask*> NewTasks;
		InOutCompletionEvent->DispatchSubsequents(NewTasks, ENamedThreads::AnyThread); // nothing to do, so nothing to wait for
	}

	bSubstepping = UPhysicsSettings::Get()->bSubstepping;
	bSubsteppingAsync = UPhysicsSettings::Get()->bSubsteppingAsync;
}

void FPhysScene_PhysX::KillVisualDebugger()
{
#if WITH_PHYSX
	if (GPhysXVisualDebugger)
	{
		GPhysXVisualDebugger->disconnect();
	}
#endif // WITH_PHYSX 
}

void FPhysScene_PhysX::WaitPhysScenes()
{
	check(IsInGameThread());

	FGraphEventArray ThingsToComplete;
	if (PhysicsSceneCompletion.GetReference())
	{
		ThingsToComplete.Add(PhysicsSceneCompletion);
	}
	// Loop through scene types to get all scenes
	// we just wait on everything, though some of these are redundant
	for (uint32 SceneType = 0; SceneType < NumPhysScenes; ++SceneType)
	{
		if (PhysicsSubsceneCompletion[SceneType].GetReference())
		{
			ThingsToComplete.Add(PhysicsSubsceneCompletion[SceneType]);
		}
		if (FrameLaggedPhysicsSubsceneCompletion[SceneType].GetReference())
		{
			ThingsToComplete.Add(FrameLaggedPhysicsSubsceneCompletion[SceneType]);
		}
	}
	if (ThingsToComplete.Num())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FPhysScene_WaitPhysScenes);
		FTaskGraphInterface::Get().WaitUntilTasksComplete(ThingsToComplete, ENamedThreads::GameThread);
	}
}

void FPhysScene_PhysX::SceneCompletionTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent, EPhysicsSceneType SceneType)
{
	ProcessPhysScene(SceneType);
}

void FPhysScene_PhysX::ProcessPhysScene(uint32 SceneType)
{
	LLM_SCOPE(ELLMTag::PhysX);

	SCOPED_NAMED_EVENT(FPhysScene_ProcessPhysScene, FColor::Orange);
	checkSlow(SceneType < PST_MAX);

	SCOPE_CYCLE_COUNTER(STAT_TotalPhysicsTime);
	CSV_SCOPED_TIMING_STAT(Basic, UWorld_Tick_TotalPhysicsTime);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_PhysicsFetchDynamicsTime, SceneType == PST_Sync);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_PhysicsFetchDynamicsTime_Async, SceneType == PST_Async);

	check(SceneType < NumPhysScenes);
	if (bPhysXSceneExecuting[SceneType] == 0)
	{
		// Not executing this scene, must call TickPhysScene before calling this function again.
		UE_LOG(LogPhysics, Log, TEXT("WaitPhysScene`: Not executing this scene (%d) - aborting."), SceneType);
		return;
	}

	if (FrameLagAsync())
	{
		static_assert(PST_MAX == 2, "Physics scene static test failed."); // Here we assume the PST_Sync is the master and never fame lagged
		if (SceneType == PST_Sync)
		{
			// the one frame lagged one should be done by now.
			check(!FrameLaggedPhysicsSubsceneCompletion[PST_Async].GetReference() || FrameLaggedPhysicsSubsceneCompletion[PST_Async]->IsComplete());
		}
		else if (SceneType == PST_Async)
		{
			FrameLaggedPhysicsSubsceneCompletion[PST_Async] = NULL;
		}
	}


	// Reset execution flag

	bool bSuccess = false;

#if WITH_PHYSX
	//This fetches and gets active transforms. It's important that the function that calls this locks because getting the transforms and using the data must be an atomic operation
	PxScene* PScene = GetPxScene(SceneType);
	check(PScene);
	PxU32 OutErrorCode = 0;

	PScene->lockWrite();
#if !WITH_APEX
	bSuccess = PScene->fetchResults(true, &OutErrorCode);
#else	//	#if !WITH_APEX
	// The APEX scene calls the fetchResults function for the PhysX scene, so we only call ApexScene->fetchResults().
	apex::Scene* ApexScene = GetApexScene(SceneType);
	check(ApexScene);
	bSuccess = ApexScene->fetchResults(true, &OutErrorCode);
#endif	//	#if !WITH_APEX

	if (OutErrorCode != 0)
	{
		UE_LOG(LogPhysics, Log, TEXT("PHYSX FETCHRESULTS ERROR: %d"), OutErrorCode);
	}

	SyncComponentsToBodies_AssumesLocked(SceneType);
	PScene->unlockWrite();
#endif // WITH_PHYSX

	PhysicsSubsceneCompletion[SceneType] = NULL;
	bPhysXSceneExecuting[SceneType] = false;

#if WITH_PHYSX
	bIsSceneSimulating[SceneType] = false;
#endif

	// Broadcast 'post tick' delegate
	OnPhysScenePostTick.Broadcast(this, SceneType);
}

/** Struct to remember a pending component transform change */
struct FPhysScenePendingComponentTransform_PhysX
{
	/** Component to move */
	TWeakObjectPtr<UPrimitiveComponent> OwningComp;
	/** New transform from physics engine */
	FTransform NewTransform;

	FPhysScenePendingComponentTransform_PhysX(UPrimitiveComponent* InOwningComp, const FTransform& InNewTransform)
		: OwningComp(InOwningComp)
		, NewTransform(InNewTransform)
	{}
};

void FPhysScene_PhysX::SyncComponentsToBodies_AssumesLocked(uint32 SceneType)
{
	checkSlow(SceneType < PST_MAX);

	SCOPE_CYCLE_COUNTER(STAT_TotalPhysicsTime);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_SyncComponentsToBodies, SceneType == PST_Sync);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_SyncComponentsToBodies_Async, SceneType == PST_Async);

#if WITH_PHYSX
	PxScene* PScene = GetPxScene(SceneType);
	check(PScene);

	/** Array of custom sync handlers (plugins) */
	TArray<FCustomPhysXSyncActors*> CustomPhysXSyncActors;

	PxU32 NumActors = 0;
	PxActor** PActiveActors = PScene->getActiveActors(NumActors);

	TArray<FPhysScenePendingComponentTransform_PhysX> PendingTransforms;

	for (PxU32 TransformIdx = 0; TransformIdx < NumActors; ++TransformIdx)
	{
		PxActor* PActiveActor = PActiveActors[TransformIdx];
#ifdef __EMSCRIPTEN__
		// emscripten doesn't seem to know how to look at <PxRigidActor> from the PxActor class...
		PxRigidActor* XRigidActor = static_cast<PxRigidActor*>(PActiveActor); // is()
		PxRigidActor* RigidActor = XRigidActor->PxRigidActor::isKindOf(PxTypeInfo<PxRigidActor>::name()) ? XRigidActor : NULL; // typeMatch<T>()
#else
		PxRigidActor* RigidActor = PActiveActor->is<PxRigidActor>();
#endif

		ensure(!RigidActor->userData || !FPhysxUserData::IsGarbage(RigidActor->userData));

		if (FBodyInstance* BodyInstance = FPhysxUserData::Get<FBodyInstance>(RigidActor->userData))
		{
			if (BodyInstance->InstanceBodyIndex == INDEX_NONE && BodyInstance->OwnerComponent.IsValid())
			{
				check(BodyInstance->OwnerComponent->IsRegistered()); // shouldn't have a physics body for a non-registered component!

				const FTransform NewTransform = BodyInstance->GetUnrealWorldTransform_AssumesLocked();

				// Add to set of transforms to process
				// We can't actually move the component now (or check for out of world), because that could destroy a body
				// elsewhere in the PActiveActors array, resulting in a bad pointer
				FPhysScenePendingComponentTransform_PhysX NewEntry(BodyInstance->OwnerComponent.Get(), NewTransform);
				PendingTransforms.Add(NewEntry);
			}
		}
		else if (const FCustomPhysXPayload* CustomPayload = FPhysxUserData::Get<FCustomPhysXPayload>(RigidActor->userData))
		{
			if(CustomPayload->CustomSyncActors)
			{
				CustomPhysXSyncActors.AddUnique(CustomPayload->CustomSyncActors);	//NOTE: AddUnique because the assumed number of plugins that rely on this is very small
				CustomPayload->CustomSyncActors->Actors.Add(RigidActor);
			}
		}
	}

	//Give custom plugins the chance to build the sync data
	for (FCustomPhysXSyncActors* CustomSync : CustomPhysXSyncActors)
	{
		CustomSync->BuildSyncData_AssumesLocked(SceneType, CustomSync->Actors);
		CustomSync->Actors.Empty(CustomSync->Actors.Num());
	}

	//Allow custom plugins to actually act on the sync data
	for (FCustomPhysXSyncActors* CustomSync : CustomPhysXSyncActors)
	{
		CustomSync->FinalizeSync(SceneType);
	}

	/// Now actually move components
	for (FPhysScenePendingComponentTransform_PhysX& Entry : PendingTransforms)
	{
		// Check if still valid (ie not destroyed)
		UPrimitiveComponent* OwnerComponent = Entry.OwningComp.Get();
		if (OwnerComponent != nullptr)
		{
			AActor* Owner = OwnerComponent->GetOwner();

			// See if the transform is actually different, and if so, move the component to match physics
			if (!Entry.NewTransform.EqualsNoScale(OwnerComponent->GetComponentTransform()))
			{
				const FVector MoveBy = Entry.NewTransform.GetLocation() - OwnerComponent->GetComponentTransform().GetLocation();
				const FQuat NewRotation = Entry.NewTransform.GetRotation();

				//@warning: do not reference BodyInstance again after calling MoveComponent() - events from the move could have made it unusable (destroying the actor, SetPhysics(), etc)
				OwnerComponent->MoveComponent(MoveBy, NewRotation, false, NULL, MOVECOMP_SkipPhysicsMove);
			}

			// Check if we didn't fall out of the world
			if (Owner != NULL && !Owner->IsPendingKill())
			{
				Owner->CheckStillInWorld();
			}
		}
	}

#endif // WITH_PHYSX 
}

void FPhysScene_PhysX::DispatchPhysNotifications_AssumesLocked()
{
#if WITH_PHYSX
	SCOPE_CYCLE_COUNTER(STAT_PhysicsEventTime);

	for(int32 SceneType = 0; SceneType < PST_MAX; ++SceneType)
	{
		TArray<FCollisionNotifyInfo>& PendingCollisionNotifies = GetPendingCollisionNotifies(SceneType);

		// Let the game-specific PhysicsCollisionHandler process any physics collisions that took place
		if (OwningWorld != NULL && OwningWorld->PhysicsCollisionHandler != NULL)
		{
			OwningWorld->PhysicsCollisionHandler->HandlePhysicsCollisions_AssumesLocked(PendingCollisionNotifies);
		}

		// Fire any collision notifies in the queue.
		for (int32 i = 0; i<PendingCollisionNotifies.Num(); i++)
		{
			FCollisionNotifyInfo& NotifyInfo = PendingCollisionNotifies[i];
			if (NotifyInfo.RigidCollisionData.ContactInfos.Num() > 0)
			{
				if (NotifyInfo.bCallEvent0 && NotifyInfo.IsValidForNotify() && NotifyInfo.Info0.Actor.IsValid())
				{
					NotifyInfo.Info0.Actor->DispatchPhysicsCollisionHit(NotifyInfo.Info0, NotifyInfo.Info1, NotifyInfo.RigidCollisionData);
				}

				// Need to check IsValidForNotify again in case first call broke something.
				if (NotifyInfo.bCallEvent1 && NotifyInfo.IsValidForNotify() && NotifyInfo.Info1.Actor.IsValid())
				{
					NotifyInfo.RigidCollisionData.SwapContactOrders();
					NotifyInfo.Info1.Actor->DispatchPhysicsCollisionHit(NotifyInfo.Info1, NotifyInfo.Info0, NotifyInfo.RigidCollisionData);
				}
			}
		}
		PendingCollisionNotifies.Reset();
	}

	for (int32 SceneType = 0; SceneType < PST_MAX; ++SceneType)
	{
		for (auto MapItr = PendingSleepEvents[SceneType].CreateIterator(); MapItr; ++MapItr)
		{
			FBodyInstance* BodyInstance = MapItr.Key();
			if(UPrimitiveComponent* PrimitiveComponent = BodyInstance->OwnerComponent.Get())
			{
				PrimitiveComponent->DispatchWakeEvents(MapItr.Value(), BodyInstance->BodySetup->BoneName);
			}
		}

		PendingSleepEvents[SceneType].Empty();
	}

	for(int32 SceneType = 0; SceneType < PST_MAX; ++SceneType)
	{
		FPendingConstraintData& ConstraintData = PendingConstraintData[SceneType];
		for(FConstraintBrokenDelegateData& ConstraintBrokenData : ConstraintData.PendingConstraintBroken)
		{
			ConstraintBrokenData.DispatchOnBroken();
		}

		ConstraintData.PendingConstraintBroken.Empty();
	}
#endif // WITH_PHYSX 

#if WITH_APEIRON
	check(false);
#else
	FPhysicsDelegates::OnPhysDispatchNotifications.Broadcast(this);
#endif

}

void FPhysScene_PhysX::SetUpForFrame(const FVector* NewGrav, float InDeltaSeconds, float InMaxPhysicsDeltaTime)
{
	DeltaSeconds = InDeltaSeconds;
	MaxPhysicsDeltaTime = InMaxPhysicsDeltaTime;
#if WITH_PHYSX
	if (NewGrav)
	{
		// Loop through scene types to get all scenes
		for (uint32 SceneType = 0; SceneType < NumPhysScenes; ++SceneType)
		{
			PxScene* PScene = GetPxScene(SceneType);
			if (PScene != NULL)
			{
				//@todo phys_thread don't do this if gravity changes

				//@todo, to me it looks like we should avoid this if the gravity has not changed, the lock is probably expensive
				// Lock scene lock, in case it is required
				SCENE_LOCK_WRITE(PScene);

				PScene->setGravity(U2PVector(*NewGrav));

				// Unlock scene lock, in case it is required
				SCENE_UNLOCK_WRITE(PScene);
			}
		}
	}
#endif
}

FAutoConsoleTaskPriority CPrio_PhyXSceneCompletion(
	TEXT("TaskGraph.TaskPriorities.PhyXSceneCompletion"),
	TEXT("Task and thread priority for PhysicsSceneCompletion."),
	ENamedThreads::HighThreadPriority, // if we have high priority task threads, then use them...
	ENamedThreads::HighTaskPriority, // .. at high task priority
	ENamedThreads::HighTaskPriority // if we don't have hi pri threads, then use normal priority threads at high task priority instead
);



void FPhysScene_PhysX::StartFrame()
{
	FGraphEventArray FinishPrerequisites;

	//Update the collision disable table before ticking
	FlushDeferredCollisionDisableTableQueue();

	// Run the sync scene
	TickPhysScene(PST_Sync, PhysicsSubsceneCompletion[PST_Sync]);
	{
		FGraphEventArray MainScenePrerequisites;
		if (FrameLagAsync() && bAsyncSceneEnabled)
		{
			if (FrameLaggedPhysicsSubsceneCompletion[PST_Async].GetReference() && !FrameLaggedPhysicsSubsceneCompletion[PST_Async]->IsComplete())
			{
				MainScenePrerequisites.Add(FrameLaggedPhysicsSubsceneCompletion[PST_Async]);
				FinishPrerequisites.Add(FrameLaggedPhysicsSubsceneCompletion[PST_Async]);
			}
		}
		if (PhysicsSubsceneCompletion[PST_Sync].GetReference())
		{
			MainScenePrerequisites.Add(PhysicsSubsceneCompletion[PST_Sync]);

			DECLARE_CYCLE_STAT(TEXT("FDelegateGraphTask.ProcessPhysScene_Sync"),
			STAT_FDelegateGraphTask_ProcessPhysScene_Sync,
				STATGROUP_TaskGraphTasks);

			new (FinishPrerequisites)FGraphEventRef(
				FDelegateGraphTask::CreateAndDispatchWhenReady(
					FDelegateGraphTask::FDelegate::CreateRaw(this, &FPhysScene_PhysX::SceneCompletionTask, PST_Sync),
					GET_STATID(STAT_FDelegateGraphTask_ProcessPhysScene_Sync), &MainScenePrerequisites,
					ENamedThreads::GameThread, ENamedThreads::GameThread
				)
			);
		}
	}

	if (!FrameLagAsync() && bAsyncSceneEnabled)
	{
		TickPhysScene(PST_Async, PhysicsSubsceneCompletion[PST_Async]);
		if (PhysicsSubsceneCompletion[PST_Async].GetReference())
		{
			DECLARE_CYCLE_STAT(TEXT("FDelegateGraphTask.ProcessPhysScene_Async"),
			STAT_FDelegateGraphTask_ProcessPhysScene_Async,
				STATGROUP_TaskGraphTasks);

			new (FinishPrerequisites)FGraphEventRef(
				FDelegateGraphTask::CreateAndDispatchWhenReady(
					FDelegateGraphTask::FDelegate::CreateRaw(this, &FPhysScene_PhysX::SceneCompletionTask, PST_Async),
					GET_STATID(STAT_FDelegateGraphTask_ProcessPhysScene_Async), PhysicsSubsceneCompletion[PST_Async],
					ENamedThreads::GameThread, ENamedThreads::GameThread
				)
			);
		}
	}

	check(!PhysicsSceneCompletion.GetReference()); // this should have been cleared
	if (FinishPrerequisites.Num())
	{
		if (FinishPrerequisites.Num() > 1)  // we don't need to create a new task if we only have one prerequisite
		{
			DECLARE_CYCLE_STAT(TEXT("FNullGraphTask.ProcessPhysScene_Join"),
			STAT_FNullGraphTask_ProcessPhysScene_Join,
				STATGROUP_TaskGraphTasks);

			PhysicsSceneCompletion = TGraphTask<FNullGraphTask>::CreateTask(&FinishPrerequisites, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(
				GET_STATID(STAT_FNullGraphTask_ProcessPhysScene_Join), PhysSingleThreadedMode() ? ENamedThreads::GameThread : CPrio_PhyXSceneCompletion.Get());
		}
		else
		{
			PhysicsSceneCompletion = FinishPrerequisites[0]; // we don't need a join
		}
	}


	// Query clothing stats from skel mesh components in this world
	// This is done outside TickPhysScene because clothing is
	// not related to a scene.
	GatherClothingStats(this->OwningWorld);

	// Record the sync tick time for use with the async tick
	SyncDeltaSeconds = DeltaSeconds;
}

void FPhysScene_PhysX::StartAsync()
{
	FGraphEventArray FinishPrerequisites;

	//If the async scene is lagged we start it here
	if (FrameLagAsync() && bAsyncSceneEnabled)
	{
		TickPhysScene(PST_Async, PhysicsSubsceneCompletion[PST_Async]);
		if (PhysicsSubsceneCompletion[PST_Async].GetReference())
		{
			DECLARE_CYCLE_STAT(TEXT("FDelegateGraphTask.ProcessPhysScene_Async"),
			STAT_FDelegateGraphTask_ProcessPhysScene_Async,
				STATGROUP_TaskGraphTasks);

			FrameLaggedPhysicsSubsceneCompletion[PST_Async] = FDelegateGraphTask::CreateAndDispatchWhenReady(
				FDelegateGraphTask::FDelegate::CreateRaw(this, &FPhysScene_PhysX::SceneCompletionTask, PST_Async),
				GET_STATID(STAT_FDelegateGraphTask_ProcessPhysScene_Async), PhysicsSubsceneCompletion[PST_Async],
				ENamedThreads::GameThread, ENamedThreads::GameThread
			);
		}
	}
}

void FPhysScene_PhysX::EndFrame(ULineBatchComponent* InLineBatcher)
{
	check(IsInGameThread());

	PhysicsSceneCompletion = NULL;

	/**
	* At this point physics simulation has finished. We obtain both scene locks so that the various read/write operations needed can be done quickly.
	* This means that anyone attempting to write on other threads will be blocked. This is OK because accessing any of these game objects from another thread is probably a bad idea!
	*/

#if WITH_PHYSX
	SCOPED_SCENE_WRITE_LOCK(GetPxScene(PST_Sync));
	SCOPED_SCENE_WRITE_LOCK(bAsyncSceneEnabled ? GetPxScene(PST_Async) : nullptr);
#endif // WITH_PHYSX 

#if ( WITH_PHYSX  && !(UE_BUILD_SHIPPING || WITH_PHYSX_RELEASE))
	GatherPhysXStats_AssumesLocked(GetPxScene(PST_Sync), HasAsyncScene() ? GetPxScene(PST_Async) : nullptr);
#endif

	// Perform any collision notification events
	DispatchPhysNotifications_AssumesLocked();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Handle debug rendering
	if (InLineBatcher)
	{
		AddDebugLines(PST_Sync, InLineBatcher);

		if (bAsyncSceneEnabled)
		{
			AddDebugLines(PST_Async, InLineBatcher);
		}

	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

#if WITH_PHYSX
/** Helper struct that puts all awake actors to sleep and then later wakes them back up */
struct FHelpEnsureCollisionTreeIsBuilt
{
	FHelpEnsureCollisionTreeIsBuilt(PxScene* InPScene)
		: PScene(InPScene)
	{
		if(PScene)
		{
			SCOPED_SCENE_WRITE_LOCK(PScene);
			const int32 NumActors = PScene->getNbActors(PxActorTypeFlag::eRIGID_DYNAMIC);

			if (NumActors)
			{
				ActorBuffer.AddUninitialized(NumActors);
				PScene->getActors(PxActorTypeFlag::eRIGID_DYNAMIC, &ActorBuffer[0], NumActors);

				for (PxActor*& PActor : ActorBuffer)
				{
					if (PActor)
					{
						if (PxRigidDynamic* PDynamic = PActor->is<PxRigidDynamic>())
						{
							if (PDynamic->isSleeping() == false)
							{
								PDynamic->putToSleep();
							}
							else
							{
								PActor = nullptr;
							}
						}
					}
				}
			}
		}
	}

	~FHelpEnsureCollisionTreeIsBuilt()
	{
		SCOPED_SCENE_WRITE_LOCK(PScene);
		for (PxActor* PActor : ActorBuffer)
		{
			if (PActor)
			{
				if (PxRigidDynamic* PDynamic = PActor->is<PxRigidDynamic>())
				{
					PDynamic->wakeUp();
				}
			}
		}
	}

private:

	TArray<PxActor*> ActorBuffer;
	PxScene* PScene;
};

#endif


DECLARE_CYCLE_STAT(TEXT("EnsureCollisionTreeIsBuilt"), STAT_PhysicsEnsureCollisionTreeIsBuilt, STATGROUP_Physics);

void FPhysScene_PhysX::EnsureCollisionTreeIsBuilt(UWorld* World)
{
	check(IsInGameThread());


	SCOPE_CYCLE_COUNTER(STAT_PhysicsEnsureCollisionTreeIsBuilt);
	//We have to call fetchResults several times to update the internal data structures. PhysX doesn't have an API for this so we have to make all actors sleep before doing this

	SetIsStaticLoading(true);

#if WITH_PHYSX
	FHelpEnsureCollisionTreeIsBuilt SyncSceneHelper(GetPxScene(PST_Sync));
	FHelpEnsureCollisionTreeIsBuilt AsyncSceneHelper(HasAsyncScene() ? GetPxScene(PST_Async) : nullptr);
#endif

	for (int Iteration = 0; Iteration < 6; ++Iteration)
	{
		World->SetupPhysicsTickFunctions(0.1f);
		StartFrame();
		WaitPhysScenes();
		EndFrame(nullptr);
	}

	SetIsStaticLoading(false);
}

void FPhysScene_PhysX::SetIsStaticLoading(bool bStaticLoading)
{
	SetPhysXTreeRebuildRateImp(bStaticLoading ? 5 : PhysXTreeRebuildRate);
}

void FPhysScene_PhysX::SetPhysXTreeRebuildRate(int32 RebuildRate)
{
	PhysXTreeRebuildRate = FMath::Max(4, RebuildRate);
	SetPhysXTreeRebuildRateImp(RebuildRate);
}

void FPhysScene_PhysX::SetPhysXTreeRebuildRateImp(int32 RebuildRate)
{
#if WITH_PHYSX
	// Loop through scene types to get all scenes
	for (uint32 SceneType = 0; SceneType < NumPhysScenes; ++SceneType)
	{
		PxScene* PScene = GetPxScene(SceneType);
		if (PScene != NULL)
		{
			// Lock scene lock, in case it is required
			SCENE_LOCK_WRITE(PScene);

			// Sets the rebuild rate hint, to 1 frame if static loading
			PScene->setDynamicTreeRebuildRateHint(PhysXTreeRebuildRate);

			// Unlock scene lock, in case it is required
			SCENE_UNLOCK_WRITE(PScene);
		}
	}
#endif
}

#if WITH_PHYSX
/** Utility for looking up the PxScene associated with this FPhysScene. */
PxScene* FPhysScene_PhysX::GetPxScene(uint32 SceneType) const
{
	if(SceneType < NumPhysScenes)
	{
#if WITH_APEX
		nvidia::apex::Scene* ApexScene = PhysXScenes[SceneType];
		return (ApexScene != nullptr) ? ApexScene->getPhysXScene() : nullptr;
#else
		return PhysXScenes[SceneType];
#endif
	}

	return nullptr;
}

#if WITH_APEX


apex::Scene* FPhysScene_PhysX::GetApexScene(uint32 SceneType) const
{
	if(SceneType < NumPhysScenes)
	{
		return PhysXScenes[SceneType];
	}

	return nullptr;

}
#endif // WITH_APEX

static void BatchPxRenderBufferLines(class ULineBatchComponent& LineBatcherToUse, const PxRenderBuffer& DebugData)
{
	int32 NumPoints = DebugData.getNbPoints();
	if (NumPoints > 0)
	{
		const PxDebugPoint* Points = DebugData.getPoints();
		for (int32 i = 0; i<NumPoints; i++)
		{
			LineBatcherToUse.DrawPoint(P2UVector(Points->pos), FColor((uint32)Points->color), 2, SDPG_World);

			Points++;
		}
	}

	// Build a list of all the lines we want to draw
	TArray<FBatchedLine> DebugLines;

	// Add all the 'lines' from PhysX
	int32 NumLines = DebugData.getNbLines();
	if (NumLines > 0)
	{
		const PxDebugLine* Lines = DebugData.getLines();
		for (int32 i = 0; i<NumLines; i++)
		{
			new(DebugLines)FBatchedLine(P2UVector(Lines->pos0), P2UVector(Lines->pos1), FColor((uint32)Lines->color0), 0.f, 0.0f, SDPG_World);
			Lines++;
		}
	}

	// Add all the 'triangles' from PhysX
	int32 NumTris = DebugData.getNbTriangles();
	if (NumTris > 0)
	{
		const PxDebugTriangle* Triangles = DebugData.getTriangles();
		for (int32 i = 0; i<NumTris; i++)
		{
			new(DebugLines)FBatchedLine(P2UVector(Triangles->pos0), P2UVector(Triangles->pos1), FColor((uint32)Triangles->color0), 0.f, 0.0f, SDPG_World);
			new(DebugLines)FBatchedLine(P2UVector(Triangles->pos1), P2UVector(Triangles->pos2), FColor((uint32)Triangles->color1), 0.f, 0.0f, SDPG_World);
			new(DebugLines)FBatchedLine(P2UVector(Triangles->pos2), P2UVector(Triangles->pos0), FColor((uint32)Triangles->color2), 0.f, 0.0f, SDPG_World);
			Triangles++;
		}
	}

	// Draw them all in one call.
	if (DebugLines.Num() > 0)
	{
		LineBatcherToUse.DrawLines(DebugLines);
	}
}
#endif // WITH_PHYSX


/** Add any debug lines from the physics scene to the supplied line batcher. */
void FPhysScene_PhysX::AddDebugLines(uint32 SceneType, class ULineBatchComponent* LineBatcherToUse)
{
	check(SceneType < NumPhysScenes);

	if (LineBatcherToUse)
	{
#if WITH_PHYSX
		// Render PhysX debug data
		PxScene* PScene = GetPxScene(SceneType);
		const PxRenderBuffer& DebugData = PScene->getRenderBuffer();
		BatchPxRenderBufferLines(*LineBatcherToUse, DebugData);
#if WITH_APEX
		// Render APEX debug data
		apex::Scene* ApexScene = GetApexScene(SceneType);
		const PxRenderBuffer* RenderBuffer = ApexScene->getRenderBuffer();
		if (RenderBuffer != NULL)
		{
			BatchPxRenderBufferLines(*LineBatcherToUse, *RenderBuffer);
			ApexScene->updateRenderResources();
		}
#endif	// WITH_APEX
#endif	// WITH_PHYSX
	}
}

#if !UE_BUILD_SHIPPING
int32 ForceSubstep = 0;
FAutoConsoleVariableRef CVarSubStep(
	TEXT("p.ForceSubstep"),
	ForceSubstep,
	TEXT("Whether to force substepping on")
	TEXT("0: Ignore, 1: Force"),
	ECVF_Default);
#else
constexpr int32 ForceSubstep = 0;
#endif

bool FPhysScene_PhysX::IsSubstepping(uint32 SceneType) const
{
	// Substepping relies on interpolating transforms over frames, but only game worlds will be ticked,
	// so we disallow this feature in non-game worlds.
	if (!OwningWorld || !OwningWorld->IsGameWorld())
	{
		return false;
	}

	if (SceneType == PST_Sync)
	{
		return ForceSubstep == 1 || bSubstepping;
	}

	if (SceneType == PST_Async)
	{
		return bSubsteppingAsync;
	}

	return false;
}


void FPhysScene_PhysX::ApplyWorldOffset(FVector InOffset)
{
#if WITH_PHYSX
	// Loop through scene types to get all scenes
	for (uint32 SceneType = 0; SceneType < NumPhysScenes; ++SceneType)
	{
		PxScene* PScene = GetPxScene(SceneType);
		if (PScene != NULL)
		{
			// Lock scene lock, in case it is required
			SCENE_LOCK_WRITE(PScene);

			PScene->shiftOrigin(U2PVector(-InOffset));

			// Unlock scene lock, in case it is required
			SCENE_UNLOCK_WRITE(PScene);
		}
	}
#endif
}

void FPhysScene_PhysX::InitPhysScene(uint32 SceneType, const AWorldSettings* Settings)
{
	check(SceneType < NumPhysScenes);

#if WITH_PHYSX

	int64 NumPhysxDispatcher = 0;
	FParse::Value(FCommandLine::Get(), TEXT("physxDispatcher="), NumPhysxDispatcher);
	if (NumPhysxDispatcher == 0 && FParse::Param(FCommandLine::Get(), TEXT("physxDispatcher")))
	{
		NumPhysxDispatcher = 4;	//by default give physx 4 threads
	}

	// Create dispatcher for tasks
	if (PhysSingleThreadedMode())
	{
		CPUDispatcher[SceneType] = new FPhysXCPUDispatcherSingleThread();
	}
	else
	{
		if (NumPhysxDispatcher)
		{
			CPUDispatcher[SceneType] = PxDefaultCpuDispatcherCreate(NumPhysxDispatcher);
		}
		else
		{
			CPUDispatcher[SceneType] = new FPhysXCPUDispatcher();
		}

	}


	PhysxUserData = FPhysxUserData(this);

	// Create sim event callback
	SimEventCallback[SceneType] = SimEventCallbackFactory.IsValid() ? SimEventCallbackFactory->Create(this, SceneType) : new FPhysXSimEventCallback(this, SceneType);
	ContactModifyCallback[SceneType] = ContactModifyCallbackFactory.IsValid() ? ContactModifyCallbackFactory->Create(this, SceneType) : nullptr;
	CCDContactModifyCallback[SceneType] = CCDContactModifyCallbackFactory.IsValid() ? CCDContactModifyCallbackFactory->Create(this, SceneType) : nullptr;

	// Include scene descriptor in loop, so that we might vary it with scene type
	PxSceneDesc PSceneDesc(GPhysXSDK->getTolerancesScale());
	PSceneDesc.cpuDispatcher = CPUDispatcher[SceneType];

	FPhysSceneShaderInfo PhysSceneShaderInfo;
	PhysSceneShaderInfo.PhysScene = this;
	PSceneDesc.filterShaderData = &PhysSceneShaderInfo;
	PSceneDesc.filterShaderDataSize = sizeof(PhysSceneShaderInfo);

	PSceneDesc.filterShader = GSimulationFilterShader ? GSimulationFilterShader : PhysXSimFilterShader;
	PSceneDesc.simulationEventCallback = SimEventCallback[SceneType];
	PSceneDesc.contactModifyCallback = ContactModifyCallback[SceneType];
	PSceneDesc.ccdContactModifyCallback = CCDContactModifyCallback[SceneType];

	if(UPhysicsSettings::Get()->bEnablePCM)
	{
		PSceneDesc.flags |= PxSceneFlag::eENABLE_PCM;
	}
	else
	{
		PSceneDesc.flags &= ~PxSceneFlag::eENABLE_PCM;
	}

	if (UPhysicsSettings::Get()->bEnableStabilization)
	{
		PSceneDesc.flags |= PxSceneFlag::eENABLE_STABILIZATION;
	}
	else
	{
		PSceneDesc.flags &= ~PxSceneFlag::eENABLE_STABILIZATION;
	}

	// Set bounce threshold
	PSceneDesc.bounceThresholdVelocity = UPhysicsSettings::Get()->BounceThresholdVelocity;

	// If we're frame lagging the async scene (truly running it async) then use the scene lock
#if USE_SCENE_LOCK
	if(UPhysicsSettings::Get()->bWarnMissingLocks)
	{
		PSceneDesc.flags |= PxSceneFlag::eREQUIRE_RW_LOCK;
	}

#endif

	if(!UPhysicsSettings::Get()->bDisableActiveActors)
	{
		// We want to use 'active actors'
		PSceneDesc.flags |= PxSceneFlag::eENABLE_ACTIVE_ACTORS;
		PSceneDesc.flags |= PxSceneFlag::eEXCLUDE_KINEMATICS_FROM_ACTIVE_ACTORS;
	}

	// enable CCD at scene level
	if (UPhysicsSettings::Get()->bDisableCCD == false)
	{
		PSceneDesc.flags |= PxSceneFlag::eENABLE_CCD;
	}

	if(!UPhysicsSettings::Get()->bDisableKinematicStaticPairs && GPhysXForceNoKinematicStaticPairs == 0)
	{
		// Need to turn this on to consider kinematics turning into dynamic. Otherwise, you'll need to call resetFiltering to do the expensive broadphase reinserting
		PSceneDesc.flags |= PxSceneFlag::eENABLE_KINEMATIC_STATIC_PAIRS;
	}

	if(!UPhysicsSettings::Get()->bDisableKinematicKinematicPairs && GPhysXForceNoKinematicKinematicPairs == 0)
	{
		PSceneDesc.flags |= PxSceneFlag::eENABLE_KINEMATIC_PAIRS;	//this is only needed for destruction, but unfortunately this flag cannot be modified after creation and the plugin has no hook (yet)
	}

	// @TODO Should we set up PSceneDesc.limits? How?

	// Do this to improve loading times, esp. for streaming in sublevels
	PSceneDesc.staticStructure = PxPruningStructureType::eDYNAMIC_AABB_TREE;
	// Default to rebuilding tree slowly
	PSceneDesc.dynamicTreeRebuildRateHint = PhysXTreeRebuildRate;

	if (UPhysicsSettings::Get()->bEnableEnhancedDeterminism) {
		PSceneDesc.flags |= PxSceneFlag::eENABLE_ENHANCED_DETERMINISM;
	}

	bool bIsValid = PSceneDesc.isValid();
	if (!bIsValid)
	{
		UE_LOG(LogPhysics, Log, TEXT("Invalid PSceneDesc"));
	}

	// Setup MBP desc settings if required
	const FBroadphaseSettings& BroadphaseSettings = (Settings && Settings->bOverrideDefaultBroadphaseSettings) ? Settings->BroadphaseSettings : UPhysicsSettings::Get()->DefaultBroadphaseSettings;
	bool bUseMBP = IsRunningDedicatedServer() ? BroadphaseSettings.bUseMBPOnServer : BroadphaseSettings.bUseMBPOnClient;

	if(bUseMBP)
	{
		MbpBroadphaseCallbacks[SceneType] = new FPhysXMbpBroadphaseCallback();
		PSceneDesc.broadPhaseType = PxBroadPhaseType::eMBP;
		PSceneDesc.broadPhaseCallback = MbpBroadphaseCallbacks[SceneType];
	}
	else
	{
		MbpBroadphaseCallbacks[SceneType] = nullptr;
	}

	// Create scene, and add to map
	PxScene* PScene = GPhysXSDK->createScene(PSceneDesc);
	if(PxPvdSceneClient* PVDClient = PScene->getScenePvdClient())
	{
		PVDClient->setScenePvdFlags(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS | PxPvdSceneFlag::eTRANSMIT_CONTACTS | PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES);
	}

	// Setup actual MBP data on live scene
	if(bUseMBP)
	{
		uint32 NumSubdivisions = BroadphaseSettings.MBPNumSubdivs;

		if(IsRunningDedicatedServer())
		{
			if(GPhysXOverrideMbpNumSubdivisions_Server > 0)
			{
				NumSubdivisions = GPhysXOverrideMbpNumSubdivisions_Server;
			}
		}
		else
		{
			if(GPhysXOverrideMbpNumSubdivisions_Client > 0)
			{
				NumSubdivisions = GPhysXOverrideMbpNumSubdivisions_Client;
			}
		}

		// Must have at least one and no more than 256 regions, subdivision is num^2 so only up to 16
		NumSubdivisions = FMath::Clamp<uint32>(NumSubdivisions, 1, 16);

		const FBox& Bounds = BroadphaseSettings.MBPBounds;
		PxBounds3 MbpBounds(U2PVector(Bounds.Min), U2PVector(Bounds.Max));

		// Storage for generated regions, the generation function will create num^2 regions
		TArray<PxBounds3> GeneratedRegions;
		GeneratedRegions.AddZeroed(NumSubdivisions * NumSubdivisions);

		// Final parameter is up axis (2 == Z for Unreal Engine)
		PxBroadPhaseExt::createRegionsFromWorldBounds(GeneratedRegions.GetData(), MbpBounds, NumSubdivisions, 2);

		for(const PxBounds3& Region : GeneratedRegions)
		{
			PxBroadPhaseRegion NewRegion;
			NewRegion.bounds = Region;
			NewRegion.userData = nullptr; // No need to track back to an Unreal instance at the moment

			PScene->addBroadPhaseRegion(NewRegion);
		}
	}

#if WITH_APEX
	// Build the APEX scene descriptor for the PhysX scene
	apex::SceneDesc ApexSceneDesc;
	ApexSceneDesc.scene = PScene;
	// This interface allows us to modify the PhysX simulation filter shader data with contact pair flags
	ApexSceneDesc.physX3Interface = GPhysX3Interface;

	// Create the APEX scene from our descriptor
	apex::Scene* ApexScene = GApexSDK->createScene(ApexSceneDesc);

	// This enables debug rendering using the "legacy" method, not using the APEX render API
	ApexScene->setUseDebugRenderable(true);

	// Allocate a view matrix for APEX scene LOD
	ApexScene->allocViewMatrix(apex::ViewMatrixType::LOOK_AT_RH);

	// Store index of APEX scene in this FPhysScene
	PhysXScenes[SceneType] = ApexScene;
#else
	// Store index of PhysX Scene in this FPhysScene
	PhysXScenes[SceneType] = PScene;
#endif	// #if WITH_APEX

	// Save pointer to FPhysScene in userdata
	PScene->userData = &PhysxUserData;
#if WITH_APEX
	ApexScene->userData = &PhysxUserData;
#endif

	//Initialize substeppers
#if WITH_PHYSX
#if WITH_APEX
	PhysSubSteppers[SceneType] = new FPhysSubstepTask(ApexScene, this, SceneType);
#else
	PhysSubSteppers[SceneType] = new FPhysSubstepTask(PScene, this, SceneType);
#endif

	if (PxPvdSceneClient* PVDSceneClient = PScene->getScenePvdClient())
	{
		PVDSceneClient->setScenePvdFlags(PxPvdSceneFlag::eTRANSMIT_CONTACTS | PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES | PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS);
	}
#endif

#if WITH_APEIRON
	check(false);
#else
	FPhysicsDelegates::OnPhysSceneInit.Broadcast(this, (EPhysicsSceneType)SceneType);
#endif

#endif // WITH_PHYSX
}

void FPhysScene_PhysX::TermPhysScene(uint32 SceneType)
{
	check(SceneType < NumPhysScenes);

#if WITH_PHYSX
	PxScene* PScene = GetPxScene(SceneType);
	if (PScene != NULL)
	{
#if WITH_APEX
		apex::Scene* ApexScene = GetApexScene(SceneType);
		if (ApexScene != NULL)
		{
			GPhysCommandHandler->DeferredRelease(ApexScene);
		}
#endif // #if WITH_APEX

#if WITH_APEIRON
		check(false);
#else
		FPhysicsDelegates::OnPhysSceneTerm.Broadcast(this, (EPhysicsSceneType)SceneType);
#endif

		delete PhysSubSteppers[SceneType];
		PhysSubSteppers[SceneType] = NULL;

		// @todo block on any running scene before calling this
		GPhysCommandHandler->DeferredRelease(PScene);
		GPhysCommandHandler->DeferredDeleteSimEventCallback(SimEventCallback[SceneType]);
		GPhysCommandHandler->DeferredDeleteContactModifyCallback(ContactModifyCallback[SceneType]);
		GPhysCommandHandler->DeferredDeleteMbpBroadphaseCallback(MbpBroadphaseCallbacks[SceneType]);

		// Commands may have accumulated as the scene is terminated - flush any commands for this scene.
		GPhysCommandHandler->Flush();

		PhysXScenes[SceneType] = nullptr;
	}
#endif
}

#if WITH_PHYSX
void FPhysScene_PhysX::AddPendingOnConstraintBreak(FConstraintInstance* ConstraintInstance, int32 SceneType)
{
	PendingConstraintData[SceneType].PendingConstraintBroken.Add( FConstraintBrokenDelegateData(ConstraintInstance) );
}

FConstraintBrokenDelegateData::FConstraintBrokenDelegateData(FConstraintInstance* ConstraintInstance)
	: OnConstraintBrokenDelegate(ConstraintInstance->OnConstraintBrokenDelegate)
	, ConstraintIndex(ConstraintInstance->ConstraintIndex)
{

}

void FPhysScene_PhysX::AddPendingSleepingEvent(FBodyInstance* BI, ESleepEvent SleepEventType, int32 SceneType)
{
	PendingSleepEvents[SceneType].FindOrAdd(BI) = SleepEventType;
}

#if WITH_PHYSX
void ListAwakeRigidBodiesFromScene(bool bIncludeKinematic, PxScene* PhysXScene, int32& totalCount)
{
	check(PhysXScene != NULL);

	SCOPED_SCENE_READ_LOCK(PhysXScene);

	PxActor* PhysXActors[2048];
	int32 NumberActors = PhysXScene->getActors(PxActorTypeFlag::eRIGID_DYNAMIC, PhysXActors, 2048);
	for(int32 i = 0; i < NumberActors; ++i)
{
		PxRigidDynamic* RgActor = (PxRigidDynamic*)PhysXActors[i];
		if(!RgActor->isSleeping() && (bIncludeKinematic || RgActor->getRigidBodyFlags() != PxRigidBodyFlag::eKINEMATIC))
	{
			++totalCount;
			FBodyInstance* BodyInst = FPhysxUserData::Get<FBodyInstance>(RgActor->userData);
			if(BodyInst)
		{
				UE_LOG(LogPhysics, Log, TEXT("BI %s %d"), BodyInst->OwnerComponent.Get() ? *BodyInst->OwnerComponent.Get()->GetPathName() : TEXT("NONE"), BodyInst->InstanceBodyIndex);
		}
		else
		{
				UE_LOG(LogPhysics, Log, TEXT("BI %s"), TEXT("NONE"));
			}
		}
			}
		}
#endif // WITH_PHYSX

/** Util to list to log all currently awake rigid bodies */
void FPhysScene_PhysX::ListAwakeRigidBodies(bool bIncludeKinematic)
	{
#if WITH_PHYSX
	int32 BodyCount = 0;
	UE_LOG(LogPhysics, Log, TEXT("TOTAL: ListAwakeRigidBodies needs fixing."));
	ListAwakeRigidBodiesFromScene(bIncludeKinematic, GetPxScene(PST_Sync), BodyCount);
	if(HasAsyncScene())
		{
		ListAwakeRigidBodiesFromScene(bIncludeKinematic, GetPxScene(PST_Async), BodyCount);
	}
	UE_LOG(LogPhysics, Log, TEXT("TOTAL: %d awake bodies."), BodyCount);
#endif // WITH_PHYSX
}

#if !WITH_APEIRON
int32 FPhysScene::GetNumAwakeBodies()
{
	int32 NumAwake = 0;

	for(int32 SceneType = 0; SceneType < PST_MAX; ++SceneType)
{
		if(PxScene* PScene = GetPxScene(SceneType))
{
			TArray<PxActor*> PxActors;
			int32 NumActors = PScene->getNbActors(PxActorTypeFlag::eRIGID_DYNAMIC);
			PxActors.AddZeroed(NumActors);

			PScene->getActors(PxActorTypeFlag::eRIGID_DYNAMIC, PxActors.GetData(), NumActors * sizeof(PxActors[0]));
			for(PxActor* PActor : PxActors)
	{
				if(!PActor->is<PxRigidDynamic>()->isSleeping())
{
					++NumAwake;
}
	}
	}
}

	return NumAwake;
}
#endif

#endif

#endif
