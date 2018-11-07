// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#if PHYSICS_INTERFACE_LLIMMEDIATE

#include "Physics/PhysicsInterfaceTypes.h"
#include "PhysicsEngine/ConstraintTypes.h"
#include "PhysicsEngine/ConstraintDrives.h"

#include "Physics/PhysScene.h"
#include "PhysScene_LLImmediate.h"
#include "PhysicsReplication.h"
#include "PhysxUserData.h"
#include "Physics/GenericPhysicsInterface.h"

class AWorldSettings;

// We still use some px types as transport/storage types
namespace physx
{
	class PxMassProperties;
	class PxGeometry;

	// Required to build actors
	class PxRigidBody;
	class PxRigidDynamic;
	class PxRigidActor;
	class  PxD6Joint;
}

// Immediate physics types
namespace ImmediatePhysics
{
	struct FActorHandle;
	struct FJointHandle;
}

struct ENGINE_API FPhysicsActorHandle_LLImmediate
{
	friend class FPhysInterface_LLImmediate;

	FPhysicsActorHandle_LLImmediate()
		: OwningScene(nullptr)
		, RefIndex(INDEX_NONE)
		, ComparisonId(0)
	{

	}

	bool IsValid() const;

	// #PHYS2 TODO, move to == operator like shape handle
	bool Equals(const FPhysicsActorHandle_LLImmediate& InOther) const;

	ImmediatePhysics::FActor* GetActor() const;
	immediate::PxRigidBodyData* GetActorData() const;

	bool IsStatic() const;

	friend uint32 GetTypeHash(const FPhysicsActorHandle_LLImmediate& InHandle)
	{
		return GetTypeHash(reinterpret_cast<uintptr_t>(InHandle.OwningScene)) + (uint32)InHandle.RefIndex + InHandle.ComparisonId;
	}

private:

	// Pointer back to the scene that created us, not necessarily required but should be set when the actor is actually in a scene
	FPhysScene * OwningScene;

	// Reference index into sparse actor ref array in the interface
	int32 RefIndex;

	// Comparison ID to differentiate between reused handle slots
	uint32 ComparisonId;
};

struct ENGINE_API FPhysicsShapeHandle_LLImmediate
{
	friend class FPhysInterface_LLImmediate;

	FPhysicsShapeHandle_LLImmediate()
		//: ShapeIndex(INDEX_NONE)
		: InnerShape(nullptr)
	{

	}

	bool IsValid() const
	{
		//return OwningActorHandle.IsValid() && ShapeIndex != INDEX_NONE;
		return InnerShape;
	}

	friend uint32 GetTypeHash(const FPhysicsShapeHandle_LLImmediate& InHandle)
	{
		//return GetTypeHash(InHandle.OwningActorHandle) + (uint32)InHandle.ShapeIndex;
		return GetTypeHash(reinterpret_cast<uintptr_t>(InHandle.InnerShape));
	}

	friend bool operator==(const FPhysicsShapeHandle_LLImmediate& InHandleA, const FPhysicsShapeHandle_LLImmediate& InHandleB)
	{
		//return InHandleA.OwningActorHandle.Equals(InHandleB.OwningActorHandle) && InHandleA.ShapeIndex == InHandleB.ShapeIndex;
		return InHandleA.InnerShape == InHandleB.InnerShape;
	}

	ImmediatePhysics::FShape* InnerShape;

private:

	//// The actor that contains the shape we point at
	//FPhysicsActorHandle_LLImmediate OwningActorHandle;
	//
	//// The index inside the actor shape array for this shape
	//int32 ShapeIndex;

};

struct ENGINE_API FPhysicsConstraintHandle_LLImmediate
{
	bool IsValid() const { return false; }
};

struct ENGINE_API FPhysicsAggregateHandle_LLImmediate
{
	bool IsValid() const
	{
		return false;
	}
};

struct ENGINE_API FPhysicsMaterialHandle_LLImmediate
{
	friend class FPhysInterface_LLImmediate;

	bool IsValid() const
	{
		return Material != nullptr;
	}

private:
	ImmediatePhysics::FMaterial* Material;
};

struct ENGINE_API FPhysicsGeometryCollection_LLImmediate
{
	FPhysicsGeometryCollection_LLImmediate() {}
	
	bool IsValid() const
	{
		return false;
	}

	ECollisionShapeType GetType() const
	{
		return ECollisionShapeType::None;
	}

	physx::PxBoxGeometry TempDummyGeom;

	physx::PxGeometry& GetGeometry() const 
	{
		return (physx::PxGeometry&)TempDummyGeom;
	}

	bool GetBoxGeometry(physx::PxBoxGeometry& OutGeom) const
	{
		return false;
	}

	bool GetSphereGeometry(physx::PxSphereGeometry& OutGeom) const
	{
		return false;
	}

	bool GetCapsuleGeometry(physx::PxCapsuleGeometry& OutGeom) const
	{
		return false;
	}

	bool GetConvexGeometry(physx::PxConvexMeshGeometry& OutGeom) const
	{
		return false;
	}

	bool GetTriMeshGeometry(physx::PxTriangleMeshGeometry& OutGeom) const
	{
		return false;
	}
};

// #PHYS2 TODO, generalise, shouldn't use physx callback structure here
class ENGINE_API FSimEventCallbackFactory
{
public:
	physx::PxSimulationEventCallback* Create(FPhysInterface_LLImmediate* PhysScene, int32 SceneType) { return nullptr; }
	void Destroy(physx::PxSimulationEventCallback* Callback) {}
};

class ENGINE_API IContactModifyCallbackFactory
{
public:
	virtual FContactModifyCallback* Create(FPhysInterface_LLImmediate* PhysScene, int32 SceneType) = 0;
	virtual void Destroy(FContactModifyCallback* Callback) = 0;
};

class ENGINE_API FContactModifyCallbackFactory : public IContactModifyCallbackFactory
{
public:
	FContactModifyCallback * Create(FPhysInterface_LLImmediate* PhysScene, int32 SceneType) { return nullptr; }
	void Destroy(FContactModifyCallback* Callback) {}
};

class ENGINE_API FPhysicsReplicationFactory
{
public:
	FPhysicsReplication * Create(FPhysInterface_LLImmediate* OwningPhysScene) { return nullptr; }
	void Destroy(FPhysicsReplication* PhysicsReplication) {}
};

struct ENGINE_API FPhysicsCommand_LLImmediate
{
	// Executes with appropriate read locking, return true if execution took place (actor was valid)
	static bool ExecuteRead(const FPhysicsActorHandle_LLImmediate& InActorReference, TFunctionRef<void(const FPhysicsActorHandle_LLImmediate& Actor)> InCallable);
	static bool ExecuteRead(USkeletalMeshComponent* InMeshComponent, TFunctionRef<void()> InCallable);
	static bool ExecuteRead(const FPhysicsActorHandle_LLImmediate& InActorReferenceA, const FPhysicsActorHandle_LLImmediate& InActorReferenceB, TFunctionRef<void(const FPhysicsActorHandle_LLImmediate& ActorA, const FPhysicsActorHandle_LLImmediate& ActorB)> InCallable);
	static bool ExecuteRead(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, TFunctionRef<void(const FPhysicsConstraintHandle_LLImmediate& Constraint)> InCallable);
	static bool ExecuteRead(FPhysScene* InScene, TFunctionRef<void()> InCallable);

	// Executes with appropriate write locking, return true if execution took place (actor was valid)
	static bool ExecuteWrite(const FPhysicsActorHandle_LLImmediate& InActorReference, TFunctionRef<void(const FPhysicsActorHandle_LLImmediate& Actor)> InCallable);
	static bool ExecuteWrite(USkeletalMeshComponent* InMeshComponent, TFunctionRef<void()> InCallable);
	static bool ExecuteWrite(const FPhysicsActorHandle_LLImmediate& InActorReferenceA, const FPhysicsActorHandle_LLImmediate& InActorReferenceB, TFunctionRef<void(const FPhysicsActorHandle_LLImmediate& ActorA, const FPhysicsActorHandle_LLImmediate& ActorB)> InCallable);
	static bool ExecuteWrite(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, TFunctionRef<void(const FPhysicsConstraintHandle_LLImmediate& Constraint)> InCallable);
	static bool ExecuteWrite(FPhysScene* InScene, TFunctionRef<void()> InCallable);

	// Executes function on a shape, handling shared shapes
	static void ExecuteShapeWrite(FBodyInstance* InInstance, FPhysicsShapeHandle_LLImmediate& InShape, TFunctionRef<void(const FPhysicsShapeHandle_LLImmediate& InShape)> InCallable);
};

// Descriptor for a pending actor that we wish to add to the scene
struct FPendingActor
{
	FPendingActor()
		: bValid(true)
	{

	}

	// Validity flag so we can remove pending actors but not have to handle removing from the pending
	// list until we update it all at once on the next tick
	bool bValid;

	// Actor shape storage
	ImmediatePhysics::FActor Actor;

	// Actor parameters
	ImmediatePhysics::FActorData ActorData;

	// Handle to an interface reference to update when this actor is added to the simulation
	FPhysicsActorHandle_LLImmediate InterfaceHandle;
};


class ENGINE_API FPhysInterface_LLImmediate : public FGenericPhysicsInterface
{
public:

	// Incremented on actor creation to give unique IDs to each actor
	// that combined with the index of the actor allow us to compare handles
	uint32 ActorIdCounter;

	struct FActorRef
	{
		// Pointer to some pending data for sim addition, only valid if the actor is waiting to be
		// added to a simulation
		int32 PendingActorIndex;

		// Only valid when the actor is actually in a simulation, this will not be the case immediately
		// after requesting an actor is created.
		ImmediatePhysics::FActorHandle* SimHandle;

		// ID to use on comparison from an external handle, will cause a mismatch when a slot is reused
		uint32 ComparisonId;

		// Userdata that the engine sets on an actor
		void* UserData;
	};

	TSparseArray<FActorRef> ActorRefs;

	void QueueNewActor(const FActorCreationParams& Params, FPhysicsActorHandle_LLImmediate& OutHandle);
	void QueueReleaseActor(FPhysicsActorHandle& InHandle);

	const FActorRef* GetActorRef(const FPhysicsActorHandle& InHandle) const;
	FActorRef* GetActorRef(const FPhysicsActorHandle& InHandle);

	FPhysInterface_LLImmediate(const AWorldSettings* InWorldSettings = nullptr);

	// Callback functions from low level scene
	void Callback_CreateActors(TArray<ImmediatePhysics::FActorHandle*>& ActorArray);

	//////////////////////////////////////////////////////////////////////////

	static FPhysicsActorHandle CreateActor(const FActorCreationParams& Params);
	static void ReleaseActor(FPhysicsActorHandle& InHandle, FPhysScene* InScene = nullptr, bool bNeverDeferRelease = false);
	//
	static FPhysicsAggregateHandle_LLImmediate CreateAggregate(int32 MaxBodies);
	static void ReleaseAggregate(FPhysicsAggregateHandle_LLImmediate& InAggregate);
	static int32 GetNumActorsInAggregate(const FPhysicsAggregateHandle_LLImmediate& InAggregate);
	static void AddActorToAggregate_AssumesLocked(const FPhysicsAggregateHandle_LLImmediate& InAggregate, const FPhysicsActorHandle_LLImmediate& InActor);

	// Material interface functions
	// @todo(mlentine): How do we set material on the solver?
	static FPhysicsMaterialHandle_LLImmediate CreateMaterial(const UPhysicalMaterial* InMaterial);
	static void ReleaseMaterial(FPhysicsMaterialHandle_LLImmediate& InHandle);
	static void UpdateMaterial(const FPhysicsMaterialHandle_LLImmediate& InHandle, UPhysicalMaterial* InMaterial);
	static void SetUserData(const FPhysicsMaterialHandle_LLImmediate& InHandle, void* InUserData);

	// Actor interface functions
	template<typename AllocatorType>
	static int32 GetAllShapes_AssumedLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, TArray<FPhysicsShapeHandle, AllocatorType>& OutShapes, EPhysicsSceneType InSceneType = PST_MAX);
	static void GetNumShapes(const FPhysicsActorHandle& InHandle, int32& OutNumSyncShapes, int32& OutNumAsyncShapes);

	static void ReleaseShape(FPhysicsShapeHandle& InShape);

	static void AttachShape(const FPhysicsActorHandle& InActor, const FPhysicsShapeHandle& InNewShape);
	static void AttachShape(const FPhysicsActorHandle& InActor, const FPhysicsShapeHandle& InNewShape, EPhysicsSceneType SceneType);
	static void DetachShape(const FPhysicsActorHandle& InActor, FPhysicsShapeHandle& InShape, bool bWakeTouching = true);

	static void SetActorUserData_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, FPhysxUserData* InUserData);

	static bool IsRigidBody(const FPhysicsActorHandle_LLImmediate& InActorReference);
	static bool IsDynamic(const FPhysicsActorHandle_LLImmediate& InActorReference);
	static bool IsStatic(const FPhysicsActorHandle_LLImmediate& InActorReference);
	static bool IsKinematic_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference);
	static bool IsSleeping(const FPhysicsActorHandle_LLImmediate& InActorReference);
	static bool IsCcdEnabled(const FPhysicsActorHandle_LLImmediate& InActorReference);
	// @todo(mlentine): We don't have a notion of sync vs async and are a bit of both. Does this work?
	static bool HasSyncSceneData(const FPhysicsActorHandle_LLImmediate& InHandle) { return true; }
	static bool HasAsyncSceneData(const FPhysicsActorHandle_LLImmediate& InHandle) { return false; }
	static bool IsInScene(const FPhysicsActorHandle_LLImmediate& InActorReference);
	static bool CanSimulate_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference);
	static float GetMass_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference);

	static void SetSendsSleepNotifies_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, bool bSendSleepNotifies);
	static void PutToSleep_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference);
	static void WakeUp_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference);

	static void SetIsKinematic_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, bool bIsKinematic);
	static void SetCcdEnabled_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, bool bIsCcdEnabled);

	static FTransform GetGlobalPose_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference);
	static void SetGlobalPose_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, const FTransform& InNewPose, bool bAutoWake = true);

	static FTransform GetTransform_AssumesLocked(const FPhysicsActorHandle& InRef, bool bForceGlobalPose = false);

	static bool HasKinematicTarget_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference);
	static FTransform GetKinematicTarget_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference);
	static void SetKinematicTarget_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, const FTransform& InNewTarget);

	static FVector GetLinearVelocity_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference);
	static void SetLinearVelocity_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, const FVector& InNewVelocity, bool bAutoWake = true);

	static FVector GetAngularVelocity_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference);
	static void SetAngularVelocity_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, const FVector& InNewVelocity, bool bAutoWake = true);
	static float GetMaxAngularVelocity_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference);
	static void SetMaxAngularVelocity_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, float InMaxAngularVelocity);

	static float GetMaxDepenetrationVelocity_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference);
	static void SetMaxDepenetrationVelocity_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, float InMaxDepenetrationVelocity);

	static FVector GetWorldVelocityAtPoint_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, const FVector& InPoint);

	static FTransform GetComTransform_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference);

	static FVector GetLocalInertiaTensor_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference);
	static FBox GetBounds_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference);

	static void SetLinearDamping_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, float InDamping);
	static void SetAngularDamping_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, float InDamping);

	static void AddForce_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, const FVector& InForce);
	static void AddTorque_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, const FVector& InTorque);
	static void AddForceMassIndependent_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, const FVector& InForce);
	static void AddTorqueMassIndependent_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, const FVector& InTorque);
	static void AddImpulseAtLocation_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, const FVector& InImpulse, const FVector& InLocation);
	static void AddRadialImpulse_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, const FVector& InOrigin, float InRadius, float InStrength, ERadialImpulseFalloff InFalloff, bool bInVelChange);

	static bool IsGravityEnabled_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference);
	static void SetGravityEnabled_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, bool bEnabled);

	static float GetSleepEnergyThreshold_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference);
	static void SetSleepEnergyThreshold_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InActorReference, float InEnergyThreshold);

	static void SetMass_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InHandle, float InMass);
	static void SetMassSpaceInertiaTensor_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InHandle, const FVector& InTensor);
	static void SetComLocalPose_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InHandle, const FTransform& InComLocalPose);

	static float GetStabilizationEnergyThreshold_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InHandle);
	static void SetStabilizationEnergyThreshold_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InHandle, float InThreshold);
	static uint32 GetSolverPositionIterationCount_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InHandle);
	static void SetSolverPositionIterationCount_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InHandle, uint32 InSolverIterationCount);
	static uint32 GetSolverVelocityIterationCount_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InHandle);
	static void SetSolverVelocityIterationCount_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InHandle, uint32 InSolverIterationCount);
	static float GetWakeCounter_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InHandle);
	static void SetWakeCounter_AssumesLocked(const FPhysicsActorHandle_LLImmediate& InHandle, float InWakeCounter);

	static SIZE_T GetResourceSizeEx(const FPhysicsActorHandle_LLImmediate& InActorRef);

	static FPhysicsConstraintHandle_LLImmediate CreateConstraint(const FPhysicsActorHandle_LLImmediate& InActorRef1, const FPhysicsActorHandle_LLImmediate& InActorRef2, const FTransform& InLocalFrame1, const FTransform& InLocalFrame2);
	static void SetConstraintUserData(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, void* InUserData);
	static void ReleaseConstraint(FPhysicsConstraintHandle_LLImmediate& InConstraintRef);

	static FTransform GetLocalPose(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, EConstraintFrame::Type InFrame);
	static FTransform GetGlobalPose(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, EConstraintFrame::Type InFrame);
	static FVector GetLocation(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef);
	static void GetForce(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, FVector& OutLinForce, FVector& OutAngForce);
	static void GetDriveLinearVelocity(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, FVector& OutLinVelocity);
	static void GetDriveAngularVelocity(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, FVector& OutAngVelocity);

	static float GetCurrentSwing1(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef);
	static float GetCurrentSwing2(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef);
	static float GetCurrentTwist(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef);

	static void SetCanVisualize(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, bool bInCanVisualize);
	static void SetCollisionEnabled(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, bool bInCollisionEnabled);
	static void SetProjectionEnabled_AssumesLocked(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, bool bInProjectionEnabled, float InLinearTolerance = 0.0f, float InAngularToleranceDegrees = 0.0f);
	static void SetParentDominates_AssumesLocked(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, bool bInParentDominates);
	static void SetBreakForces_AssumesLocked(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, float InLinearBreakForce, float InAngularBreakForce);
	static void SetLocalPose(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, const FTransform& InPose, EConstraintFrame::Type InFrame);

	static void SetLinearMotionLimitType_AssumesLocked(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, PhysicsInterfaceTypes::ELimitAxis InAxis, ELinearConstraintMotion InMotion);
	static void SetAngularMotionLimitType_AssumesLocked(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, PhysicsInterfaceTypes::ELimitAxis InAxis, EAngularConstraintMotion InMotion);

	static void UpdateLinearLimitParams_AssumesLocked(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, float InLimit, float InAverageMass, const FLinearConstraint& InParams);
	static void UpdateConeLimitParams_AssumesLocked(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, float InAverageMass, const FConeConstraint& InParams);
	static void UpdateTwistLimitParams_AssumesLocked(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, float InAverageMass, const FTwistConstraint& InParams);
	static void UpdateLinearDrive_AssumesLocked(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, const FLinearDriveConstraint& InDriveParams);
	static void UpdateAngularDrive_AssumesLocked(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, const FAngularDriveConstraint& InDriveParams);
	static void UpdateDriveTarget_AssumesLocked(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, const FLinearDriveConstraint& InLinDrive, const FAngularDriveConstraint& InAngDrive);
	static void SetDrivePosition(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, const FVector& InPosition);
	static void SetDriveOrientation(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, const FQuat& InOrientation);
	static void SetDriveLinearVelocity(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, const FVector& InLinVelocity);
	static void SetDriveAngularVelocity(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, const FVector& InAngVelocity);

	static void SetTwistLimit(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, float InLowerLimit, float InUpperLimit, float InContactDistance);
	static void SetSwingLimit(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, float InYLimit, float InZLimit, float InContactDistance);
	static void SetLinearLimit(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, float InLimit);

	static bool IsBroken(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef);

	static bool ExecuteOnUnbrokenConstraintReadOnly(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, TFunctionRef<void(const FPhysicsConstraintHandle_LLImmediate&)> Func);
	static bool ExecuteOnUnbrokenConstraintReadWrite(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, TFunctionRef<void(const FPhysicsConstraintHandle_LLImmediate&)> Func);

	/////////////////////////////////////////////

	// Interface needed for cmd
	static bool ExecuteRead(const FPhysicsActorHandle_LLImmediate& InActorReference, TFunctionRef<void(const FPhysicsActorHandle_LLImmediate& Actor)> InCallable)
	{
		InCallable(InActorReference);
		return true;
	}

	static bool ExecuteRead(USkeletalMeshComponent* InMeshComponent, TFunctionRef<void()> InCallable)
	{
		InCallable();
		return true;
	}

	static bool ExecuteRead(const FPhysicsActorHandle_LLImmediate& InActorReferenceA, const FPhysicsActorHandle_LLImmediate& InActorReferenceB, TFunctionRef<void(const FPhysicsActorHandle_LLImmediate& ActorA, const FPhysicsActorHandle_LLImmediate& ActorB)> InCallable)
	{
		InCallable(InActorReferenceA, InActorReferenceB);
		return true;
	}

	static bool ExecuteRead(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, TFunctionRef<void(const FPhysicsConstraintHandle_LLImmediate& Constraint)> InCallable)
	{
		InCallable(InConstraintRef);
		return true;
	}

	static bool ExecuteRead(FPhysScene* InScene, TFunctionRef<void()> InCallable)
	{
		InCallable();
		return true;
	}

	static bool ExecuteWrite(const FPhysicsActorHandle_LLImmediate& InActorReference, TFunctionRef<void(const FPhysicsActorHandle_LLImmediate& Actor)> InCallable)
	{
		InCallable(InActorReference);
		return true;
	}

	static bool ExecuteWrite(USkeletalMeshComponent* InMeshComponent, TFunctionRef<void()> InCallable)
	{
		InCallable();
		return true;
	}

	static bool ExecuteWrite(const FPhysicsActorHandle_LLImmediate& InActorReferenceA, const FPhysicsActorHandle_LLImmediate& InActorReferenceB, TFunctionRef<void(const FPhysicsActorHandle_LLImmediate& ActorA, const FPhysicsActorHandle_LLImmediate& ActorB)> InCallable)
	{
		InCallable(InActorReferenceA, InActorReferenceB);
		return true;
	}

	static bool ExecuteWrite(const FPhysicsConstraintHandle_LLImmediate& InConstraintRef, TFunctionRef<void(const FPhysicsConstraintHandle_LLImmediate& Constraint)> InCallable)
	{
		InCallable(InConstraintRef);
		return true;
	}

	static bool ExecuteWrite(FPhysScene* InScene, TFunctionRef<void()> InCallable)
	{
		InCallable();
		return true;
	}

	static void ExecuteShapeWrite(FBodyInstance* InInstance, FPhysicsShapeHandle& InShape, TFunctionRef<void(const FPhysicsShapeHandle& InShape)> InCallable)
	{
		InCallable(InShape);
	}

	// Scene query interface functions

	static bool RaycastTest(const UWorld* World, const FVector Start, const FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)
	{
		return false;
	}
	static bool RaycastSingle(const UWorld* World, struct FHitResult& OutHit, const FVector Start, const FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)
	{
		return false;
	}
	static bool RaycastMulti(const UWorld* World, TArray<struct FHitResult>& OutHits, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)
	{
		return false;
	}

	static bool GeomOverlapBlockingTest(const UWorld* World, const FCollisionShape& CollisionShape, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)
	{
		return false;
	}
	static bool GeomOverlapAnyTest(const UWorld* World, const FCollisionShape& CollisionShape, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)
	{
		return false;
	}
	static bool GeomOverlapMulti(const UWorld* World, const FCollisionShape& CollisionShape, const FVector& Pos, const FQuat& Rot, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)
	{
		return false;
	}

	// GEOM SWEEP

	static bool GeomSweepTest(const UWorld* World, const FCollisionShape& CollisionShape, const FQuat& Rot, FVector Start, FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)
	{
		return false;
	}
	static bool GeomSweepSingle(const UWorld* World, const FCollisionShape& CollisionShape, const FQuat& Rot, FHitResult& OutHit, FVector Start, FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)
	{
		return false;
	}
	static bool GeomSweepMulti(const UWorld* World, const FCollisionShape& CollisionShape, const FQuat& Rot, TArray<FHitResult>& OutHits, FVector Start, FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)
	{
		return false;
	}

	template<typename GeomType>
	static bool GeomSweepMulti(const UWorld* World, const GeomType& InGeom, const FQuat& InGeomRot, TArray<FHitResult>& OutHits, FVector Start, FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)
	{
		return false;
	}
	template<typename GeomType>
	static bool GeomOverlapMulti(const UWorld* World, const GeomType& InGeom, const FVector& InPosition, const FQuat& InRotation, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams)
	{
		return false;
	}

	// Misc

	static bool ExecPhysCommands(const TCHAR* Cmd, FOutputDevice* Ar, UWorld* InWorld);

	static FPhysScene* GetCurrentScene(const FPhysicsActorHandle& InActorReference);

	static void CalculateMassPropertiesFromShapeCollection(physx::PxMassProperties& OutProperties, const TArray<FPhysicsShapeHandle>& InShapes, float InDensityKGPerCM);

	//static const UEPhysics::TPBDRigidParticles<float, 3>& GetParticlesAndIndex(const FPhysicsActorHandle_LLImmediate& InActorReference, uint32& Index);
	//static const TArray<UEPhysics::Vector<int32, 2>>& GetConstraintArrayAndIndex(const FPhysicsConstraintHandle_LLImmediate& InActorReference, uint32& Index);

	// Shape interface functions
	static FPhysicsShapeHandle CreateShape(physx::PxGeometry* InGeom, bool bSimulation = true, bool bQuery = true, UPhysicalMaterial* InSimpleMaterial = nullptr, TArray<UPhysicalMaterial*>* InComplexMaterials = nullptr, bool bShared = false);

	static void AddGeometry(const FPhysicsActorHandle& InActor, const FGeometryAddParams& InParams, TArray<FPhysicsShapeHandle>* OutOptShapes = nullptr);
	static FPhysicsShapeHandle CloneShape(const FPhysicsShapeHandle& InShape);

	static bool IsSimulationShape(const FPhysicsShapeHandle& InShape);
	static bool IsQueryShape(const FPhysicsShapeHandle& InShape);
	static bool IsShapeType(const FPhysicsShapeHandle& InShape, ECollisionShapeType InType);
	// @todo(mlentine): We don't keep track of what is shared but anything can be
	static bool IsShared(const FPhysicsShapeHandle& InShape) { return true; }
	static ECollisionShapeType GetShapeType(const FPhysicsShapeHandle& InShape);
	static FPhysicsGeometryCollection GetGeometryCollection(const FPhysicsShapeHandle& InShape);
	static FTransform GetLocalTransform(const FPhysicsShapeHandle& InShape);
	static void* GetUserData(const FPhysicsShapeHandle& InShape);

	// Trace functions for testing specific geometry (not against a world)
	static bool LineTrace_Geom(FHitResult& OutHit, const FBodyInstance* InInstance, const FVector& InStart, const FVector& InEnd, bool bTraceComplex, bool bExtractPhysMaterial = false);
	static bool Sweep_Geom(FHitResult& OutHit, const FBodyInstance* InInstance, const FVector& InStart, const FVector& InEnd, const FQuat& InShapeRotation, const FCollisionShape& InShape, bool bSweepComplex);
	static bool Overlap_Geom(const FBodyInstance* InBodyInstance, const FPhysicsGeometryCollection& InGeometry, const FTransform& InShapeTransform, FMTDResult* OutOptResult = nullptr);
	static bool Overlap_Geom(const FBodyInstance* InBodyInstance, const FCollisionShape& InCollisionShape, const FQuat& InShapeRotation, const FTransform& InShapeTransform, FMTDResult* OutOptResult = nullptr);
	static bool GetSquaredDistanceToBody(const FBodyInstance* InInstance, const FVector& InPoint, float& OutDistanceSquared, FVector* OutOptPointOnBody = nullptr);

	// @todo(mlentine): Which of these do we need to support?
	// Set the mask filter of a shape, which is an extra level of filtering during collision detection / query for extra channels like "Blue Team" and "Red Team"
	static void SetMaskFilter(const FPhysicsShapeHandle& InShape, FMaskFilter InFilter) {}
	static void SetSimulationFilter(const FPhysicsShapeHandle& InShape, const FCollisionFilterData& InFilter) {}
	static void SetQueryFilter(const FPhysicsShapeHandle& InShape, const FCollisionFilterData& InFilter) {}
	static void SetIsSimulationShape(const FPhysicsShapeHandle& InShape, bool bIsSimShape) {}
	static void SetIsQueryShape(const FPhysicsShapeHandle& InShape, bool bIsQueryShape) {}
	static void SetUserData(const FPhysicsShapeHandle& InShape, void* InUserData);
	static void SetGeometry(const FPhysicsShapeHandle& InShape, physx::PxGeometry& InGeom) {}
	static void SetLocalTransform(const FPhysicsShapeHandle& InShape, const FTransform& NewLocalTransform);
	static void SetMaterials(const FPhysicsShapeHandle& InShape, const TArrayView<UPhysicalMaterial*>InMaterials) {}

	// Scene
	void AddActorsToScene_AssumesLocked(const TArray<FPhysicsActorHandle>& InActors);
	void AddAggregateToScene(const FPhysicsAggregateHandle& InAggregate, bool bUseAsyncScene) {}

	void SetOwningWorld(UWorld* InOwningWorld) { OwningWorld = InOwningWorld; }
	UWorld* GetOwningWorld() { return OwningWorld; }
	const UWorld* GetOwningWorld() const { return OwningWorld; }

	FPhysicsReplication* GetPhysicsReplication() { return nullptr; }
	void RemoveBodyInstanceFromPendingLists_AssumesLocked(FBodyInstance* BodyInstance, int32 SceneType);
	void AddCustomPhysics_AssumesLocked(FBodyInstance* BodyInstance, FCalculateCustomPhysics& CalculateCustomPhysics)
	{
		CalculateCustomPhysics.ExecuteIfBound(DeltaTime, BodyInstance);
	}
	void AddForce_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Force, bool bAllowSubstepping, bool bAccelChange);
	void AddForceAtPosition_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Force, const FVector& Position, bool bAllowSubstepping, bool bIsLocalForce = false);
	void AddRadialForceToBody_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Origin, const float Radius, const float Strength, const uint8 Falloff, bool bAccelChange, bool bAllowSubstepping);
	void ClearForces_AssumesLocked(FBodyInstance* BodyInstance, bool bAllowSubstepping);
	void AddTorque_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Torque, bool bAllowSubstepping, bool bAccelChange);
	void ClearTorques_AssumesLocked(FBodyInstance* BodyInstance, bool bAllowSubstepping);
	void SetKinematicTarget_AssumesLocked(FBodyInstance* BodyInstance, const FTransform& TargetTM, bool bAllowSubstepping);
	//bool GetKinematicTarget_AssumesLocked(const FBodyInstance* BodyInstance, FTransform& OutTM) const;

	void DeferredAddCollisionDisableTable(uint32 SkelMeshCompID, TMap<struct FRigidBodyIndexPair, bool> * CollisionDisableTable) {}
	void DeferredRemoveCollisionDisableTable(uint32 SkelMeshCompID) {}

	void AddPendingOnConstraintBreak(FConstraintInstance* ConstraintInstance, int32 SceneType) {}
	void AddPendingSleepingEvent(FBodyInstance* BI, ESleepEvent SleepEventType, int32 SceneType) {}

	TArray<FCollisionNotifyInfo>& GetPendingCollisionNotifies(int32 SceneType) 
	{
		return PendingNotifies;
	}

	static bool SupportsOriginShifting() { return false; }
	void ApplyWorldOffset(FVector InOffset) { check(InOffset.Size() == 0); }
	void SetUpForFrame(const FVector* NewGrav, float InDeltaSeconds = 0.0f, float InMaxPhysicsDeltaTime = 0.0f)
	{
		//SetGravity(*NewGrav);
		DeltaTime = InDeltaSeconds;
	}
	void StartFrame()
	{
		Scene.Tick(DeltaTime);
		//SyncBodies();
	}
	void EndFrame(ULineBatchComponent* InLineBatcher);
	void WaitPhysScenes() {}

	FGraphEventRef GetCompletionEvent()
	{
		return FGraphEventRef();
	}

	bool HandleExecCommands(const TCHAR* Cmd, FOutputDevice* Ar); // { return false; }
	void ListAwakeRigidBodies(bool bIncludeKinematic);
	int32 GetNumAwakeBodies() const;
	static TSharedPtr<IContactModifyCallbackFactory> ContactModifyCallbackFactory;
	static TSharedPtr<FPhysicsReplicationFactory> PhysicsReplicationFactory;

	//ENGINE_API physx::PxScene* GetPxScene(uint32 SceneType) const { return nullptr; }
	//ENGINE_API nvidia::apex::Scene* GetApexScene(uint32 SceneType) const { return nullptr; }

	void StartAsync() {}
	bool HasAsyncScene() const { return false; }
	//void SetPhysXTreeRebuildRate(int32 RebuildRate) {}
	void EnsureCollisionTreeIsBuilt(UWorld* World) {}
	void KillVisualDebugger() {}

	static TSharedPtr<FSimEventCallbackFactory> SimEventCallbackFactory;

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPhysScenePreTick, FPhysInterface_LLImmediate*, uint32 /*SceneType*/, float /*DeltaSeconds*/);
	FOnPhysScenePreTick OnPhysScenePreTick;
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPhysSceneStep, FPhysInterface_LLImmediate*, uint32 /*SceneType*/, float /*DeltaSeconds*/);
	FOnPhysSceneStep OnPhysSceneStep;

	bool ExecPxVis(uint32 SceneType, const TCHAR* Cmd, FOutputDevice* Ar);
	bool ExecApexVis(uint32 SceneType, const TCHAR* Cmd, FOutputDevice* Ar);

	TArray<FPendingActor>& GetPendingActors()
	{
		return PendingActors;
	};

	const TArray<FPendingActor>& GetPendingActors() const
	{
		return PendingActors;
	};

private:

	// Copy of the current body state #PHYS2 should we double buffer this in the scene and template read/write?
	TArray<immediate::PxRigidBodyData> RigidBodiesData;

	// Base low-level physics scene for this interface
	FPhysScene_Base<FPhysScene_LLImmediate> Scene;

	// Delta for the upcoming frame
	float DeltaTime;

	// The world that owns this physics interface
	UWorld* OwningWorld;

	// Pending collisions to dispatch
	TArray<FCollisionNotifyInfo> PendingNotifies;

	// Actors waiting to be added to the scene
	TArray<FPendingActor> PendingActors;
	TArray<ImmediatePhysics::FActorHandle*> PendingRemoveActors;

	// High-level handles maintained by this interface
	TArray<FPhysicsActorHandle_LLImmediate*> ActorHandles;
};

#endif
