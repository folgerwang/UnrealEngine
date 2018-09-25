// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#if !WITH_APEIRON && !WITH_IMMEDIATE_PHYSX && !PHYSICS_INTERFACE_LLIMMEDIATE

#include "EngineGlobals.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"
#include "PhysicsEngine/ConstraintTypes.h"
#include "PhysicsInterfaceTypes.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "GenericPhysicsInterface.h"

struct FLinearDriveConstraint;
struct FAngularDriveConstraint;
struct FConstraintDrive;
struct FPhysxUserData;
struct FKAggregateGeom;

struct FCollisionQueryParams;
struct FCollisionResponseParams;
struct FCollisionObjectQueryParams;
struct FCollisionShape;
class UWorld;
struct FBodyInstance;
class UPhysicalMaterial;

namespace physx
{
	class PxRigidActor;
	class PxRigidStatic;
	class PxRigidDynamic;
	class PxRigidBody;
	class PxAggregate;
	class PxShape;
	class PxD6Joint;
	class PxGeometry;
	class PxQuat;
	class PxTransform;
	class PxScene;
	class PxBoxGeometry;
	class PxCapsuleGeometry;
	class PxSphereGeometry;
	class PxConvexMeshGeometry;
	class PxTriangleMeshGeometry;
	class PxMassProperties;
	class PxGeometryHolder;
	class PxMaterial;
}

struct ENGINE_API FPhysicsActorHandle_PhysX
{
	FPhysicsActorHandle_PhysX();

	bool IsValid() const;
	bool Equals(const FPhysicsActorHandle_PhysX& Other) const;

	physx::PxRigidActor* SyncActor;
	physx::PxRigidActor* AsyncActor;

private:

	struct FPxActorContainer
	{
		physx::PxRigidActor* Actor;
		physx::PxRigidStatic* Static;
		physx::PxRigidDynamic* Dynamic;
		physx::PxRigidBody* Body;
	};
};

struct ENGINE_API FPhysicsConstraintHandle_PhysX
{
	FPhysicsConstraintHandle_PhysX();

	bool IsValid() const;
	bool Equals(const FPhysicsConstraintHandle_PhysX& Other) const;

	physx::PxD6Joint* ConstraintData;
};

struct ENGINE_API FPhysicsAggregateHandle_PhysX
{
	FPhysicsAggregateHandle_PhysX();

	bool IsValid() const;

	physx::PxAggregate* Aggregate;
};

struct ENGINE_API FPhysicsShapeHandle_PhysX
{
	FPhysicsShapeHandle_PhysX();
	explicit FPhysicsShapeHandle_PhysX(physx::PxShape* InShape);

	bool IsValid() const;

	physx::PxShape* Shape;

	friend uint32 GetTypeHash(const FPhysicsShapeHandle_PhysX& InHandle)
	{
		return GetTypeHash(InHandle.Shape);
	}

	friend bool operator==(const FPhysicsShapeHandle_PhysX& InHandleA, const FPhysicsShapeHandle_PhysX& InHandleB)
	{
		return InHandleA.Shape == InHandleB.Shape;
	}
};

/**
 * This object is essentially a one-stop container for any geometry a shape can have and is necessary because of the
 * PxGeometryHolder type. This needs to have a longer lifetime than any usage of the geometry types it returns.
 * Because we want to have that inside the interface this container is required to manage the lifetime of the holder
 */
struct ENGINE_API FPhysicsGeometryCollection_PhysX
{
	// Delete default constructor, want only construction by interface (private constructor below)
	FPhysicsGeometryCollection_PhysX() = delete;
	// No copying or assignment, move construction only, these are defaulted in the source file as they need
	// to be able to delete physx::PxGeometryHolder which is incomplete here
	FPhysicsGeometryCollection_PhysX(const FPhysicsGeometryCollection_PhysX& Copy) = delete;
	FPhysicsGeometryCollection_PhysX& operator=(const FPhysicsGeometryCollection_PhysX& Copy) = delete;
	FPhysicsGeometryCollection_PhysX(FPhysicsGeometryCollection_PhysX&& Steal);
	FPhysicsGeometryCollection_PhysX& operator=(FPhysicsGeometryCollection_PhysX&& Steal);
	~FPhysicsGeometryCollection_PhysX();

	ECollisionShapeType GetType() const;
	physx::PxGeometry& GetGeometry() const;
	bool GetBoxGeometry(physx::PxBoxGeometry& OutGeom) const;
	bool GetSphereGeometry(physx::PxSphereGeometry& OutGeom) const;
	bool GetCapsuleGeometry(physx::PxCapsuleGeometry& OutGeom) const;
	bool GetConvexGeometry(physx::PxConvexMeshGeometry& OutGeom) const;
	bool GetTriMeshGeometry(physx::PxTriangleMeshGeometry& OutGeom) const;

private:
	friend struct FPhysicsInterface_PhysX;
	explicit FPhysicsGeometryCollection_PhysX(const FPhysicsShapeHandle_PhysX& InShape);

	FPhysicsShapeHandle_PhysX ShapeRef;

	// PhysX geom holder, needs to exist longer than the uses of any geometry it returns
	TUniquePtr<physx::PxGeometryHolder> GeomHolder;
};

/**
 * Wrapper for internal PhysX materials
 */
struct ENGINE_API FPhysicsMaterialHandle_PhysX
{
	FPhysicsMaterialHandle_PhysX();
	explicit FPhysicsMaterialHandle_PhysX(physx::PxMaterial* InMaterial);

	bool IsValid() const;

	physx::PxMaterial* Material;
};

/**
* API to access the physics interface. All calls to FPhysicsInterface functions should be inside an Execute* callable.
* This is to ensure correct lock semantics and command buffering if the specific API supports deferred commands.
*/
struct ENGINE_API FPhysicsCommand_PhysX
{
	// Executes with appropriate read locking, return true if execution took place (actor was valid)
	static bool ExecuteRead(const FPhysicsActorHandle_PhysX& InHandle, TFunctionRef<void(const FPhysicsActorHandle_PhysX& Actor)> InCallable);
	static bool ExecuteRead(USkeletalMeshComponent* InMeshComponent, TFunctionRef<void()> InCallable);
	static bool ExecuteRead(const FPhysicsActorHandle_PhysX& InHandleA, const FPhysicsActorHandle_PhysX& InHandleB, TFunctionRef<void(const FPhysicsActorHandle_PhysX& ActorA, const FPhysicsActorHandle_PhysX& ActorB)> InCallable);
	static bool ExecuteRead(const FPhysicsConstraintHandle_PhysX& InHandle, TFunctionRef<void(const FPhysicsConstraintHandle_PhysX& Constraint)> InCallable);
	static bool ExecuteRead(FPhysScene_PhysX* InScene, TFunctionRef<void()> InCallable);

	// Executes with appropriate write locking, return true if execution took place (actor was valid)
	static bool ExecuteWrite(const FPhysicsActorHandle_PhysX& InHandle, TFunctionRef<void(const FPhysicsActorHandle_PhysX& Actor)> InCallable);
	static bool ExecuteWrite(USkeletalMeshComponent* InMeshComponent, TFunctionRef<void()> InCallable);
	static bool ExecuteWrite(const FPhysicsActorHandle_PhysX& InHandleA, const FPhysicsActorHandle_PhysX& InHandleB, TFunctionRef<void(const FPhysicsActorHandle_PhysX& ActorA, const FPhysicsActorHandle_PhysX& ActorB)> InCallable);
	static bool ExecuteWrite(const FPhysicsConstraintHandle_PhysX& InHandle, TFunctionRef<void(const FPhysicsConstraintHandle_PhysX& Constraint)> InCallable);
	static bool ExecuteWrite(FPhysScene_PhysX* InScene, TFunctionRef<void()> InCallable);

	// Executes function on a shape, handling shared shapes
	static void ExecuteShapeWrite(FBodyInstance* InInstance, FPhysicsShapeHandle_PhysX& InShape, TFunctionRef<void(const FPhysicsShapeHandle_PhysX& InShape)> InCallable);
};

struct ENGINE_API FPhysicsInterface_PhysX : public FGenericPhysicsInterface
{
	// PhysX Only functions, not related to wider physics interface
	// To be used only in code that handles PhysX
	static physx::PxRigidActor* GetPxRigidActorFromScene_AssumesLocked(const FPhysicsActorHandle_PhysX& InActorHandle, int32 SceneType = -1);
	static physx::PxRigidActor* GetPxRigidActor_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle);
	static physx::PxRigidDynamic* GetPxRigidDynamic_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle);
	static physx::PxRigidBody* GetPxRigidBody_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle);
	static const FBodyInstance* ShapeToOriginalBodyInstance(const FBodyInstance* InCurrentInstance, const physx::PxShape* InShape);
	//////////////////////////////////////////////////////////////////////////

	// Aggregate interface functions
	static FPhysicsAggregateHandle_PhysX CreateAggregate(int32 MaxBodies);
	static void ReleaseAggregate(FPhysicsAggregateHandle_PhysX& InAggregate);
	static int32 GetNumActorsInAggregate(const FPhysicsAggregateHandle_PhysX& InAggregate);
	static void AddActorToAggregate_AssumesLocked(const FPhysicsAggregateHandle_PhysX& InAggregate, const FPhysicsActorHandle_PhysX& InActor);

	//////////////////////////////////////////////////////////////////////////
	// Shape interface functions
	//////////////////////////////////////////////////////////////////////////
	static FPhysicsShapeHandle_PhysX CreateShape(physx::PxGeometry* InGeom, bool bSimulation = true, bool bQuery = true, UPhysicalMaterial* InSimpleMaterial = nullptr, TArray<UPhysicalMaterial*>* InComplexMaterials = nullptr, bool bShared = false);
	
	static void AddGeometry(const FPhysicsActorHandle& InActor, const FGeometryAddParams& InParams, TArray<FPhysicsShapeHandle_PhysX>* OutOptShapes = nullptr);
	static FPhysicsShapeHandle_PhysX CloneShape(const FPhysicsShapeHandle_PhysX& InShape);

	static FCollisionFilterData GetSimulationFilter(const FPhysicsShapeHandle_PhysX& InShape);
	static FCollisionFilterData GetQueryFilter(const FPhysicsShapeHandle_PhysX& InShape);
	static bool IsSimulationShape(const FPhysicsShapeHandle_PhysX& InShape);
	static bool IsQueryShape(const FPhysicsShapeHandle_PhysX& InShape);
	static bool IsShapeType(const FPhysicsShapeHandle_PhysX& InShape, ECollisionShapeType InType);
	static bool IsShared(const FPhysicsShapeHandle_PhysX& InShape);
	static ECollisionShapeType GetShapeType(const FPhysicsShapeHandle_PhysX& InShape);
	static FPhysicsGeometryCollection_PhysX GetGeometryCollection(const FPhysicsShapeHandle_PhysX& InShape);
	static FTransform GetLocalTransform(const FPhysicsShapeHandle_PhysX& InShape);
	static FTransform GetTransform(const FPhysicsShapeHandle_PhysX& InShape);
	static void* GetUserData(const FPhysicsShapeHandle_PhysX& InShape);

	// Set the mask filter of a shape, which is an extra level of filtering during collision detection / query for extra channels like "Blue Team" and "Red Team"
	static void SetMaskFilter(const FPhysicsShapeHandle_PhysX& InShape, FMaskFilter InFilter);
	static void SetSimulationFilter(const FPhysicsShapeHandle_PhysX& InShape, const FCollisionFilterData& InFilter);
	static void SetQueryFilter(const FPhysicsShapeHandle_PhysX& InShape, const FCollisionFilterData& InFilter);
	static void SetIsSimulationShape(const FPhysicsShapeHandle_PhysX& InShape, bool bIsSimShape);
	static void SetIsQueryShape(const FPhysicsShapeHandle_PhysX& InShape, bool bIsQueryShape);
	static void SetUserData(const FPhysicsShapeHandle_PhysX& InShape, void* InUserData);
	static void SetGeometry(const FPhysicsShapeHandle_PhysX& InShape, physx::PxGeometry& InGeom);
	static void SetLocalTransform(const FPhysicsShapeHandle_PhysX& InShape, const FTransform& NewLocalTransform);
	static void SetMaterials(const FPhysicsShapeHandle_PhysX& InShape, const TArrayView<UPhysicalMaterial*>InMaterials);

	//////////////////////////////////////////////////////////////////////////
	// Material interface functions
	//////////////////////////////////////////////////////////////////////////
	static FPhysicsMaterialHandle CreateMaterial(const UPhysicalMaterial* InMaterial);
	static void ReleaseMaterial(FPhysicsMaterialHandle_PhysX& InHandle);
	static void UpdateMaterial(const FPhysicsMaterialHandle_PhysX& InHandle, UPhysicalMaterial* InMaterial);
	static void SetUserData(const FPhysicsMaterialHandle_PhysX& InHandle, void* InUserData);

	//////////////////////////////////////////////////////////////////////////
	// Actor interface functions
	//////////////////////////////////////////////////////////////////////////

	// #PHYS2 - These should be on the scene, but immediate mode stops us for now, eventually that should spawn its own minimal IM scene and these should move
	static FPhysicsActorHandle CreateActor(const FActorCreationParams& Params);
	static void ReleaseActor(FPhysicsActorHandle_PhysX& InHandle, FPhysScene* InScene = nullptr, bool bNeverDeferRelease = false);
	//////////////////////////////////////////////////////////////////////////

	template<typename AllocatorType>
	static int32 GetAllShapes_AssumedLocked(const FPhysicsActorHandle_PhysX& InHandle, TArray<FPhysicsShapeHandle_PhysX, AllocatorType>& OutShapes, EPhysicsSceneType InSceneType = PST_MAX);
	static void GetNumShapes(const FPhysicsActorHandle_PhysX& InHandle, int32& OutNumSyncShapes, int32& OutNumAsyncShapes);

	static void ReleaseShape(const FPhysicsShapeHandle_PhysX& InShape);

	static void AttachShape(const FPhysicsActorHandle_PhysX& InActor, const FPhysicsShapeHandle_PhysX& InNewShape);
	static void AttachShape(const FPhysicsActorHandle_PhysX& InActor, const FPhysicsShapeHandle_PhysX& InNewShape, EPhysicsSceneType SceneType);
	static void DetachShape(const FPhysicsActorHandle_PhysX& InActor, FPhysicsShapeHandle_PhysX& InShape, bool bWakeTouching = true);

	static void SetActorUserData_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, FPhysxUserData* InUserData);

	static bool IsRigidBody(const FPhysicsActorHandle_PhysX& InHandle);
	static bool IsDynamic(const FPhysicsActorHandle_PhysX& InHandle);
	static bool IsStatic(const FPhysicsActorHandle_PhysX& InHandle);
	static bool IsKinematic_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle);
	static bool IsSleeping(const FPhysicsActorHandle_PhysX& InHandle);
	static bool IsCcdEnabled(const FPhysicsActorHandle_PhysX& InHandle);
	static bool IsInScene(const FPhysicsActorHandle_PhysX& InHandle);
	static bool HasSyncSceneData(const FPhysicsActorHandle_PhysX& InHandle);
	static bool HasAsyncSceneData(const FPhysicsActorHandle_PhysX& InHandle);
	static FPhysScene* GetCurrentScene(const FPhysicsActorHandle_PhysX& InHandle);
	static bool CanSimulate_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle);
	static float GetMass_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle);

	static void SetSendsSleepNotifies_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, bool bSendSleepNotifies);
	static void PutToSleep_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle);
	static void WakeUp_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle);

	static void SetIsKinematic_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, bool bIsKinematic);
	static void SetCcdEnabled_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, bool bIsCcdEnabled);

	static FTransform GetGlobalPose_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle);
	static void SetGlobalPose_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, const FTransform& InNewPose, bool bAutoWake = true);

	static FTransform GetTransform_AssumesLocked(const FPhysicsActorHandle& InRef, bool bForceGlobalPose = false);

	static bool HasKinematicTarget_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle);
	static FTransform GetKinematicTarget_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle);
	static void SetKinematicTarget_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, const FTransform& InNewTarget);

	static FVector GetLinearVelocity_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle);
	static void SetLinearVelocity_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, const FVector& InNewVelocity, bool bAutoWake = true);

	static FVector GetAngularVelocity_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle);
	static void SetAngularVelocity_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, const FVector& InNewVelocity, bool bAutoWake = true);
	static float GetMaxAngularVelocity_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle);
	static void SetMaxAngularVelocity_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, float InMaxAngularVelocity);

	static float GetMaxDepenetrationVelocity_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle);
	static void SetMaxDepenetrationVelocity_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, float InMaxDepenetrationVelocity);

	static FVector GetWorldVelocityAtPoint_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, const FVector& InPoint);

	static FTransform GetComTransform_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle);
	static FTransform GetComTransformLocal_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle);

	static FVector GetLocalInertiaTensor_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle);
	static FBox GetBounds_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle);

	static void SetLinearDamping_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, float InDamping);
	static void SetAngularDamping_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, float InDamping);

	static void AddForce_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, const FVector& InForce);
	static void AddTorque_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, const FVector& InTorque);
	static void AddForceMassIndependent_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, const FVector& InForce);
	static void AddTorqueMassIndependent_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, const FVector& InTorque);
	static void AddImpulseAtLocation_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, const FVector& InImpulse, const FVector& InLocation);
	static void AddRadialImpulse_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, const FVector& InOrigin, float InRadius, float InStrength, ERadialImpulseFalloff InFalloff, bool bInVelChange);

	static bool IsGravityEnabled_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle);
	static void SetGravityEnabled_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, bool bEnabled);

	static float GetSleepEnergyThreshold_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle);
	static void SetSleepEnergyThreshold_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, float InEnergyThreshold);

	static void SetMass_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, float InMass);
	static void SetMassSpaceInertiaTensor_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, const FVector& InTensor);
	static void SetComLocalPose_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, const FTransform& InComLocalPose);

	static float GetStabilizationEnergyThreshold_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle);
	static void SetStabilizationEnergyThreshold_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, float InThreshold);
	static uint32 GetSolverPositionIterationCount_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle);
	static void SetSolverPositionIterationCount_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, uint32 InSolverIterationCount);
	static uint32 GetSolverVelocityIterationCount_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle);
	static void SetSolverVelocityIterationCount_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, uint32 InSolverIterationCount);
	static float GetWakeCounter_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle);
	static void SetWakeCounter_AssumesLocked(const FPhysicsActorHandle_PhysX& InHandle, float InWakeCounter);

	static SIZE_T GetResourceSizeEx(const FPhysicsActorHandle_PhysX& InActorHandle);

	//////////////////////////////////////////////////////////////////////////

	// Constraint interface

	// Functions
	static FPhysicsConstraintHandle_PhysX CreateConstraint(const FPhysicsActorHandle_PhysX& InActorHandle1, const FPhysicsActorHandle_PhysX& InActorHandle2, const FTransform& InLocalFrame1, const FTransform& InLocalFrame2);
	static void SetConstraintUserData(const FPhysicsConstraintHandle_PhysX& InHandle, void* InUserData);
	static void ReleaseConstraint(FPhysicsConstraintHandle_PhysX& InHandle);

	static FTransform GetLocalPose(const FPhysicsConstraintHandle_PhysX& InHandle, EConstraintFrame::Type InFrame);
	static FTransform GetGlobalPose(const FPhysicsConstraintHandle_PhysX& InHandle, EConstraintFrame::Type InFrame);
	static FVector GetLocation(const FPhysicsConstraintHandle_PhysX& InHandle);
	static void GetForce(const FPhysicsConstraintHandle_PhysX& InHandle, FVector& OutLinForce, FVector& OutAngForce);
	static void GetDriveLinearVelocity(const FPhysicsConstraintHandle_PhysX& InHandle, FVector& OutLinVelocity);
	static void GetDriveAngularVelocity(const FPhysicsConstraintHandle_PhysX& InHandle, FVector& OutAngVelocity);

	static float GetCurrentSwing1(const FPhysicsConstraintHandle_PhysX& InHandle);
	static float GetCurrentSwing2(const FPhysicsConstraintHandle_PhysX& InHandle);
	static float GetCurrentTwist(const FPhysicsConstraintHandle_PhysX& InHandle);

	static void SetCanVisualize(const FPhysicsConstraintHandle_PhysX& InHandle, bool bInCanVisualize);
	static void SetCollisionEnabled(const FPhysicsConstraintHandle_PhysX& InHandle, bool bInCollisionEnabled);
	static void SetProjectionEnabled_AssumesLocked(const FPhysicsConstraintHandle_PhysX& InHandle, bool bInProjectionEnabled, float InLinearTolerance = 0.0f, float InAngularToleranceDegrees = 0.0f);
	static void SetParentDominates_AssumesLocked(const FPhysicsConstraintHandle_PhysX& InHandle, bool bInParentDominates);
	static void SetBreakForces_AssumesLocked(const FPhysicsConstraintHandle_PhysX& InHandle, float InLinearBreakForce, float InAngularBreakForce);
	static void SetLocalPose(const FPhysicsConstraintHandle_PhysX& InHandle, const FTransform& InPose, EConstraintFrame::Type InFrame);

	static void SetLinearMotionLimitType_AssumesLocked(const FPhysicsConstraintHandle_PhysX& InHandle, PhysicsInterfaceTypes::ELimitAxis InAxis, ELinearConstraintMotion InMotion);
	static void SetAngularMotionLimitType_AssumesLocked(const FPhysicsConstraintHandle_PhysX& InHandle, PhysicsInterfaceTypes::ELimitAxis InAxis, EAngularConstraintMotion InMotion);

	static void UpdateLinearLimitParams_AssumesLocked(const FPhysicsConstraintHandle_PhysX& InHandle, float InLimit, float InAverageMass, const FLinearConstraint& InParams);
	static void UpdateConeLimitParams_AssumesLocked(const FPhysicsConstraintHandle_PhysX& InHandle, float InAverageMass, const FConeConstraint& InParams);
	static void UpdateTwistLimitParams_AssumesLocked(const FPhysicsConstraintHandle_PhysX& InHandle, float InAverageMass, const FTwistConstraint& InParams);
	static void UpdateLinearDrive_AssumesLocked(const FPhysicsConstraintHandle_PhysX& InHandle, const FLinearDriveConstraint& InDriveParams);
	static void UpdateAngularDrive_AssumesLocked(const FPhysicsConstraintHandle_PhysX& InHandle, const FAngularDriveConstraint& InDriveParams);
	static void UpdateDriveTarget_AssumesLocked(const FPhysicsConstraintHandle_PhysX& InHandle, const FLinearDriveConstraint& InLinDrive, const FAngularDriveConstraint& InAngDrive);
	static void SetDrivePosition(const FPhysicsConstraintHandle_PhysX& InHandle, const FVector& InPosition);
	static void SetDriveOrientation(const FPhysicsConstraintHandle_PhysX& InHandle, const FQuat& InOrientation);
	static void SetDriveLinearVelocity(const FPhysicsConstraintHandle_PhysX& InHandle, const FVector& InLinVelocity);
	static void SetDriveAngularVelocity(const FPhysicsConstraintHandle_PhysX& InHandle, const FVector& InAngVelocity);

	static void SetTwistLimit(const FPhysicsConstraintHandle_PhysX& InHandle, float InLowerLimit, float InUpperLimit, float InContactDistance);
	static void SetSwingLimit(const FPhysicsConstraintHandle_PhysX& InHandle, float InYLimit, float InZLimit, float InContactDistance);
	static void SetLinearLimit(const FPhysicsConstraintHandle_PhysX& InHandle, float InLimit);

	static bool IsBroken(const FPhysicsConstraintHandle_PhysX& InHandle);

	static bool ExecuteOnUnbrokenConstraintReadOnly(const FPhysicsConstraintHandle_PhysX& InHandle, TFunctionRef<void(const FPhysicsConstraintHandle_PhysX&)> Func);
	static bool ExecuteOnUnbrokenConstraintReadWrite(const FPhysicsConstraintHandle_PhysX& InHandle, TFunctionRef<void(const FPhysicsConstraintHandle_PhysX&)> Func);

	//////////////////////////////////////////////////////////////////////////

	// Scene query interface functions

	/** Trace a ray against the world and return if a blocking hit is found */
	static bool RaycastTest(const UWorld* World, const FVector Start, const FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam);

	/** Trace a ray against the world and return the first blocking hit */
	static bool RaycastSingle(const UWorld* World, struct FHitResult& OutHit, const FVector Start, const FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam);

	/**
	*  Trace a ray against the world and return touching hits and then first blocking hit
	*  Results are sorted, so a blocking hit (if found) will be the last element of the array
	*  Only the single closest blocking result will be generated, no tests will be done after that
	*/
	static bool RaycastMulti(const UWorld* World, TArray<struct FHitResult>& OutHits, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam);

	// GEOM OVERLAP

	/** Function for testing overlaps between a supplied PxGeometry and the world. Returns true if at least one overlapping shape is blocking*/
	static bool GeomOverlapBlockingTest(const UWorld* World, const FCollisionShape& CollisionShape, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam);

	/** Function for testing overlaps between a supplied PxGeometry and the world. Returns true if anything is overlapping (blocking or touching)*/
	static bool GeomOverlapAnyTest(const UWorld* World, const FCollisionShape& CollisionShape, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam);

	// GEOM SWEEP

	/** Function used for sweeping a supplied PxGeometry against the world as a test */
	static bool GeomSweepTest(const UWorld* World, const FCollisionShape& CollisionShape, const FQuat& Rot, FVector Start, FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam);

	/** Function for sweeping a supplied PxGeometry against the world */
	static bool GeomSweepSingle(const UWorld* World, const FCollisionShape& CollisionShape, const FQuat& Rot, FHitResult& OutHit, FVector Start, FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam);

	/**
	 * Templated sweep designed to take collision shapes and geometry collections (specialized for those types in implementation)
	 * When implementing replacement interfaces all accepted types should be specialized/accepted, currently this is:
	 *			FCollisionShape
	 *			FPhysicsGeometryCollection
	 * Accepted specializations underneath FPhysicsInterface_PhysX declaration
	 */
	template<typename GeomType>
	static bool GeomSweepMulti(const UWorld* World, const GeomType& InGeom, const FQuat& InGeomRot, TArray<FHitResult>& OutHits, FVector Start, FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam);
	template<typename GeomType>
	static bool GeomOverlapMulti(const UWorld* World, const GeomType& InGeom, const FVector& InPosition, const FQuat& InRotation, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams);

	//////////////////////////////////////////////////////////////////////////
	// Trace functions for testing specific geometry (not against a world)
	//////////////////////////////////////////////////////////////////////////
	static bool LineTrace_Geom(FHitResult& OutHit, const FBodyInstance* InInstance, const FVector& InStart, const FVector& InEnd, bool bTraceComplex, bool bExtractPhysMaterial = false);
	static bool Sweep_Geom(FHitResult& OutHit, const FBodyInstance* InInstance, const FVector& InStart, const FVector& InEnd, const FQuat& InShapeRotation, const FCollisionShape& InShape, bool bSweepComplex);

	static bool Overlap_Geom(const FBodyInstance* InBodyInstance, const FPhysicsGeometryCollection& InGeometry, const FTransform& InShapeTransform, FMTDResult* OutOptResult = nullptr);
	static bool Overlap_Geom(const FBodyInstance* InBodyInstance, const FCollisionShape& InCollisionShape, const FQuat& InShapeRotation, const FTransform& InShapeTransform, FMTDResult* OutOptResult = nullptr);

	static bool GetSquaredDistanceToBody(const FBodyInstance* InInstance, const FVector& InPoint, float& OutDistanceSquared, FVector* OutOptPointOnBody = nullptr);

	//////////////////////////////////////////////////////////////////////////
	// Misc
	//////////////////////////////////////////////////////////////////////////

	static bool ExecPhysCommands(const TCHAR* Cmd, FOutputDevice* Ar, UWorld* InWorld);

	static void CalculateMassPropertiesFromShapeCollection(physx::PxMassProperties& OutProperties, const TArray<FPhysicsShapeHandle>& InShapes, float InDensityKGPerCM);

};

template<>
bool FPhysicsInterface_PhysX::GeomSweepMulti(const UWorld* World, const FPhysicsGeometryCollection& InGeom, const FQuat& InGeomRot, TArray<FHitResult>& OutHits, FVector Start, FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams /*= FCollisionObjectQueryParams::DefaultObjectQueryParam*/);
template<>
bool FPhysicsInterface_PhysX::GeomSweepMulti(const UWorld* World, const FCollisionShape& InGeom, const FQuat& InGeomRot, TArray<FHitResult>& OutHits, FVector Start, FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams /*= FCollisionObjectQueryParams::DefaultObjectQueryParam*/);

template<>
bool FPhysicsInterface_PhysX::GeomOverlapMulti(const UWorld* World, const FPhysicsGeometryCollection& InGeom, const FVector& InPosition, const FQuat& InRotation, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams);
template<>
bool FPhysicsInterface_PhysX::GeomOverlapMulti(const UWorld* World, const FCollisionShape& InGeom, const FVector& InPosition, const FQuat& InRotation, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams);

#endif