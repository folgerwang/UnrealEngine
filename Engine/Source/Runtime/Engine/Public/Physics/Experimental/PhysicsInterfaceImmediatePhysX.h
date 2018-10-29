// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_IMMEDIATE_PHYSX

#include "EngineGlobals.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"
#include "PhysicsEngine/ConstraintTypes.h"
#include "PhysicsInterfaceTypes.h"
#include "PhysScene_ImmediatePhysX.h"

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
}

struct ENGINE_API FPhysicsActorReference_ImmediatePhysX
{
	FPhysicsActorReference_ImmediatePhysX();

	bool IsValid() const;
	bool Equals(const FPhysicsActorReference_ImmediatePhysX& Other) const;

	FPhysScene_ImmediatePhysX* Scene;
	uint32 Index;
};

struct ENGINE_API FPhysicsConstraintReference_ImmediatePhysX
{
	FPhysicsConstraintReference_ImmediatePhysX();

	bool IsValid() const;
	bool Equals(const FPhysicsConstraintReference_ImmediatePhysX& Other) const;

	FPhysScene_ImmediatePhysX* Scene;
	uint32 Index;
};

struct ENGINE_API FPhysicsAggregateReference_ImmediatePhysX
{
	FPhysicsAggregateReference_ImmediatePhysX();

	bool IsValid() const;

	FPhysScene_ImmediatePhysX* Scene;
	TArray<uint32> Indices;
};

struct ENGINE_API FPhysicsShapeReference_ImmediatePhysX
{
	FPhysicsShapeReference_ImmediatePhysX(FPhysScene_ImmediatePhysX::FShape&& Shape);

	bool IsValid() const;
    bool operator==(const FPhysicsShapeReference_ImmediatePhysX& Other) const { return Actor == Other.Actor && Index == Other.Index && Shape == Other.Shape; }

    FPhysScene_ImmediatePhysX::FShape Shape;
    FPhysicsActorReference_ImmediatePhysX* Actor;
    int32 Index;
};

// Dummy holder for materials
struct ENGINE_API FPhysicsMaterialReference_ImmediatePhysX
{
    FPhysScene_ImmediatePhysX::FMaterial Material;

    bool IsValid() const { return true; }
};

struct ENGINE_API FPhysicsGeometryCollection_ImmediatePhysX
{
    FPhysicsGeometryCollection_ImmediatePhysX() {}
    
    bool IsValid() const { return Geometry != nullptr; }

    physx::PxGeometry* Geometry;

    ECollisionShapeType GetType() const
    {
        if (!IsValid())
        {
            return ECollisionShapeType::None;
        }
        if (Geometry->getType() == PxGeometryType::Enum::eBOX)
        {
            return ECollisionShapeType::Box;
        }
        if (Geometry->getType() == PxGeometryType::Enum::eCAPSULE)
        {
            return ECollisionShapeType::Capsule;
        }
        if (Geometry->getType() == PxGeometryType::Enum::eCONVEXMESH)
        {
            return ECollisionShapeType::Convex;
        }
        if (Geometry->getType() == PxGeometryType::Enum::eHEIGHTFIELD)
        {
            return ECollisionShapeType::Heightfield;
        }
        if (Geometry->getType() == PxGeometryType::Enum::eSPHERE)
        {
            return ECollisionShapeType::Sphere;
        }
        if (Geometry->getType() == PxGeometryType::Enum::eTRIANGLEMESH)
        {
            return ECollisionShapeType::Trimesh;
        }
        return ECollisionShapeType::None;
    }
    physx::PxGeometry& GetGeometry() const { return *Geometry; }
    bool GetBoxGeometry(physx::PxBoxGeometry& OutGeom) const
    {
        return GPhysXSDK->createShape(GetGeometry(), nullptr, 0)->getBoxGeometry(OutGeom);
    }
    bool GetSphereGeometry(physx::PxSphereGeometry& OutGeom) const
    {
        return GPhysXSDK->createShape(GetGeometry(), nullptr, 0)->getSphereGeometry(OutGeom);
    }
    bool GetCapsuleGeometry(physx::PxCapsuleGeometry& OutGeom) const
    {
        return GPhysXSDK->createShape(GetGeometry(), nullptr, 0)->getCapsuleGeometry(OutGeom);
    }
    bool GetConvexGeometry(physx::PxConvexMeshGeometry& OutGeom) const
    {
        return GPhysXSDK->createShape(GetGeometry(), nullptr, 0)->getConvexMeshGeometry(OutGeom);
    }
    bool GetTriMeshGeometry(physx::PxTriangleMeshGeometry& OutGeom) const
    {
        return GPhysXSDK->createShape(GetGeometry(), nullptr, 0)->getTriangleMeshGeometry(OutGeom);
    }
};

// @todo(mlentine): Once we finalize the format of this we need to redo this
FORCEINLINE uint32 GetTypeHash(const FPhysicsShapeReference_ImmediatePhysX& InShapeReference)
{
    return GetTypeHash(InShapeReference.Index);
}

/**
* API to access the physics interface. All calls to FPhysicsInterface functions should be inside an Execute* callable.
* This is to ensure correct lock semantics and command buffering if the specific API supports deferred commands.
*/
struct ENGINE_API FPhysicsCommand_ImmediatePhysX
{
	// Executes with appropriate read locking, return true if execution took place (actor was valid)
	static bool ExecuteRead(const FPhysicsActorReference_ImmediatePhysX& InActorReference, TFunctionRef<void(const FPhysicsActorReference_ImmediatePhysX& Actor)> InCallable);
	static bool ExecuteRead(USkeletalMeshComponent* InMeshComponent, TFunctionRef<void()> InCallable);
	static bool ExecuteRead(const FPhysicsActorReference_ImmediatePhysX& InActorReferenceA, const FPhysicsActorReference_ImmediatePhysX& InActorReferenceB, TFunctionRef<void(const FPhysicsActorReference_ImmediatePhysX& ActorA, const FPhysicsActorReference_ImmediatePhysX& ActorB)> InCallable);
	static bool ExecuteRead(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, TFunctionRef<void(const FPhysicsConstraintReference_ImmediatePhysX& Constraint)> InCallable);
	static bool ExecuteRead(FPhysScene* InScene, TFunctionRef<void()> InCallable);

	// Executes with appropriate write locking, return true if execution took place (actor was valid)
	static bool ExecuteWrite(const FPhysicsActorReference_ImmediatePhysX& InActorReference, TFunctionRef<void(const FPhysicsActorReference_ImmediatePhysX& Actor)> InCallable);
	static bool ExecuteWrite(USkeletalMeshComponent* InMeshComponent, TFunctionRef<void()> InCallable);
	static bool ExecuteWrite(const FPhysicsActorReference_ImmediatePhysX& InActorReferenceA, const FPhysicsActorReference_ImmediatePhysX& InActorReferenceB, TFunctionRef<void(const FPhysicsActorReference_ImmediatePhysX& ActorA, const FPhysicsActorReference_ImmediatePhysX& ActorB)> InCallable);
	static bool ExecuteWrite(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, TFunctionRef<void(const FPhysicsConstraintReference_ImmediatePhysX& Constraint)> InCallable);
	static bool ExecuteWrite(FPhysScene* InScene, TFunctionRef<void()> InCallable);
	
    // Executes function on a shape, handling shared shapes
	static void ExecuteShapeWrite(FBodyInstance* InInstance, FPhysicsShapeHandle& InShape, TFunctionRef<void(const FPhysicsShapeHandle& InShape)> InCallable);
};

struct ENGINE_API FPhysicsInterface_ImmediatePhysX
{
	// PhysX Only functions, not related to wider physics interface
	// To be used only in code that handles PhysX
    static physx::PxRigidActor* GetPxRigidActorFromScene_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorRef, int32 SceneType = -1) { return nullptr; }
    static physx::PxRigidActor* GetPxRigidActor_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InRef) { return nullptr; }
	//////////////////////////////////////////////////////////////////////////

	// Aggregate interface functions
	static FPhysicsAggregateReference_ImmediatePhysX CreateAggregate(int32 MaxBodies);
	static void ReleaseAggregate(FPhysicsAggregateReference_ImmediatePhysX& InAggregate);
	static int32 GetNumActorsInAggregate(const FPhysicsAggregateReference_ImmediatePhysX& InAggregate);
	static void AddActorToAggregate_AssumesLocked(const FPhysicsAggregateReference_ImmediatePhysX& InAggregate, const FPhysicsActorReference_ImmediatePhysX& InActor);

	// Shape interface functions
	static FPhysicsShapeReference_ImmediatePhysX CreateShape(physx::PxGeometry* InGeom, bool bSimulation = true, bool bQuery = true, UPhysicalMaterial* InSimpleMaterial = nullptr, TArray<UPhysicalMaterial*>* InComplexMaterials = nullptr, bool bShared = false);

	static void AddGeometry(const FPhysicsActorHandle& InActor, const FGeometryAddParams& InParams, TArray<FPhysicsShapeHandle>* OutOptShapes = nullptr);
	static FPhysicsShapeReference_ImmediatePhysX CloneShape(const FPhysicsShapeReference_ImmediatePhysX& InShape);

	static bool IsSimulationShape(const FPhysicsShapeReference_ImmediatePhysX& InShape);
	static bool IsQueryShape(const FPhysicsShapeReference_ImmediatePhysX& InShape);
	static bool IsShapeType(const FPhysicsShapeReference_ImmediatePhysX& InShape, ECollisionShapeType InType);
    // @todo(mlentine): We don't keep track of what is shared but anything can be
    static bool IsShared(const FPhysicsShapeHandle& InShape) { return true; }
	static ECollisionShapeType GetShapeType(const FPhysicsShapeReference_ImmediatePhysX& InShape);
	static FPhysicsGeometryCollection GetGeometryCollection(const FPhysicsShapeReference_ImmediatePhysX& InShape);
	static FTransform GetLocalTransform(const FPhysicsShapeReference_ImmediatePhysX& InShape);
	static void* GetUserData(const FPhysicsShapeReference_ImmediatePhysX& InShape);

	// Set the mask filter of a shape, which is an extra level of filtering during collision detection / query for extra channels like "Blue Team" and "Red Team"
	static void SetMaskFilter(const FPhysicsShapeReference_ImmediatePhysX& InShape, FMaskFilter InFilter);
	static void SetSimulationFilter(const FPhysicsShapeReference_ImmediatePhysX& InShape, const FCollisionFilterData& InFilter);
	static void SetQueryFilter(const FPhysicsShapeReference_ImmediatePhysX& InShape, const FCollisionFilterData& InFilter);
	static void SetIsSimulationShape(const FPhysicsShapeReference_ImmediatePhysX& InShape, bool bIsSimShape);
	static void SetIsQueryShape(const FPhysicsShapeReference_ImmediatePhysX& InShape, bool bIsQueryShape);
	static void SetUserData(const FPhysicsShapeReference_ImmediatePhysX& InShape, void* InUserData);
	static void SetGeometry(const FPhysicsShapeReference_ImmediatePhysX& InShape, physx::PxGeometry& InGeom);
	static void SetLocalTransform(const FPhysicsShapeReference_ImmediatePhysX& InShape, const FTransform& NewLocalTransform);
	static void SetMaterials(const FPhysicsShapeReference_ImmediatePhysX& InShape, const TArrayView<UPhysicalMaterial*>InMaterials);

	// Material interface functions
	static FPhysicsMaterialHandle CreateMaterial(const UPhysicalMaterial* InMaterial);
	static void ReleaseMaterial(FPhysicsMaterialHandle& InHandle);
	static void UpdateMaterial(const FPhysicsMaterialHandle& InHandle, UPhysicalMaterial* InMaterial);
	static void SetUserData(const FPhysicsMaterialHandle& InHandle, void* InUserData);

	// Actor interface functions

	// #PHYS2 - These should be on the scene, but immediate mode stops us for now, eventually that should spawn its own minimal IM scene and these should move
	static FPhysicsActorHandle CreateActor(const FActorCreationParams& Params);
	static void ReleaseActor(FPhysicsActorReference_ImmediatePhysX& InActorReference, FPhysScene* InScene = nullptr);
	//////////////////////////////////////////////////////////////////////////

	template<typename AllocatorType>
	static int32 GetAllShapes_AssumedLocked(const FPhysicsActorReference_ImmediatePhysX& InHandle, TArray<FPhysicsShapeHandle, AllocatorType>& OutShapes, EPhysicsSceneType InSceneType = PST_MAX);
	static void GetNumShapes(const FPhysicsActorHandle& InHandle, int32& OutNumSyncShapes, int32& OutNumAsyncShapes);
	static void ReleaseShape(const FPhysicsShapeHandle& InShape);

	static void AttachShape(const FPhysicsActorHandle& InActor, const FPhysicsShapeHandle& InNewShape);
	static void AttachShape(const FPhysicsActorHandle& InActor, const FPhysicsShapeHandle& InNewShape, EPhysicsSceneType SceneType);
	static void DetachShape(const FPhysicsActorHandle& InActor, FPhysicsShapeHandle& InShape, bool bWakeTouching = true);

	static void SetActorUserData_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference, FPhysxUserData* InUserData);

	static bool IsRigidBody(const FPhysicsActorReference_ImmediatePhysX& InActorReference);
	static bool IsDynamic(const FPhysicsActorReference_ImmediatePhysX& InActorReference);
	static bool IsStatic(const FPhysicsActorReference_ImmediatePhysX& InActorReference);
	static bool IsKinematic_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference);
	static bool IsSleeping(const FPhysicsActorReference_ImmediatePhysX& InActorReference);
	static bool IsCcdEnabled(const FPhysicsActorReference_ImmediatePhysX& InActorReference);
	static bool IsInScene(const FPhysicsActorReference_ImmediatePhysX& InActorReference);
    static bool HasSyncSceneData(const FPhysicsActorReference_ImmediatePhysX& InHandle) { return false; }
    static bool HasAsyncSceneData(const FPhysicsActorReference_ImmediatePhysX& InHandle) { return false; }
	static FPhysScene* GetCurrentScene(const FPhysicsActorReference_ImmediatePhysX& InActorReference);
	static bool CanSimulate_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference);
	static float GetMass_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference);

	static void SetSendsSleepNotifies_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference, bool bSendSleepNotifies);
	static void PutToSleep_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference);
	static void WakeUp_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference);

	static void SetIsKinematic_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference, bool bIsKinematic);
	static void SetCcdEnabled_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference, bool bIsCcdEnabled);

	static FTransform GetGlobalPose_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference);
	static void SetGlobalPose_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference, const FTransform& InNewPose, bool bAutoWake = true);

	static FTransform GetTransform_AssumesLocked(const FPhysicsActorHandle& InRef, bool bForceGlobalPose = false);

	static bool HasKinematicTarget_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference);
	static FTransform GetKinematicTarget_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference);
	static void SetKinematicTarget_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference, const FTransform& InNewTarget);

	static FVector GetLinearVelocity_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference);
	static void SetLinearVelocity_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference, const FVector& InNewVelocity, bool bAutoWake = true);

	static FVector GetAngularVelocity_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference);
	static void SetAngularVelocity_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference, const FVector& InNewVelocity, bool bAutoWake = true);
	static float GetMaxAngularVelocity_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference);
	static void SetMaxAngularVelocity_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference, float InMaxAngularVelocity);

	static float GetMaxDepenetrationVelocity_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference);
	static void SetMaxDepenetrationVelocity_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference, float InMaxDepenetrationVelocity);

	static FVector GetWorldVelocityAtPoint_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference, const FVector& InPoint);

	static FTransform GetComTransform_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference);

	static FVector GetLocalInertiaTensor_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference);
	static FBox GetBounds_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference);

	static void SetLinearDamping_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference, float InDamping);
	static void SetAngularDamping_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference, float InDamping);

	static void AddForce_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference, const FVector& InForce);
	static void AddTorque_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference, const FVector& InTorque);
	static void AddForceMassIndependent_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference, const FVector& InForce);
	static void AddTorqueMassIndependent_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference, const FVector& InTorque);
	static void AddImpulseAtLocation_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference, const FVector& InImpulse, const FVector& InLocation);
	static void AddRadialImpulse_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference, const FVector& InOrigin, float InRadius, float InStrength, ERadialImpulseFalloff InFalloff, bool bInVelChange);

	static bool IsGravityEnabled_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference);
	static void SetGravityEnabled_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference, bool bEnabled);

	static float GetSleepEnergyThreshold_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference);
	static void SetSleepEnergyThreshold_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InActorReference, float InEnergyThreshold);

	static void SetMass_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InHandle, float InMass);
	static void SetMassSpaceInertiaTensor_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InHandle, const FVector& InTensor);
	static void SetComLocalPose_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InHandle, const FTransform& InComLocalPose);

	static float GetStabilizationEnergyThreshold_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InHandle);
	static void SetStabilizationEnergyThreshold_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InHandle, float InThreshold);
	static uint32 GetSolverPositionIterationCount_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InHandle);
	static void SetSolverPositionIterationCount_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InHandle, uint32 InSolverIterationCount);
	static uint32 GetSolverVelocityIterationCount_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InHandle);
	static void SetSolverVelocityIterationCount_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InHandle, uint32 InSolverIterationCount);
	static float GetWakeCounter_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InHandle);
	static void SetWakeCounter_AssumesLocked(const FPhysicsActorReference_ImmediatePhysX& InHandle, float InWakeCounter);

	static SIZE_T GetResourceSizeEx(const FPhysicsActorReference_ImmediatePhysX& InActorRef);

	//////////////////////////////////////////////////////////////////////////

	// Constraint interface

	// Functions
	static FPhysicsConstraintReference_ImmediatePhysX CreateConstraint(const FPhysicsActorReference_ImmediatePhysX& InActorRef1, const FPhysicsActorReference_ImmediatePhysX& InActorRef2, const FTransform& InLocalFrame1, const FTransform& InLocalFrame2);
	static void SetConstraintUserData(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, void* InUserData);
	static void ReleaseConstraint(FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef);

	static FTransform GetLocalPose(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, EConstraintFrame::Type InFrame);
	static FTransform GetGlobalPose(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, EConstraintFrame::Type InFrame);
	static FVector GetLocation(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef);
	static void GetForce(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, FVector& OutLinForce, FVector& OutAngForce);
	static void GetDriveLinearVelocity(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, FVector& OutLinVelocity);
	static void GetDriveAngularVelocity(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, FVector& OutAngVelocity);

	static float GetCurrentSwing1(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef);
	static float GetCurrentSwing2(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef);
	static float GetCurrentTwist(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef);

	static void SetCanVisualize(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, bool bInCanVisualize);
	static void SetCollisionEnabled(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, bool bInCollisionEnabled);
	static void SetProjectionEnabled_AssumesLocked(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, bool bInProjectionEnabled, float InLinearTolerance = 0.0f, float InAngularToleranceDegrees = 0.0f);
	static void SetParentDominates_AssumesLocked(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, bool bInParentDominates);
	static void SetBreakForces_AssumesLocked(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, float InLinearBreakForce, float InAngularBreakForce);
	static void SetLocalPose(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, const FTransform& InPose, EConstraintFrame::Type InFrame);

	static void SetLinearMotionLimitType_AssumesLocked(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, PhysicsInterfaceTypes::ELimitAxis InAxis, ELinearConstraintMotion InMotion);
	static void SetAngularMotionLimitType_AssumesLocked(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, PhysicsInterfaceTypes::ELimitAxis InAxis, EAngularConstraintMotion InMotion);

	static void UpdateLinearLimitParams_AssumesLocked(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, float InLimit, float InAverageMass, const FLinearConstraint& InParams);
	static void UpdateConeLimitParams_AssumesLocked(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, float InAverageMass, const FConeConstraint& InParams);
	static void UpdateTwistLimitParams_AssumesLocked(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, float InAverageMass, const FTwistConstraint& InParams);
	static void UpdateLinearDrive_AssumesLocked(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, const FLinearDriveConstraint& InDriveParams);
	static void UpdateAngularDrive_AssumesLocked(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, const FAngularDriveConstraint& InDriveParams);
	static void UpdateDriveTarget_AssumesLocked(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, const FLinearDriveConstraint& InLinDrive, const FAngularDriveConstraint& InAngDrive);
	static void SetDrivePosition(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, const FVector& InPosition);
	static void SetDriveOrientation(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, const FQuat& InOrientation);
	static void SetDriveLinearVelocity(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, const FVector& InLinVelocity);
	static void SetDriveAngularVelocity(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, const FVector& InAngVelocity);

	static void SetTwistLimit(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, float InLowerLimit, float InUpperLimit, float InContactDistance);
	static void SetSwingLimit(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, float InYLimit, float InZLimit, float InContactDistance);
	static void SetLinearLimit(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, float InLimit);

	static bool IsBroken(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef);

	static bool ExecuteOnUnbrokenConstraintReadOnly(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, TFunctionRef<void(const FPhysicsConstraintReference_ImmediatePhysX&)> Func);
	static bool ExecuteOnUnbrokenConstraintReadWrite(const FPhysicsConstraintReference_ImmediatePhysX& InConstraintRef, TFunctionRef<void(const FPhysicsConstraintReference_ImmediatePhysX&)> Func);

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

	static bool ExecPhysCommands(const TCHAR* Cmd, FOutputDevice* Ar, UWorld* InWorld);
    
    static void CalculateMassPropertiesFromShapeCollection(physx::PxMassProperties& OutProperties, const TArray<FPhysicsShapeHandle>& InShapes, float InDensityKGPerCM);
};

template<>
bool FPhysicsInterface_ImmediatePhysX::GeomSweepMulti(const UWorld* World, const FPhysicsGeometryCollection& InGeom, const FQuat& InGeomRot, TArray<FHitResult>& OutHits, FVector Start, FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams /*= FCollisionObjectQueryParams::DefaultObjectQueryParam*/);
template<>
bool FPhysicsInterface_ImmediatePhysX::GeomSweepMulti(const UWorld* World, const FCollisionShape& InGeom, const FQuat& InGeomRot, TArray<FHitResult>& OutHits, FVector Start, FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams /*= FCollisionObjectQueryParams::DefaultObjectQueryParam*/);

template<>
bool FPhysicsInterface_ImmediatePhysX::GeomOverlapMulti(const UWorld* World, const FPhysicsGeometryCollection& InGeom, const FVector& InPosition, const FQuat& InRotation, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams);
template<>
bool FPhysicsInterface_ImmediatePhysX::GeomOverlapMulti(const UWorld* World, const FCollisionShape& InGeom, const FVector& InPosition, const FQuat& InRotation, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams);

#endif
