// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_CHAOS

#include "Engine/Engine.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Physics/PhysicsInterfaceDeclares.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "Physics/PhysicsInterfaceTypes.h"
#include "PhysicsEngine/ConstraintDrives.h"
#include "PhysicsEngine/ConstraintTypes.h"
#include "PhysicsPublic.h"
#include "PhysicsReplication.h"
#include "Chaos/ArrayCollection.h"
#include "Chaos/ArrayCollectionArray.h"
#include "Chaos/Capsule.h"
#include "Chaos/ImplicitObject.h"
#include "Chaos/Pair.h"
#include "Chaos/Transform.h"
#include "GameFramework/WorldSettings.h"
#include "Physics/GenericPhysicsInterface.h"
#include "PhysicsInterfaceWrapperShared.h"

//NOTE: Do not include Chaos headers directly as it means recompiling all of engine. This should be reworked to avoid allocations

template<typename T>
struct FCallbackDummy
{};

template <typename T>
using FPhysicsHitCallback = FCallbackDummy<T>;

class FPxQueryFilterCallback;
using FPhysicsQueryFilterCallback = FPxQueryFilterCallback;

static int32 NextBodyIdValue = 0;
static int32 NextConstraintIdValue = 0;
static TMap<uint32, TMap<struct FRigidBodyIndexPair, bool> *> EmptyCollisionMap = TMap<uint32, TMap<struct FRigidBodyIndexPair, bool> *>();

class FPhysInterface_Chaos;
struct FBodyInstance;
struct FPhysxUserData;

namespace Chaos
{
	template <typename T, int>
	class TBVHParticles;

	template <typename T, int>
	class TImplicitObject;

	template <typename T, int>
	class TPBDRigidParticles;

	template <typename T, int>
	class PerParticleGravity;

	template <typename T, int>
	class TPBDSpringConstraints;
}

#if COMPILE_ID_TYPES_AS_INTS
typedef uint32 RigidBodyId;
typedef uint32 RigidConstraintId;
typedef uint32 RigidAggregateId;

static FORCEINLINE uint32 ToValue(uint32 Id) { return Id; }
#else
#define CREATEIDTYPE(IDNAME) \
    class IDNAME \
    { \
      public: \
        IDNAME() {} \
        IDNAME(const uint32 InValue) : Value(InValue) {} \
        bool operator==(const IDNAME& Other) const { return Value == Other.Value; } \
        uint32 Value; \
    }

CREATEIDTYPE(RigidBodyId);
CREATEIDTYPE(RigidConstraintId);
CREATEIDTYPE(RigidAggregateId);

template<class T_ID>
static uint32 ToValue(T_ID Id)
{
    return Id.Value;
}
#endif

#define CREATEIDSCENEPAIR(NAME, IDNAME) \
    class ENGINE_API NAME : public Chaos::Pair<IDNAME, FPhysInterface_Chaos*> \
    { \
      public: \
        using Chaos::Pair<IDNAME, FPhysInterface_Chaos*>::Second; \
        \
        NAME() \
        { \
            Second = nullptr; \
        } \
        bool IsValid() const; \
        bool Equals(const NAME& Other) const \
        { \
            return static_cast<Chaos::Pair<IDNAME, FPhysInterface_Chaos*>>(Other) == static_cast<Chaos::Pair<IDNAME, FPhysInterface_Chaos*>>(*this); \
        } \
    } \

CREATEIDSCENEPAIR(FPhysicsActorReference_Chaos, RigidBodyId);
CREATEIDSCENEPAIR(FPhysicsConstraintReference_Chaos, RigidConstraintId);
CREATEIDSCENEPAIR(FPhysicsAggregateReference_Chaos, RigidAggregateId);

class FPhysicsShapeReference_Chaos
{
public:
	bool IsValid() const { return (Object != nullptr); }
	bool Equals(const FPhysicsShapeReference_Chaos& Other) const { return Object == Other.Object; }
    bool operator==(const FPhysicsShapeReference_Chaos& Other) const { return Equals(Other); }
	Chaos::TImplicitObject<float, 3>* Object;
    bool bSimulation;
    bool bQuery;
    FPhysicsActorReference_Chaos ActorRef;
};

FORCEINLINE uint32 GetTypeHash(const FPhysicsShapeReference_Chaos& InShapeReference)
{
    return GetTypeHash(reinterpret_cast<uintptr_t>(InShapeReference.Object));
}

// Temp interface
namespace physx
{
    class PxActor;
    class PxScene;
	class PxSimulationEventCallback;
    class PxGeometry;
    class PxTransform;
    class PxQuat;
	class PxMassProperties;
}
struct FContactModifyCallback;
class ULineBatchComponent;

class FSimEventCallbackFactory
{
public:
    physx::PxSimulationEventCallback* Create(FPhysInterface_Chaos* PhysScene, int32 SceneType) { return nullptr; }
    void Destroy(physx::PxSimulationEventCallback* Callback) {}
};
class FContactModifyCallbackFactory
{
public:
    FContactModifyCallback* Create(FPhysInterface_Chaos* PhysScene, int32 SceneType) { return nullptr; }
    void Destroy(FContactModifyCallback* Callback) {}
};
class FPhysicsReplicationFactory
{
public:
    FPhysicsReplication* Create(FPhysScene_PhysX* OwningPhysScene) { return nullptr; }
    void Destroy(FPhysicsReplication* PhysicsReplication) {}
};



class FPhysInterface_Chaos : public FGenericPhysicsInterface
{
public:
    ENGINE_API FPhysInterface_Chaos(const AWorldSettings* Settings=nullptr);
    ENGINE_API ~FPhysInterface_Chaos();

    void SetKinematicTransform(const RigidBodyId BodyId, const Chaos::TRigidTransform<float, 3>& NewTransform)
    {
        MCriticalSection.Lock();
        DelayedAnimationTransforms[GetIndexFromId(BodyId)] = NewTransform;
        MCriticalSection.Unlock();
    }

    RigidBodyId AddNewRigidParticle(const Chaos::TVector<float, 3>& X, const Chaos::TRotation<float, 3>& R, const Chaos::TVector<float, 3>& V, const Chaos::TVector<float, 3>& W, const float M, const Chaos::PMatrix<float, 3, 3>& I, Chaos::TImplicitObject<float, 3>* Geometry, Chaos::TBVHParticles<float, 3>* CollisionParticles, const bool Kinematic, const bool Disabled);

    Chaos::TPBDRigidParticles<float, 3>& BeginAddNewRigidParticles(const int32 Num, int32& Index, RigidBodyId& Id);
    Chaos::TPBDRigidParticles<float, 3>& BeginUpdateRigidParticles(const TArray<RigidBodyId> Ids);

    void EndAddNewRigidParticles()
    {
        MCriticalSection.Unlock();
    }

    void EndUpdateRigidParticles()
    {
        MCriticalSection.Unlock();
    }

    void EnableCollisionPair(const TTuple<int32, int32>& CollisionPair)
    {
        MCriticalSection.Lock();
        DelayedEnabledCollisions.Add(CollisionPair);
        MCriticalSection.Unlock();
    }

    void DisableCollisionPair(const TTuple<int32, int32>& CollisionPair)
    {
        MCriticalSection.Lock();
        DelayedDisabledCollisions.Add(CollisionPair);
        MCriticalSection.Unlock();
    }

    void SetGravity(const Chaos::TVector<float, 3>& Acceleration)
    {
        DelayedGravityAcceleration = Acceleration;
    }

	RigidConstraintId AddSpringConstraint(const Chaos::TVector<RigidBodyId, 2>& Constraint);
	void RemoveSpringConstraint(const RigidConstraintId Constraint);

    void AddForce(const Chaos::TVector<float, 3>& Force, RigidBodyId BodyId)
    {
        MCriticalSection.Lock();
        DelayedForce[GetIndexFromId(BodyId)] += Force;
        MCriticalSection.Unlock();
    }

    void AddTorque(const Chaos::TVector<float, 3>& Torque, RigidBodyId BodyId)
    {
        MCriticalSection.Lock();
        DelayedTorque[GetIndexFromId(BodyId)] += Torque;
        MCriticalSection.Unlock();
    }

    uint32 GetConstraintIndexFromId(const RigidConstraintId Id)
    {
        check(ConstraintIdToIndexMap.Contains(ToValue(Id)));
        return ConstraintIdToIndexMap[ToValue(Id)];
    }

    uint32 GetIndexFromId(const RigidBodyId Id)
    {
        check(IdToIndexMap.Contains(ToValue(Id)));
        return IdToIndexMap[ToValue(Id)];
    }

	void SetBodyInstance(FBodyInstance* OwningInstance, RigidBodyId Id);
    
    ENGINE_API void SyncBodies();

    // Interface needed for interface
	static ENGINE_API FPhysicsActorHandle CreateActor(const FActorCreationParams& Params);
	static ENGINE_API void ReleaseActor(FPhysicsActorReference_Chaos& InActorReference, FPhysScene* InScene = nullptr, bool bNeverDeferRelease=false);

	static ENGINE_API FPhysicsAggregateReference_Chaos CreateAggregate(int32 MaxBodies);
	static ENGINE_API void ReleaseAggregate(FPhysicsAggregateReference_Chaos& InAggregate);
	static ENGINE_API int32 GetNumActorsInAggregate(const FPhysicsAggregateReference_Chaos& InAggregate);
	static ENGINE_API void AddActorToAggregate_AssumesLocked(const FPhysicsAggregateReference_Chaos& InAggregate, const FPhysicsActorReference_Chaos& InActor);

	// Material interface functions
    // @todo(mlentine): How do we set material on the solver?
    static FPhysicsMaterialHandle CreateMaterial(const UPhysicalMaterial* InMaterial) {}
    static void ReleaseMaterial(FPhysicsMaterialHandle& InHandle) {}
    static void UpdateMaterial(const FPhysicsMaterialHandle& InHandle, UPhysicalMaterial* InMaterial) {}
    static void SetUserData(const FPhysicsMaterialHandle& InHandle, void* InUserData) {}

	// Actor interface functions
	template<typename AllocatorType>
	static int32 GetAllShapes_AssumedLocked(const FPhysicsActorReference_Chaos& InActorReference, TArray<FPhysicsShapeHandle, AllocatorType>& OutShapes);
	static int32 GetNumShapes(const FPhysicsActorHandle& InHandle);

	static void ReleaseShape(const FPhysicsShapeHandle& InShape);

	static void AttachShape(const FPhysicsActorHandle& InActor, const FPhysicsShapeHandle& InNewShape);
	static void DetachShape(const FPhysicsActorHandle& InActor, FPhysicsShapeHandle& InShape, bool bWakeTouching = true);

	static ENGINE_API void SetActorUserData_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference, FPhysxUserData* InUserData);

	static ENGINE_API bool IsRigidBody(const FPhysicsActorReference_Chaos& InActorReference);
	static ENGINE_API bool IsDynamic(const FPhysicsActorReference_Chaos& InActorReference)
    {
        return !IsStatic(InActorReference);
    }
    static ENGINE_API bool IsStatic(const FPhysicsActorReference_Chaos& InActorReference);
	static ENGINE_API bool IsKinematic_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference);
	static ENGINE_API bool IsSleeping(const FPhysicsActorReference_Chaos& InActorReference);
	static ENGINE_API bool IsCcdEnabled(const FPhysicsActorReference_Chaos& InActorReference);
    // @todo(mlentine): We don't have a notion of sync vs async and are a bit of both. Does this work?
    static bool HasSyncSceneData(const FPhysicsActorReference_Chaos& InHandle) { return true; }
    static bool HasAsyncSceneData(const FPhysicsActorReference_Chaos& InHandle) { return false; }
	static ENGINE_API bool IsInScene(const FPhysicsActorReference_Chaos& InActorReference);
	static ENGINE_API bool CanSimulate_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference);
	static ENGINE_API float GetMass_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference);

	static ENGINE_API void SetSendsSleepNotifies_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference, bool bSendSleepNotifies);
	static ENGINE_API void PutToSleep_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference);
	static ENGINE_API void WakeUp_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference);

	static ENGINE_API void SetIsKinematic_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference, bool bIsKinematic);
	static ENGINE_API void SetCcdEnabled_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference, bool bIsCcdEnabled);

	static ENGINE_API FTransform GetGlobalPose_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference);
	static ENGINE_API void SetGlobalPose_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference, const FTransform& InNewPose, bool bAutoWake = true);

    static ENGINE_API FTransform GetTransform_AssumesLocked(const FPhysicsActorHandle& InRef, bool bForceGlobalPose = false);

	static ENGINE_API bool HasKinematicTarget_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference);
	static ENGINE_API FTransform GetKinematicTarget_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference);
	static ENGINE_API void SetKinematicTarget_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference, const FTransform& InNewTarget);

	static ENGINE_API FVector GetLinearVelocity_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference);
	static ENGINE_API void SetLinearVelocity_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference, const FVector& InNewVelocity, bool bAutoWake = true);

	static ENGINE_API FVector GetAngularVelocity_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference);
	static ENGINE_API void SetAngularVelocity_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference, const FVector& InNewVelocity, bool bAutoWake = true);
	static ENGINE_API float GetMaxAngularVelocity_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference);
	static ENGINE_API void SetMaxAngularVelocity_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference, float InMaxAngularVelocity);

	static ENGINE_API float GetMaxDepenetrationVelocity_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference);
	static ENGINE_API void SetMaxDepenetrationVelocity_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference, float InMaxDepenetrationVelocity);

	static ENGINE_API FVector GetWorldVelocityAtPoint_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference, const FVector& InPoint);

	static ENGINE_API FTransform GetComTransform_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference);
	static ENGINE_API FTransform GetComTransformLocal_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference);

	static ENGINE_API FVector GetLocalInertiaTensor_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference);
	static ENGINE_API FBox GetBounds_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference);

	static ENGINE_API void SetLinearDamping_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference, float InDamping);
	static ENGINE_API void SetAngularDamping_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference, float InDamping);

	static ENGINE_API void AddForce_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference, const FVector& InForce);
	static ENGINE_API void AddTorque_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference, const FVector& InTorque);
	static ENGINE_API void AddForceMassIndependent_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference, const FVector& InForce);
	static ENGINE_API void AddTorqueMassIndependent_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference, const FVector& InTorque);
	static ENGINE_API void AddImpulseAtLocation_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference, const FVector& InImpulse, const FVector& InLocation);
	static ENGINE_API void AddRadialImpulse_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference, const FVector& InOrigin, float InRadius, float InStrength, ERadialImpulseFalloff InFalloff, bool bInVelChange);

	static ENGINE_API bool IsGravityEnabled_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference);
	static ENGINE_API void SetGravityEnabled_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference, bool bEnabled);

	static ENGINE_API float GetSleepEnergyThreshold_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference);
	static ENGINE_API void SetSleepEnergyThreshold_AssumesLocked(const FPhysicsActorReference_Chaos& InActorReference, float InEnergyThreshold);

	static void SetMass_AssumesLocked(const FPhysicsActorReference_Chaos& InHandle, float InMass);
	static void SetMassSpaceInertiaTensor_AssumesLocked(const FPhysicsActorReference_Chaos& InHandle, const FVector& InTensor);
	static void SetComLocalPose_AssumesLocked(const FPhysicsActorReference_Chaos& InHandle, const FTransform& InComLocalPose);

	static ENGINE_API float GetStabilizationEnergyThreshold_AssumesLocked(const FPhysicsActorReference_Chaos& InHandle);
	static ENGINE_API void SetStabilizationEnergyThreshold_AssumesLocked(const FPhysicsActorReference_Chaos& InHandle, float InThreshold);
	static ENGINE_API uint32 GetSolverPositionIterationCount_AssumesLocked(const FPhysicsActorReference_Chaos& InHandle);
	static ENGINE_API void SetSolverPositionIterationCount_AssumesLocked(const FPhysicsActorReference_Chaos& InHandle, uint32 InSolverIterationCount);
	static ENGINE_API uint32 GetSolverVelocityIterationCount_AssumesLocked(const FPhysicsActorReference_Chaos& InHandle);
	static ENGINE_API void SetSolverVelocityIterationCount_AssumesLocked(const FPhysicsActorReference_Chaos& InHandle, uint32 InSolverIterationCount);
	static ENGINE_API float GetWakeCounter_AssumesLocked(const FPhysicsActorReference_Chaos& InHandle);
	static ENGINE_API void SetWakeCounter_AssumesLocked(const FPhysicsActorReference_Chaos& InHandle, float InWakeCounter);

	static ENGINE_API SIZE_T GetResourceSizeEx(const FPhysicsActorReference_Chaos& InActorRef);
	
    static ENGINE_API FPhysicsConstraintReference_Chaos CreateConstraint(const FPhysicsActorReference_Chaos& InActorRef1, const FPhysicsActorReference_Chaos& InActorRef2, const FTransform& InLocalFrame1, const FTransform& InLocalFrame2);
	static ENGINE_API void SetConstraintUserData(const FPhysicsConstraintReference_Chaos& InConstraintRef, void* InUserData);
	static ENGINE_API void ReleaseConstraint(FPhysicsConstraintReference_Chaos& InConstraintRef);

	static ENGINE_API FTransform GetLocalPose(const FPhysicsConstraintReference_Chaos& InConstraintRef, EConstraintFrame::Type InFrame);
	static ENGINE_API FTransform GetGlobalPose(const FPhysicsConstraintReference_Chaos& InConstraintRef, EConstraintFrame::Type InFrame);
	static ENGINE_API FVector GetLocation(const FPhysicsConstraintReference_Chaos& InConstraintRef);
	static ENGINE_API void GetForce(const FPhysicsConstraintReference_Chaos& InConstraintRef, FVector& OutLinForce, FVector& OutAngForce);
	static ENGINE_API void GetDriveLinearVelocity(const FPhysicsConstraintReference_Chaos& InConstraintRef, FVector& OutLinVelocity);
	static ENGINE_API void GetDriveAngularVelocity(const FPhysicsConstraintReference_Chaos& InConstraintRef, FVector& OutAngVelocity);

	static ENGINE_API float GetCurrentSwing1(const FPhysicsConstraintReference_Chaos& InConstraintRef);
	static ENGINE_API float GetCurrentSwing2(const FPhysicsConstraintReference_Chaos& InConstraintRef);
	static ENGINE_API float GetCurrentTwist(const FPhysicsConstraintReference_Chaos& InConstraintRef);

	static ENGINE_API void SetCanVisualize(const FPhysicsConstraintReference_Chaos& InConstraintRef, bool bInCanVisualize);
	static ENGINE_API void SetCollisionEnabled(const FPhysicsConstraintReference_Chaos& InConstraintRef, bool bInCollisionEnabled);
	static ENGINE_API void SetProjectionEnabled_AssumesLocked(const FPhysicsConstraintReference_Chaos& InConstraintRef, bool bInProjectionEnabled, float InLinearTolerance = 0.0f, float InAngularToleranceDegrees = 0.0f);
	static ENGINE_API void SetParentDominates_AssumesLocked(const FPhysicsConstraintReference_Chaos& InConstraintRef, bool bInParentDominates);
	static ENGINE_API void SetBreakForces_AssumesLocked(const FPhysicsConstraintReference_Chaos& InConstraintRef, float InLinearBreakForce, float InAngularBreakForce);
	static ENGINE_API void SetLocalPose(const FPhysicsConstraintReference_Chaos& InConstraintRef, const FTransform& InPose, EConstraintFrame::Type InFrame);

	static ENGINE_API void SetLinearMotionLimitType_AssumesLocked(const FPhysicsConstraintReference_Chaos& InConstraintRef, PhysicsInterfaceTypes::ELimitAxis InAxis, ELinearConstraintMotion InMotion);
	static ENGINE_API void SetAngularMotionLimitType_AssumesLocked(const FPhysicsConstraintReference_Chaos& InConstraintRef, PhysicsInterfaceTypes::ELimitAxis InAxis, EAngularConstraintMotion InMotion);

	static ENGINE_API void UpdateLinearLimitParams_AssumesLocked(const FPhysicsConstraintReference_Chaos& InConstraintRef, float InLimit, float InAverageMass, const FLinearConstraint& InParams);
	static ENGINE_API void UpdateConeLimitParams_AssumesLocked(const FPhysicsConstraintReference_Chaos& InConstraintRef, float InAverageMass, const FConeConstraint& InParams);
	static ENGINE_API void UpdateTwistLimitParams_AssumesLocked(const FPhysicsConstraintReference_Chaos& InConstraintRef, float InAverageMass, const FTwistConstraint& InParams);
	static ENGINE_API void UpdateLinearDrive_AssumesLocked(const FPhysicsConstraintReference_Chaos& InConstraintRef, const FLinearDriveConstraint& InDriveParams);
	static ENGINE_API void UpdateAngularDrive_AssumesLocked(const FPhysicsConstraintReference_Chaos& InConstraintRef, const FAngularDriveConstraint& InDriveParams);
	static ENGINE_API void UpdateDriveTarget_AssumesLocked(const FPhysicsConstraintReference_Chaos& InConstraintRef, const FLinearDriveConstraint& InLinDrive, const FAngularDriveConstraint& InAngDrive);
	static ENGINE_API void SetDrivePosition(const FPhysicsConstraintReference_Chaos& InConstraintRef, const FVector& InPosition);
	static ENGINE_API void SetDriveOrientation(const FPhysicsConstraintReference_Chaos& InConstraintRef, const FQuat& InOrientation);
	static ENGINE_API void SetDriveLinearVelocity(const FPhysicsConstraintReference_Chaos& InConstraintRef, const FVector& InLinVelocity);
	static ENGINE_API void SetDriveAngularVelocity(const FPhysicsConstraintReference_Chaos& InConstraintRef, const FVector& InAngVelocity);

	static ENGINE_API void SetTwistLimit(const FPhysicsConstraintReference_Chaos& InConstraintRef, float InLowerLimit, float InUpperLimit, float InContactDistance);
	static ENGINE_API void SetSwingLimit(const FPhysicsConstraintReference_Chaos& InConstraintRef, float InYLimit, float InZLimit, float InContactDistance);
	static ENGINE_API void SetLinearLimit(const FPhysicsConstraintReference_Chaos& InConstraintRef, float InLimit);

	static ENGINE_API bool IsBroken(const FPhysicsConstraintReference_Chaos& InConstraintRef);

	static ENGINE_API bool ExecuteOnUnbrokenConstraintReadOnly(const FPhysicsConstraintReference_Chaos& InConstraintRef, TFunctionRef<void(const FPhysicsConstraintReference_Chaos&)> Func);
	static ENGINE_API bool ExecuteOnUnbrokenConstraintReadWrite(const FPhysicsConstraintReference_Chaos& InConstraintRef, TFunctionRef<void(const FPhysicsConstraintReference_Chaos&)> Func);

    /////////////////////////////////////////////

    // Interface needed for cmd
    static ENGINE_API bool ExecuteRead(const FPhysicsActorReference_Chaos& InActorReference, TFunctionRef<void(const FPhysicsActorReference_Chaos& Actor)> InCallable)
    {
        InCallable(InActorReference);
        return true;
    }

    static ENGINE_API bool ExecuteRead(USkeletalMeshComponent* InMeshComponent, TFunctionRef<void()> InCallable)
    {
        InCallable();
        return true;
    }

    static ENGINE_API bool ExecuteRead(const FPhysicsActorReference_Chaos& InActorReferenceA, const FPhysicsActorReference_Chaos& InActorReferenceB, TFunctionRef<void(const FPhysicsActorReference_Chaos& ActorA, const FPhysicsActorReference_Chaos& ActorB)> InCallable)
    {
        InCallable(InActorReferenceA, InActorReferenceB);
        return true;
    }

    static ENGINE_API bool ExecuteRead(const FPhysicsConstraintReference_Chaos& InConstraintRef, TFunctionRef<void(const FPhysicsConstraintReference_Chaos& Constraint)> InCallable)
    {
        InCallable(InConstraintRef);
        return true;
    }

    static ENGINE_API bool ExecuteRead(FPhysScene* InScene, TFunctionRef<void()> InCallable)
    {
        InCallable();
        return true;
    }

    static ENGINE_API bool ExecuteWrite(const FPhysicsActorReference_Chaos& InActorReference, TFunctionRef<void(const FPhysicsActorReference_Chaos& Actor)> InCallable)
    {
        InCallable(InActorReference);
        return true;
    }

    static ENGINE_API bool ExecuteWrite(USkeletalMeshComponent* InMeshComponent, TFunctionRef<void()> InCallable)
    {
        InCallable();
        return true;
    }

    static ENGINE_API bool ExecuteWrite(const FPhysicsActorReference_Chaos& InActorReferenceA, const FPhysicsActorReference_Chaos& InActorReferenceB, TFunctionRef<void(const FPhysicsActorReference_Chaos& ActorA, const FPhysicsActorReference_Chaos& ActorB)> InCallable)
    {
        InCallable(InActorReferenceA, InActorReferenceB);
        return true;
    }

    static ENGINE_API bool ExecuteWrite(const FPhysicsConstraintReference_Chaos& InConstraintRef, TFunctionRef<void(const FPhysicsConstraintReference_Chaos& Constraint)> InCallable)
    {
        InCallable(InConstraintRef);
        return true;
    }
	
    static ENGINE_API bool ExecuteWrite(FPhysScene* InScene, TFunctionRef<void()> InCallable)
    {
        InCallable();
        return true;
    }

    static void ExecuteShapeWrite(FBodyInstance* InInstance, FPhysicsShapeHandle& InShape, TFunctionRef<void(FPhysicsShapeHandle& InShape)> InCallable)
    {
        InCallable(InShape);
    }

	// Scene query interface functions

	static ENGINE_API bool RaycastTest(const UWorld* World, const FVector Start, const FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)
    {
        return false;
    }
    static ENGINE_API bool RaycastSingle(const UWorld* World, struct FHitResult& OutHit, const FVector Start, const FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)
    {
        return false;
    }
    static ENGINE_API bool RaycastMulti(const UWorld* World, TArray<struct FHitResult>& OutHits, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)
    {
        return false;
    }

    static ENGINE_API bool GeomOverlapBlockingTest(const UWorld* World, const FCollisionShape& CollisionShape, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)
    {
        return false;
    }
    static ENGINE_API bool GeomOverlapAnyTest(const UWorld* World, const FCollisionShape& CollisionShape, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)
    {
        return false;
    }
    static ENGINE_API bool GeomOverlapMulti(const UWorld* World, const FCollisionShape& CollisionShape, const FVector& Pos, const FQuat& Rot, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)
    {
        return false;
    }

	// GEOM SWEEP

    static ENGINE_API bool GeomSweepTest(const UWorld* World, const FCollisionShape& CollisionShape, const FQuat& Rot, FVector Start, FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)
    {
        return false;
    }
    static ENGINE_API bool GeomSweepSingle(const UWorld* World, const FCollisionShape& CollisionShape, const FQuat& Rot, FHitResult& OutHit, FVector Start, FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)
    {
        return false;
    }
    static ENGINE_API bool GeomSweepMulti(const UWorld* World, const FCollisionShape& CollisionShape, const FQuat& Rot, TArray<FHitResult>& OutHits, FVector Start, FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)
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

	static ENGINE_API bool ExecPhysCommands(const TCHAR* Cmd, FOutputDevice* Ar, UWorld* InWorld);

	static ENGINE_API FPhysScene* GetCurrentScene(const FPhysicsActorHandle& InActorReference)
	{
		return InActorReference.Second;
	}

#if WITH_PHYSX
    static void CalculateMassPropertiesFromShapeCollection(physx::PxMassProperties& OutProperties, const TArray<FPhysicsShapeHandle>& InShapes, float InDensityKGPerCM);
#endif

    static const Chaos::TPBDRigidParticles<float, 3>& GetParticlesAndIndex(const FPhysicsActorReference_Chaos& InActorReference, uint32& Index);
    static const TArray<Chaos::TVector<int32, 2>>& GetConstraintArrayAndIndex(const FPhysicsConstraintReference_Chaos& InActorReference, uint32& Index);

	// Shape interface functions
	static FPhysicsShapeHandle CreateShape(physx::PxGeometry* InGeom, bool bSimulation = true, bool bQuery = true, UPhysicalMaterial* InSimpleMaterial = nullptr, TArray<UPhysicalMaterial*>* InComplexMaterials = nullptr);
	
	static void AddGeometry(const FPhysicsActorHandle& InActor, const FGeometryAddParams& InParams, TArray<FPhysicsShapeHandle>* OutOptShapes = nullptr);
	static FPhysicsShapeHandle CloneShape(const FPhysicsShapeHandle& InShape);

	static bool IsSimulationShape(const FPhysicsShapeHandle& InShape);
	static bool IsQueryShape(const FPhysicsShapeHandle& InShape);
	static bool IsShapeType(const FPhysicsShapeHandle& InShape, ECollisionShapeType InType);
	static ECollisionShapeType GetShapeType(const FPhysicsShapeHandle& InShape);
	static FPhysicsGeometryCollection GetGeometryCollection(const FPhysicsShapeHandle& InShape);
	static FTransform GetLocalTransform(const FPhysicsShapeHandle& InShape);
    static void* GetUserData(const FPhysicsShapeHandle& InShape) { return nullptr; }

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
    static void SetIsSimulationShape(const FPhysicsShapeHandle& InShape, bool bIsSimShape) { const_cast<FPhysicsShapeHandle&>(InShape).bSimulation = bIsSimShape; }
    static void SetIsQueryShape(const FPhysicsShapeHandle& InShape, bool bIsQueryShape) { const_cast<FPhysicsShapeHandle&>(InShape).bSimulation = bIsQueryShape; }
    static void SetUserData(const FPhysicsShapeHandle& InShape, void* InUserData) {}
    static void SetGeometry(const FPhysicsShapeHandle& InShape, physx::PxGeometry& InGeom) {}
	static void SetLocalTransform(const FPhysicsShapeHandle& InShape, const FTransform& NewLocalTransform);
    static void SetMaterials(const FPhysicsShapeHandle& InShape, const TArrayView<UPhysicalMaterial*>InMaterials) {}

    // Scene
    void AddActorsToScene_AssumesLocked(const TArray<FPhysicsActorHandle>& InActors);
    void AddAggregateToScene(const FPhysicsAggregateHandle& InAggregate) {}

    void SetOwningWorld(UWorld* InOwningWorld) { MOwningWorld = InOwningWorld; }
    UWorld* GetOwningWorld() { return MOwningWorld; }
    const UWorld* GetOwningWorld() const { return MOwningWorld; }

    FPhysicsReplication* GetPhysicsReplication() { return nullptr; }
    void RemoveBodyInstanceFromPendingLists_AssumesLocked(FBodyInstance* BodyInstance, int32 SceneType);
    void AddCustomPhysics_AssumesLocked(FBodyInstance* BodyInstance, FCalculateCustomPhysics& CalculateCustomPhysics)
    {
        CalculateCustomPhysics.ExecuteIfBound(MDeltaTime, BodyInstance);
    }
    void AddForce_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Force, bool bAllowSubstepping, bool bAccelChange);
    void AddForceAtPosition_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Force, const FVector& Position, bool bAllowSubstepping, bool bIsLocalForce = false);
    void AddRadialForceToBody_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Origin, const float Radius, const float Strength, const uint8 Falloff, bool bAccelChange, bool bAllowSubstepping);
	void ClearForces_AssumesLocked(FBodyInstance* BodyInstance, bool bAllowSubstepping);
    void AddTorque_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Torque, bool bAllowSubstepping, bool bAccelChange);
	void ClearTorques_AssumesLocked(FBodyInstance* BodyInstance, bool bAllowSubstepping);
    void SetKinematicTarget_AssumesLocked(FBodyInstance* BodyInstance, const FTransform& TargetTM, bool bAllowSubstepping);
    bool GetKinematicTarget_AssumesLocked(const FBodyInstance* BodyInstance, FTransform& OutTM) const;

    ENGINE_API void DeferredAddCollisionDisableTable(uint32 SkelMeshCompID, TMap<struct FRigidBodyIndexPair, bool> * CollisionDisableTable) {}
    ENGINE_API void DeferredRemoveCollisionDisableTable(uint32 SkelMeshCompID) {}

	void MarkForPreSimKinematicUpdate(USkeletalMeshComponent* InSkelComp, ETeleportType InTeleport, bool bNeedsSkinning) {}
	void ClearPreSimKinematicUpdate(USkeletalMeshComponent* InSkelComp) {}
    
    void AddPendingOnConstraintBreak(FConstraintInstance* ConstraintInstance, int32 SceneType) {}
    void AddPendingSleepingEvent(FBodyInstance* BI, ESleepEvent SleepEventType, int32 SceneType) {}

	TArray<FCollisionNotifyInfo>& GetPendingCollisionNotifies(int32 SceneType) { return MNotifies; }

	static bool SupportsOriginShifting() { return false; }
    void ApplyWorldOffset(FVector InOffset) { check(InOffset.Size() == 0); }
    ENGINE_API void SetUpForFrame(const FVector* NewGrav, float InDeltaSeconds = 0.0f, float InMaxPhysicsDeltaTime = 0.0f)
    {
        SetGravity(*NewGrav);
        MDeltaTime = InDeltaSeconds;
    }
    ENGINE_API void StartFrame()
    {
        Scene.Tick(MDeltaTime);
        SyncBodies();
    }
    ENGINE_API void EndFrame(ULineBatchComponent* InLineBatcher) {}
    void WaitPhysScenes() {}
    FGraphEventRef GetCompletionEvent()
	{
        return FGraphEventRef();
	}

    bool HandleExecCommands(const TCHAR* Cmd, FOutputDevice* Ar) { return false; }
    void ListAwakeRigidBodies(bool bIncludeKinematic);
	ENGINE_API int32 GetNumAwakeBodies() const;
	ENGINE_API static TSharedPtr<FContactModifyCallbackFactory> ContactModifyCallbackFactory;
    ENGINE_API static TSharedPtr<FPhysicsReplicationFactory> PhysicsReplicationFactory;

    //ENGINE_API physx::PxScene* GetPxScene(uint32 SceneType) const { return nullptr; }
    //ENGINE_API nvidia::apex::Scene* GetApexScene(uint32 SceneType) const { return nullptr; }

    ENGINE_API void StartAsync() {}
	ENGINE_API bool HasAsyncScene() const { return false; }
    void SetPhysXTreeRebuildRate(int32 RebuildRate) {}
    ENGINE_API void EnsureCollisionTreeIsBuilt(UWorld* World) {}
    ENGINE_API void KillVisualDebugger() {}

    ENGINE_API static TSharedPtr<FSimEventCallbackFactory> SimEventCallbackFactory;

    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPhysScenePreTick, FPhysInterface_Chaos*, float /*DeltaSeconds*/);
    FOnPhysScenePreTick OnPhysScenePreTick;
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPhysSceneStep, FPhysInterface_Chaos*, float /*DeltaSeconds*/);
    FOnPhysSceneStep OnPhysSceneStep;

    ENGINE_API bool ExecPxVis(uint32 SceneType, const TCHAR* Cmd, FOutputDevice* Ar);
    ENGINE_API bool ExecApexVis(uint32 SceneType, const TCHAR* Cmd, FOutputDevice* Ar);

private:
    FPhysScene_Chaos Scene;

    // @todo(mlentine): Locking is very heavy handed right now; need to make less so.
    FCriticalSection MCriticalSection;
    float MDeltaTime;
    TMap<uint32, uint32> IdToIndexMap;
    TMap<uint32, uint32> ConstraintIdToIndexMap;
    TArray<uint32> ConstraintIds;
    TArray<Chaos::TRigidTransform<float, 3>> OldAnimationTransforms;
    TArray<Chaos::TRigidTransform<float, 3>> NewAnimationTransforms;
    TArray<Chaos::TRigidTransform<float, 3>> DelayedAnimationTransforms;
	TUniquePtr<Chaos::TPBDRigidParticles<float, 3>> DelayedNewParticles;
	TUniquePtr<Chaos::TPBDRigidParticles<float, 3>> DelayedUpdateParticles;
    TSet<int32> DelayedUpdateIndices;
    //Collisions
    TArray<TTuple<int32, int32>> DelayedDisabledCollisions;
    TArray<TTuple<int32, int32>> DelayedEnabledCollisions;
    //Gravity
    Chaos::TVector<float, 3> DelayedGravityAcceleration;
    TUniquePtr<Chaos::PerParticleGravity<float, 3>> MGravity;
	//Springs
    TArray<Chaos::TVector<int32, 2>> DelayedSpringConstraints;
    TArray<uint32> DelayedRemoveSpringConstraints;
    TUniquePtr<Chaos::TPBDSpringConstraints<float, 3>> MSpringConstraints;
    //Force
    TArray<Chaos::TVector<float, 3>> DelayedForce;
    TArray<Chaos::TVector<float, 3>> DelayedTorque;
    //Body Instances
    Chaos::TArrayCollectionArray<FBodyInstance*> BodyInstances;
    Chaos::TArrayCollectionArray<FBodyInstance*> DelayedBodyInstances;
    Chaos::TArrayCollectionArray<FBodyInstance*> DelayedUpdateBodyInstances;
    // Temp Interface
    UWorld* MOwningWorld;
    TArray<FCollisionNotifyInfo> MNotifies;
};

FORCEINLINE ECollisionShapeType GetType(const Chaos::TImplicitObject<float, 3>& Geom)
{
	if (Geom.GetType() == Chaos::ImplicitObjectType::Box)
	{
		return ECollisionShapeType::Box;
	}
	if (Geom.GetType() == Chaos::ImplicitObjectType::Sphere)
	{
		return ECollisionShapeType::Sphere;
	}
	if (Geom.GetType() == Chaos::ImplicitObjectType::Plane)
	{
		return ECollisionShapeType::Plane;
	}
	return ECollisionShapeType::None;
}

FORCEINLINE ECollisionShapeType GetGeometryType(const Chaos::TImplicitObject<float, 3>& Geom)
{
	return GetType(Geom);
}

FORCEINLINE float GetRadius(const Chaos::TCapsule<float>& Capsule)
{
	return Capsule.GetRadius();
}

FORCEINLINE float GetHalfHeight(const Chaos::TCapsule<float>& Capsule)
{
	return Capsule.GetHeight() / 2.f;
}

FORCEINLINE FVector FindBoxOpposingNormal(const FPhysTypeDummy& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal)
{
	return FVector(0.0f, 0.0f, 1.0f);
}

FORCEINLINE FVector FindHeightFieldOpposingNormal(const FPhysTypeDummy& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal)
{
	return FVector(0.0f, 0.0f, 1.0f);
}

FORCEINLINE FVector FindConvexMeshOpposingNormal(const FPhysTypeDummy& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal)
{
	return FVector(0.0f, 0.0f, 1.0f);
}

FORCEINLINE FVector FindTriMeshOpposingNormal(const FPhysTypeDummy& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal)
{
	return FVector(0.0f, 0.0f, 1.0f);
}

FORCEINLINE void DrawOverlappingTris(const UWorld* World, const FPhysTypeDummy& Hit, const Chaos::TImplicitObject<float, 3>& Geom, const FTransform& QueryTM)
{

}

FORCEINLINE void ComputeZeroDistanceImpactNormalAndPenetration(const UWorld* World, const FPhysTypeDummy& Hit, const Chaos::TImplicitObject<float, 3>& Geom, const FTransform& QueryTM, FHitResult& OutResult)
{

}

inline bool HadInitialOverlap(const FPhysTypeDummy& Hit)
{
	return false;
} 

inline Chaos::TImplicitObject<float, 3>* GetShape(const FPhysTypeDummy& Hit)
{
	return nullptr;
}

inline FPhysActorDummy* GetActor(const FPhysTypeDummy& Hit)
{
	return nullptr;
}

inline float GetDistance(const FPhysTypeDummy& Hit)
{
	return 0.0f;
}

inline FVector GetPosition(const FPhysTypeDummy& Hit)
{
	return FVector::ZeroVector;
}

inline FVector GetNormal(const FPhysTypeDummy& Hit)
{
	return FVector(0.0f, 0.0f, 1.0f);
}

inline UPhysicalMaterial* GetUserData(const FPhysTypeDummy& Material)
{
	return nullptr;
}

inline FBodyInstance* GetUserData(const FPhysActorDummy& Actor)
{
	return nullptr;
}

inline FPhysTypeDummy* GetMaterialFromInternalFaceIndex(const FPhysicsShape& Shape, uint32 InternalFaceIndex)
{
	return nullptr;
}

inline FHitFlags GetFlags(const FPhysTypeDummy& Hit)
{
	return FHitFlags(EHitFlags::None);
}

FORCEINLINE void SetFlags(FPhysTypeDummy& Hit, FHitFlags Flags)
{
	//Hit.flags = U2PHitFlags(Flags);
}

inline uint32 GetInternalFaceIndex(const FPhysTypeDummy& Hit)
{
	return 0;
}

inline void SetInternalFaceIndex(FPhysTypeDummy& Hit, uint32 FaceIndex)
{
	
}

inline FCollisionFilterData GetQueryFilterData(const FPhysicsShape& Shape)
{
	return FCollisionFilterData();
}

inline FCollisionFilterData GetSimulationFilterData(const FPhysicsShape& Shape)
{
	return FCollisionFilterData();
}

inline uint32 GetInvalidPhysicsFaceIndex()
{
	return 0xffffffff;
}

inline uint32 GetTriangleMeshExternalFaceIndex(const FPhysicsShape& Shape, uint32 InternalFaceIndex)
{
	return GetInvalidPhysicsFaceIndex();
}

inline FTransform GetGlobalPose(const FPhysActorDummy& RigidActor)
{
	return FTransform::Identity;
}

inline uint32 GetNumShapes(const FPhysActorDummy& RigidActor)
{
	return 0;
}

inline void GetShapes(const FPhysActorDummy& RigidActor, FPhysTypeDummy** ShapesBuffer, uint32 NumShapes)
{
	
}

inline void SetActor(FPhysTypeDummy& Hit, FPhysActorDummy* Actor)
{
	
}

inline void SetShape(FPhysTypeDummy& Hit, FPhysTypeDummy* Shape)
{

}

template <typename HitType>
void SetBlock(FPhysicsHitCallback<HitType>& Callback, const HitType& Hit)
{
	
}

template <typename HitType>
void SetHasBlock(FPhysicsHitCallback<HitType>& Callback, bool bHasBlock)
{
	
}

template <typename HitType>
void ProcessTouches(FPhysicsHitCallback<HitType>& Callback, const TArray<HitType>& TouchingHits)
{
	
}

template <typename HitType>
void FinalizeQuery(FPhysicsHitCallback<HitType>& Callback)
{
	
}

template <typename HitType>
HitType* GetBlock(const FPhysicsHitCallback<HitType>& Callback)
{
	return nullptr;
}

template <typename HitType>
bool GetHasBlock(const FPhysicsHitCallback<HitType>& Callback)
{
	return false;
}

bool IsBlocking(const FPhysicsShape& PShape, const FCollisionFilterData& QueryFilter);
#endif
