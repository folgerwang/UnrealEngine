// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Array.h"
#include "Chaos/ArrayND.h"
#include "Chaos/BoundingVolumeUtilities.h"
#include "Chaos/Box.h"
#include "Chaos/Defines.h"
#include "Chaos/Framework/Parallel.h"
#include "Chaos/Transform.h"
#include "Chaos/UniformGrid.h"

#include <memory>
#include <unordered_set>

namespace Chaos
{
template<class OBJECT_ARRAY, class T, int d>
class TBoundingVolume
{
  public:
	TBoundingVolume(const OBJECT_ARRAY& Objects, const bool bUseVelocity = false, const T Dt = 0, const int32 MaxCells = 100)
	    : MObjects(Objects)
	{
		check(GetObjectCount(Objects) > 0);
		TArray<int32> AllObjects;
		for (int32 i = 0; i < GetObjectCount(Objects); ++i)
		{
			if (IsDisabled(Objects, i))
				continue;
			if (HasBoundingBox(Objects, i))
			{
				AllObjects.Add(i);
			}
			else
			{
				MGlobalObjects.Add(i);
			}
		}
		GenerateTree(Objects, AllObjects, bUseVelocity, Dt, MaxCells);
	}

	TBoundingVolume(const OBJECT_ARRAY& Objects, const TArray<uint32>& ActiveIndices, const bool bUseVelocity = false, const T Dt = 0, const int32 MaxCells = 100)
	    : MObjects(Objects)
	{
		check(GetObjectCount(Objects) > 0);
		TArray<int32> AllObjects;
		for (int32 i = 0; i < GetObjectCount(Objects); ++i)
		{
			if (!HasBoundingBox(Objects, i))
			{
				MGlobalObjects.Add(i);
			}
		}
		for (const auto& Index : ActiveIndices)
		{
			check(!IsDisabled(Objects, Index));
			if (HasBoundingBox(Objects, Index))
			{
				AllObjects.Add(Index);
			}
		}
		GenerateTree(Objects, AllObjects, bUseVelocity, Dt, MaxCells);
	}

	void GenerateTree(const OBJECT_ARRAY& Objects, const TArray<int32>& AllObjects, const bool bUseVelocity, const T Dt, const int32 MaxCells)
	{
		if (!AllObjects.Num())
		{
			return;
		}
		ComputeAllWorldSpaceBoundingBoxes(Objects, AllObjects, bUseVelocity, Dt, MWorldSpaceBoxes);
		TBox<T, d> GlobalBox = Chaos::GetWorldSpaceBoundingBox(Objects, AllObjects[0], MWorldSpaceBoxes);
		T Dx = TVector<T, d>::DotProduct(GlobalBox.Extents(), TVector<T, d>(1)) / d;
		for (int32 i = 1; i < AllObjects.Num(); ++i)
		{
			const auto& WorldBox = Chaos::GetWorldSpaceBoundingBox(Objects, AllObjects[i], MWorldSpaceBoxes);
			Dx += TVector<T, d>::DotProduct(WorldBox.Extents(), TVector<T, d>(1)) / d;
			GlobalBox.GrowToInclude(WorldBox);
		}
		Dx /= AllObjects.Num();
		TVector<int32, d> Cells = Dx > 0 ? GlobalBox.Extents() / Dx : TVector<int32, d>(MaxCells);
		Cells += TVector<int32, d>(1);
		for (int32 Axis = 0; Axis < d; ++Axis)
		{
			if (Cells[Axis] > MaxCells)
				Cells[Axis] = MaxCells;
			if (!(ensure(Cells[Axis] >=0 )))	//seeing this because GlobalBox is huge leading to int overflow. Need to investigate why bounds get so big
			{
				Cells[Axis] = MaxCells;
			}
		}
		MGrid = TUniformGrid<T, d>(GlobalBox.Min(), GlobalBox.Max(), Cells);
		MElements = TArrayND<TArray<int32>, d>(MGrid);
		for (int32 i = 0; i < AllObjects.Num(); ++i)
		{
			const auto& ObjectBox = Chaos::GetWorldSpaceBoundingBox(Objects, AllObjects[i], MWorldSpaceBoxes);
			const auto StartIndex = MGrid.ClampIndex(MGrid.Cell(ObjectBox.Min()));
			const auto EndIndex = MGrid.ClampIndex(MGrid.Cell(ObjectBox.Max()));
			for (int32 x = StartIndex[0]; x <= EndIndex[0]; ++x)
			{
				for (int32 y = StartIndex[1]; y <= EndIndex[1]; ++y)
				{
					for (int32 z = StartIndex[2]; z <= EndIndex[2]; ++z)
					{
						MElements(x, y, z).Add(AllObjects[i]);
					}
				}
			}
		}
		T NumObjects = 0;
		for (int32 i = 0; i < MGrid.GetNumCells(); ++i)
		{
			NumObjects += MElements(MGrid.GetIndex(i)).Num();
		}
		NumObjects /= AllObjects.Num();
		UE_LOG(LogChaos, Verbose, TEXT("Generated Tree with (%d, %d, %d) Nodes and %f Per Cell"), MGrid.Counts()[0], MGrid.Counts()[1], MGrid.Counts()[2], NumObjects);
	}

	template<class T_INTERSECTION>
	TArray<int32> FindAllIntersections(const T_INTERSECTION& Intersection)
	{
		TArray<int32> IntersectionList = FindAllIntersectionsHelper(Intersection);
		IntersectionList.Append(MGlobalObjects);
		return IntersectionList;
	}

	TArray<int32> FindAllIntersections(const TGeometryParticles<T, d>& InParticles, const int32 i)
	{
		return FindAllIntersections(Chaos::GetWorldSpaceBoundingBox(InParticles, i, MWorldSpaceBoxes));
	}

	const TArray<int32>& GlobalObjects()
	{
		return MGlobalObjects;
	}

	// TODO(mlentine): Need to move this elsewhere; probably on CollisionConstraint
	const TBox<T, d>& GetWorldSpaceBoundingBox(const TGeometryParticles<T, d>& InParticles, const int32 Index)
	{
		return Chaos::GetWorldSpaceBoundingBox(InParticles, Index, MWorldSpaceBoxes);
	}

  private:
	TArray<int32> FindAllIntersectionsHelper(const TVector<T, d>& Point)
	{
		return MElements(MGrid.Cell(Point));
	}

	TArray<int32> FindAllIntersectionsHelper(const TBox<T, d>& ObjectBox)
	{
		TArray<int32> Intersections;
		TSet<int32> Visited;
		const auto StartIndex = MGrid.ClampIndex(MGrid.Cell(ObjectBox.Min()));
		const auto EndIndex = MGrid.ClampIndex(MGrid.Cell(ObjectBox.Max()));
		for (int32 x = StartIndex[0]; x <= EndIndex[0]; ++x)
		{
			for (int32 y = StartIndex[1]; y <= EndIndex[1]; ++y)
			{
				for (int32 z = StartIndex[2]; z <= EndIndex[2]; ++z)
				{
					const auto LocalList = MElements(x, y, z);
					for (const auto& Element : LocalList)
					{
						if (!Visited.Contains(Element))
						{
							Visited.Add(Element);
							Intersections.Add(Element);
						}
					}
				}
			}
		}
		return Intersections;
	}

	const OBJECT_ARRAY& MObjects;
	TArray<int32> MGlobalObjects;
	TArray<TBox<T, d>> MWorldSpaceBoxes;
	TUniformGrid<T, d> MGrid;
	TArrayND<TArray<int32>, d> MElements;
	FCriticalSection CriticalSection;
};
}