// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_IMMEDIATE_PHYSX

#include "EngineGlobals.h"
#include "PhysicsPublic.h"
#include "PhysxUserData.h"
#include "Physics/PhysicsInterfaceUtils.h"
#include "PhysScene_PhysX.h"
#include "PhysicsEngine/PhysicsSettingsEnums.h"
#include "GenericPhysicsInterface.h"

struct FConstraintBrokenDelegateData;
class IPhysicsReplicationFactory;

namespace physx
{
	class PxMaterial;
	class PxActor;
	class PxSimulationEventCallback;
}

struct FContactModifyCallback;
struct FPhysXMbpBroadphaseCallback;

#if WITH_PHYSX
/** Interface for the creation of customized simulation event callbacks. */
class ISimEventCallbackFactory
{
public:
	virtual physx::PxSimulationEventCallback* Create(FPhysScene_ImmediatePhysX* PhysScene, int32 SceneType) = 0;
	virtual void Destroy(physx::PxSimulationEventCallback* Callback) = 0;
};
#endif // WITH PHYSX

/** Interface for the creation of contact modify callbacks. */
class IContactModifyCallbackFactory
{
public:
	virtual FContactModifyCallback* Create(FPhysScene_ImmediatePhysX* PhysScene, int32 SceneType) = 0;
	virtual void Destroy(FContactModifyCallback* Callback) = 0;
};

/** Container object for a physics engine 'scene'. */

class FPhysScene_ImmediatePhysX : public FGenericPhysicsInterface
{
    friend struct FPhysicsInterface_ImmediatePhysX;
    friend struct FPhysicsMaterialReference_ImmediatePhysX;
    friend struct FPhysicsConstraintReference_ImmediatePhysX;
    friend struct FPhysicsShapeReference_ImmediatePhysX;
	
    /** Holds shape data*/
	struct FKinematicTarget
	{
	#if WITH_PHYSX
		PxTransform BodyToWorld;
	#endif
		bool bTargetSet;
	
		FKinematicTarget()
			: bTargetSet(false)
		{
		}
	};

	/** Contact pair generated between entities */
	struct FContactPair
	{
		/** Index of the dynamic actor that we generated the contact pair for*/
		int32 DynamicActorDataIndex;

		/** Index of the other actor that we generated the contact pair for. This could be either dynamic or static */
		uint32 OtherActorDataIndex;

		/** Index into the first contact point associated with this pair*/
		uint32 StartContactIndex;

		/** Number of contacts associated with this pair. */
		uint32 NumContacts;

		/** Identifies the pair index from the original contact generation test */
		int32 PairIdx;
	};

	struct D6JointData
	{
		D6JointData(PxD6Joint* Joint);

		/** End solver API */


		PxConstraintInvMassScale	invMassScale;
		PxTransform					c2b[2];

		PxU32					locked;		// bitmap of locked DOFs
		PxU32					limited;	// bitmap of limited DOFs
		PxU32					driving;	// bitmap of active drives (implies driven DOFs not locked)

	
		PxD6Motion::Enum		motion[6];
		PxJointLinearLimit		linearLimit;
		PxJointAngularLimitPair	twistLimit;
		PxJointLimitCone		swingLimit;

		PxD6JointDrive			drive[PxD6Drive::eCOUNT];

		PxTransform				drivePosition;
		PxVec3					driveLinearVelocity;
		PxVec3					driveAngularVelocity;

		// derived quantities

	
											// tan-half and tan-quarter angles

		PxReal					thSwingY;
		PxReal					thSwingZ;
		PxReal					thSwingPad;

		PxReal					tqSwingY;
		PxReal					tqSwingZ;
		PxReal					tqSwingPad;

		PxReal					tqTwistLow;
		PxReal					tqTwistHigh;
		PxReal					tqTwistPad;

		PxReal					linearMinDist;	// linear limit minimum distance to get a good direction

												// projection quantities
		//PxReal					projectionLinearTolerance;
		//PxReal					projectionAngularTolerance;

		FTransform					ActorToBody[2];

		bool HasConstraints() const
		{
			return locked || limited || driving;
		}
	};

	struct FLinearBlockAllocator
	{
		const static int PageBufferSize = 1024 * 64;

		struct FPageStruct
		{
			//We assume FPageStruct is allocated with 16-byte alignment. Do not move Buffer. If we get full support for alignas(16) we could make this better
			uint8 Buffer[PageBufferSize];

			FPageStruct* NextPage;
			FPageStruct* PrevPage;
			int32 SeekPosition;

			FPageStruct()
				: NextPage(nullptr)
				, PrevPage(nullptr)
				, SeekPosition(0)
			{
			}
		};

		FPageStruct* FreePage;
		FPageStruct* FirstPage;

		FLinearBlockAllocator()
		{
			FreePage = AllocPage();
			FirstPage = FreePage;
		}
	
		FPageStruct* AllocPage()
		{
			FPageStruct* ReturnPage = (FPageStruct*)FMemory::Malloc(sizeof(FPageStruct), 16);
			new(ReturnPage) FPageStruct();
		
			FPlatformMisc::TagBuffer("ImmediatePhysicsSim", 0, (const void*)ReturnPage, sizeof(FPageStruct));
			return ReturnPage;
		}

		void ReleasePage(FPageStruct* Page)
		{
			FMemory::Free(Page);
		}

		uint8* Alloc(int32 Bytes)
		{
			check(Bytes < PageBufferSize);	//Page size needs to be increased since we don't allow spillover
			if (Bytes)
			{
				//Assumes 16 byte alignment
				int32 BytesLeft = PageBufferSize - FreePage->SeekPosition;	//don't switch to uint because negative implies we're out of 16 byte aligned space
				if (BytesLeft < Bytes)
				{
					//no space so allocate new page if needed
					if (FreePage->NextPage)
					{
						FreePage = FreePage->NextPage;
					}
					else
					{
						FPageStruct* NewPage = AllocPage();
						NewPage->PrevPage = FreePage;
						FreePage->NextPage = NewPage;
						FreePage = NewPage;
					}
				}

				uint32 ReturnSlot = FreePage->SeekPosition;
				FreePage->SeekPosition = (FreePage->SeekPosition + Bytes + 15)&(~15);

				return &FreePage->Buffer[ReturnSlot];
			}
			else
			{
				return nullptr;
			}
		}

		void Reset()
		{
			for (FPageStruct* Page = FirstPage; Page; Page = Page->NextPage)
			{
				Page->SeekPosition = 0;
			}

			FreePage = FirstPage;
		}

		void Empty()
		{
			FPageStruct* CurrentPage = FirstPage->NextPage;
			while (CurrentPage)
			{
				FPageStruct* OldPage = CurrentPage;
				CurrentPage = CurrentPage->NextPage;
				ReleasePage(OldPage);
			}

			FirstPage->NextPage = nullptr;
			FirstPage->SeekPosition = 0;
			FreePage = FirstPage;
			//Note we do not deallocate FirstPage until the allocator deallocates
		}

		~FLinearBlockAllocator()
		{
			Empty();
			ReleasePage(FirstPage);
		}

	private:
		//Don't copy these around
		FLinearBlockAllocator(const FLinearBlockAllocator& Other);
		const FLinearBlockAllocator& operator=(const FLinearBlockAllocator& Other);
	};

	/** TODO: Use a smarter memory allocator */
	struct FCacheAllocator : public PxCacheAllocator
	{
		FCacheAllocator() : External(0){}
		PxU8* allocateCacheData(const PxU32 ByteSize) override
		{
			return BlockAllocator[External].Alloc(ByteSize);
		}

		void Reset()
		{
	#if PERSISTENT_CONTACT_PAIRS
			External = 1 - External;	//flip buffer so we maintain cache for 1 extra step
	#endif
			BlockAllocator[External].Reset();
		}

		FLinearBlockAllocator BlockAllocator[2];
		int32 External;
	};

	/** TODO: Use a smarter memory allocator */
	class FConstraintAllocator : public PxConstraintAllocator
	{
	public:
		FConstraintAllocator() : External(0){}

		PxU8* reserveConstraintData(const PxU32 ByteSize) override
		{
			return BlockAllocator[External].Alloc(ByteSize);

		}

		PxU8* reserveFrictionData(const PxU32 ByteSize)
		{
			return BlockAllocator[External].Alloc(ByteSize);
		}

		void Reset()
		{
	#if PERSISTENT_CONTACT_PAIRS
			External = 1 - External;	//flip buffer so we maintain cache for 1 extra step
	#endif
			BlockAllocator[External].Reset();
		}

		FLinearBlockAllocator BlockAllocator[2];
		int32 External;
	};

	struct FJoint
	{
        int32 ParentIndex, ChildIndex;
        FTransform JointToParent, JointToChild;

        FJoint(const int32 ParentIndexIn, const int32 ChildIndexIn, const FTransform JointToParentIn, const FTransform JointToChildIn)
            : ParentIndex(ParentIndexIn), ChildIndex(ChildIndexIn), JointToParent(JointToParentIn), JointToChild(JointToChildIn)
        {
        }
	};

	struct FMaterial
	{
		FMaterial()
			: StaticFriction(0.7f)
			, DynamicFriction(0.7f)
			, Restitution(0.3f)
		{
		}

		FMaterial(physx::PxMaterial* InPxMaterial)
		    : StaticFriction(InPxMaterial->getStaticFriction())
		    , DynamicFriction(InPxMaterial->getDynamicFriction())
		    , Restitution(InPxMaterial->getRestitution())
		    , FrictionCombineMode((EFrictionCombineMode::Type)InPxMaterial->getFrictionCombineMode())
		    , RestitutionCombineMode((EFrictionCombineMode::Type)InPxMaterial->getRestitutionCombineMode())
	    {
	    }

        bool operator==(const FMaterial& Other) const
        {
            return StaticFriction == Other.StaticFriction && DynamicFriction == Other.DynamicFriction && Restitution == Other.Restitution;
        }

		float StaticFriction;
		float DynamicFriction;
		float Restitution;

		EFrictionCombineMode::Type FrictionCombineMode;
		EFrictionCombineMode::Type RestitutionCombineMode;
	};

	/** Holds shape data*/
	struct FShape
	{
	#if WITH_PHYSX
		const PxTransform LocalTM;
		const FMaterial Material;
		const PxGeometry* Geometry;
		const PxVec3 BoundsOffset;
		const float BoundsMagnitude;

		FShape(const PxTransform& InLocalTM, const PxVec3& InBoundsOffset, const float InBoundsMagnitude, const PxGeometry* InGeometry, const FMaterial& InMaterial)
			: LocalTM(InLocalTM)
			, Material(InMaterial)
			, Geometry(InGeometry)
			, BoundsOffset(InBoundsOffset)
			, BoundsMagnitude(InBoundsMagnitude)
		{
		}

        FShape& operator=(const FShape& Other)
        {
            const_cast<PxTransform&>(LocalTM) = Other.LocalTM;
            const_cast<FMaterial&>(Material) = Other.Material;
            const_cast<PxGeometry*&>(Geometry) = const_cast<PxGeometry*>(Other.Geometry);
            const_cast<PxVec3&>(BoundsOffset) = Other.BoundsOffset;
            const_cast<float&>(BoundsMagnitude) = Other.BoundsMagnitude;
            return *this;
        }

        bool operator==(const FShape& Other) const
        {
            return LocalTM == Other.LocalTM && Material == Other.Material && Geometry == Other.Geometry && BoundsOffset == Other.BoundsOffset && BoundsMagnitude == Other.BoundsMagnitude;
        }
	#endif
	};

	/** Holds geometry data*/
	struct FActor
	{
		TArray<FShape> Shapes;

	#if WITH_PHYSX
		/** Create geometry data for the entity */
		void CreateGeometry(PxRigidActor* RigidActor, const PxTransform& ActorToBodyTM);
	#endif

		/** Ensures all the geometry data has been properly freed */
		void TerminateGeometry();
	};

	friend struct FPhysicsActorReference_ImmediatePhysX;
	friend struct FPhysicsInterface_ImmediatePhysX;
public:
	ENGINE_API FPhysScene_ImmediatePhysX();
	ENGINE_API ~FPhysScene_ImmediatePhysX();

	void SwapActorData(uint32 Actor1DataIdx, uint32 Actor2DataIdx);
	void ResizeActorData(uint32 ActorDataLen);

	//////////////////////////////////////////////////////////////////////////
	// PhysicsInterface

	//// Actor creation/registration
	//void ReleaseActor(FPhysicsActorReference& InActor);
	void AddActorsToScene_AssumesLocked(const TArray<FPhysicsActorHandle>& InActors);
	void AddAggregateToScene(const FPhysicsAggregateHandle& InAggregate, bool bUseAsyncScene); // #PHYS2 Remove bUseAsyncScene flag somehow

	//Owning world is made private so that any code which depends on setting an owning world can update
    void SetOwningWorld(UWorld* InOwningWorld) { OwningWorld = InOwningWorld; }
	UWorld* GetOwningWorld() { return OwningWorld; }
	const UWorld* GetOwningWorld() const { return OwningWorld; }
	
    FPhysicsReplication* GetPhysicsReplication() { return nullptr; }

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
	TArray<FCollisionNotifyInfo>& GetPendingCollisionNotifies(int32 SceneType) { return PendingCollisionData.PendingCollisionNotifies; }

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

	/** Static factory used to override the simulation contact modify callback from other modules.*/
	ENGINE_API static TSharedPtr<IContactModifyCallbackFactory> ContactModifyCallbackFactory;

	/** Static factory used to override the physics replication manager from other modules. This is useful for custom game logic.
	If not set it defaults to using FPhysicsReplication. */
	ENGINE_API static TSharedPtr<IPhysicsReplicationFactory> PhysicsReplicationFactory;

	//////////////////////////////////////////////////////////////////////////
	// PhysScene_PhysX interface

	/** Utility for looking up the PxScene of the given EPhysicsSceneType associated with this FPhysScene.  SceneType must be in the range [0,PST_MAX). */
    ENGINE_API physx::PxScene* GetPxScene(uint32 SceneType) const { return nullptr; }

#if WITH_APEX
	/** Utility for looking up the ApexScene of the given EPhysicsSceneType associated with this FPhysScene.  SceneType must be in the range [0,PST_MAX). */
    ENGINE_API nvidia::apex::Scene*	GetApexScene(uint32 SceneType) const { return nullptr; }
#endif

	/** Starts cloth Simulation*/
    ENGINE_API void StartAsync() {}

	/** Returns whether an async scene is setup and can be used. This depends on the console variable "p.EnableAsyncScene". */
	ENGINE_API bool HasAsyncScene() const { return false; }

	/** Ensures that the collision tree is built. */
    ENGINE_API void EnsureCollisionTreeIsBuilt(UWorld* World) {}

	/** The number of frames it takes to rebuild the PhysX scene query AABB tree. The bigger the number, the smaller fetchResults takes per frame, but the more the tree deteriorates until a new tree is built */
    void SetPhysXTreeRebuildRate(int32 RebuildRate) { PhysXTreeRebuildRate = RebuildRate; }

	/** Kill the visual debugger */
    ENGINE_API void KillVisualDebugger();

#if WITH_PHYSX
	/** Static factory used to override the simulation event callback from other modules.
	If not set it defaults to using FPhysXSimEventCallback. */
	ENGINE_API static TSharedPtr<ISimEventCallbackFactory> SimEventCallbackFactory;

#endif	// WITH_PHYSX

	//////////////////////////////////////////////////////////////////////////

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPhysScenePreTick, FPhysScene_ImmediatePhysX*, uint32 /*SceneType*/, float /*DeltaSeconds*/);
	FOnPhysScenePreTick OnPhysScenePreTick;

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPhysSceneStep, FPhysScene_ImmediatePhysX*, uint32 /*SceneType*/, float /*DeltaSeconds*/);
	FOnPhysSceneStep OnPhysSceneStep;

	const TArray<immediate::PxRigidBodyData>& GetRigidBodiesData() { return RigidBodiesData; }

private:
	/** Indicates whether the scene is using substepping */
	bool							bSubstepping;

	/** World that owns this physics scene */
	UWorld*							OwningWorld;

	/** DeltaSeconds from UWorld. */
	float										DeltaSeconds;
	/** DeltaSeconds from the WorldSettings. */
	float										MaxPhysicsDeltaTime;
	/** DeltaSeconds used by the last synchronous scene tick.  This may be used for the async scene tick. */
	float										SyncDeltaSeconds;
	/** LineBatcher from UWorld. */
	ULineBatchComponent*						LineBatcher;

	/** Completion events (task) for the physics scenes	(both apex and non-apex). This is a "join" of the above. */
	FGraphEventRef PhysicsSceneCompletion;

#if WITH_PHYSX
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

	/** Sync components in the scene to physics bodies that changed */
	void SyncComponentsToBodies_AssumesLocked(uint32 SceneType);

	/** Call after WaitPhysScene on the synchronous scene to make deferred OnRigidBodyCollision calls.  */
	ENGINE_API void DispatchPhysNotifications_AssumesLocked();

	/** Add any debug lines from the physics scene of the given type to the supplied line batcher. */
	ENGINE_API void AddDebugLines(uint32 SceneType, class ULineBatchComponent* LineBatcherToUse);

	/** Helper function for determining which scene a dyanmic body is in*/
	EPhysicsSceneType SceneType_AssumesLocked(const FBodyInstance* BodyInstance) const;

	/** Set whether we're doing a static load and want to stall, or are during gameplay and want to distribute over many frames */
	void SetIsStaticLoading(bool bStaticLoading);

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
	TMap<FBodyInstance*, ESleepEvent> PendingSleepEvents;
#endif

    TArray<FBodyInstance*> BodyInstances;

	FDelegateHandle PreGarbageCollectDelegateHandle;

	int32 PhysXTreeRebuildRate;

	// TAKEN FROM IMMEDIATEPHYSICS
	/** Entities holding loose data. NOTE: for performance reasons we don't automatically cleanup on destructor (needed for tarray swaps etc...) it's very important that Terminate is called */
	TArray<FActor> Actors;

	TArray<FJoint> Joints;

	/** Workspace memory that we use for per frame allocations */
	FLinearBlockAllocator Workspace;

#if WITH_PHYSX
	/** Low level rigid body data */
	TArray<immediate::PxRigidBodyData> RigidBodiesData;
	
	/** Low level solver bodies data */
	TArray<PxSolverBodyData> SolverBodiesData;
	
	/** Kinematic targets used to implicitly compute the velocity of moving kinematic actors */
	TArray<FKinematicTarget> KinematicTargets;

	TArray<PxVec3> PendingAcceleration;
	TArray<PxVec3> PendingVelocityChange;
	TArray<PxVec3> PendingAngularAcceleration;
	TArray<PxVec3> PendingAngularVelocityChange;

	/** Low level contact points generated for this frame. Points are grouped together by pairs */
	TArray<Gu::ContactPoint> ContactPoints;

	/** Shapes used in the entire simulation. Shapes are sorted in the same order as actors. Note that an actor can have multiple shapes which will be adjacent*/
	struct FShapeSOA
	{
		TArray<PxTransform> LocalTMs;
		TArray<FMaterial> Materials;
		TArray<const PxGeometry*> Geometries;
		TArray<float> Bounds;
		TArray<PxVec3> BoundsOffsets;
		TArray<int32> OwningActors;
#if PERSISTENT_CONTACT_PAIRS
		TArray<FPersistentContactPairData> ContactPairData;
#endif
	} ShapeSOA;
	
	/** Low level solver bodies */
	PxSolverBody* SolverBodies;

	/** Low level constraint descriptors.*/
	TArray<PxSolverConstraintDesc> OrderedDescriptors;
	TArray<PxConstraintBatchHeader> BatchHeaders;

	/** JointData as passed in from physics constraint template */
	TArray<D6JointData> JointData;

	/** When new joints are created we have to update the processing order */
	bool bDirtyJointData;

	PxU32 NumContactHeaders;
	PxU32 NumJointHeaders;
	uint32 NumActiveJoints;
#endif // WITH_PHYSX

	/** Contact pairs generated for this frame */
	TArray<FContactPair> ContactPairs;

	/** Number of dynamic bodies associated with the simulation */
	uint32 NumSimulatedBodies;

	/** Number of dynamic bodies that are actually active */
	uint32 NumActiveSimulatedBodies;

	/** Number of kinematic bodies (dynamic but not simulated) associated with the simulation */
	uint32 NumKinematicBodies;

	/** Total number of simulated shapes in the scene */
	uint32 NumSimulatedShapesWithCollision;

	/** Number of position iterations used by solver */
	uint32 NumPositionIterations;

	/** Number of velocity iterations used by solver */
	uint32 NumVelocityIterations;

	/** Count of how many times we've ticked. Useful for cache invalidation */
	uint32 SimCount;

	/** This cache is used to record which generate contact iteration we can skip. This assumes the iteration order has not changed (add/remove/swap actors must invalidate this) */
	bool bRecreateIterationCache;

	TArray<int32> SkipCollisionCache;	//Holds the iteration count that we should skip due to ignore filtering

	friend struct FContactPointRecorder;

#if WITH_PHYSX
	FCacheAllocator CacheAllocator;
	FConstraintAllocator ConstraintAllocator;
#endif // WITH_PHYSX
};

#endif
