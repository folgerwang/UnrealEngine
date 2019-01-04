// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Chaos/PBDCollisionConstraintUtil.h"

#include "Chaos/BoundingVolume.h"
#include "Chaos/BoundingVolumeHierarchy.h"
#include "Chaos/Defines.h"
#include "Chaos/Pair.h"
#include "Chaos/PBDContactGraph.h"
#include "Chaos/Sphere.h"
#include "Chaos/Transform.h"
#include "ChaosStats.h"
#include "ChaosLog.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "Chaos/ImplicitObjectUnion.h"
#include "Chaos/PBDCollisionTypes.h"

namespace Chaos
{
	void ComputeHashTable(const TArray<Chaos::TPBDCollisionConstraint<float, 3>::FRigidBodyContactConstraint>& ConstraintsArray,
						  const FBox& BoundingBox, TMultiMap<int32, int32>& HashTableMap, const float SpatialHashRadius)
	{
		float CellSize = 2.f * SpatialHashRadius;
		check(CellSize > 0.f);

		// Compute number of cells along the principal axis
		FVector Extent = 2.f * BoundingBox.GetExtent();
		float PrincipalAxisLength;
		if (Extent.X > Extent.Y && Extent.X > Extent.Z)
		{
			PrincipalAxisLength = Extent.X;
		}
		else if (Extent.Y > Extent.Z)
		{
			PrincipalAxisLength = Extent.Y;
		}
		else
		{
			PrincipalAxisLength = Extent.Z;
		}
		int32 NumberOfCells = FMath::CeilToInt(PrincipalAxisLength / CellSize);
		check(NumberOfCells > 0);

		CellSize = PrincipalAxisLength / (float)NumberOfCells;
		float CellSizeInv = 1.f / CellSize;

		int32 NumberOfCellsX = FMath::CeilToInt(Extent.X * CellSizeInv) + 1;
		int32 NumberOfCellsY = FMath::CeilToInt(Extent.Y * CellSizeInv) + 1;
		int32 NumberOfCellsZ = FMath::CeilToInt(Extent.Z * CellSizeInv) + 1;

		// Create a Hash Table, but only store the buckets with constraint(s) as a map
		// HashTableMap<BucketIdx, ConstraintIdx>
		int32 NumberOfCellsXY = NumberOfCellsX * NumberOfCellsY;
		int32 NumberOfCellsXYZ = NumberOfCellsX * NumberOfCellsY * NumberOfCellsZ;
		for (int32 IdxConstraint = 0; IdxConstraint < ConstraintsArray.Num(); ++IdxConstraint)
		{
			FVector Location = (FVector)ConstraintsArray[IdxConstraint].Location - BoundingBox.Min + FVector(0.5f * CellSize);
			int32 HashTableIdx = (int32)(Location.X * CellSizeInv) +
								 (int32)(Location.Y * CellSizeInv) * NumberOfCellsX +
								 (int32)(Location.Z * CellSizeInv) * NumberOfCellsXY;
			if (ensure(HashTableIdx < NumberOfCellsXYZ))
			{
				HashTableMap.Add(HashTableIdx, IdxConstraint);
			}
		}
	}

	void ComputeHashTable(const TArray<FVector>& ParticleArray, const FBox& BoundingBox, TMultiMap<int32, int32>& HashTableMap, const float SpatialHashRadius)
	{
		float CellSize = 2.f * SpatialHashRadius;
		check(CellSize > 0.f);

		// Compute number of cells along the principal axis
		FVector Extent = 2.f * BoundingBox.GetExtent();
		float PrincipalAxisLength;
		if (Extent.X > Extent.Y && Extent.X > Extent.Z)
		{
			PrincipalAxisLength = Extent.X;
		}
		else if (Extent.Y > Extent.Z)
		{
			PrincipalAxisLength = Extent.Y;
		}
		else
		{
			PrincipalAxisLength = Extent.Z;
		}
		int32 NumberOfCells = FMath::CeilToInt(PrincipalAxisLength / CellSize);
		check(NumberOfCells > 0);

		CellSize = PrincipalAxisLength / (float)NumberOfCells;
		float CellSizeInv = 1.f / CellSize;

		int32 NumberOfCellsX = FMath::CeilToInt(Extent.X * CellSizeInv) + 1;
		int32 NumberOfCellsY = FMath::CeilToInt(Extent.Y * CellSizeInv) + 1;
		int32 NumberOfCellsZ = FMath::CeilToInt(Extent.Z * CellSizeInv) + 1;

		// Create a Hash Table, but only store the buckets with constraint(s) as a map
		// HashTableMap<BucketIdx, ConstraintIdx>
		int32 NumberOfCellsXY = NumberOfCellsX * NumberOfCellsY;
		int32 NumberOfCellsXYZ = NumberOfCellsX * NumberOfCellsY * NumberOfCellsZ;
		for (int32 IdxConstraint = 0; IdxConstraint < ParticleArray.Num(); ++IdxConstraint)
		{
			FVector Location = ParticleArray[IdxConstraint] - BoundingBox.Min + FVector(0.5f * CellSize);
			int32 HashTableIdx = (int32)(Location.X * CellSizeInv) +
								 (int32)(Location.Y * CellSizeInv) * NumberOfCellsX +
								 (int32)(Location.Z * CellSizeInv) * NumberOfCellsXY;
			if (ensure(HashTableIdx < NumberOfCellsXYZ))
			{
				HashTableMap.Add(HashTableIdx, IdxConstraint);
			}
		}
	}

}

