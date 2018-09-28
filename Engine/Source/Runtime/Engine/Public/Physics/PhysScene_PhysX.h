// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EngineGlobals.h"
#include "PhysicsPublic.h"
#include "PhysxUserData.h"
#include "Physics/PhysicsInterfaceTypes.h"

/** Buffers used as scratch space for PhysX to avoid allocations during simulation */
struct FSimulationScratchBuffer
{
	FSimulationScratchBuffer()
		: Buffer(nullptr)
		, BufferSize(0)
	{}

	// The scratch buffer
	uint8* Buffer;

	// Allocated size of the buffer
	int32 BufferSize;
};

#if !WITH_APEIRON && !WITH_IMMEDIATE_PHYSX && !PHYSICS_INTERFACE_LLIMMEDIATE

class FPhysicsReplication;
class IPhysicsReplicationFactory;
struct FConstraintBrokenDelegateData;

namespace physx
{
	class PxActor;
	class PxScene;
	class PxSimulationEventCallback;
}

struct FContactModifyCallback;
struct FPhysXMbpBroadphaseCallback;

#if WITH_APEX
namespace nvidia
{
	namespace apex
	{
		class Scene;
	}
}
#endif // WITH_APEX

#if WITH_PHYSX
/** Interface for the creation of customized simulation event callbacks. */
class ISimEventCallbackFactory
{
public:
	virtual physx::PxSimulationEventCallback* Create(FPhysScene_PhysX* PhysScene, int32 SceneType) = 0;
	virtual void Destroy(physx::PxSimulationEventCallback* Callback) = 0;
};
#endif // WITH PHYSX

/** Interface for the creation of contact modify callbacks. */
class IContactModifyCallbackFactory
{
public:
	virtual FContactModifyCallback* Create(FPhysScene_PhysX* PhysScene, int32 SceneType) = 0;
	virtual void Destroy(FContactModifyCallback* Callback) = 0;
};

/** Interface for the creation of ccd contact modify callbacks. */
class ICCDContactModifyCallbackFactory
{
public:
	virtual FCCDContactModifyCallback* Create(FPhysScene_PhysX* PhysScene, int32 SceneType) = 0;
	virtual void Destroy(FCCDContactModifyCallback* Callback) = 0;
};

/** Container object for a physics engine 'scene'. */

class FPhysScene_PhysX
{
public:
	ENGINE_API FPhysScene_PhysX(const AWorldSettings* Settings = nullptr);
	ENGINE_API ~FPhysScene_PhysX();

	//////////////////////////////////////////////////////////////////////////
	// PhysicsInterface

	//// Actor creation/registration
	//void ReleaseActor(FPhysicsActorReference& InActor);
	void AddActorsToScene_AssumesLocked(const TArray<FPhysicsActorHandle>& InActors);
	void AddAggregateToScene(const FPhysicsAggregateHandle& InAggregate, bool bUseAsyncScene); // #PHYS2 Remove bUseAsyncScene flag somehow

	//Owning world is made private so that any code which depends on setting an owning world can update
	void SetOwningWorld(UWorld* InOwningWorld);
	UWorld* GetOwningWorld() { return OwningWorld; }
	const UWorld* GetOwningWorld() const { return OwningWorld; }

	FPhysicsReplication* GetPhysicsReplication() { return PhysicsReplication; }

	/** Lets the scene update anything related to this BodyInstance as it's now being terminated */
	void RemoveBodyInstanceFromPendingLists_AssumesLocked(FBodyInstance* BodyInstance, int32 SceneType);

	/** Add a custom callback for next step that will be called on every substep */
	void AddCustomPhysics_AssumesLocked(FBodyInstance* BodyInstance, FCalculateCustomPhysics& CalculateCustomPhysics);

	/** Adds a force to a body - We need to go through scene to support substepping */
	void AddForce_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Force, bool bAllowSubstepping, bool bAccelChange);

	/** Adds a force to a body at a specific position - We need to go through scene to support substepping */
	void AddForceAtPosition_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Force, const FVector& Position, bool bAllowSubstepping, bool bIsLocalForce = false);

	/** Adds a radial force to a body - We need to go through scene to support substepping */
	void AddRadialForceToBody_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Origin, const float Radius, const float Strength, const uint8 Falloff, bool bAccelChange, bool bAllowSubstepping);

	/** Clears currently accumulated forces on a specified body instance */
	void ClearForces_AssumesLocked(FBodyInstance* BodyInstance, bool bAllowSubstepping);

	/** Adds torque to a body - We need to go through scene to support substepping */
	void AddTorque_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Torque, bool bAllowSubstepping, bool bAccelChange);

	/** Clears currently accumulated torques on a specified body instance */
	void ClearTorques_AssumesLocked(FBodyInstance* BodyInstance, bool bAllowSubstepping);

	/** Sets a Kinematic actor's target position - We need to do this here to support substepping*/
	void SetKinematicTarget_AssumesLocked(FBodyInstance* BodyInstance, const FTransform& TargetTM, bool bAllowSubstepping);

	/** Gets a Kinematic actor's target position - We need to do this here to support substepping
	* Returns true if kinematic target has been set. If false the OutTM is invalid */
	bool GetKinematicTarget_AssumesLocked(const FBodyInstance* BodyInstance, FTransform& OutTM) const;

	/** Gets the collision disable table */
	const TMap<uint32, TMap<struct FRigidBodyIndexPair, bool> *> & GetCollisionDisableTableLookup()
	{
		return CollisionDisableTableLookup;
	}

	/** Adds to queue of skelmesh we want to add to collision disable table */
	ENGINE_API void DeferredAddCollisionDisableTable(uint32 SkelMeshCompID, TMap<struct FRigidBodyIndexPair, bool> * CollisionDisableTable);

	/** Adds to queue of skelmesh we want to remove from collision disable table */
	ENGINE_API void DeferredRemoveCollisionDisableTable(uint32 SkelMeshCompID);

	/** Pending constraint break events */
	void AddPendingOnConstraintBreak(FConstraintInstance* ConstraintInstance, int32 SceneType);
	/** Pending wake/sleep events */
	void AddPendingSleepingEvent(FBodyInstance* BI, ESleepEvent SleepEventType, int32 SceneType);

	/** Gets the array of collision notifications, pending execution at the end of the physics engine run. */
	TArray<FCollisionNotifyInfo>& GetPendingCollisionNotifies(int32 SceneType) { return PendingCollisionData[SceneType].PendingCollisionNotifies; }

	/** @return Whether physics scene supports scene origin shifting */
	static bool SupportsOriginShifting() { return true; }

	/** Shifts physics scene origin by specified offset */
	void ApplyWorldOffset(FVector InOffset);

	/** Set the gravity and timing of all physics scenes */
	ENGINE_API void SetUpForFrame(const FVector* NewGrav, float InDeltaSeconds = 0.0f, float InMaxPhysicsDeltaTime = 0.0f);

	/** Starts a frame */
	ENGINE_API void StartFrame();

	/** Ends a frame */
	ENGINE_API void EndFrame(ULineBatchComponent* InLineBatcher);

	/** Waits for all physics scenes to complete */
	ENGINE_API void WaitPhysScenes();

	/** returns the completion event for a frame */
	FGraphEventRef GetCompletionEvent()
	{
		return PhysicsSceneCompletion;
	}

	/** Handle exec commands related to scene (PXVIS and APEXVIS) */
	bool HandleExecCommands(const TCHAR* Cmd, FOutputDevice* Ar);

	void ListAwakeRigidBodies(bool bIncludeKinematic);

	ENGINE_API int32 GetNumAwakeBodies();

	/** Static factories used to override the simulation contact modify callback from other modules.*/
	ENGINE_API static TSharedPtr<IContactModifyCallbackFactory> ContactModifyCallbackFactory;
	ENGINE_API static TSharedPtr<ICCDContactModifyCallbackFactory> CCDContactModifyCallbackFactory;

	/** Static factory used to override the physics replication manager from other modules. This is useful for custom game logic.
	If not set it defaults to using FPhysicsReplication. */
	ENGINE_API static TSharedPtr<IPhysicsReplicationFactory> PhysicsReplicationFactory;

	//////////////////////////////////////////////////////////////////////////
	// PhysScene_PhysX interface

	/** Utility for looking up the PxScene of the given EPhysicsSceneType associated with this FPhysScene.  SceneType must be in the range [0,PST_MAX). */
	ENGINE_API physx::PxScene* GetPxScene(uint32 SceneType) const;

#if WITH_APEX
	/** Utility for looking up the ApexScene of the given EPhysicsSceneType associated with this FPhysScene.  SceneType must be in the range [0,PST_MAX). */
	ENGINE_API nvidia::apex::Scene*	GetApexScene(uint32 SceneType) const;
#endif

	/** Starts cloth Simulation*/
	ENGINE_API void StartAsync();

	/** Returns whether an async scene is setup and can be used. This depends on the console variable "p.EnableAsyncScene". */
	ENGINE_API bool HasAsyncScene() const { return bAsyncSceneEnabled; }

	/** The number of frames it takes to rebuild the PhysX scene query AABB tree. The bigger the number, the smaller fetchResults takes per frame, but the more the tree deteriorates until a new tree is built */
	void SetPhysXTreeRebuildRate(int32 RebuildRate);

	/** Ensures that the collision tree is built. */
	ENGINE_API void EnsureCollisionTreeIsBuilt(UWorld* World);

	/** Kill the visual debugger */
	ENGINE_API void KillVisualDebugger();

#if WITH_PHYSX
	/** Static factory used to override the simulation event callback from other modules.
	If not set it defaults to using FPhysXSimEventCallback. */
	ENGINE_API static TSharedPtr<ISimEventCallbackFactory> SimEventCallbackFactory;

#endif	// WITH_PHYSX

	//////////////////////////////////////////////////////////////////////////

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPhysScenePreTick, FPhysScene_PhysX*, uint32 /*SceneType*/, float /*DeltaSeconds*/);
	FOnPhysScenePreTick OnPhysScenePreTick;

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPhysSceneStep, FPhysScene_PhysX*, uint32 /*SceneType*/, float /*DeltaSeconds*/);
	FOnPhysSceneStep OnPhysSceneStep;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPhysScenePostTick, FPhysScene*, uint32 /*SceneType*/);
	FOnPhysScenePostTick OnPhysScenePostTick;

private:
	/** Indicates whether the async scene is enabled or not. */
	bool							bAsyncSceneEnabled;

	/** Indicates whether the scene is using substepping */
	bool							bSubstepping;

	/** Indicates whether the scene is using substepping */
	bool							bSubsteppingAsync;

	/** Stores the number of valid scenes we are working with. This will be PST_MAX or PST_Async,
	depending on whether the async scene is enabled or not*/
	uint32							NumPhysScenes;

	/** World that owns this physics scene */
	UWorld*							OwningWorld;

	/** Replication manager that updates physics bodies towards replicated physics state */
	FPhysicsReplication*			PhysicsReplication;

#if WITH_APEX
	nvidia::apex::Scene* PhysXScenes[PST_MAX];
#else // #if WITH_APEX
	PxScene* PhysXScenes[PST_MAX];
#endif // #if WITH_APEX

	/** Whether or not the given scene is between its execute and sync point. */
	bool							bPhysXSceneExecuting[PST_MAX];

	/** Frame time, weighted with current frame time. */
	float							AveragedFrameTime[PST_MAX];

	/**
	* Weight for averaged frame time.  Value should be in the range [0.0f, 1.0f].
	* Weight = 0.0f => no averaging; current frame time always used.
	* Weight = 1.0f => current frame time ignored; initial value of AveragedFrameTime[i] is always used.
	*/
	float							FrameTimeSmoothingFactor[PST_MAX];

	/** DeltaSeconds from UWorld. */
	float										DeltaSeconds;
	/** DeltaSeconds from the WorldSettings. */
	float										MaxPhysicsDeltaTime;
	/** DeltaSeconds used by the last synchronous scene tick.  This may be used for the async scene tick. */
	float										SyncDeltaSeconds;
	/** LineBatcher from UWorld. */
	ULineBatchComponent*						LineBatcher;

	/** Completion event (not tasks) for the physics scenes these are fired by the physics system when it is done; prerequisites for the below */
	FGraphEventRef PhysicsSubsceneCompletion[PST_MAX];
	/** Completion events (not tasks) for the frame lagged physics scenes these are fired by the physics system when it is done; prerequisites for the below */
	FGraphEventRef FrameLaggedPhysicsSubsceneCompletion[PST_MAX];
	/** Completion events (task) for the physics scenes	(both apex and non-apex). This is a "join" of the above. */
	FGraphEventRef PhysicsSceneCompletion;

	// Data for scene scratch buffers, these will be allocated once on FPhysScene construction and used
	// for the calls to PxScene::simulate to save it calling into the OS to allocate during simulation
	FSimulationScratchBuffer SimScratchBuffers[PST_MAX];

	// Boundary value for PhysX scratch buffers (currently PhysX requires the buffer length be a multiple of 16K)
	static const int32 SimScratchBufferBoundary = 16 * 1024;

#if WITH_PHYSX

	bool bIsSceneSimulating[PST_MAX];

	/** Dispatcher for CPU tasks */
	class PxCpuDispatcher*			CPUDispatcher[PST_MAX];
	/** Simulation event callback object */
	physx::PxSimulationEventCallback*			SimEventCallback[PST_MAX];
	FContactModifyCallback*			ContactModifyCallback[PST_MAX];
	FCCDContactModifyCallback*			CCDContactModifyCallback[PST_MAX];
	FPhysXMbpBroadphaseCallback* MbpBroadphaseCallbacks[PST_MAX];

	struct FPendingCollisionData
	{
		/** Array of collision notifications, pending execution at the end of the physics engine run. */
		TArray<FCollisionNotifyInfo>	PendingCollisionNotifies;
	};

	FPendingCollisionData PendingCollisionData[PST_MAX];

	struct FPendingConstraintData
	{
		/** Array of constraint broken notifications, pending execution at the end of the physics engine run. */
		TArray<FConstraintBrokenDelegateData> PendingConstraintBroken;
	};

	FPendingConstraintData PendingConstraintData[PST_MAX];

#endif	// WITH_PHYSX

	/** Start simulation on the physics scene of the given type */
	ENGINE_API void TickPhysScene(uint32 SceneType, FGraphEventRef& InOutCompletionEvent);

	/** Fetches results, fires events, and adds debug lines */
	void ProcessPhysScene(uint32 SceneType);

	/** Sync components in the scene to physics bodies that changed */
	void SyncComponentsToBodies_AssumesLocked(uint32 SceneType);

	/** Call after WaitPhysScene on the synchronous scene to make deferred OnRigidBodyCollision calls.  */
	ENGINE_API void DispatchPhysNotifications_AssumesLocked();

	/** Add any debug lines from the physics scene of the given type to the supplied line batcher. */
	ENGINE_API void AddDebugLines(uint32 SceneType, class ULineBatchComponent* LineBatcherToUse);

	void AddActorsToPhysXScene_AssumesLocked(int32 SceneType, const TArray<FPhysicsActorHandle>& InActors);

	/** @return Whether physics scene is using substepping */
	bool IsSubstepping(uint32 SceneType) const;

	/** Initialize a scene of the given type.  Must only be called once for each scene type. */
	void InitPhysScene(uint32 SceneType, const AWorldSettings* Settings = nullptr);

	/** Terminate a scene of the given type.  Must only be called once for each scene type. */
	void TermPhysScene(uint32 SceneType);

	/** Called when all subscenes of a given scene are complete, calls  ProcessPhysScene*/
	void SceneCompletionTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent, EPhysicsSceneType SceneType);

	/** Helper function for determining which scene a dyanmic body is in*/
	EPhysicsSceneType SceneType_AssumesLocked(const FBodyInstance* BodyInstance) const;

	/** Task created from TickPhysScene so we can substep without blocking */
	bool SubstepSimulation(uint32 SceneType, FGraphEventRef& InOutCompletionEvent);

	/** Set whether we're doing a static load and want to stall, or are during gameplay and want to distribute over many frames */
	void SetIsStaticLoading(bool bStaticLoading);

	/** The number of frames it takes to rebuild the PhysX scene query AABB tree. The bigger the number, the smaller fetchResults takes per frame, but the more the tree deteriorates until a new tree is built */
	void SetPhysXTreeRebuildRateImp(int32 RebuildRate);

#if WITH_PHYSX
	/** User data wrapper passed to physx */
	FPhysxUserData PhysxUserData;

#endif

	class FPhysSubstepTask * PhysSubSteppers[PST_MAX];

	struct FPendingCollisionDisableTable
	{
		uint32 SkelMeshCompID;
		TMap<struct FRigidBodyIndexPair, bool>* CollisionDisableTable;
	};

	/** Updates CollisionDisableTableLookup with the deferred insertion and deletion */
	void FlushDeferredCollisionDisableTableQueue();

	bool ExecPxVis(uint32 SceneType, const TCHAR* Cmd, FOutputDevice* Ar);

	bool ExecApexVis(uint32 SceneType, const TCHAR* Cmd, FOutputDevice* Ar);


	/** Queue of deferred collision table insertion and deletion */
	TArray<FPendingCollisionDisableTable> DeferredCollisionDisableTableQueue;

	/** Map from SkeletalMeshComponent UniqueID to a pointer to the collision disable table inside its PhysicsAsset */
	TMap< uint32, TMap<struct FRigidBodyIndexPair, bool>* >		CollisionDisableTableLookup;

#if WITH_PHYSX
	TMap<FBodyInstance*, ESleepEvent> PendingSleepEvents[PST_MAX];
#endif

	FDelegateHandle PreGarbageCollectDelegateHandle;

	int32 PhysXTreeRebuildRate;
};

#endif