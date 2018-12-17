// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
GeometryCollectionActor.cpp: AGeometryCollectionActor methods.
=============================================================================*/

#if INCLUDE_CHAOS

#include "GeometryCollection/GeometryCollectionActor.h"

#include "Chaos/Utilities.h"
#include "Chaos/Plane.h"
#include "Chaos/Box.h"
#include "Chaos/Sphere.h"
#include "Chaos/PerParticleGravity.h"
#include "Chaos/ImplicitObject.h"
#include "Engine/SkeletalMesh.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionUtility.h"
#include "GeometryCollection/GeometryCollectionBoneNode.h"
#include "Math/Box.h"
#include "Physics/PhysicsInterfaceCore.h"


DEFINE_LOG_CATEGORY_STATIC(AGeometryCollectionActorLogging, Log, All);

AGeometryCollectionActor::AGeometryCollectionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UE_LOG(AGeometryCollectionActorLogging, Verbose, TEXT("AGeometryCollectionActor::AGeometryCollectionActor()"));

	GeometryCollectionComponent = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("GeometryCollectionComponent0"));
	RootComponent = GeometryCollectionComponent;

#if GEOMETRYCOLLECTION_DEBUG_DRAW
	GeometryCollectionDebugDrawComponent = CreateDefaultSubobject<UGeometryCollectionDebugDrawComponent>(TEXT("GeometryCollectionDrawComponent0"));
	GeometryCollectionDebugDrawComponent->GeometryCollectionComponent = GeometryCollectionComponent;
#else
	GeometryCollectionDebugDrawComponent = nullptr;
#endif

	PrimaryActorTick.bCanEverTick = true;
	SetActorTickEnabled(true);
}


void AGeometryCollectionActor::Tick(float DeltaTime) 
{
	UE_LOG(AGeometryCollectionActorLogging, Verbose, TEXT("AGeometryCollectionActor::Tick()"));
	if (GeometryCollectionComponent)
	{
		GeometryCollectionComponent->SetRenderStateDirty();
	}
}


const Chaos::PBDRigidsSolver* GetSolver(const AGeometryCollectionActor& GeomCollectionActor)
{
	return GeomCollectionActor.GetGeometryCollectionComponent()->ChaosSolverActor != nullptr ? GeomCollectionActor.GetGeometryCollectionComponent()->ChaosSolverActor->GetSolver() : FPhysScene_Chaos::GetInstance()->GetSolver();
}


bool LowLevelRaycastImp(const Chaos::TVector<float, 3>& Start, const Chaos::TVector<float, 3>& Dir, float DeltaMag, const AGeometryCollectionActor& GeomCollectionActor, FHitResult& OutHit)
{
	using namespace Chaos;
	//todo(ocohen): need to add thread safety / lock semantics
	const TManagedArray<int32>& RigidBodyIdArray = GeomCollectionActor.GetGeometryCollectionComponent()->GetRigidBodyIdArray();
	const TSharedPtr<FPhysScene_Chaos> Scene = GeomCollectionActor.GetGeometryCollectionComponent()->GetPhysicsScene();
	ensure(Scene);

	const Chaos::PBDRigidsSolver* Solver = GetSolver(GeomCollectionActor);
	ensure(Solver);

	const TPBDRigidParticles<float, 3>& Particles = Solver->GetRigidParticles();	//todo(ocohen): should these just get passed in instead of hopping through scene?

	for (int32 Idx = 0; Idx < RigidBodyIdArray.Num(); ++Idx)
	{
		const int32 RigidBodyIdx = RigidBodyIdArray[Idx];
		const TRigidTransform<float, 3> TM(Particles.X(RigidBodyIdx), Particles.R(RigidBodyIdx));
		const TVector<float, 3> StartLocal = TM.InverseTransformPositionNoScale(Start);
		const TVector<float, 3> DirLocal = TM.InverseTransformVectorNoScale(Dir);
		const TVector<float, 3> EndLocal = StartLocal + DirLocal * DeltaMag;	//todo(ocohen): apeiron just undoes this later, we should fix the API

		const TImplicitObject<float, 3>* Object = Particles.Geometry(RigidBodyIdx);	//todo(ocohen): can this ever be null?
		Pair<TVector<float, 3>, bool> Result = Object->FindClosestIntersection(StartLocal, EndLocal, /*Thickness=*/0.f);
		if (Result.Second)	//todo(ocohen): once we do more than just a bool we need to get the closest point
		{
			const float Distance = (Result.First - StartLocal).Size();
			OutHit.Actor = const_cast<AGeometryCollectionActor*>(&GeomCollectionActor);
			OutHit.Component = GeomCollectionActor.GetGeometryCollectionComponent();
			OutHit.bBlockingHit = true;
			OutHit.Distance = Distance;
			OutHit.Time = Distance / (EndLocal - StartLocal).Size();
			OutHit.Location = TM.TransformPositionNoScale(Result.First);
			OutHit.ImpactPoint = OutHit.Location;
			const TVector<float, 3> LocalNormal = Object->Normal(Result.First);
			OutHit.ImpactNormal = TM.TransformVectorNoScale(LocalNormal);
			OutHit.Normal = OutHit.ImpactNormal;

			return true;
		}
	}

	return false;
}

bool AGeometryCollectionActor::RaycastSingle(FVector Start, FVector End, FHitResult& OutHit) const
{
	if (GeometryCollectionComponent)
	{
		OutHit = FHitResult();
		OutHit.TraceStart = Start;
		OutHit.TraceEnd = End;
		const FVector Delta = (End - Start);
		const float DeltaMag = Delta.Size();
		if (DeltaMag > KINDA_SMALL_NUMBER)
		{
			const FVector Dir = Delta / DeltaMag;
			return LowLevelRaycastImp(Start, Dir, DeltaMag, *this, OutHit);
		}
	}
	return false;
}

#endif
