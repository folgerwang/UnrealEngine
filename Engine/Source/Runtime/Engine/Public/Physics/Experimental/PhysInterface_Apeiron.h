// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_APEIRON

#include "Engine/Engine.h"
#include "Physics/Experimental/PhysScene_Apeiron.h"
#include "Physics/PhysicsInterfaceDeclares.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "Physics/PhysicsInterfaceTypes.h"
#include "PhysicsEngine/ConstraintDrives.h"
#include "PhysicsEngine/ConstraintTypes.h"
#include "PhysicsPublic.h"
#include "PhysicsReplication.h"
#include "Apeiron/ArrayCollection.h"
#include "Apeiron/BVHParticles.h"
#include "GameFramework/WorldSettings.h"
#include "Physics/GenericPhysicsInterface.h"

static int32 NextBodyIdValue = 0;
static int32 NextConstraintIdValue = 0;
static TMap<uint32, TMap<struct FRigidBodyIndexPair, bool> *> EmptyCollisionMap = TMap<uint32, TMap<struct FRigidBodyIndexPair, bool> *>();

class FPhysInterface_Apeiron;
struct FBodyInstance;
struct FPhysxUserData;

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
    class ENGINE_API NAME : public Apeiron::Pair<IDNAME, FPhysInterface_Apeiron*> \
    { \
      public: \
        using Apeiron::Pair<IDNAME, FPhysInterface_Apeiron*>::Second; \
        \
        NAME() \
        { \
            Second = nullptr; \
        } \
        bool IsValid() const; \
        bool Equals(const NAME& Other) const \
        { \
            return static_cast<Apeiron::Pair<IDNAME, FPhysInterface_Apeiron*>>(Other) == static_cast<Apeiron::Pair<IDNAME, FPhysInterface_Apeiron*>>(*this); \
        } \
    } \

CREATEIDSCENEPAIR(FPhysicsActorReference_Apeiron, RigidBodyId);
CREATEIDSCENEPAIR(FPhysicsConstraintReference_Apeiron, RigidConstraintId);
CREATEIDSCENEPAIR(FPhysicsAggregateReference_Apeiron, RigidAggregateId);

class FPhysicsShapeReference_Apeiron
{
public:
	bool IsValid() const { return (Object != nullptr); }
	bool Equals(const FPhysicsShapeReference_Apeiron& Other) const { return Object == Other.Object; }
    bool operator==(const FPhysicsShapeReference_Apeiron& Other) const { return Equals(Other); }
	Apeiron::TImplicitObject<float, 3>* Object;
    bool bSimulation;
    bool bQuery;
    FPhysicsActorReference_Apeiron ActorRef;
};

FORCEINLINE uint32 GetTypeHash(const FPhysicsShapeReference_Apeiron& InShapeReference)
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
    physx::PxSimulationEventCallback* Create(FPhysInterface_Apeiron* PhysScene, int32 SceneType) { return nullptr; }
    void Destroy(physx::PxSimulationEventCallback* Callback) {}
};
class FContactModifyCallbackFactory
{
public:
    FContactModifyCallback* Create(FPhysInterface_Apeiron* PhysScene, int32 SceneType) { return nullptr; }
    void Destroy(FContactModifyCallback* Callback) {}
};
class FPhysicsReplicationFactory
{
public:
    FPhysicsReplication* Create(FPhysScene_PhysX* OwningPhysScene) { return nullptr; }
    void Destroy(FPhysicsReplication* PhysicsReplication) {}
};



class FPhysInterface_Apeiron : public FGenericPhysicsInterface
{
public:
    ENGINE_API FPhysInterface_Apeiron(const AWorldSettings* Settings=nullptr);
    ENGINE_API ~FPhysInterface_Apeiron();

    void SetKinematicTransform(const RigidBodyId BodyId, const Apeiron::TRigidTransform<float, 3>& NewTransform)
    {
        MCriticalSection.Lock();
        DelayedAnimationTransforms[GetIndexFromId(BodyId)] = NewTransform;
        MCriticalSection.Unlock();
    }

    RigidBodyId AddNewRigidParticle(const Apeiron::TVector<float, 3>& X, const Apeiron::TRotation<float, 3>& R, const Apeiron::TVector<float, 3>& V, const Apeiron::TVector<float, 3>& W, const float M, const Apeiron::PMatrix<float, 3, 3>& I, Apeiron::TImplicitObject<float, 3>* Geometry, Apeiron::TBVHParticles<float, 3>* CollisionParticles, const bool Kinematic, const bool Disabled);

    Apeiron::TPBDRigidParticles<float, 3>& BeginAddNewRigidParticles(const int32 Num, int32& Index, RigidBodyId& Id);
    Apeiron::TPBDRigidParticles<float, 3>& BeginUpdateRigidParticles(const TArray<RigidBodyId> Ids);

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

    void SetGravity(const Apeiron::TVector<float, 3>& Acceleration)
    {
        DelayedGravityAcceleration = Acceleration;
    }

    RigidConstraintId AddSpringConstraint(const Apeiron::TVector<RigidBodyId, 2>& Constraint)
    {
        MCriticalSection.Lock();
        RigidConstraintId Id(NextBodyIdValue++);
        ConstraintIdToIndexMap.Add(ToValue(Id), DelayedSpringConstraints.Num() + MSpringConstraints.Constraints().Num());
        ConstraintIds.Add(ToValue(Id));
        DelayedSpringConstraints.Add(Apeiron::TVector<int32, 2>(GetIndexFromId(Constraint[0]), GetIndexFromId(Constraint[1])));
        MCriticalSection.Unlock();
        return Id;
    }

    void RemoveSpringConstraint(const RigidConstraintId Constraint)
    {
        uint32 Index = GetConstraintIndexFromId(Constraint);
        if (Index >= static_cast<uint32>(MSpringConstraints.Constraints().Num()))
        {
            DelayedSpringConstraints.RemoveAt(Index - MSpringConstraints.Constraints().Num());
        }
        else
        {
            DelayedRemoveSpringConstraints.Add(Index);
        }
    }

    void AddForce(const Apeiron::TVector<float, 3>& Force, RigidBodyId BodyId)
    {
        MCriticalSection.Lock();
        DelayedForce[GetIndexFromId(BodyId)] += Force;
        MCriticalSection.Unlock();
    }

    void AddTorque(const Apeiron::TVector<float, 3>& Torque, RigidBodyId BodyId)
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

    void SetBodyInstance(FBodyInstance* OwningInstance, RigidBodyId Id)
    {
        const auto& Index = GetIndexFromId(Id);
        if (Index < Scene.GetRigidParticles().Size())
        {
            DelayedUpdateBodyInstances[Index] = OwningInstance;
        }
        {
            DelayedBodyInstances[Index - Scene.GetRigidParticles().Size()] = OwningInstance;
        }
    }

    ENGINE_API void SyncBodies();

    // Interface needed for interface
	static ENGINE_API FPhysicsActorHandle CreateActor(const FActorCreationParams& Params);
	static ENGINE_API void ReleaseActor(FPhysicsActorReference_Apeiron& InActorReference, FPhysScene* InScene = nullptr, bool bNeverDeferRelease=false);

	static ENGINE_API FPhysicsAggregateReference_Apeiron CreateAggregate(int32 MaxBodies);
	static ENGINE_API void ReleaseAggregate(FPhysicsAggregateReference_Apeiron& InAggregate);
	static ENGINE_API int32 GetNumActorsInAggregate(const FPhysicsAggregateReference_Apeiron& InAggregate);
	static ENGINE_API void AddActorToAggregate_AssumesLocked(const FPhysicsAggregateReference_Apeiron& InAggregate, const FPhysicsActorReference_Apeiron& InActor);

	// Material interface functions
    // @todo(mlentine): How do we set material on the solver?
    static FPhysicsMaterialHandle CreateMaterial(const UPhysicalMaterial* InMaterial) {}
    static void ReleaseMaterial(FPhysicsMaterialHandle& InHandle) {}
    static void UpdateMaterial(const FPhysicsMaterialHandle& InHandle, UPhysicalMaterial* InMaterial) {}
    static void SetUserData(const FPhysicsMaterialHandle& InHandle, void* InUserData) {}

	// Actor interface functions
	template<typename AllocatorType>
	static int32 GetAllShapes_AssumedLocked(const FPhysicsActorReference_Apeiron& InActorReference, TArray<FPhysicsShapeHandle, AllocatorType>& OutShapes, EPhysicsSceneType InSceneType = PST_MAX);
	static void GetNumShapes(const FPhysicsActorHandle& InHandle, int32& OutNumSyncShapes, int32& OutNumAsyncShapes);

	static void ReleaseShape(const FPhysicsShapeHandle& InShape);

	static void AttachShape(const FPhysicsActorHandle& InActor, const FPhysicsShapeHandle& InNewShape);
	static void AttachShape(const FPhysicsActorHandle& InActor, const FPhysicsShapeHandle& InNewShape, EPhysicsSceneType SceneType);
	static void DetachShape(const FPhysicsActorHandle& InActor, FPhysicsShapeHandle& InShape, bool bWakeTouching = true);

	static ENGINE_API void SetActorUserData_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, FPhysxUserData* InUserData);

	static ENGINE_API bool IsRigidBody(const FPhysicsActorReference_Apeiron& InActorReference);
	static ENGINE_API bool IsDynamic(const FPhysicsActorReference_Apeiron& InActorReference)
    {
        return !IsStatic(InActorReference);
    }
    static ENGINE_API bool IsStatic(const FPhysicsActorReference_Apeiron& InActorReference);
	static ENGINE_API bool IsKinematic_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference);
	static ENGINE_API bool IsSleeping(const FPhysicsActorReference_Apeiron& InActorReference);
	static ENGINE_API bool IsCcdEnabled(const FPhysicsActorReference_Apeiron& InActorReference);
    // @todo(mlentine): We don't have a notion of sync vs async and are a bit of both. Does this work?
    static bool HasSyncSceneData(const FPhysicsActorReference_Apeiron& InHandle) { return true; }
    static bool HasAsyncSceneData(const FPhysicsActorReference_Apeiron& InHandle) { return false; }
	static ENGINE_API bool IsInScene(const FPhysicsActorReference_Apeiron& InActorReference);
	static ENGINE_API bool CanSimulate_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference);
	static ENGINE_API float GetMass_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference);

	static ENGINE_API void SetSendsSleepNotifies_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, bool bSendSleepNotifies);
	static ENGINE_API void PutToSleep_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference);
	static ENGINE_API void WakeUp_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference);

	static ENGINE_API void SetIsKinematic_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, bool bIsKinematic);
	static ENGINE_API void SetCcdEnabled_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, bool bIsCcdEnabled);

	static ENGINE_API FTransform GetGlobalPose_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference);
	static ENGINE_API void SetGlobalPose_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, const FTransform& InNewPose, bool bAutoWake = true);

    static ENGINE_API FTransform GetTransform_AssumesLocked(const FPhysicsActorHandle& InRef, bool bForceGlobalPose = false);

	static ENGINE_API bool HasKinematicTarget_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference);
	static ENGINE_API FTransform GetKinematicTarget_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference);
	static ENGINE_API void SetKinematicTarget_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, const FTransform& InNewTarget);

	static ENGINE_API FVector GetLinearVelocity_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference);
	static ENGINE_API void SetLinearVelocity_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, const FVector& InNewVelocity, bool bAutoWake = true);

	static ENGINE_API FVector GetAngularVelocity_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference);
	static ENGINE_API void SetAngularVelocity_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, const FVector& InNewVelocity, bool bAutoWake = true);
	static ENGINE_API float GetMaxAngularVelocity_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference);
	static ENGINE_API void SetMaxAngularVelocity_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, float InMaxAngularVelocity);

	static ENGINE_API float GetMaxDepenetrationVelocity_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference);
	static ENGINE_API void SetMaxDepenetrationVelocity_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, float InMaxDepenetrationVelocity);

	static ENGINE_API FVector GetWorldVelocityAtPoint_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, const FVector& InPoint);

	static ENGINE_API FTransform GetComTransform_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference);
	static ENGINE_API FTransform GetComTransformLocal_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference);

	static ENGINE_API FVector GetLocalInertiaTensor_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference);
	static ENGINE_API FBox GetBounds_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference);

	static ENGINE_API void SetLinearDamping_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, float InDamping);
	static ENGINE_API void SetAngularDamping_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, float InDamping);

	static ENGINE_API void AddForce_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, const FVector& InForce);
	static ENGINE_API void AddTorque_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, const FVector& InTorque);
	static ENGINE_API void AddForceMassIndependent_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, const FVector& InForce);
	static ENGINE_API void AddTorqueMassIndependent_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, const FVector& InTorque);
	static ENGINE_API void AddImpulseAtLocation_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, const FVector& InImpulse, const FVector& InLocation);
	static ENGINE_API void AddRadialImpulse_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, const FVector& InOrigin, float InRadius, float InStrength, ERadialImpulseFalloff InFalloff, bool bInVelChange);

	static ENGINE_API bool IsGravityEnabled_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference);
	static ENGINE_API void SetGravityEnabled_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, bool bEnabled);

	static ENGINE_API float GetSleepEnergyThreshold_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference);
	static ENGINE_API void SetSleepEnergyThreshold_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, float InEnergyThreshold);

	static void SetMass_AssumesLocked(const FPhysicsActorReference_Apeiron& InHandle, float InMass);
	static void SetMassSpaceInertiaTensor_AssumesLocked(const FPhysicsActorReference_Apeiron& InHandle, const FVector& InTensor);
	static void SetComLocalPose_AssumesLocked(const FPhysicsActorReference_Apeiron& InHandle, const FTransform& InComLocalPose);

	static ENGINE_API float GetStabilizationEnergyThreshold_AssumesLocked(const FPhysicsActorReference_Apeiron& InHandle);
	static ENGINE_API void SetStabilizationEnergyThreshold_AssumesLocked(const FPhysicsActorReference_Apeiron& InHandle, float InThreshold);
	static ENGINE_API uint32 GetSolverPositionIterationCount_AssumesLocked(const FPhysicsActorReference_Apeiron& InHandle);
	static ENGINE_API void SetSolverPositionIterationCount_AssumesLocked(const FPhysicsActorReference_Apeiron& InHandle, uint32 InSolverIterationCount);
	static ENGINE_API uint32 GetSolverVelocityIterationCount_AssumesLocked(const FPhysicsActorReference_Apeiron& InHandle);
	static ENGINE_API void SetSolverVelocityIterationCount_AssumesLocked(const FPhysicsActorReference_Apeiron& InHandle, uint32 InSolverIterationCount);
	static ENGINE_API float GetWakeCounter_AssumesLocked(const FPhysicsActorReference_Apeiron& InHandle);
	static ENGINE_API void SetWakeCounter_AssumesLocked(const FPhysicsActorReference_Apeiron& InHandle, float InWakeCounter);

	static ENGINE_API SIZE_T GetResourceSizeEx(const FPhysicsActorReference_Apeiron& InActorRef);
	
    static ENGINE_API FPhysicsConstraintReference_Apeiron CreateConstraint(const FPhysicsActorReference_Apeiron& InActorRef1, const FPhysicsActorReference_Apeiron& InActorRef2, const FTransform& InLocalFrame1, const FTransform& InLocalFrame2);
	static ENGINE_API void SetConstraintUserData(const FPhysicsConstraintReference_Apeiron& InConstraintRef, void* InUserData);
	static ENGINE_API void ReleaseConstraint(FPhysicsConstraintReference_Apeiron& InConstraintRef);

	static ENGINE_API FTransform GetLocalPose(const FPhysicsConstraintReference_Apeiron& InConstraintRef, EConstraintFrame::Type InFrame);
	static ENGINE_API FTransform GetGlobalPose(const FPhysicsConstraintReference_Apeiron& InConstraintRef, EConstraintFrame::Type InFrame);
	static ENGINE_API FVector GetLocation(const FPhysicsConstraintReference_Apeiron& InConstraintRef);
	static ENGINE_API void GetForce(const FPhysicsConstraintReference_Apeiron& InConstraintRef, FVector& OutLinForce, FVector& OutAngForce);
	static ENGINE_API void GetDriveLinearVelocity(const FPhysicsConstraintReference_Apeiron& InConstraintRef, FVector& OutLinVelocity);
	static ENGINE_API void GetDriveAngularVelocity(const FPhysicsConstraintReference_Apeiron& InConstraintRef, FVector& OutAngVelocity);

	static ENGINE_API float GetCurrentSwing1(const FPhysicsConstraintReference_Apeiron& InConstraintRef);
	static ENGINE_API float GetCurrentSwing2(const FPhysicsConstraintReference_Apeiron& InConstraintRef);
	static ENGINE_API float GetCurrentTwist(const FPhysicsConstraintReference_Apeiron& InConstraintRef);

	static ENGINE_API void SetCanVisualize(const FPhysicsConstraintReference_Apeiron& InConstraintRef, bool bInCanVisualize);
	static ENGINE_API void SetCollisionEnabled(const FPhysicsConstraintReference_Apeiron& InConstraintRef, bool bInCollisionEnabled);
	static ENGINE_API void SetProjectionEnabled_AssumesLocked(const FPhysicsConstraintReference_Apeiron& InConstraintRef, bool bInProjectionEnabled, float InLinearTolerance = 0.0f, float InAngularToleranceDegrees = 0.0f);
	static ENGINE_API void SetParentDominates_AssumesLocked(const FPhysicsConstraintReference_Apeiron& InConstraintRef, bool bInParentDominates);
	static ENGINE_API void SetBreakForces_AssumesLocked(const FPhysicsConstraintReference_Apeiron& InConstraintRef, float InLinearBreakForce, float InAngularBreakForce);
	static ENGINE_API void SetLocalPose(const FPhysicsConstraintReference_Apeiron& InConstraintRef, const FTransform& InPose, EConstraintFrame::Type InFrame);

	static ENGINE_API void SetLinearMotionLimitType_AssumesLocked(const FPhysicsConstraintReference_Apeiron& InConstraintRef, PhysicsInterfaceTypes::ELimitAxis InAxis, ELinearConstraintMotion InMotion);
	static ENGINE_API void SetAngularMotionLimitType_AssumesLocked(const FPhysicsConstraintReference_Apeiron& InConstraintRef, PhysicsInterfaceTypes::ELimitAxis InAxis, EAngularConstraintMotion InMotion);

	static ENGINE_API void UpdateLinearLimitParams_AssumesLocked(const FPhysicsConstraintReference_Apeiron& InConstraintRef, float InLimit, float InAverageMass, const FLinearConstraint& InParams);
	static ENGINE_API void UpdateConeLimitParams_AssumesLocked(const FPhysicsConstraintReference_Apeiron& InConstraintRef, float InAverageMass, const FConeConstraint& InParams);
	static ENGINE_API void UpdateTwistLimitParams_AssumesLocked(const FPhysicsConstraintReference_Apeiron& InConstraintRef, float InAverageMass, const FTwistConstraint& InParams);
	static ENGINE_API void UpdateLinearDrive_AssumesLocked(const FPhysicsConstraintReference_Apeiron& InConstraintRef, const FLinearDriveConstraint& InDriveParams);
	static ENGINE_API void UpdateAngularDrive_AssumesLocked(const FPhysicsConstraintReference_Apeiron& InConstraintRef, const FAngularDriveConstraint& InDriveParams);
	static ENGINE_API void UpdateDriveTarget_AssumesLocked(const FPhysicsConstraintReference_Apeiron& InConstraintRef, const FLinearDriveConstraint& InLinDrive, const FAngularDriveConstraint& InAngDrive);
	static ENGINE_API void SetDrivePosition(const FPhysicsConstraintReference_Apeiron& InConstraintRef, const FVector& InPosition);
	static ENGINE_API void SetDriveOrientation(const FPhysicsConstraintReference_Apeiron& InConstraintRef, const FQuat& InOrientation);
	static ENGINE_API void SetDriveLinearVelocity(const FPhysicsConstraintReference_Apeiron& InConstraintRef, const FVector& InLinVelocity);
	static ENGINE_API void SetDriveAngularVelocity(const FPhysicsConstraintReference_Apeiron& InConstraintRef, const FVector& InAngVelocity);

	static ENGINE_API void SetTwistLimit(const FPhysicsConstraintReference_Apeiron& InConstraintRef, float InLowerLimit, float InUpperLimit, float InContactDistance);
	static ENGINE_API void SetSwingLimit(const FPhysicsConstraintReference_Apeiron& InConstraintRef, float InYLimit, float InZLimit, float InContactDistance);
	static ENGINE_API void SetLinearLimit(const FPhysicsConstraintReference_Apeiron& InConstraintRef, float InLimit);

	static ENGINE_API bool IsBroken(const FPhysicsConstraintReference_Apeiron& InConstraintRef);

	static ENGINE_API bool ExecuteOnUnbrokenConstraintReadOnly(const FPhysicsConstraintReference_Apeiron& InConstraintRef, TFunctionRef<void(const FPhysicsConstraintReference_Apeiron&)> Func);
	static ENGINE_API bool ExecuteOnUnbrokenConstraintReadWrite(const FPhysicsConstraintReference_Apeiron& InConstraintRef, TFunctionRef<void(const FPhysicsConstraintReference_Apeiron&)> Func);

    /////////////////////////////////////////////

    // Interface needed for cmd
    static ENGINE_API bool ExecuteRead(const FPhysicsActorReference_Apeiron& InActorReference, TFunctionRef<void(const FPhysicsActorReference_Apeiron& Actor)> InCallable)
    {
        InCallable(InActorReference);
        return true;
    }

    static ENGINE_API bool ExecuteRead(USkeletalMeshComponent* InMeshComponent, TFunctionRef<void()> InCallable)
    {
        InCallable();
        return true;
    }

    static ENGINE_API bool ExecuteRead(const FPhysicsActorReference_Apeiron& InActorReferenceA, const FPhysicsActorReference_Apeiron& InActorReferenceB, TFunctionRef<void(const FPhysicsActorReference_Apeiron& ActorA, const FPhysicsActorReference_Apeiron& ActorB)> InCallable)
    {
        InCallable(InActorReferenceA, InActorReferenceB);
        return true;
    }

    static ENGINE_API bool ExecuteRead(const FPhysicsConstraintReference_Apeiron& InConstraintRef, TFunctionRef<void(const FPhysicsConstraintReference_Apeiron& Constraint)> InCallable)
    {
        InCallable(InConstraintRef);
        return true;
    }

    static ENGINE_API bool ExecuteRead(FPhysScene* InScene, TFunctionRef<void()> InCallable)
    {
        InCallable();
        return true;
    }

    static ENGINE_API bool ExecuteWrite(const FPhysicsActorReference_Apeiron& InActorReference, TFunctionRef<void(const FPhysicsActorReference_Apeiron& Actor)> InCallable)
    {
        InCallable(InActorReference);
        return true;
    }

    static ENGINE_API bool ExecuteWrite(USkeletalMeshComponent* InMeshComponent, TFunctionRef<void()> InCallable)
    {
        InCallable();
        return true;
    }

    static ENGINE_API bool ExecuteWrite(const FPhysicsActorReference_Apeiron& InActorReferenceA, const FPhysicsActorReference_Apeiron& InActorReferenceB, TFunctionRef<void(const FPhysicsActorReference_Apeiron& ActorA, const FPhysicsActorReference_Apeiron& ActorB)> InCallable)
    {
        InCallable(InActorReferenceA, InActorReferenceB);
        return true;
    }

    static ENGINE_API bool ExecuteWrite(const FPhysicsConstraintReference_Apeiron& InConstraintRef, TFunctionRef<void(const FPhysicsConstraintReference_Apeiron& Constraint)> InCallable)
    {
        InCallable(InConstraintRef);
        return true;
    }
	
    static ENGINE_API bool ExecuteWrite(FPhysScene* InScene, TFunctionRef<void()> InCallable)
    {
        InCallable();
        return true;
    }

    static void ExecuteShapeWrite(FBodyInstance* InInstance, FPhysicsShapeHandle& InShape, TFunctionRef<void(const FPhysicsShapeHandle& InShape)> InCallable)
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

    static const Apeiron::TPBDRigidParticles<float, 3>& GetParticlesAndIndex(const FPhysicsActorReference_Apeiron& InActorReference, uint32& Index);
    static const TArray<Apeiron::TVector<int32, 2>>& GetConstraintArrayAndIndex(const FPhysicsConstraintReference_Apeiron& InActorReference, uint32& Index);

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
    void AddAggregateToScene(const FPhysicsAggregateHandle& InAggregate, bool bUseAsyncScene) {}

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

    DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPhysScenePreTick, FPhysInterface_Apeiron*, uint32 /*SceneType*/, float /*DeltaSeconds*/);
    FOnPhysScenePreTick OnPhysScenePreTick;
    DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPhysSceneStep, FPhysInterface_Apeiron*, uint32 /*SceneType*/, float /*DeltaSeconds*/);
    FOnPhysSceneStep OnPhysSceneStep;

    ENGINE_API bool ExecPxVis(uint32 SceneType, const TCHAR* Cmd, FOutputDevice* Ar);
    ENGINE_API bool ExecApexVis(uint32 SceneType, const TCHAR* Cmd, FOutputDevice* Ar);

private:
    FPhysScene_Apeiron Scene;

    // @todo(mlentine): Locking is very heavy handed right now; need to make less so.
    FCriticalSection MCriticalSection;
    float MDeltaTime;
    TMap<uint32, uint32> IdToIndexMap;
    TMap<uint32, uint32> ConstraintIdToIndexMap;
    TArray<uint32> ConstraintIds;
    TArray<Apeiron::TRigidTransform<float, 3>> OldAnimationTransforms;
    TArray<Apeiron::TRigidTransform<float, 3>> NewAnimationTransforms;
    TArray<Apeiron::TRigidTransform<float, 3>> DelayedAnimationTransforms;
    Apeiron::TPBDRigidParticles<float, 3> DelayedNewParticles;
    Apeiron::TPBDRigidParticles<float, 3> DelayedUpdateParticles;
    TSet<int32> DelayedUpdateIndices;
    //Collisions
    TArray<TTuple<int32, int32>> DelayedDisabledCollisions;
    TArray<TTuple<int32, int32>> DelayedEnabledCollisions;
    //Gravity
    Apeiron::TVector<float, 3> DelayedGravityAcceleration;
    Apeiron::PerParticleGravity<float, 3> MGravity;
	//Springs
    TArray<Apeiron::TVector<int32, 2>> DelayedSpringConstraints;
    TArray<uint32> DelayedRemoveSpringConstraints;
    Apeiron::TPBDSpringConstraints<float, 3> MSpringConstraints;
    //Force
    TArray<Apeiron::TVector<float, 3>> DelayedForce;
    TArray<Apeiron::TVector<float, 3>> DelayedTorque;
    //Body Instances
    Apeiron::TArrayCollectionArray<FBodyInstance*> BodyInstances;
    Apeiron::TArrayCollectionArray<FBodyInstance*> DelayedBodyInstances;
    Apeiron::TArrayCollectionArray<FBodyInstance*> DelayedUpdateBodyInstances;
    // Temp Interface
    UWorld* MOwningWorld;
    TArray<FCollisionNotifyInfo> MNotifies;
};

#endif
