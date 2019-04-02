// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EngineGlobals.h"
#include "PhysicsPublic.h"
#include "PhysxUserData.h"
#include "Physics/PhysicsInterfaceTypes.h"

class ISQAccelerator;
class FSQAccelerator;
class FSQAcceleratorUnion;
class FSQAcceleratorEntry;

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

#if !WITH_CHAOS && !WITH_IMMEDIATE_PHYSX && !PHYSICS_INTERFACE_LLIMMEDIATE

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
	virtual physx::PxSimulationEventCallback* Create(FPhysScene_PhysX* PhysScene) = 0;
	virtual void Destroy(physx::PxSimulationEventCallback* Callback) = 0;
};
#endif // WITH PHYSX

/** Interface for the creation of contact modify callbacks. */
class IContactModifyCallbackFactory
{
public:
	virtual FContactModifyCallback* Create(FPhysScene_PhysX* PhysScene) = 0;
	virtual void Destroy(FContactModifyCallback* Callback) = 0;
};

/** Interface for the creation of ccd contact modify callbacks. */
class ICCDContactModifyCallbackFactory
{
public:
	virtual FCCDContactModifyCallback* Create(FPhysScene_PhysX* PhysScene) = 0;
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
	void AddAggregateToScene(const FPhysicsAggregateHandle& InAggregate);

	//Owning world is made private so that any code which depends on setting an owning world can update
	void SetOwningWorld(UWorld* InOwningWorld);
	UWorld* GetOwningWorld() { return OwningWorld; }
	const UWorld* GetOwningWorld() const { return OwningWorld; }

	FPhysicsReplication* GetPhysicsReplication() { return PhysicsReplication; }

	/** Lets the scene update anything related to this BodyInstance as it's now being terminated */
	void RemoveBodyInstanceFromPendingLists_AssumesLocked(FBodyInstance* BodyInstance);

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

	/** Add this SkeletalMeshComponent to the list needing kinematic bodies updated before simulating physics */
	void MarkForPreSimKinematicUpdate(USkeletalMeshComponent* InSkelComp, ETeleportType InTeleport, bool bNeedsSkinning);

	/** Remove this SkeletalMeshComponent from set needing kinematic update before simulating physics*/
	void ClearPreSimKinematicUpdate(USkeletalMeshComponent* InSkelComp);

	/** Pending constraint break events */
	void AddPendingOnConstraintBreak(FConstraintInstance* ConstraintInstance);
	/** Pending wake/sleep events */
	void AddPendingSleepingEvent(FBodyInstance* BI, ESleepEvent SleepEventType);

	/** Gets the array of collision notifications, pending execution at the end of the physics engine run. */
	TArray<FCollisionNotifyInfo>& GetPendingCollisionNotifies() { return PendingCollisionData.PendingCollisionNotifies; }

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

	/** Get the PxScene */
	ENGINE_API physx::PxScene* GetPxScene() const;

	ENGINE_API ISQAccelerator* GetSQAccelerator() const;

	ENGINE_API FSQAcceleratorUnion* GetSQAcceleratorUnion() const;

#if WITH_APEX
	/** Get the ApexScene*/
	ENGINE_API nvidia::apex::Scene*	GetApexScene() const { return PhysXScene; }
#endif
	
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

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPhysScenePreTick, FPhysScene_PhysX*, float /*DeltaSeconds*/);
	FOnPhysScenePreTick OnPhysScenePreTick;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPhysSceneStep, FPhysScene_PhysX*, float /*DeltaSeconds*/);
	FOnPhysSceneStep OnPhysSceneStep;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPhysScenePostTick, FPhysScene*);
	FOnPhysScenePostTick OnPhysScenePostTick;

private:
	/** Indicates whether the scene is using substepping */
	bool							bSubstepping;

	/** World that owns this physics scene */
	UWorld*							OwningWorld;

	/** Replication manager that updates physics bodies towards replicated physics state */
	FPhysicsReplication*			PhysicsReplication;

#if WITH_CUSTOM_SQ_STRUCTURE
	TMap<physx::PxRigidActor*, FSQAcceleratorEntry*> RigidActorToSQEntries;
#endif

#if WITH_APEX
	nvidia::apex::Scene* PhysXScene;
#else // #if WITH_APEX
	PxScene* PhysXScene;
#endif // #if WITH_APEX

	/** Whether or not the given scene is between its execute and sync point. */
	bool							bPhysXSceneExecuting;

	/** Frame time, weighted with current frame time. */
	float							AveragedFrameTime;

	/**
	* Weight for averaged frame time.  Value should be in the range [0.0f, 1.0f].
	* Weight = 0.0f => no averaging; current frame time always used.
	* Weight = 1.0f => current frame time ignored; initial value of AveragedFrameTime[i] is always used.
	*/
	float							FrameTimeSmoothingFactor;

	/** DeltaSeconds from UWorld. */
	float										DeltaSeconds;
	/** DeltaSeconds from the WorldSettings. */
	float										MaxPhysicsDeltaTime;

	/** LineBatcher from UWorld. */
	ULineBatchComponent*						LineBatcher;

	/** Completion event (not tasks) for the physics scenes these are fired by the physics system when it is done; prerequisites for the below */
	FGraphEventRef PhysicsSubsceneCompletion;
	/** Completion events (not tasks) for the frame lagged physics scenes these are fired by the physics system when it is done; prerequisites for the below */
	FGraphEventRef FrameLaggedPhysicsSubsceneCompletion;
	/** Completion events (task) for the physics scenes	(both apex and non-apex). This is a "join" of the above. */
	FGraphEventRef PhysicsSceneCompletion;

	// Data for scene scratch buffers, these will be allocated once on FPhysScene construction and used
	// for the calls to PxScene::simulate to save it calling into the OS to allocate during simulation
	FSimulationScratchBuffer SimScratchBuffer;

	// Boundary value for PhysX scratch buffers (currently PhysX requires the buffer length be a multiple of 16K)
	static const int32 SimScratchBufferBoundary = 16 * 1024;

#if WITH_CUSTOM_SQ_STRUCTURE
	class FSQAcceleratorUnion* SQAcceleratorUnion;
	class FSQAccelerator* SQAccelerator;
#endif

#if WITH_PHYSX

	bool bIsSceneSimulating;

	/** Dispatcher for CPU tasks */
	class PxCpuDispatcher*			CPUDispatcher;
	/** Simulation event callback object */
	physx::PxSimulationEventCallback*			SimEventCallback;
	FContactModifyCallback*			ContactModifyCallback;
	FCCDContactModifyCallback*			CCDContactModifyCallback;
	FPhysXMbpBroadphaseCallback* MbpBroadphaseCallback;

	struct FPendingCollisionData
	{
		/** Array of collision notifications, pending execution at the end of the physics engine run. */
		TArray<FCollisionNotifyInfo>	PendingCollisionNotifies;
	};

	FPendingCollisionData PendingCollisionData;

	struct FPendingConstraintData
	{
		/** Array of constraint broken notifications, pending execution at the end of the physics engine run. */
		TArray<FConstraintBrokenDelegateData> PendingConstraintBroken;
	};

	FPendingConstraintData PendingConstraintData;

#endif	// WITH_PHYSX

	/** Start simulation on the physics scene of the given type */
	ENGINE_API void TickPhysScene(FGraphEventRef& InOutCompletionEvent);

	/** Fetches results, fires events, and adds debug lines */
	void ProcessPhysScene();

	/** Sync components in the scene to physics bodies that changed */
	void SyncComponentsToBodies_AssumesLocked();

	/** Call after WaitPhysScene on the synchronous scene to make deferred OnRigidBodyCollision calls.  */
	ENGINE_API void DispatchPhysNotifications_AssumesLocked();

	/** Add any debug lines from the physics scene of the given type to the supplied line batcher. */
	ENGINE_API void AddDebugLines(class ULineBatchComponent* LineBatcherToUse);

	void AddActorsToPhysXScene_AssumesLocked(const TArray<FPhysicsActorHandle>& InActors);

	/** @return Whether physics scene is using substepping */
	bool IsSubstepping() const;

	/** Initialize a scene of the given type.  Must only be called once for each scene type. */
	void InitPhysScene(const AWorldSettings* Settings = nullptr);

	/** Terminate a scene of the given type.  Must only be called once for each scene type. */
	void TermPhysScene();

	/** Called when all subscenes of a given scene are complete, calls  ProcessPhysScene*/
	void SceneCompletionTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);
	
	/** Process kinematic updates on any deferred skeletal meshes */
	void UpdateKinematicsOnDeferredSkelMeshes();

	/** Task created from TickPhysScene so we can substep without blocking */
	bool SubstepSimulation(FGraphEventRef& InOutCompletionEvent);

	/** Set whether we're doing a static load and want to stall, or are during gameplay and want to distribute over many frames */
	void SetIsStaticLoading(bool bStaticLoading);

	/** The number of frames it takes to rebuild the PhysX scene query AABB tree. The bigger the number, the smaller fetchResults takes per frame, but the more the tree deteriorates until a new tree is built */
	void SetPhysXTreeRebuildRateImp(int32 RebuildRate);

#if WITH_PHYSX
	/** User data wrapper passed to physx */
	FPhysxUserData PhysxUserData;

#endif

	class FPhysSubstepTask * PhysSubStepper;

	struct FPendingCollisionDisableTable
	{
		uint32 SkelMeshCompID;
		TMap<struct FRigidBodyIndexPair, bool>* CollisionDisableTable;
	};

	/** Updates CollisionDisableTableLookup with the deferred insertion and deletion */
	void FlushDeferredCollisionDisableTableQueue();

	bool ExecPxVis(const TCHAR* Cmd, FOutputDevice* Ar);

	bool ExecApexVis(const TCHAR* Cmd, FOutputDevice* Ar);


	/** Queue of deferred collision table insertion and deletion */
	TArray<FPendingCollisionDisableTable> DeferredCollisionDisableTableQueue;

	/** Map from SkeletalMeshComponent UniqueID to a pointer to the collision disable table inside its PhysicsAsset */
	TMap< uint32, TMap<struct FRigidBodyIndexPair, bool>* >		CollisionDisableTableLookup;

#if WITH_PHYSX
	TMap<FBodyInstance*, ESleepEvent> PendingSleepEvents;
#endif

	/** Information about how to perform kinematic update before physics */
	struct FDeferredKinematicUpdateInfo
	{
		/** Whether to teleport physics bodies or not */
		ETeleportType	TeleportType;
		/** Whether to update skinning info */
		bool			bNeedsSkinning;
	};

	/** Map of SkeletalMeshComponents that need their bone transforms sent to the physics engine before simulation. */
	TArray<TPair<USkeletalMeshComponent*, FDeferredKinematicUpdateInfo>>	DeferredKinematicUpdateSkelMeshes;

	FDelegateHandle PreGarbageCollectDelegateHandle;

	int32 PhysXTreeRebuildRate;
};

#endif