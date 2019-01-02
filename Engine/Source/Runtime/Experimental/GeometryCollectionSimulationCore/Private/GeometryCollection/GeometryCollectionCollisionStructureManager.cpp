// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if INCLUDE_CHAOS

#include "GeometryCollection/GeometryCollectionCollisionStructureManager.h"
#include "HAL/IConsoleManager.h"
#include "Chaos/Box.h"
#include "Chaos/Levelset.h"
#include "Chaos/Sphere.h"
#include "Chaos/Vector.h"
#include "Chaos/Particles.h"

DEFINE_LOG_CATEGORY_STATIC(GCS_Log, NoLogging, All);

FCollisionStructureManager::FCollisionStructureManager()
{
}

FCollisionStructureManager::~FCollisionStructureManager()
{
	for (const TTuple<int32, FElement>& Entry : Map)
	{
		delete Entry.Value.Implicit;
		delete Entry.Value.Simplicial;
	}
	Map.Empty();
}

FCollisionStructureManager::FSimplicial*
FCollisionStructureManager::NewSimplicial(
	const Chaos::TParticles<float,3>& AllParticles,
	const TManagedArray<int32>& BoneMap,
	const TManagedArray<int32>& CollisionMask,
	const ECollisionTypeEnum CollisionType,
	Chaos::TTriangleMesh<float>& TriMesh,
	const float CollisionParticlesFraction
)
{
	const bool bEnableCollisionParticles = (CollisionType == ECollisionTypeEnum::Chaos_Surface_Volumetric);
	if (bEnableCollisionParticles || true)
	{
		// @todo : Clean collision particles need to operate on the collision mask from the DynamicCollection,
		//         then transfer only the good collision particles during the initialization. 

		const TArrayView<const Chaos::TVector<float, 3>> ArrayView(&AllParticles.X(0), AllParticles.Size());
		return new TArray<Chaos::TVector<float, 3>>(Chaos::CleanCollisionParticles(TriMesh, ArrayView, CollisionParticlesFraction));
	}
	return nullptr;
}

void FCollisionStructureManager::UpdateImplicitFlags(FImplicit* Implicit, ECollisionTypeEnum CollisionType)
{
	if (Implicit && (CollisionType == ECollisionTypeEnum::Chaos_Surface_Volumetric))
	{
		Implicit->IgnoreAnalyticCollisions();
		Implicit->SetConvex(false);
	}
}

Chaos::TLevelSet<float, 3>* FCollisionStructureManager::NewLevelset(
	const Chaos::TParticles<float, 3>& MeshParticles,
	const Chaos::TTriangleMesh<float>& TriMesh,
	const FBox& CollisionBounds,
	int32 MinRes,
	int32 MaxRes,
	ECollisionTypeEnum CollisionType
	)
{
	Chaos::TVector<int32, 3> Counts;
	const FVector Extents = CollisionBounds.GetExtent();
	if (Extents.X < Extents.Y && Extents.X < Extents.Z)
	{
		Counts.X = MinRes;
		Counts.Y = MinRes * static_cast<int32>(Extents.Y / Extents.X);
		Counts.Z = MinRes * static_cast<int32>(Extents.Z / Extents.X);
	}
	else if (Extents.Y < Extents.Z)
	{
		Counts.X = MinRes * static_cast<int32>(Extents.X / Extents.Y);
		Counts.Y = MinRes;
		Counts.Z = MinRes * static_cast<int32>(Extents.Z / Extents.Y);
	}
	else
	{
		Counts.X = MinRes * static_cast<int32>(Extents.X / Extents.Z);
		Counts.Y = MinRes * static_cast<int32>(Extents.Y / Extents.Z);
		Counts.Z = MinRes;
	}
	if (Counts.X > MaxRes)
	{
		Counts.X = MaxRes;
	}
	if (Counts.Y > MaxRes)
	{
		Counts.Y = MaxRes;
	}
	if (Counts.Z > MaxRes)
	{
		Counts.Z = MaxRes;
	}
	Chaos::TUniformGrid<float, 3> Grid(CollisionBounds.Min, CollisionBounds.Max, Counts, 1);
	Chaos::TLevelSet<float, 3>* Implicit = new Chaos::TLevelSet<float, 3>(Grid, MeshParticles, TriMesh);
	UpdateImplicitFlags(Implicit, CollisionType);
	return Implicit;
}

FCollisionStructureManager::FImplicit* 
FCollisionStructureManager::NewImplicit(
	const Chaos::TParticles<float, 3>& MeshParticles,
	const Chaos::TTriangleMesh<float>& TriMesh,
	const FBox& CollisionBounds,
	const float Radius, 
	const int32 MinRes,
	const int32 MaxRes,
	const ECollisionTypeEnum CollisionType,
	const EImplicitTypeEnum ImplicitType)
{
	Chaos::TImplicitObject<float, 3>* Implicit = nullptr;
	if (ImplicitType == EImplicitTypeEnum::Chaos_Implicit_Cube)
	{
		Implicit = new Chaos::TBox<float, 3>(CollisionBounds.Min, CollisionBounds.Max);
	}
	else if (ImplicitType == EImplicitTypeEnum::Chaos_Implicit_Sphere)
	{
		Implicit = new Chaos::TSphere<float, 3>(Chaos::TVector<float, 3>(0), Radius);
	}
	else if (ImplicitType == EImplicitTypeEnum::Chaos_Implicit_LevelSet)
	{
		return NewLevelset(MeshParticles, TriMesh, CollisionBounds, MinRes, MaxRes, CollisionType);
	}
	
	UpdateImplicitFlags(Implicit, CollisionType);
	return Implicit;
}

FVector 
FCollisionStructureManager::CalculateUnitMassInertiaTensor(
	const FBox& Bounds,
	const float Radius,
	const EImplicitTypeEnum ImplicitType
)
{	
	FVector Tensor(1);
	if (ImplicitType == EImplicitTypeEnum::Chaos_Implicit_Cube)
	{
		FVector Size = Bounds.GetSize();
		FVector SideSquared(Size.X * Size.X, Size.Y * Size.Y, Size.Z * Size.Z);
		Tensor = FVector((SideSquared.Y + SideSquared.Z) / 12.f, (SideSquared.X + SideSquared.Z) / 12.f, (SideSquared.X + SideSquared.Y) / 12.f);
	}
	else if (ImplicitType == EImplicitTypeEnum::Chaos_Implicit_Sphere)
	{
		Tensor = FVector((2.f / 5.f)*Radius*Radius);
	}
	ensureMsgf(Tensor.X != 0.f && Tensor.Y != 0.f && Tensor.Z != 0.f, TEXT("Rigid bounds check failure."));
	return Tensor;
}


float
FCollisionStructureManager::CalculateVolume(
	const FBox& Bounds,
	const float Radius,
	const EImplicitTypeEnum ImplicitType
)
{
	float Volume = 1.f;
	if (ImplicitType == EImplicitTypeEnum::Chaos_Implicit_Cube)
	{
		Volume = Bounds.GetVolume();
	}
	else if (ImplicitType == EImplicitTypeEnum::Chaos_Implicit_Sphere)
	{
		Volume = (4.f / 3.f)* 3.14159 *Radius*Radius;
	}
	ensureMsgf(Volume != 0.f, TEXT("Rigid volume check failure."));
	return Volume;
}


#endif