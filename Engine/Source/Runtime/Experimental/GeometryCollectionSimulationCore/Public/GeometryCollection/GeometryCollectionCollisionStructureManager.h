// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#if INCLUDE_CHAOS

#include "CoreMinimal.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionSimulationCoreTypes.h"
#include "GeometryCollection/GeometryCollectionSimulationTypes.h"
#include "UObject/ObjectMacros.h"

#include "PBDRigidsSolver.h"

namespace Chaos
{
	template <typename T>
	class TTriangleMesh;
	template <typename T, int d>
	class TLevelSet;
	template <typename T, int d>
	class TParticles;
}

class GEOMETRYCOLLECTIONSIMULATIONCORE_API FCollisionStructureManager
{
public:
	FCollisionStructureManager();
	virtual ~FCollisionStructureManager();

	typedef TArray<Chaos::TVector<float, 3>> FSimplicial;

	typedef Chaos::TImplicitObject<float, 3> FImplicit;

	struct FElement {
		FSimplicial* Simplicial;
		FImplicit* Implicit;
		FVector InertiaTensor;
		Chaos::TTriangleMesh<float>* TriangleMesh;
		float Volume;
		float Mass;
	};

	static FSimplicial * NewSimplicial(
		const Chaos::TParticles<float,3>& AllParticles,
		const TManagedArray<int32>& BoneMap,
		const TManagedArray<int32>& CollisionMask,
		const ECollisionTypeEnum CollisionType,
		Chaos::TTriangleMesh<float>& TriMesh,
		const float CollisionParticlesFraction
	);

	static FImplicit * NewImplicit(
		const Chaos::TParticles<float, 3>& MeshParticles,
		const Chaos::TTriangleMesh<float>& TriMesh,
		const FBox& CollisionBoundsArray,
		const float Radius,
		const int32 MinRes,
		const int32 MaxRes,
		const ECollisionTypeEnum CollisionType,
		const EImplicitTypeEnum ImplicitType
	);

	static FVector CalculateUnitMassInertiaTensor(
		const FBox& BoundingBox,
		const float Radius,
		const EImplicitTypeEnum ImplicitType
	);

	static float CalculateVolume(
		const FBox& BoundingBox,
		const float Radius,
		const EImplicitTypeEnum ImplicitType
	);


	static Chaos::TLevelSet<float, 3>* NewLevelset(
		const Chaos::TParticles<float, 3>& MeshParticles,
		const Chaos::TTriangleMesh<float>& TriMesh,
		const FBox& CollisionBounds,
		int32 MinRes,
		int32 MaxRes,
		ECollisionTypeEnum CollisionType
	);

	static void UpdateImplicitFlags(FImplicit* Implicit, ECollisionTypeEnum CollisionType);


	TMap<int32, FElement> Map;
};

#endif