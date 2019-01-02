// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionSQAccelerator.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Chaos/PBDRigidParticles.h"
#include "GeometryCollection/ManagedArray.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionActor.h"
#include "Components/BoxComponent.h"
#include "ChaosSolversModule.h"
#include "ChaosStats.h"

#if INCLUDE_CHAOS

bool LowLevelRaycast(const UGeometryCollectionComponent& GeomCollectionComponent, const FVector& Start, const FVector& Dir, float DeltaMag, EHitFlags OutputFlags, FHitRaycast& OutHit)
{
	using namespace Chaos;

	const TManagedArray<int32>& RigidBodyIdArray = GeomCollectionComponent.GetRigidBodyIdArray();
	bool bFound = false;

	if (const Chaos::PBDRigidsSolver* Solver = GeomCollectionComponent.ChaosSolverActor != nullptr ? GeomCollectionComponent.ChaosSolverActor->GetSolver() : FPhysScene_Chaos::GetInstance()->GetSolver())
	{
		const TPBDRigidParticles<float, 3>& Particles = Solver->GetRigidParticles();	//todo(ocohen): should these just get passed in instead of hopping through scene?

		for (int32 Idx = 0; Idx < RigidBodyIdArray.Num(); ++Idx)
		{
			const int32 RigidBodyIdx = RigidBodyIdArray[Idx];
			if (RigidBodyIdx == -1) { continue; }	//todo(ocohen): managed to avoid this invalid index, but need to investigate a bit more into whether we can always assume it's valid

			if (Particles.Disabled(RigidBodyIdx)) { continue; }	//disabled particles can actually have stale geometry in them and are clearly not useful anyway

			if (!(ensure(!FMath::IsNaN(Particles.X(RigidBodyIdx)[0])) && ensure(!FMath::IsNaN(Particles.X(RigidBodyIdx)[1])) && ensure(!FMath::IsNaN(Particles.X(RigidBodyIdx)[2]))))
			{
				continue;
			}
			const TRigidTransform<float, 3> TM(Particles.X(RigidBodyIdx), Particles.R(RigidBodyIdx));
			const TVector<float, 3> StartLocal = TM.InverseTransformPositionNoScale(Start);
			const TVector<float, 3> DirLocal = TM.InverseTransformVectorNoScale(Dir);
			const TVector<float, 3> EndLocal = StartLocal + DirLocal * DeltaMag;	//todo(ocohen): apeiron just undoes this later, we should fix the API

			const TImplicitObject<float, 3>* Object = Particles.Geometry(RigidBodyIdx);	//todo(ocohen): can this ever be null?
			Pair<TVector<float, 3>, bool> Result = Object->FindClosestIntersection(StartLocal, EndLocal, /*Thickness=*/0.f);
			if (Result.Second)	//todo(ocohen): once we do more than just a bool we need to get the closest point
			{
#if WITH_PHYSX
				//todo(ocohen): check output flags?
				const float Distance = (Result.First - StartLocal).Size();
				if (!bFound || Distance < OutHit.distance)
				{
					OutHit.distance = Distance;	//todo(ocohen): assuming physx structs for now
					OutHit.position = U2PVector(TM.TransformPositionNoScale(Result.First));
					const TVector<float, 3> LocalNormal = Object->Normal(Result.First);
					OutHit.normal = U2PVector(TM.TransformVectorNoScale(LocalNormal));
					SetFlags(OutHit, EHitFlags::Distance | EHitFlags::Normal | EHitFlags::Position);
				}
				bFound = true;
#endif
			}
		}
	}

	return bFound;
}

void FGeometryCollectionSQAccelerator::Raycast(const FVector& Start, const FVector& Dir, FPhysicsHitCallback<FHitRaycast>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& QueryFilter, FCollisionQueryFilterCallback& QueryCallback) const
{
	// #BGallagher Temp lock semantics. Essentially this guarantees t or t+1 depending on which is closer by stalling
	// the physics thread at the next update step so we can perform the query. Long term we want multiple methods for
	// queries as t-1 can be made much cheaper for applications where immediate results don't matter.
	FChaosScopedPhysicsThreadLock ThreadLock;

	SCOPE_CYCLE_COUNTER(STAT_GCRaycast);

#if WITH_PHYSX
	for (const UGeometryCollectionComponent* GeomCollectionComponent : Components)
	{
		FHitRaycast Hit;
		if (LowLevelRaycast(*GeomCollectionComponent, Start, Dir, GetCurrentBlockTraceDistance(HitBuffer), OutputFlags, Hit))	//assume all blocking hits for now
		{
#if !WITH_IMMEDIATE_PHYSX && PHYSICS_INTERFACE_PHYSX
			//todo(ocohen):hack placeholder while we convert over to non physx API
			const FPhysicsActorHandle& ActorHandle = GeomCollectionComponent->DummyBoxComponent->BodyInstance.GetPhysicsActorHandle();
			PxRigidActor* PRigidActor = ActorHandle.SyncActor;
			uint32 PNumShapes = PRigidActor->getNbShapes();
			TArray<PxShape*> PShapes;
			PShapes.AddZeroed(PNumShapes);
			PRigidActor->getShapes(PShapes.GetData(), sizeof(PShapes[0]) * PNumShapes);
			SetActor(Hit, ActorHandle.SyncActor);
			SetShape(Hit, PShapes[0]);
#else
			check(false);	//this can't actually return nullptr since higher up API assumes both shape and actor exists in the low level
			SetActor(Hit, nullptr);
			SetShape(Hit, nullptr);	//todo(ocohen): what do we return for apeiron?
#endif
			Insert(HitBuffer, Hit, true);	//for now assume all blocking hits
		}
	}
#endif
}

void FGeometryCollectionSQAccelerator::Sweep(const FPhysicsGeometry& QueryGeom, const FTransform& StartTM, const FVector& Dir, FPhysicsHitCallback<FHitSweep>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& QueryFilter, FCollisionQueryFilterCallback& QueryCallback) const
{
}

void FGeometryCollectionSQAccelerator::Overlap(const FPhysicsGeometry& QueryGeom, const FTransform& GeomPose, FPhysicsHitCallback<FHitOverlap>& HitBuffer, FQueryFlags QueryFlags, const FCollisionFilterData& QueryFilter, FCollisionQueryFilterCallback& QueryCallback) const
{
}

void FGeometryCollectionSQAccelerator::AddComponent(UGeometryCollectionComponent* Component)
{
	Components.Add(Component);
}

void FGeometryCollectionSQAccelerator::RemoveComponent(UGeometryCollectionComponent* Component)
{
	Components.Remove(Component);
}
#endif