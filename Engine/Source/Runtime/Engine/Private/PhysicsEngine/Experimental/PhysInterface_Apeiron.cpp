// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#if WITH_APEIRON

#include "Physics/Experimental/PhysInterface_Apeiron.h"
#include "Physics/PhysicsInterfaceTypes.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "Apeiron/Box.h"
#include "Apeiron/Cylinder.h"
#include "Apeiron/ImplicitObjectTransformed.h"
#include "Apeiron/ImplicitObjectUnion.h"
#include "Apeiron/Levelset.h"
#include "Apeiron/Sphere.h"

#include "Async/ParallelFor.h"
#include "Components/PrimitiveComponent.h"

#if WITH_PHYSX
#include "geometry/PxConvexMesh.h"
#include "geometry/PxTriangleMesh.h"
#include "foundation/PxVec3.h"
#include "extensions/PxMassProperties.h"
#endif

#define FORCE_ANALYTICS 0

DEFINE_STAT(STAT_TotalPhysicsTime);
DEFINE_STAT(STAT_NumCloths);
DEFINE_STAT(STAT_NumClothVerts);

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

float DebugLineLifetime = 2.f;

static void CopyParticleData(Apeiron::TPBDRigidParticles<float, 3>& ToParticles, const int32 ToIndex, Apeiron::TPBDRigidParticles<float, 3>& FromParticles, const int32 FromIndex)
{
    ToParticles.X(ToIndex) = FromParticles.X(FromIndex);
    ToParticles.R(ToIndex) = FromParticles.R(FromIndex);
    ToParticles.V(ToIndex) = FromParticles.V(FromIndex);
    ToParticles.W(ToIndex) = FromParticles.W(FromIndex);
    ToParticles.M(ToIndex) = FromParticles.M(FromIndex);
    ToParticles.InvM(ToIndex) = FromParticles.InvM(FromIndex);
    ToParticles.I(ToIndex) = FromParticles.I(FromIndex);
    ToParticles.InvI(ToIndex) = FromParticles.InvI(FromIndex);
    if (ToParticles.Geometry(ToIndex) != FromParticles.Geometry(FromIndex))
    {
        delete ToParticles.Geometry(ToIndex);
    }
    ToParticles.Geometry(ToIndex) = FromParticles.Geometry(FromIndex);
    ToParticles.CollisionParticles(ToIndex) = MoveTemp(FromParticles.CollisionParticles(FromIndex));
    ToParticles.Disabled(ToIndex) = FromParticles.Disabled(FromIndex);
    ToParticles.Sleeping(ToIndex) = FromParticles.Sleeping(FromIndex);
}

const Apeiron::TPBDRigidParticles<float, 3>& FPhysInterface_Apeiron::GetParticlesAndIndex(const FPhysicsActorReference_Apeiron& InActorReference, uint32& Index)
{
    Index = InActorReference.Second->GetIndexFromId(InActorReference.First);
    if (InActorReference.Second->DelayedUpdateIndices.Contains(Index))
    {
        return InActorReference.Second->DelayedUpdateParticles;
    }
    uint32 NumParticles = InActorReference.Second->Scene.GetRigidParticles().Size();
    if (Index >= NumParticles)
    {
        Index -= NumParticles;
        return InActorReference.Second->DelayedNewParticles;
    }
    return InActorReference.Second->Scene.GetRigidParticles();
}

const TArray<Apeiron::TVector<int32, 2>>& FPhysInterface_Apeiron::GetConstraintArrayAndIndex(const FPhysicsConstraintReference_Apeiron& InConstraintReference, uint32& Index)
{
    Index = InConstraintReference.Second->GetConstraintIndexFromId(InConstraintReference.First);
    uint32 NumConstraints = InConstraintReference.Second->MSpringConstraints.Constraints().Num();
    if (Index >= NumConstraints)
    {
        Index -= NumConstraints;
        return InConstraintReference.Second->DelayedSpringConstraints;
    }
    return InConstraintReference.Second->MSpringConstraints.Constraints();
}

bool FPhysicsActorReference_Apeiron::IsValid() const
{
    if (Second == nullptr)
    {
        return false;
    }
    uint32 Index = UINT_MAX;
	const auto& Particles = FPhysInterface_Apeiron::GetParticlesAndIndex(*this, Index);
    return !Particles.Disabled(Index);
}

#define IMPLEMENTIDSCENEPAIR(NAME) \
    bool NAME::IsValid() const \
    { \
        return Second != nullptr; \
    } \

IMPLEMENTIDSCENEPAIR(FPhysicsConstraintReference_Apeiron);
IMPLEMENTIDSCENEPAIR(FPhysicsAggregateReference_Apeiron);

FPhysInterface_Apeiron::FPhysInterface_Apeiron(const AWorldSettings* Settings)
{
    Scene.MEvolution->Particles().AddArray(&BodyInstances);
    DelayedNewParticles.AddArray(&DelayedBodyInstances);
    DelayedUpdateParticles.AddArray(&DelayedUpdateBodyInstances);

    Scene.SetKinematicUpdateFunction([&](Apeiron::TPBDRigidParticles<float, 3>& ParticlesInput, const float Dt, const float LocalTime, int32 Index) {
        if (ParticlesInput.InvM(Index) > 0)
            return;
        float Alpha = (LocalTime - Scene.MTime) / MDeltaTime;
        ParticlesInput.X(Index) = Alpha * NewAnimationTransforms[Index].GetTranslation() + (1 - Alpha) * OldAnimationTransforms[Index].GetTranslation();
        ParticlesInput.R(Index) = FQuat::Slerp(OldAnimationTransforms[Index].GetRotation(), NewAnimationTransforms[Index].GetRotation(), Alpha);
    });
    Scene.SetStartFrameFunction([&](const float DeltaTime) {
        MCriticalSection.Lock();
        MDeltaTime = DeltaTime;
        // Force Update
        MGravity.SetAcceleration(DelayedGravityAcceleration);
        int32 FirstNewIndex = MSpringConstraints.Constraints().Num();
        MSpringConstraints.Constraints().Append(DelayedSpringConstraints);
        for (int32 i = 0; i < DelayedRemoveSpringConstraints.Num(); ++i)
        {
            const int32 NewNum = MSpringConstraints.Constraints().Num() - 1;
            const int32 RemoveIndex = DelayedRemoveSpringConstraints[i];
            MSpringConstraints.Constraints()[RemoveIndex] = MSpringConstraints.Constraints().Last();
            MSpringConstraints.Constraints().SetNum(NewNum, false);
            const auto OldId = ConstraintIds[RemoveIndex];
            const auto RemapId = ConstraintIds[NewNum];
            ConstraintIds[RemoveIndex] = RemapId;
            ConstraintIds.SetNum(NewNum);
            ConstraintIdToIndexMap.Remove(OldId);
            ConstraintIdToIndexMap[RemapId] = RemoveIndex;
        }
        MSpringConstraints.UpdateDistances(Scene.MEvolution->Particles(), FirstNewIndex);
        // Animation Update
        OldAnimationTransforms = NewAnimationTransforms;
        NewAnimationTransforms = DelayedAnimationTransforms;
        if (NewAnimationTransforms.Num() > OldAnimationTransforms.Num())
        {
            int32 OldSize = OldAnimationTransforms.Num();
            OldAnimationTransforms.SetNum(NewAnimationTransforms.Num());
            for (int32 i = OldSize; i < OldAnimationTransforms.Num(); ++i)
            {
                OldAnimationTransforms[i] = NewAnimationTransforms[i];
            }
        }
        DelayedUpdateIndices.Reset();
        DelayedSpringConstraints.Reset();
        MCriticalSection.Unlock();
    });
    Scene.SetCreateBodiesFunction([&](Apeiron::TPBDRigidParticles<float, 3>& ParticlesInput) {
        MCriticalSection.Lock();
        int32 StartIndex = ParticlesInput.Size();
        ParticlesInput.AddParticles(DelayedNewParticles.Size());
        ParallelFor(DelayedNewParticles.Size(), [&](const int32 Index) {
            CopyParticleData(ParticlesInput, StartIndex + Index, DelayedNewParticles, Index);
            BodyInstances[StartIndex + Index] = DelayedBodyInstances[Index];
        });
        DelayedNewParticles.Resize(0);
        MCriticalSection.Unlock();
    });
    Scene.SetParameterUpdateFunction([&](Apeiron::TPBDRigidParticles<float, 3>& ParticlesInput, const float Time, const int32 Index) {
        MCriticalSection.Lock();
        if (DelayedUpdateIndices.Contains(Index))
        {
            CopyParticleData(ParticlesInput, Index, DelayedUpdateParticles, Index);
            BodyInstances[Index] = DelayedUpdateBodyInstances[Index];
        }
        MCriticalSection.Unlock();
    });
    Scene.SetDisableCollisionsUpdateFunction([&](TSet<TTuple<int32, int32>>& DisabledCollisions) {
        MCriticalSection.Lock();
        for (auto DisabledCollision : DelayedDisabledCollisions)
        {
            check(!DisabledCollisions.Contains(DisabledCollision));
            DisabledCollisions.Add(DisabledCollision);
        }
        for (auto EnabledCollision : DelayedEnabledCollisions)
        {
            check(DisabledCollisions.Contains(EnabledCollision));
            DisabledCollisions.Remove(EnabledCollision);
        }
        DelayedDisabledCollisions.Reset();
        DelayedEnabledCollisions.Reset();
        MCriticalSection.Unlock();
    });
    Scene.AddForceFunction([&](Apeiron::TPBDRigidParticles<float, 3>& ParticlesInput, const float Dt, const int32 Index) {
        MGravity.Apply(ParticlesInput, Dt, Index);
    });
    Scene.AddForceFunction([&](Apeiron::TPBDRigidParticles<float, 3>& ParticlesInput, const float Dt, const int32 Index) {
        MCriticalSection.Lock();
        ParticlesInput.F(Index) += DelayedForce[Index];
        ParticlesInput.Torque(Index) += DelayedTorque[Index];
        DelayedForce[Index] = Apeiron::TVector<float, 3>(0);
        DelayedTorque[Index] = Apeiron::TVector<float, 3>(0);
        MCriticalSection.Unlock();
    });
    Scene.AddPBDConstraintFunction([&](Apeiron::TPBDRigidParticles<float, 3>& ParticlesInput, const float Dt) {
        MSpringConstraints.Apply(ParticlesInput, Dt);
    });
	Scene.SetEndFrameFunction([this](const float EndFrame) {
	});
}

FPhysInterface_Apeiron::~FPhysInterface_Apeiron()
{
}

RigidBodyId FPhysInterface_Apeiron::AddNewRigidParticle(const Apeiron::TVector<float, 3>& X, const Apeiron::TRotation<float, 3>& R, const Apeiron::TVector<float, 3>& V, const Apeiron::TVector<float, 3>& W, const float M, const Apeiron::PMatrix<float, 3, 3>& I, Apeiron::TImplicitObject<float, 3>* Geometry, Apeiron::TBVHParticles<float, 3>* CollisionParticles, const bool Kinematic, const bool Disabled)
{
    MCriticalSection.Lock();
    const auto Index = DelayedNewParticles.Size();
    RigidBodyId Id(NextBodyIdValue++);
    IdToIndexMap.Add(ToValue(Id), Index + Scene.GetRigidParticles().Size());
    DelayedNewParticles.AddParticles(1);
    DelayedNewParticles.X(Index) = X;
    DelayedNewParticles.R(Index) = R;
    DelayedNewParticles.V(Index) = V;
    DelayedNewParticles.W(Index) = W;
    DelayedNewParticles.M(Index) = M;
    DelayedNewParticles.InvM(Index) = Kinematic ? 0 : 1 / M;
    DelayedNewParticles.I(Index) = I;
    DelayedNewParticles.InvI(Index) = Kinematic ? Apeiron::PMatrix<float, 3, 3>(0) : I.Inverse();
    DelayedNewParticles.Geometry(Index) = Geometry;
    if (CollisionParticles)
    {
        DelayedNewParticles.CollisionParticles(Index) = MoveTemp(*CollisionParticles);
    }
    DelayedNewParticles.Disabled(Index) = Disabled;
    DelayedUpdateParticles.AddParticles(1);
    DelayedForce.Add(Apeiron::TVector<float, 3>(0));
    DelayedTorque.Add(Apeiron::TVector<float, 3>(0));
    DelayedAnimationTransforms.SetNum(DelayedAnimationTransforms.Num() + 1);
    MCriticalSection.Unlock();
    return Id;
}

Apeiron::TPBDRigidParticles<float, 3>& FPhysInterface_Apeiron::BeginAddNewRigidParticles(const int32 Num, int32& Index, RigidBodyId& Id)
{
    MCriticalSection.Lock();
    Index = DelayedNewParticles.Size();
    Id = RigidBodyId(NextBodyIdValue++);
    IdToIndexMap.Add(ToValue(Id), Index + Scene.GetRigidParticles().Size());
    for (int32 i = 1; i < Num; ++i)
    {
        IdToIndexMap.Add(NextBodyIdValue++, i + Index + Scene.GetRigidParticles().Size());
    }
    DelayedNewParticles.AddParticles(Num);
    DelayedUpdateParticles.AddParticles(Num);
    int32 OldNum = DelayedForce.Num();
    DelayedForce.SetNum(OldNum + Num);
    DelayedTorque.SetNum(OldNum + Num);
    for (int32 i = 0; i < Num; ++i)
    {
        DelayedForce[i + OldNum] = Apeiron::TVector<float, 3>(0);
        DelayedTorque[i + OldNum] = Apeiron::TVector<float, 3>(0);
    }
    DelayedAnimationTransforms.SetNum(DelayedAnimationTransforms.Num() + Num);
    return DelayedNewParticles;
}

Apeiron::TPBDRigidParticles<float, 3>& FPhysInterface_Apeiron::BeginUpdateRigidParticles(const TArray<RigidBodyId> Ids)
{
    MCriticalSection.Lock();
    for (const auto Id : Ids)
    {
        const auto Index = GetIndexFromId(Id);
        if (DelayedUpdateIndices.Contains(Index))
        {
            continue;
        }
        if (Index < Scene.GetRigidParticles().Size())
        {
            CopyParticleData(DelayedUpdateParticles, Index, Scene.MEvolution->Particles(), Index);
            DelayedUpdateBodyInstances[Index] = BodyInstances[Index];
        }
        else
        {
            CopyParticleData(DelayedUpdateParticles, Index, DelayedNewParticles, Index - Scene.GetRigidParticles().Size());
            DelayedUpdateBodyInstances[Index] = DelayedBodyInstances[Index - Scene.GetRigidParticles().Size()];
        }
        DelayedUpdateIndices.Add(Index);
    }
    return DelayedUpdateParticles;
}

/** Struct to remember a pending component transform change */
struct FPhysScenePendingComponentTransform_Apeiron
{
	/** Component to move */
	TWeakObjectPtr<UPrimitiveComponent> OwningComp;
	/** New transform from physics engine */
	FTransform NewTransform;

	FPhysScenePendingComponentTransform_Apeiron(UPrimitiveComponent* InOwningComp, const FTransform& InNewTransform)
		: OwningComp(InOwningComp)
		, NewTransform(InNewTransform)
	{}
};

void FPhysInterface_Apeiron::SyncBodies()
{
	TArray<FPhysScenePendingComponentTransform_Apeiron> PendingTransforms;
	
	for (uint32 Index = 0; Index < Scene.GetRigidParticles().Size(); ++Index)
	{
		if (BodyInstances[Index]) 
		{
			Apeiron::TRigidTransform<float, 3> NewTransform(Scene.GetRigidParticles().X(Index), Scene.GetRigidParticles().R(Index));
			FPhysScenePendingComponentTransform_Apeiron NewEntry(BodyInstances[Index]->OwnerComponent.Get(), NewTransform);
			PendingTransforms.Add(NewEntry);
		}
	}

	for (FPhysScenePendingComponentTransform_Apeiron& Entry : PendingTransforms)
	{
		UPrimitiveComponent* OwnerComponent = Entry.OwningComp.Get();
		if (OwnerComponent != nullptr)
		{
			AActor* Owner = OwnerComponent->GetOwner();

			if (!Entry.NewTransform.EqualsNoScale(OwnerComponent->GetComponentTransform()))
			{
				const FVector MoveBy = Entry.NewTransform.GetLocation() - OwnerComponent->GetComponentTransform().GetLocation();
				const FQuat NewRotation = Entry.NewTransform.GetRotation();

				OwnerComponent->MoveComponent(MoveBy, NewRotation, false, NULL, MOVECOMP_SkipPhysicsMove);
			}

			if (Owner != NULL && !Owner->IsPendingKill())
			{
				Owner->CheckStillInWorld();
			}
		}
	}
}

void FPhysInterface_Apeiron::SetKinematicTarget_AssumesLocked(FBodyInstance* BodyInstance, const FTransform& TargetTM, bool bAllowSubstepping)
{
    check(BodyInstance->ActorHandle.Second == this);
    SetKinematicTransform(BodyInstance->ActorHandle.First, TargetTM);
}

bool FPhysInterface_Apeiron::GetKinematicTarget_AssumesLocked(const FBodyInstance* BodyInstance, FTransform& OutTM) const
{
    OutTM = GetKinematicTarget_AssumesLocked(BodyInstance->ActorHandle);
    return true;
}

void FPhysInterface_Apeiron::AddForce_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Force, bool bAllowSubstepping, bool bAccelChange)
{
    check(!bAccelChange);
    check(BodyInstance->ActorHandle.Second == this);
    AddForce(Force, BodyInstance->ActorHandle.First);
}

void FPhysInterface_Apeiron::AddForceAtPosition_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Force, const FVector& Position, bool bAllowSubstepping, bool bIsLocalForce)
{
    check(!bIsLocalForce);
    check(BodyInstance->ActorHandle.Second == this);
    RigidBodyId Id = BodyInstance->ActorHandle.First;
    AddTorque(Apeiron::TVector<float, 3>::CrossProduct(Position - Scene.GetRigidParticles().X(GetIndexFromId(Id)), Force), Id);
    AddForce(Force, Id);
}

void FPhysInterface_Apeiron::AddRadialForceToBody_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Origin, const float Radius, const float Strength, const uint8 Falloff, bool bAccelChange, bool bAllowSubstepping)
{
    check(BodyInstance->ActorHandle.Second == this);
    RigidBodyId Id = BodyInstance->ActorHandle.First;
    uint32 Index = GetIndexFromId(Id);
    Apeiron::TVector<float, 3> Direction = (static_cast<FVector>(Scene.GetRigidParticles().X(Index)) - Origin);
    const float Distance = Direction.Size();
    if (Distance > Radius)
    {
        return;
    }
    Direction = Direction.GetSafeNormal();
    Apeiron::TVector<float, 3> Force(0);
    check(Falloff == RIF_Constant || Falloff == RIF_Linear);
    if (Falloff == RIF_Constant)
    {
        Force = Strength * Direction;
    }
    if (Falloff == RIF_Linear)
    {
        Force = (Radius - Distance) / Radius * Strength * Direction;
    }
    AddForce(bAccelChange ? (Force * Scene.GetRigidParticles().M(Index)) : Force, Id);
}

void FPhysInterface_Apeiron::ClearForces_AssumesLocked(FBodyInstance* BodyInstance, bool bAllowSubstepping)
{
    BodyInstance->ActorHandle.Second->DelayedForce[GetIndexFromId(BodyInstance->ActorHandle.First)] += FVector(0);
}

void FPhysInterface_Apeiron::AddTorque_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Torque, bool bAllowSubstepping, bool bAccelChange)
{
    check(BodyInstance->ActorHandle.Second == this);
    AddTorque(Torque, BodyInstance->ActorHandle.First);
}

void FPhysInterface_Apeiron::ClearTorques_AssumesLocked(FBodyInstance* BodyInstance, bool bAllowSubstepping)
{
    BodyInstance->ActorHandle.Second->DelayedTorque[GetIndexFromId(BodyInstance->ActorHandle.First)] += FVector(0);
}

void FPhysInterface_Apeiron::AddActorsToScene_AssumesLocked(const TArray<FPhysicsActorHandle>& InActors)
{
    for (const auto& Actor : InActors)
    {
        check(Actor.Second);
    }
}

void FPhysInterface_Apeiron::RemoveBodyInstanceFromPendingLists_AssumesLocked(FBodyInstance* BodyInstance, int32 SceneType)
{
}

// Interface functions
FPhysicsActorHandle FPhysInterface_Apeiron::CreateActor(const FActorCreationParams& Params)
{
    RigidBodyId Id;
    int32 Index;
    auto& Particles = Params.Scene->BeginAddNewRigidParticles(1, Index, Id);
    Particles.X(Index) = Params.InitialTM.GetTranslation();
    Particles.R(Index) = Params.InitialTM.GetRotation();
    Particles.M(Index) = 1;
    Particles.I(Index) = FMatrix::Identity;
    Particles.V(Index) = Apeiron::TVector<float, 3>(0);
    Particles.W(Index) = Apeiron::TVector<float, 3>(0);
    if (Params.bStatic)
    {
        Particles.InvM(Index) = 0;
        Particles.InvI(Index) = Apeiron::PMatrix<float, 3, 3>(0);
    }
    else
    {
        Particles.InvM(Index) = 1 / Particles.M(Index);
        Particles.InvI(Index) = Particles.I(Index).Inverse();
    }
    if (Params.bQueryOnly)
    {
        Particles.Disabled(Index) = true;
    }
    else
    {
        Particles.Disabled(Index) = false;
    }
    Params.Scene->EndAddNewRigidParticles();
    check(Params.bEnableGravity);
    FPhysicsActorHandle NewActor;
    NewActor.First = Id;
    NewActor.Second = Params.Scene;
    return NewActor;
}
void FPhysInterface_Apeiron::ReleaseActor(FPhysicsActorReference_Apeiron& InActorReference, FPhysScene* InScene, bool bNeverDerferRelease)
{
    // @todo(mlentine): Actually delete body
    check(InScene == InActorReference.Second);
    TArray<RigidBodyId> BodiesToTerminate = { InActorReference.First };
    auto& Particles = InScene->BeginUpdateRigidParticles(BodiesToTerminate);
    Particles.Disabled(InScene->GetIndexFromId(InActorReference.First)) = true;
    InScene->EndUpdateRigidParticles();
}

// Aggregate is not relevant for Apeiron yet
FPhysicsAggregateReference_Apeiron FPhysInterface_Apeiron::CreateAggregate(int32 MaxBodies)
{
    FPhysicsAggregateReference_Apeiron NewAggregate;
    NewAggregate.First = RigidAggregateId(0);
    NewAggregate.Second = nullptr;
    return NewAggregate;
}
void FPhysInterface_Apeiron::ReleaseAggregate(FPhysicsAggregateReference_Apeiron& InAggregate) {}
int32 FPhysInterface_Apeiron::GetNumActorsInAggregate(const FPhysicsAggregateReference_Apeiron& InAggregate) { return 0; }
void FPhysInterface_Apeiron::AddActorToAggregate_AssumesLocked(const FPhysicsAggregateReference_Apeiron& InAggregate, const FPhysicsActorReference_Apeiron& InActor) {}

// Actor interface functions
template<typename AllocatorType>
int32 FPhysInterface_Apeiron::GetAllShapes_AssumedLocked(const FPhysicsActorReference_Apeiron& InActorReference, TArray<FPhysicsShapeHandle, AllocatorType>& OutShapes, EPhysicsSceneType InSceneType)
{
    OutShapes.Reset();
    uint32 Index;
    const auto& LocalParticles = InActorReference.Second->GetParticlesAndIndex(InActorReference, Index);
    if (LocalParticles.Geometry(Index))
    {
        FPhysicsShapeHandle NewShape;
        NewShape.bSimulation = true;
        NewShape.bQuery = true;
        NewShape.Object = LocalParticles.Geometry(Index);
        OutShapes.Add(NewShape);
    }
    return OutShapes.Num();
}

void FPhysInterface_Apeiron::GetNumShapes(const FPhysicsActorHandle& InHandle, int32& OutNumSyncShapes, int32& OutNumAsyncShapes)
{
    uint32 Index;
    OutNumAsyncShapes = 0;
    const auto& LocalParticles = InHandle.Second->GetParticlesAndIndex(InHandle, Index);
    if (LocalParticles.Geometry(Index))
    {
        OutNumSyncShapes = 1;
    }
    else
    {
        OutNumSyncShapes = 0;
    }
}

void FPhysInterface_Apeiron::ReleaseShape(const FPhysicsShapeHandle& InShape)
{
    check(!InShape.ActorRef.IsValid());
    delete InShape.Object;
}

void FPhysInterface_Apeiron::AttachShape(const FPhysicsActorHandle& InActor, const FPhysicsShapeHandle& InNewShape)
{
    const_cast<FPhysicsShapeHandle&>(InNewShape).ActorRef = InActor;
    TArray<RigidBodyId> Ids = { InActor.First };
    auto& LocalParticles = InActor.Second->BeginUpdateRigidParticles(Ids);
    uint32 Index = InActor.Second->GetIndexFromId(InActor.First);
    check(LocalParticles.Geometry(Index) == nullptr);
    LocalParticles.Geometry(Index) = InNewShape.Object;
    InActor.Second->EndUpdateRigidParticles();
}

void FPhysInterface_Apeiron::AttachShape(const FPhysicsActorHandle& InActor, const FPhysicsShapeHandle& InNewShape, EPhysicsSceneType SceneType)
{
    AttachShape(InActor, InNewShape);
}

void FPhysInterface_Apeiron::DetachShape(const FPhysicsActorHandle& InActor, FPhysicsShapeHandle& InShape, bool bWakeTouching)
{
    TArray<RigidBodyId> Ids = { InActor.First };
    auto& LocalParticles = InActor.Second->BeginUpdateRigidParticles(Ids);
    uint32 Index = InActor.Second->GetIndexFromId(InActor.First);
    InShape.Object = LocalParticles.Geometry(Index);
    LocalParticles.Geometry(Index) = nullptr;
    InActor.Second->EndUpdateRigidParticles();
    InShape.bSimulation = false;
    InShape.ActorRef.Second = nullptr;
}

// @todo(mlentine): What else do we need?
void FPhysInterface_Apeiron::SetActorUserData_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, FPhysxUserData* InUserData)
{
    FBodyInstance* BodyInstance = InUserData->Get<FBodyInstance>(InUserData);
    if (BodyInstance)
    {
        InActorReference.Second->SetBodyInstance(BodyInstance, InActorReference.First);
    }
}

bool FPhysInterface_Apeiron::IsRigidBody(const FPhysicsActorReference_Apeiron& InActorReference)
{
    return true;
}

bool FPhysInterface_Apeiron::IsStatic(const FPhysicsActorReference_Apeiron& InActorReference)
{
    uint32 Index = UINT32_MAX;
    const auto& Particles = GetParticlesAndIndex(InActorReference, Index);
	return Particles.InvM(Index) == 0;
}

bool FPhysInterface_Apeiron::IsKinematic_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference)
{
    return IsStatic(InActorReference);
}

bool FPhysInterface_Apeiron::IsSleeping(const FPhysicsActorReference_Apeiron& InActorReference)
{
    uint32 Index = UINT32_MAX;
    const auto& Particles = GetParticlesAndIndex(InActorReference, Index);
    return Particles.Sleeping(Index);
}

bool FPhysInterface_Apeiron::IsCcdEnabled(const FPhysicsActorReference_Apeiron& InActorReference)
{
    return false;
}

bool FPhysInterface_Apeiron::IsInScene(const FPhysicsActorReference_Apeiron& InActorReference)
{
    return InActorReference.Second != nullptr;
}

bool FPhysInterface_Apeiron::CanSimulate_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference)
{
    uint32 Index = UINT32_MAX;
	const auto& Particles = GetParticlesAndIndex(InActorReference, Index);
    return !Particles.Disabled(Index);
}

float FPhysInterface_Apeiron::GetMass_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference)
{
    uint32 Index = UINT32_MAX;
    const auto& Particles = GetParticlesAndIndex(InActorReference, Index);
	return Particles.M(Index);
}

void FPhysInterface_Apeiron::SetSendsSleepNotifies_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, bool bSendSleepNotifies)
{
    check(bSendSleepNotifies == false);
}

void FPhysInterface_Apeiron::PutToSleep_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference)
{
    TArray<RigidBodyId> BodiesToSleep = { InActorReference.First };
    auto& Particles = InActorReference.Second->BeginUpdateRigidParticles(BodiesToSleep);
    Particles.Sleeping(InActorReference.Second->GetIndexFromId(InActorReference.First)) = true;
    InActorReference.Second->EndUpdateRigidParticles();
}

void FPhysInterface_Apeiron::WakeUp_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference)
{
    TArray<RigidBodyId> BodiesToSleep = { InActorReference.First };
    auto& Particles = InActorReference.Second->BeginUpdateRigidParticles(BodiesToSleep);
    Particles.Sleeping(InActorReference.Second->GetIndexFromId(InActorReference.First)) = false;
    InActorReference.Second->EndUpdateRigidParticles();
}

void FPhysInterface_Apeiron::SetIsKinematic_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, bool bIsKinematic)
{
    TArray<RigidBodyId> BodiesToSleep = { InActorReference.First };
    auto& Particles = InActorReference.Second->BeginUpdateRigidParticles(BodiesToSleep);
    int32 Index = InActorReference.Second->GetIndexFromId(InActorReference.First);
    Particles.InvM(Index) = bIsKinematic ? 0 : 1.f / Particles.M(Index);
    InActorReference.Second->EndUpdateRigidParticles();
}

void FPhysInterface_Apeiron::SetCcdEnabled_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, bool bIsCcdEnabled)
{
    check(bIsCcdEnabled == false);
}

FTransform FPhysInterface_Apeiron::GetGlobalPose_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference)
{
    uint32 Index = UINT32_MAX;
    const auto& LocalParticles = GetParticlesAndIndex(InActorReference, Index);
    return Apeiron::TRigidTransform<float, 3>(LocalParticles.X(Index), LocalParticles.R(Index));
}

void FPhysInterface_Apeiron::SetGlobalPose_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, const FTransform& InNewPose, bool bAutoWake)
{
    TArray<RigidBodyId> BodiesToModify = { InActorReference.First };
    auto& Particles = InActorReference.Second->BeginUpdateRigidParticles(BodiesToModify);
    int32 Index = InActorReference.Second->GetIndexFromId(InActorReference.First);
    Particles.X(Index) = InNewPose.GetTranslation();
    Particles.R(Index) = InNewPose.GetRotation();
    InActorReference.Second->EndUpdateRigidParticles();
}

FTransform FPhysInterface_Apeiron::GetTransform_AssumesLocked(const FPhysicsActorHandle& InRef, bool bForceGlobalPose /*= false*/)
{
	if(!bForceGlobalPose)
	{
		if(IsDynamic(InRef))
		{
			if(HasKinematicTarget_AssumesLocked(InRef))
			{
				return GetKinematicTarget_AssumesLocked(InRef);
			}
		}
	}

	return GetGlobalPose_AssumesLocked(InRef);
}

bool FPhysInterface_Apeiron::HasKinematicTarget_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference)
{
    return IsStatic(InActorReference);
}

FTransform FPhysInterface_Apeiron::GetKinematicTarget_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference)
{
    InActorReference.Second->MCriticalSection.Lock();
    FTransform CurrentTransform = InActorReference.Second->NewAnimationTransforms[InActorReference.Second->GetIndexFromId(InActorReference.First)];
    InActorReference.Second->MCriticalSection.Unlock();
    return CurrentTransform;
}

void FPhysInterface_Apeiron::SetKinematicTarget_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, const FTransform& InNewTarget)
{
    InActorReference.Second->SetKinematicTransform(InActorReference.First, InNewTarget);
}

FVector FPhysInterface_Apeiron::GetLinearVelocity_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference)
{
    uint32 Index = UINT32_MAX;
    const auto& Particles = GetParticlesAndIndex(InActorReference, Index);
	return Particles.V(Index);
}

void FPhysInterface_Apeiron::SetLinearVelocity_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, const FVector& InNewVelocity, bool bAutoWake)
{
    TArray<RigidBodyId> BodiesToModify = { InActorReference.First };
    auto& Particles = InActorReference.Second->BeginUpdateRigidParticles(BodiesToModify);
    Particles.V(InActorReference.Second->GetIndexFromId(InActorReference.First)) = InNewVelocity;
    InActorReference.Second->EndUpdateRigidParticles();
}

FVector FPhysInterface_Apeiron::GetAngularVelocity_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference)
{
    return InActorReference.Second->Scene.GetRigidParticles().W(InActorReference.Second->GetIndexFromId(InActorReference.First));
}

void FPhysInterface_Apeiron::SetAngularVelocity_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, const FVector& InNewVelocity, bool bAutoWake)
{
    TArray<RigidBodyId> BodiesToModify = { InActorReference.First };
    auto& Particles = InActorReference.Second->BeginUpdateRigidParticles(BodiesToModify);
    Particles.W(InActorReference.Second->GetIndexFromId(InActorReference.First)) = InNewVelocity;
    InActorReference.Second->EndUpdateRigidParticles();
}

float FPhysInterface_Apeiron::GetMaxAngularVelocity_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference)
{
    return FLT_MAX;
}

void FPhysInterface_Apeiron::SetMaxAngularVelocity_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, float InMaxAngularVelocity)
{
}

float FPhysInterface_Apeiron::GetMaxDepenetrationVelocity_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference)
{
    return FLT_MAX;
}

void FPhysInterface_Apeiron::SetMaxDepenetrationVelocity_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, float InMaxDepenetrationVelocity)
{
}

FVector FPhysInterface_Apeiron::GetWorldVelocityAtPoint_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, const FVector& InPoint)
{
    uint32 Index = UINT32_MAX;
    const auto& LocalParticles = GetParticlesAndIndex(InActorReference, Index);
    return LocalParticles.V(Index) + Apeiron::TVector<float, 3>::CrossProduct(LocalParticles.W(Index), InPoint - LocalParticles.X(Index));
}

FTransform FPhysInterface_Apeiron::GetComTransform_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference)
{
    uint32 Index = UINT32_MAX;
    const auto& LocalParticles = GetParticlesAndIndex(InActorReference, Index);
    Apeiron::TRigidTransform<float, 3> GlobalTransform(LocalParticles.X(Index), LocalParticles.R(Index));
    Apeiron::TRigidTransform<float, 3> ComTransform(LocalParticles.Geometry(Index) ? LocalParticles.Geometry(Index)->BoundingBox().Center() : Apeiron::TVector<float, 3>(0), Apeiron::TRotation<float, 3>(FQuat(0, 0, 0, 1)));
    return GlobalTransform * ComTransform;
}

FVector FPhysInterface_Apeiron::GetLocalInertiaTensor_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference)
{
    // @todo(mlentine): Just return directly once we implement DiagonalMatrix
    uint32 Index = UINT32_MAX;
	const auto& Particles = GetParticlesAndIndex(InActorReference, Index);
    Apeiron::PMatrix<float, 3, 3> Inertia = Particles.I(Index);
    return FVector(Inertia.M[0][0], Inertia.M[1][1], Inertia.M[2][2]);
}

FBox FPhysInterface_Apeiron::GetBounds_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference)
{
    const auto& Box = InActorReference.Second->Scene.GetRigidParticles().Geometry(InActorReference.Second->GetIndexFromId(InActorReference.First))->BoundingBox();
    return FBox(Box.Min(), Box.Max());
}

void FPhysInterface_Apeiron::SetLinearDamping_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, float InDamping)
{

}

void FPhysInterface_Apeiron::SetAngularDamping_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, float InDamping)
{

}

void FPhysInterface_Apeiron::AddForce_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, const FVector& InForce)
{
    InActorReference.Second->AddForce(InForce, InActorReference.First);
}

void FPhysInterface_Apeiron::AddTorque_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, const FVector& InTorque)
{
    InActorReference.Second->AddTorque(InTorque, InActorReference.First);
}

void FPhysInterface_Apeiron::AddForceMassIndependent_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, const FVector& InForce)
{
    InActorReference.Second->AddForce(InForce * InActorReference.Second->Scene.GetRigidParticles().M(InActorReference.Second->GetIndexFromId(InActorReference.First)), InActorReference.First);
}

void FPhysInterface_Apeiron::AddTorqueMassIndependent_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, const FVector& InTorque)
{
    InActorReference.Second->AddTorque(InActorReference.Second->Scene.GetRigidParticles().I(InActorReference.Second->GetIndexFromId(InActorReference.First)) * Apeiron::TVector<float, 3>(InTorque), InActorReference.First);
}

void FPhysInterface_Apeiron::AddImpulseAtLocation_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, const FVector& InImpulse, const FVector& InLocation)
{
    // @todo(mlentine): We don't currently have a way to apply an instantaneous force. Do we need this?
}

void FPhysInterface_Apeiron::AddRadialImpulse_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, const FVector& InOrigin, float InRadius, float InStrength, ERadialImpulseFalloff InFalloff, bool bInVelChange)
{
    // @todo(mlentine): We don't currently have a way to apply an instantaneous force. Do we need this?
}

bool FPhysInterface_Apeiron::IsGravityEnabled_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference)
{
    // @todo(mlentine): Gravity is system wide currently. This should change.
    return true;
}
void FPhysInterface_Apeiron::SetGravityEnabled_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, bool bEnabled)
{
    // @todo(mlentine): Gravity is system wide currently. This should change.
}

float FPhysInterface_Apeiron::GetSleepEnergyThreshold_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference)
{
    return 0;
}
void FPhysInterface_Apeiron::SetSleepEnergyThreshold_AssumesLocked(const FPhysicsActorReference_Apeiron& InActorReference, float InEnergyThreshold)
{
}

void FPhysInterface_Apeiron::SetMass_AssumesLocked(const FPhysicsActorReference_Apeiron& InHandle, float InMass)
{
    TArray<RigidBodyId> Ids = { InHandle.First };
    uint32 Index = InHandle.Second->GetIndexFromId(InHandle.First);
    auto& LocalParticles = InHandle.Second->BeginUpdateRigidParticles(Ids);
    LocalParticles.M(Index) = InMass;
    InHandle.Second->EndUpdateRigidParticles();
}

void FPhysInterface_Apeiron::SetMassSpaceInertiaTensor_AssumesLocked(const FPhysicsActorReference_Apeiron& InHandle, const FVector& InTensor)
{
    TArray<RigidBodyId> Ids = { InHandle.First };
    uint32 Index = InHandle.Second->GetIndexFromId(InHandle.First);
    auto& LocalParticles = InHandle.Second->BeginUpdateRigidParticles(Ids);
    LocalParticles.I(Index).M[0][0] = InTensor[0];
    LocalParticles.I(Index).M[1][1] = InTensor[1];
    LocalParticles.I(Index).M[2][2] = InTensor[2];
    InHandle.Second->EndUpdateRigidParticles();
}

void FPhysInterface_Apeiron::SetComLocalPose_AssumesLocked(const FPhysicsActorReference_Apeiron& InHandle, const FTransform& InComLocalPose)
{
    //@todo(mlentine): What is InComLocalPose? If the center of an object is not the local pose then many things break including the three vector represtnation of inertia.
}

float FPhysInterface_Apeiron::GetStabilizationEnergyThreshold_AssumesLocked(const FPhysicsActorReference_Apeiron& InHandle)
{
	// #PHYS2 implement
	return 0.0f;
}

void FPhysInterface_Apeiron::SetStabilizationEnergyThreshold_AssumesLocked(const FPhysicsActorReference_Apeiron& InHandle, float InThreshold)
{
	// #PHYS2 implement
}

uint32 FPhysInterface_Apeiron::GetSolverPositionIterationCount_AssumesLocked(const FPhysicsActorReference_Apeiron& InHandle)
{
	// #PHYS2 implement
	return 0;
}

void FPhysInterface_Apeiron::SetSolverPositionIterationCount_AssumesLocked(const FPhysicsActorReference_Apeiron& InHandle, uint32 InSolverIterationCount)
{
	// #PHYS2 implement
}

uint32 FPhysInterface_Apeiron::GetSolverVelocityIterationCount_AssumesLocked(const FPhysicsActorReference_Apeiron& InHandle)
{
	// #PHYS2 implement
	return 0;
}

void FPhysInterface_Apeiron::SetSolverVelocityIterationCount_AssumesLocked(const FPhysicsActorReference_Apeiron& InHandle, uint32 InSolverIterationCount)
{
	// #PHYS2 implement
}

float FPhysInterface_Apeiron::GetWakeCounter_AssumesLocked(const FPhysicsActorReference_Apeiron& InHandle)
{
	// #PHYS2 implement
	return 0.0f;
}

void FPhysInterface_Apeiron::SetWakeCounter_AssumesLocked(const FPhysicsActorReference_Apeiron& InHandle, float InWakeCounter)
{
	// #PHYS2 implement
}

SIZE_T FPhysInterface_Apeiron::GetResourceSizeEx(const FPhysicsActorReference_Apeiron& InActorRef)
{
    return sizeof(FPhysicsActorReference_Apeiron);
}
	
// Constraints
FPhysicsConstraintReference_Apeiron FPhysInterface_Apeiron::CreateConstraint(const FPhysicsActorReference_Apeiron& InActorRef1, const FPhysicsActorReference_Apeiron& InActorRef2, const FTransform& InLocalFrame1, const FTransform& InLocalFrame2)
{
    check(InActorRef1.Second == InActorRef2.Second);
    FPhysicsConstraintReference_Apeiron ConstraintRef;
    ConstraintRef.First = InActorRef1.Second->AddSpringConstraint(Apeiron::TVector<RigidBodyId, 2>(InActorRef1.First, InActorRef2.First));
    ConstraintRef.Second = InActorRef1.Second;
    return ConstraintRef;
}

void FPhysInterface_Apeiron::SetConstraintUserData(const FPhysicsConstraintReference_Apeiron& InConstraintRef, void* InUserData)
{
    
}

void FPhysInterface_Apeiron::ReleaseConstraint(FPhysicsConstraintReference_Apeiron& InConstraintRef)
{
    InConstraintRef.Second->RemoveSpringConstraint(InConstraintRef.First);
}

FTransform FPhysInterface_Apeiron::GetLocalPose(const FPhysicsConstraintReference_Apeiron& InConstraintRef, EConstraintFrame::Type InFrame)
{
    if (InFrame == EConstraintFrame::Frame1)
    {
        return  FTransform();
    }
    else
    {
        int32 Index1 = InConstraintRef.Second->MSpringConstraints.Constraints()[InConstraintRef.Second->GetConstraintIndexFromId(InConstraintRef.First)][0];
        int32 Index2 = InConstraintRef.Second->MSpringConstraints.Constraints()[InConstraintRef.Second->GetConstraintIndexFromId(InConstraintRef.First)][1];
        Apeiron::TRigidTransform<float, 3> Transform1(InConstraintRef.Second->Scene.GetRigidParticles().X(Index1), InConstraintRef.Second->Scene.GetRigidParticles().R(Index1));
        Apeiron::TRigidTransform<float, 3> Transform2(InConstraintRef.Second->Scene.GetRigidParticles().X(Index2), InConstraintRef.Second->Scene.GetRigidParticles().R(Index2));
        return Transform1.Inverse() * Transform2;
    }
}

FTransform FPhysInterface_Apeiron::GetGlobalPose(const FPhysicsConstraintReference_Apeiron& InConstraintRef, EConstraintFrame::Type InFrame)
{
    if (InFrame == EConstraintFrame::Frame1)
    {
        int32 Index1 = InConstraintRef.Second->MSpringConstraints.Constraints()[InConstraintRef.Second->GetConstraintIndexFromId(InConstraintRef.First)][0];
        return Apeiron::TRigidTransform<float, 3>(InConstraintRef.Second->Scene.GetRigidParticles().X(Index1), InConstraintRef.Second->Scene.GetRigidParticles().R(Index1));
    }
    else
    {
        int32 Index2 = InConstraintRef.Second->MSpringConstraints.Constraints()[InConstraintRef.Second->GetConstraintIndexFromId(InConstraintRef.First)][1];
        return Apeiron::TRigidTransform<float, 3>(InConstraintRef.Second->Scene.GetRigidParticles().X(Index2), InConstraintRef.Second->Scene.GetRigidParticles().R(Index2));
    }
}

FVector FPhysInterface_Apeiron::GetLocation(const FPhysicsConstraintReference_Apeiron& InConstraintRef)
{
    int32 Index1 = InConstraintRef.Second->MSpringConstraints.Constraints()[InConstraintRef.Second->GetConstraintIndexFromId(InConstraintRef.First)][0];
    return InConstraintRef.Second->Scene.GetRigidParticles().X(Index1);
}

void FPhysInterface_Apeiron::GetForce(const FPhysicsConstraintReference_Apeiron& InConstraintRef, FVector& OutLinForce, FVector& OutAngForce)
{
    // @todo(mlentine): There is no concept of a force for a constraint in pbd. Need some way to resolve this
    check(false);
}

void FPhysInterface_Apeiron::GetDriveLinearVelocity(const FPhysicsConstraintReference_Apeiron& InConstraintRef, FVector& OutLinVelocity)
{
    OutLinVelocity = Apeiron::TVector<float, 3>(0);
}

void FPhysInterface_Apeiron::GetDriveAngularVelocity(const FPhysicsConstraintReference_Apeiron& InConstraintRef, FVector& OutAngVelocity)
{
    OutAngVelocity = Apeiron::TVector<float, 3>(0);
}

float FPhysInterface_Apeiron::GetCurrentSwing1(const FPhysicsConstraintReference_Apeiron& InConstraintRef)
{
    return GetLocalPose(InConstraintRef, EConstraintFrame::Frame2).GetRotation().Euler().X;
}

float FPhysInterface_Apeiron::GetCurrentSwing2(const FPhysicsConstraintReference_Apeiron& InConstraintRef)
{
    return GetLocalPose(InConstraintRef, EConstraintFrame::Frame2).GetRotation().Euler().Y;
}

float FPhysInterface_Apeiron::GetCurrentTwist(const FPhysicsConstraintReference_Apeiron& InConstraintRef)
{
    return GetLocalPose(InConstraintRef, EConstraintFrame::Frame2).GetRotation().Euler().Z;
}

void FPhysInterface_Apeiron::SetCanVisualize(const FPhysicsConstraintReference_Apeiron& InConstraintRef, bool bInCanVisualize)
{

}

void FPhysInterface_Apeiron::SetCollisionEnabled(const FPhysicsConstraintReference_Apeiron& InConstraintRef, bool bInCollisionEnabled)
{
	uint32 Index = UINT32_MAX;
	const auto& Constraints = GetConstraintArrayAndIndex(InConstraintRef, Index);
	int32 Index1 = Constraints[Index][0];
	int32 Index2 = Constraints[Index][1];
    if (bInCollisionEnabled)
    {
        InConstraintRef.Second->DelayedEnabledCollisions.Add(TTuple<int32, int32>(Index1, Index2));
    }
    else
    {
        InConstraintRef.Second->DelayedDisabledCollisions.Add(TTuple<int32, int32>(Index1, Index2));
    }
}

void FPhysInterface_Apeiron::SetProjectionEnabled_AssumesLocked(const FPhysicsConstraintReference_Apeiron& InConstraintRef, bool bInProjectionEnabled, float InLinearTolerance, float InAngularToleranceDegrees)
{

}

void FPhysInterface_Apeiron::SetParentDominates_AssumesLocked(const FPhysicsConstraintReference_Apeiron& InConstraintRef, bool bInParentDominates)
{

}

void FPhysInterface_Apeiron::SetBreakForces_AssumesLocked(const FPhysicsConstraintReference_Apeiron& InConstraintRef, float InLinearBreakForce, float InAngularBreakForce)
{

}

void FPhysInterface_Apeiron::SetLocalPose(const FPhysicsConstraintReference_Apeiron& InConstraintRef, const FTransform& InPose, EConstraintFrame::Type InFrame)
{

}

void FPhysInterface_Apeiron::SetLinearMotionLimitType_AssumesLocked(const FPhysicsConstraintReference_Apeiron& InConstraintRef, PhysicsInterfaceTypes::ELimitAxis InAxis, ELinearConstraintMotion InMotion)
{

}

void FPhysInterface_Apeiron::SetAngularMotionLimitType_AssumesLocked(const FPhysicsConstraintReference_Apeiron& InConstraintRef, PhysicsInterfaceTypes::ELimitAxis InAxis, EAngularConstraintMotion InMotion)
{

}

void FPhysInterface_Apeiron::UpdateLinearLimitParams_AssumesLocked(const FPhysicsConstraintReference_Apeiron& InConstraintRef, float InLimit, float InAverageMass, const FLinearConstraint& InParams)
{

}

void FPhysInterface_Apeiron::UpdateConeLimitParams_AssumesLocked(const FPhysicsConstraintReference_Apeiron& InConstraintRef, float InAverageMass, const FConeConstraint& InParams)
{

}

void FPhysInterface_Apeiron::UpdateTwistLimitParams_AssumesLocked(const FPhysicsConstraintReference_Apeiron& InConstraintRef, float InAverageMass, const FTwistConstraint& InParams)
{

}

void FPhysInterface_Apeiron::UpdateLinearDrive_AssumesLocked(const FPhysicsConstraintReference_Apeiron& InConstraintRef, const FLinearDriveConstraint& InDriveParams)
{

}

void FPhysInterface_Apeiron::UpdateAngularDrive_AssumesLocked(const FPhysicsConstraintReference_Apeiron& InConstraintRef, const FAngularDriveConstraint& InDriveParams)
{

}

void FPhysInterface_Apeiron::UpdateDriveTarget_AssumesLocked(const FPhysicsConstraintReference_Apeiron& InConstraintRef, const FLinearDriveConstraint& InLinDrive, const FAngularDriveConstraint& InAngDrive)
{

}

void FPhysInterface_Apeiron::SetDrivePosition(const FPhysicsConstraintReference_Apeiron& InConstraintRef, const FVector& InPosition)
{

}

void FPhysInterface_Apeiron::SetDriveOrientation(const FPhysicsConstraintReference_Apeiron& InConstraintRef, const FQuat& InOrientation)
{

}

void FPhysInterface_Apeiron::SetDriveLinearVelocity(const FPhysicsConstraintReference_Apeiron& InConstraintRef, const FVector& InLinVelocity)
{

}

void FPhysInterface_Apeiron::SetDriveAngularVelocity(const FPhysicsConstraintReference_Apeiron& InConstraintRef, const FVector& InAngVelocity)
{

}

void FPhysInterface_Apeiron::SetTwistLimit(const FPhysicsConstraintReference_Apeiron& InConstraintRef, float InLowerLimit, float InUpperLimit, float InContactDistance)
{

}

void FPhysInterface_Apeiron::SetSwingLimit(const FPhysicsConstraintReference_Apeiron& InConstraintRef, float InYLimit, float InZLimit, float InContactDistance)
{

}

void FPhysInterface_Apeiron::SetLinearLimit(const FPhysicsConstraintReference_Apeiron& InConstraintRef, float InLimit)
{

}

bool FPhysInterface_Apeiron::IsBroken(const FPhysicsConstraintReference_Apeiron& InConstraintRef)
{
	// @todo(mlentine): What is an invalid constraint?
	if (InConstraintRef.IsValid())
	{
		return InConstraintRef.Second->ConstraintIdToIndexMap.Contains(InConstraintRef.Second->GetConstraintIndexFromId(InConstraintRef.First));
	}
	return true;
}

bool FPhysInterface_Apeiron::ExecuteOnUnbrokenConstraintReadOnly(const FPhysicsConstraintReference_Apeiron& InConstraintRef, TFunctionRef<void(const FPhysicsConstraintReference_Apeiron&)> Func)
{
    if (!IsBroken(InConstraintRef))
    {
        Func(InConstraintRef);
        return true;
    }
    return false;
}

bool FPhysInterface_Apeiron::ExecuteOnUnbrokenConstraintReadWrite(const FPhysicsConstraintReference_Apeiron& InConstraintRef, TFunctionRef<void(const FPhysicsConstraintReference_Apeiron&)> Func)
{
    if (!IsBroken(InConstraintRef))
    {
        Func(InConstraintRef);
        return true;
    }
    return false;
}

template<class PHYSX_MESH>
TArray<Apeiron::TVector<int32, 3>> GetMeshElements(const PHYSX_MESH* PhysXMesh)
{
	check(false);
}

#if WITH_PHYSX

template<>
TArray<Apeiron::TVector<int32, 3>> GetMeshElements(const physx::PxConvexMesh* PhysXMesh)
{
	TArray<Apeiron::TVector<int32, 3>> CollisionMeshElements;
	int32 offset = 0;
	int32 NbPolygons = static_cast<int32>(PhysXMesh->getNbPolygons());
	for (int32 i = 0; i < NbPolygons; i++)
	{
		PxHullPolygon Poly;
		bool status = PhysXMesh->getPolygonData(i, Poly);
		const auto Indices = PhysXMesh->getIndexBuffer() + Poly.mIndexBase;

		for (int32 j = 2; j < static_cast<int32>(Poly.mNbVerts); j++)
		{
			CollisionMeshElements.Add(Apeiron::TVector<int32, 3>(Indices[offset], Indices[offset + j], Indices[offset + j - 1]));
		}
	}
	return CollisionMeshElements;
}

template<>
TArray<Apeiron::TVector<int32, 3>> GetMeshElements(const physx::PxTriangleMesh* PhysXMesh)
{
	TArray<Apeiron::TVector<int32, 3>> CollisionMeshElements;
	const auto MeshFlags = PhysXMesh->getTriangleMeshFlags();
	for (int32 j = 0; j < static_cast<int32>(PhysXMesh->getNbTriangles()); ++j)
	{
		if (MeshFlags | PxTriangleMeshFlag::e16_BIT_INDICES)
		{
			const PxU16* Indices = reinterpret_cast<const PxU16*>(PhysXMesh->getTriangles());
			CollisionMeshElements.Add(Apeiron::TVector<int32, 3>(Indices[3 * j], Indices[3 * j + 1], Indices[3 * j + 2]));
		}
		else
		{
			const PxU32* Indices = reinterpret_cast<const PxU32*>(PhysXMesh->getTriangles());
			CollisionMeshElements.Add(Apeiron::TVector<int32, 3>(Indices[3 * j], Indices[3 * j + 1], Indices[3 * j + 2]));
		}
	}
	return CollisionMeshElements;
}

template<class PHYSX_MESH>
TUniquePtr<Apeiron::TImplicitObject<float, 3>> ConvertPhysXMeshToLevelset(const PHYSX_MESH* PhysXMesh, const FVector& Scale)
{
    TArray<Apeiron::TVector<int32, 3>> CollisionMeshElements = GetMeshElements(PhysXMesh);
	Apeiron::TParticles<float, 3> CollisionMeshParticles;
	CollisionMeshParticles.AddParticles(PhysXMesh->getNbVertices());
	for (uint32 j = 0; j < CollisionMeshParticles.Size(); ++j)
	{
		const auto& Vertex = PhysXMesh->getVertices()[j];
		CollisionMeshParticles.X(j) = Scale * Apeiron::TVector<float, 3>(Vertex.x, Vertex.y, Vertex.z);
	}
	Apeiron::TBox<float, 3> BoundingBox(CollisionMeshParticles.X(0), CollisionMeshParticles.X(0));
	for (uint32 j = 1; j < CollisionMeshParticles.Size(); ++j)
	{
		BoundingBox.GrowToInclude(CollisionMeshParticles.X(j));
	}
#if FORCE_ANALYTICS
	return TUniquePtr<Apeiron::TImplicitObject<float, 3>>(new Apeiron::TBox<float, 3>(BoundingBox));
#else
	int32 MaxAxisSize = 10;
	int32 MaxAxis;
	const auto Extents = BoundingBox.Extents();
	if (Extents[0] > Extents[1] && Extents[0] > Extents[2])
	{
		MaxAxis = 0;
	}
	else if (Extents[1] > Extents[2])
	{
		MaxAxis = 1;
	}
	else
	{
		MaxAxis = 2;
	}
    Apeiron::TVector<int32, 3> Counts(MaxAxisSize * Extents[0] / Extents[MaxAxis], MaxAxisSize * Extents[1] / Extents[MaxAxis], MaxAxisSize * Extents[2] / Extents[MaxAxis]);
    Counts[0] = Counts[0] < 1 ? 1 : Counts[0];
    Counts[1] = Counts[1] < 1 ? 1 : Counts[1];
    Counts[2] = Counts[2] < 1 ? 1 : Counts[2];
    Apeiron::TUniformGrid<float, 3> Grid(BoundingBox.Min(), BoundingBox.Max(), Counts, 1);
	Apeiron::TTriangleMesh<float> CollisionMesh(MoveTemp(CollisionMeshElements));
	return TUniquePtr<Apeiron::TImplicitObject<float, 3>>(new Apeiron::TLevelSet<float, 3>(Grid, CollisionMeshParticles, CollisionMesh));
#endif
}

#endif

FPhysicsShapeHandle FPhysInterface_Apeiron::CreateShape(physx::PxGeometry* InGeom, bool bSimulation, bool bQuery, UPhysicalMaterial* InSimpleMaterial, TArray<UPhysicalMaterial*>* InComplexMaterials, bool bShared)
{
    // @todo(mlentine): Should we be doing anything with the InGeom here?
    FPhysicsShapeHandle NewShape;
    NewShape.Object = nullptr;
    NewShape.bSimulation = bSimulation;
    NewShape.bQuery = bQuery;
    FPhysicsActorHandle NewActor;
    NewActor.First = RigidBodyId(0);
    NewActor.Second = nullptr;
    NewShape.ActorRef = NewActor;
    return NewShape;
}

void FPhysInterface_Apeiron::AddGeometry(const FPhysicsActorHandle& InActor, const FGeometryAddParams& InParams, TArray<FPhysicsShapeHandle>* OutOptShapes)
{
	const FVector& Scale = InParams.Scale;
    TArray<TUniquePtr<Apeiron::TImplicitObject<float, 3>>> Objects;
	if (InParams.Geometry)
	{
		for (uint32 i = 0; i < static_cast<uint32>(InParams.Geometry->SphereElems.Num()); ++i)
		{
			check(Scale[0] == Scale[1] && Scale[1] == Scale[2]);
			const auto& CollisionSphere = InParams.Geometry->SphereElems[i];
			Objects.Add(TUniquePtr<Apeiron::TImplicitObject<float, 3>>(new Apeiron::TSphere<float, 3>(Apeiron::TVector<float, 3>(0.f, 0.f, 0.f), CollisionSphere.Radius * Scale[0])));
		}
		for (uint32 i = 0; i < static_cast<uint32>(InParams.Geometry->BoxElems.Num()); ++i)
		{
			const auto& Box = InParams.Geometry->BoxElems[i];
			Apeiron::TVector<float, 3> half_extents = Scale * Apeiron::TVector<float, 3>(Box.X / 2.f, Box.Y / 2.f, Box.Z / 2.f);
			Objects.Add(TUniquePtr<Apeiron::TImplicitObject<float, 3>>(new Apeiron::TBox<float, 3>(-half_extents, half_extents)));
		}
		for (uint32 i = 0; i < static_cast<uint32>(InParams.Geometry->SphylElems.Num()); ++i)
		{
			check(Scale[0] == Scale[1] && Scale[1] == Scale[2]);
			const auto& Capsule = InParams.Geometry->SphylElems[i];
			if (Capsule.Length == 0)
			{
				Objects.Add(TUniquePtr<Apeiron::TImplicitObject<float, 3>>(new Apeiron::TSphere<float, 3>(Apeiron::TVector<float, 3>(0), Capsule.Radius * Scale[0])));
			}
			else
			{
				Apeiron::TVector<float, 3> half_extents(0, 0, Capsule.Length / 2 * Scale[0]);
				Objects.Add(TUniquePtr<Apeiron::TImplicitObject<float, 3>>(new Apeiron::TCylinder<float>(-half_extents, half_extents, Capsule.Radius * Scale[0])));
				Objects.Add(TUniquePtr<Apeiron::TImplicitObject<float, 3>>(new Apeiron::TSphere<float, 3>(-half_extents, Capsule.Radius * Scale[0])));
				Objects.Add(TUniquePtr<Apeiron::TImplicitObject<float, 3>>(new Apeiron::TSphere<float, 3>(half_extents, Capsule.Radius * Scale[0])));
			}
		}
#if 0
		for (uint32 i = 0; i < static_cast<uint32>(InParams.Geometry->TaperedCapsuleElems.Num()); ++i)
		{
			check(Scale[0] == Scale[1] && Scale[1] == Scale[2]);
            const auto& Capsule = InParams.Geometry->TaperedCapsuleElems[i];
            if (Capsule.Length == 0)
            {
                Objects.Add(TUniquePtr<Apeiron::TImplicitObject<float, 3>>(new Apeiron::TSphere<float, 3>(Vector<float, 3>(0), (Capsule.Radius1 > Capsule.Radius0 ? Capsule.Radius1 : Capsule.Radius0) * Scale[0])));
            }
            else
            {
				Apeiron::TVector<float, 3> half_extents(0, 0, Capsule.Length / 2 * Scale[0]);
				Objects.Add(TUniquePtr<Apeiron::TImplicitObject<float, 3>>(new Apeiron::TTaperedCylinder<float>(-half_extents, half_extents, Capsule.Radius * Scale[0])));
                Objects.Add(TUniquePtr<Apeiron::TImplicitObject<float, 3>>(new Apeiron::TSphere<float, 3>(-half_extents, Capsule.Radius * Scale[0])));
                Objects.Add(TUniquePtr<Apeiron::TImplicitObject<float, 3>>(new Apeiron::TSphere<float, 3>(half_extents, Capsule.Radius * Scale[0])));
			}
		}
#endif
#if WITH_PHYSX
		for (uint32 i = 0; i < static_cast<uint32>(InParams.Geometry->ConvexElems.Num()); ++i)
		{
			const auto& CollisionBody = InParams.Geometry->ConvexElems[i];
			Objects.Add(ConvertPhysXMeshToLevelset(CollisionBody.GetConvexMesh(), Scale));
		}
#endif
	}
	else
	{
#if WITH_PHYSX
		for (const auto& PhysXMesh : InParams.TriMeshes)
		{
			Objects.Add(ConvertPhysXMeshToLevelset(PhysXMesh, Scale));
		}
#endif
	}
	if (Objects.Num() == 0) return;
    TArray<RigidBodyId> BodiesToModify = { InActor.First };
    auto& Particles = InActor.Second->BeginUpdateRigidParticles(BodiesToModify);
	int32 Index = InActor.Second->GetIndexFromId(InActor.First);
	if (InParams.LocalTransform.Equals(FTransform()))
	{
		if (Objects.Num() == 1)
		{
			Particles.Geometry(Index) = Objects[0].Release();
		}
		else
		{
			Particles.Geometry(Index) = new Apeiron::TImplicitObjectUnion<float, 3>(MoveTemp(Objects));
		}
	}
	else
	{
		if (Objects.Num() == 1)
		{
			Particles.Geometry(Index) = new Apeiron::TImplicitObjectTransformed<float, 3>(Objects[0].Release(), InParams.LocalTransform);
		}
		else
		{
			Particles.Geometry(Index) = new Apeiron::TImplicitObjectTransformed<float, 3>(new Apeiron::TImplicitObjectUnion<float, 3>(MoveTemp(Objects)), InParams.LocalTransform);
		}
	}
    if (OutOptShapes)
    {
        FPhysicsShapeHandle NewShape;
        NewShape.Object = Particles.Geometry(Index);
        NewShape.bSimulation = true;
        NewShape.bQuery = true;
        NewShape.ActorRef = InActor;
        OutOptShapes->SetNum(1);
        (*OutOptShapes)[0] = NewShape;
    }
    InActor.Second->EndUpdateRigidParticles();
}

// @todo(mlentine): We probably need to actually duplicate the data here
FPhysicsShapeHandle FPhysInterface_Apeiron::CloneShape(const FPhysicsShapeHandle& InShape)
{
    FPhysicsShapeHandle NewShape;
    NewShape.Object = InShape.Object;
    NewShape.bSimulation = InShape.bSimulation;
    NewShape.bQuery = InShape.bQuery;
    FPhysicsActorHandle NewActor;
    NewActor.First = RigidBodyId(0);
    NewActor.Second = nullptr;
    NewShape.ActorRef = NewActor;
    return NewShape;
}

bool FPhysInterface_Apeiron::IsSimulationShape(const FPhysicsShapeHandle& InShape)
{
    return InShape.bSimulation;
}

bool FPhysInterface_Apeiron::IsQueryShape(const FPhysicsShapeHandle& InShape)
{
    return InShape.bQuery;
}

bool FPhysInterface_Apeiron::IsShapeType(const FPhysicsShapeHandle& InShape, ECollisionShapeType InType)
{
    if (InType == ECollisionShapeType::Box && InShape.Object->GetType() == Apeiron::ImplicitObjectType::Box)
    {
        return true;
    }
    if (InType == ECollisionShapeType::Sphere && InShape.Object->GetType() == Apeiron::ImplicitObjectType::Sphere)
    {
        return true;
    }
    // Other than sphere and box the basic types do not correlate so we return false
    return false;
}

ECollisionShapeType FPhysInterface_Apeiron::GetShapeType(const FPhysicsShapeHandle& InShape)
{
    if (InShape.Object->GetType() == Apeiron::ImplicitObjectType::Box)
    {
        return ECollisionShapeType::Box;
    }
    if (InShape.Object->GetType() == Apeiron::ImplicitObjectType::Sphere)
    {
        return ECollisionShapeType::Sphere;
    }
    return ECollisionShapeType::None;
}

FPhysicsGeometryCollection FPhysInterface_Apeiron::GetGeometryCollection(const FPhysicsShapeHandle& InShape)
{
    FPhysicsGeometryCollection NewGeometry;
    NewGeometry.Object = InShape.Object;
    return NewGeometry;
}

FTransform FPhysInterface_Apeiron::GetLocalTransform(const FPhysicsShapeHandle& InShape)
{
    // Transforms are baked into the object so there is never a local transform
    if (InShape.Object->GetType() == Apeiron::ImplicitObjectType::Transformed && InShape.ActorRef.IsValid())
    {
        return InShape.Object->GetObject<Apeiron::TImplicitObjectTransformed<float, 3>>()->GetTransform();
    }
    else
    {
        return FTransform();
    }
}

void FPhysInterface_Apeiron::SetLocalTransform(const FPhysicsShapeHandle& InShape, const FTransform& NewLocalTransform)
{
    if (InShape.ActorRef.IsValid())
    {
        TArray<RigidBodyId> Ids = {InShape.ActorRef.First};
        const auto Index = InShape.ActorRef.Second->GetIndexFromId(InShape.ActorRef.First);
        auto& LocalParticles = InShape.ActorRef.Second->BeginUpdateRigidParticles(Ids);
        if (InShape.Object->GetType() == Apeiron::ImplicitObjectType::Transformed)
        {
            // @todo(mlentine): We can avoid creating a new object here by adding delayed update support for the object transforms
            LocalParticles.Geometry(Index) = new Apeiron::TImplicitObjectTransformed<float, 3>(InShape.Object->GetObject<Apeiron::TImplicitObjectTransformed<float, 3>>()->Object(), NewLocalTransform);
        }
        else
        {
            LocalParticles.Geometry(Index) = new Apeiron::TImplicitObjectTransformed<float, 3>(InShape.Object, NewLocalTransform);
        }
        InShape.ActorRef.Second->EndUpdateRigidParticles();
    }
    {
        if (InShape.Object->GetType() == Apeiron::ImplicitObjectType::Transformed)
        {
            InShape.Object->GetObject<Apeiron::TImplicitObjectTransformed<float, 3>>()->SetTransform(NewLocalTransform);
        }
        else
        {
            const_cast<FPhysicsShapeHandle&>(InShape).Object = new Apeiron::TImplicitObjectTransformed<float, 3>(InShape.Object, NewLocalTransform);
        }
    }
    uint32 Index = UINT32_MAX;
    const auto& LocalParticles = InShape.ActorRef.Second->GetParticlesAndIndex(InShape.ActorRef, Index);
}

void FPhysInterface_Apeiron::ListAwakeRigidBodies(bool bIncludeKinematic)
{

}

int32 FPhysInterface_Apeiron::GetNumAwakeBodies() const
{
	int32 Count = 0;
	for (uint32 i = 0; i < Scene.GetRigidParticles().Size(); ++i)
	{
		if (!(Scene.GetRigidParticles().Disabled(i) || Scene.GetRigidParticles().Sleeping(i)))
		{
			Count++;
		}
	}
	return Count;
}

void FinishSceneStat(uint32 Scene)
{
}

#if WITH_PHYSX

void FPhysInterface_Apeiron::CalculateMassPropertiesFromShapeCollection(physx::PxMassProperties& OutProperties, const TArray<FPhysicsShapeHandle>& InShapes, float InDensityKGPerCM)
{
    // What does it mean when if there is more than one collision object?
    check(InShapes.Num() == 1);
    if (InShapes[0].ActorRef.IsValid())
    {
        uint32_t Index;
        const auto& LocalParticles = InShapes[0].ActorRef.Second->GetParticlesAndIndex(InShapes[0].ActorRef, Index);
        const auto& X = LocalParticles.X(Index);
        OutProperties.centerOfMass = physx::PxVec3(X[0], X[1], X[2]);
        const auto& Inertia = LocalParticles.I(Index);
        OutProperties.inertiaTensor = physx::PxMat33();
        OutProperties.inertiaTensor(0, 0) = Inertia.M[0][0];
        OutProperties.inertiaTensor(1, 1) = Inertia.M[1][1];
        OutProperties.inertiaTensor(2, 2) = Inertia.M[2][2];
        OutProperties.mass = LocalParticles.M(Index);
    }
}

#endif

bool FPhysInterface_Apeiron::LineTrace_Geom(FHitResult& OutHit, const FBodyInstance* InInstance, const FVector& InStart, const FVector& InEnd, bool bTraceComplex, bool bExtractPhysMaterial)
{
	OutHit.TraceStart = InStart;
	OutHit.TraceEnd = InEnd;
    
    TArray<FPhysicsShapeReference_Apeiron> OutShapes;
    InInstance->GetAllShapes_AssumesLocked(OutShapes);
    check(OutShapes.Num() == 1);
    const auto& Result = OutShapes[0].Object->FindClosestIntersection(InStart, InEnd, 0);
    if (Result.Second)
    {
        // @todo(mlentine): What should we fill here?
        OutHit.ImpactPoint = Result.First;
        OutHit.ImpactNormal = OutShapes[0].Object->Normal(OutHit.ImpactPoint);
    }
    return Result.Second;
}

bool FPhysInterface_Apeiron::Sweep_Geom(FHitResult& OutHit, const FBodyInstance* InInstance, const FVector& InStart, const FVector& InEnd, const FQuat& InShapeRotation, const FCollisionShape& InShape, bool bSweepComplex)
{
    // @todo(mlentine): Need to implement this
    return false;
}

bool FPhysInterface_Apeiron::Overlap_Geom(const FBodyInstance* InBodyInstance, const FPhysicsGeometryCollection& InGeometry, const FTransform& InShapeTransform, FMTDResult* OutOptResult)
{
    // @todo(mlentine): Need to implement this
    return false;
}

bool FPhysInterface_Apeiron::Overlap_Geom(const FBodyInstance* InBodyInstance, const FCollisionShape& InCollisionShape, const FQuat& InShapeRotation, const FTransform& InShapeTransform, FMTDResult* OutOptResult)
{
    // @todo(mlentine): Need to implement this
    return false;
}

bool FPhysInterface_Apeiron::GetSquaredDistanceToBody(const FBodyInstance* InInstance, const FVector& InPoint, float& OutDistanceSquared, FVector* OutOptPointOnBody)
{
    // @todo(mlentine): What spaces are in and out point?
    TArray<FPhysicsShapeReference_Apeiron> OutShapes;
    InInstance->GetAllShapes_AssumesLocked(OutShapes);
    check(OutShapes.Num() == 1);
    Apeiron::TVector<float, 3> Normal;
    const auto Phi = OutShapes[0].Object->PhiWithNormal(InPoint, Normal);
    OutDistanceSquared = Phi * Phi;
    if (OutOptPointOnBody)
    {
        *OutOptPointOnBody = InPoint - Phi * Normal;
    }
    return true;
}

TSharedPtr<FSimEventCallbackFactory> FPhysInterface_Apeiron::SimEventCallbackFactory;
TSharedPtr<FContactModifyCallbackFactory> FPhysInterface_Apeiron::ContactModifyCallbackFactory;

template int32 FPhysInterface_Apeiron::GetAllShapes_AssumedLocked(const FPhysicsActorReference_Apeiron& InActorHandle, TArray<FPhysicsShapeHandle>& OutShapes, EPhysicsSceneType InSceneType);
template int32 FPhysInterface_Apeiron::GetAllShapes_AssumedLocked(const FPhysicsActorReference_Apeiron& InActorHandle, PhysicsInterfaceTypes::FInlineShapeArray& OutShapes, EPhysicsSceneType InSceneType);

#endif
