// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Chaos/BoundingVolumeHierarchy.h"
#include "Chaos/BoundingVolumeUtilities.h"
#include "ChaosStats.h"

#define MIN_NUM_OBJECTS 5

using namespace Chaos;

namespace Chaos
{
	float BoundsThicknessMultiplier = 1.f;	//should not be based on bounds, should be based on distance
	FAutoConsoleVariableRef CVarBoundsThicknessMultiplier(TEXT("p.BoundsThicknessMultiplier"), BoundsThicknessMultiplier, TEXT(""));

	float MinBoundsThickness = 0.1f;
	FAutoConsoleVariableRef CVarMinBoundsThickness(TEXT("p.MinBoundsThickness"), MinBoundsThickness, TEXT(""));
}

template<class OBJECT_ARRAY, class T, int d>
TBoundingVolumeHierarchy<OBJECT_ARRAY, T, d>::TBoundingVolumeHierarchy(const OBJECT_ARRAY& Objects, const int32 MaxLevels)
    : MObjects(&Objects), MMaxLevels(MaxLevels)
{
	if (GetObjectCount(Objects) > 0)
	{
		UpdateHierarchy();
	}
}

template<class OBJECT_ARRAY, class T, int d>
void TBoundingVolumeHierarchy<OBJECT_ARRAY, T, d>::UpdateHierarchy(const bool bAllowMultipleSplitting)
{
	check(GetObjectCount(*MObjects) > 0);
	TArray<int32> AllObjects;
	for (int32 i = 0; i < GetObjectCount(*MObjects); ++i)
	{
		if (HasBoundingBox(*MObjects, i))
		{
			AllObjects.Add(i);
		}
		else
		{
			MGlobalObjects.Add(i);
		}
	}
	Elements.Reset();
	MWorldSpaceBoxes.Reset();

	if (!AllObjects.Num())
	{
		return;
	}
	ComputeAllWorldSpaceBoundingBoxes(*MObjects, AllObjects, false, (T)0, MWorldSpaceBoxes);

	int32 Axis;
	const TBox<T, d> GlobalBox = ComputeGlobalBoxAndSplitAxis(*MObjects, AllObjects, MWorldSpaceBoxes, bAllowMultipleSplitting, Axis);

	Node RootNode;
	RootNode.MMin = GlobalBox.Min();
	RootNode.MMax = GlobalBox.Max();
	RootNode.MAxis = Axis;
	RootNode.MObjects = AllObjects;
	Elements.Add(RootNode);
	if (AllObjects.Num() > MIN_NUM_OBJECTS) // TODO(mlentine): What is a good number to stop at?
	{
		int32 StartIndex = GenerateNextLevel(GlobalBox.Min(), GlobalBox.Max(), AllObjects, Axis, 1, bAllowMultipleSplitting);
		for (int32 i = 0; i < (Axis == -1 ? 8 : 2); i++)
		{
			Elements[0].MChildren.Add(StartIndex + i);
		}
	}
	UE_LOG(LogChaos, Verbose, TEXT("Generated Tree with %d Nodes"), Elements.Num());
	//PrintTree("", &Elements[0]);
}

template<class OBJECT_ARRAY, class T, int d>
TArray<int32> TBoundingVolumeHierarchy<OBJECT_ARRAY, T, d>::FindAllIntersectionsHelper(const Node& MyNode, const TVector<T, d>& Point) const
{
	TBox<T, d> MBox(MyNode.MMin, MyNode.MMax);
	if (MBox.SignedDistance(Point) > 0)
	{
		return TArray<int32>();
	}
	if (MyNode.MChildren.Num() == 0)
	{
		return MyNode.MObjects;
	}
	const TVector<T, d> ObjectCenter = MBox.Center();
	int32 Child = 0;
	if (MyNode.MAxis >= 0)
	{
		if (Point[MyNode.MAxis] > ObjectCenter[MyNode.MAxis])
			Child += 1;
	}
	else
	{
		if (Point[0] > ObjectCenter[0])
			Child += 1;
		if (Point[1] > ObjectCenter[1])
			Child += 2;
		if (Point[2] > ObjectCenter[2])
			Child += 4;
	}
	return FindAllIntersectionsHelper(Elements[MyNode.MChildren[Child]], Point);
}

template <typename OBJECT_ARRAY>
struct TSpecializeParticlesHelper
{
	template <typename T, int d>
	static void AccumulateChildrenResults(TArray<int32>& AccumIntersectionList, TSet<int32>& AccumIntersectionSet, const TArray<int32>& PotentialChildren, const TBox<T, d>& ObjectBox, const TArray<TBox<T,d>>& WorldSpaceBoxes)
	{
		for (int32 ChildIndex : PotentialChildren)
		{
			if (!AccumIntersectionSet.Contains(ChildIndex))
			{
				if (WorldSpaceBoxes[ChildIndex].Intersects(ObjectBox))
				{
					AccumIntersectionSet.Add(ChildIndex);
					AccumIntersectionList.Add(ChildIndex);
				}
			}
		}
	}
};

int32 CheckBox = 1;
FAutoConsoleVariableRef CVarCheckBox(TEXT("p.checkbox"), CheckBox, TEXT(""));

template <typename T, int d>
struct TSpecializeParticlesHelper<TParticles<T,d>>
{
	static void AccumulateChildrenResults(TArray<int32>& AccumIntersectionList, TSet<int32>& AccumIntersectionSet, const TArray<int32>& PotentialChildren, const TBox<T, d>& ObjectBox, const TArray<TBox<T, d>>& WorldSpaceBoxes)
	{
		if (CheckBox)
		{
			for (int32 ChildIndex : PotentialChildren)
			{
				if (WorldSpaceBoxes[ChildIndex].Intersects(ObjectBox))	//todo(ocohen): actually just a single point so should call Contains
				{
					AccumIntersectionList.Add(ChildIndex);
				}
			}
		}
		else
		{
			AccumIntersectionList.Append(PotentialChildren);
		}
	}
};

int32 FindAllIntersectionsSingleThreaded = 1;
FAutoConsoleVariableRef CVarFindAllIntersectionsSingleThreaded(TEXT("p.FindAllIntersectionsSingleThreaded"), FindAllIntersectionsSingleThreaded, TEXT(""));


template<class OBJECT_ARRAY, class T, int d>
void TBoundingVolumeHierarchy<OBJECT_ARRAY, T, d>::FindAllIntersectionsHelperRecursive(const Node& MyNode, const TBox<T, d>& ObjectBox, TArray<int32>& AccumulateElements, TSet<int32>& AccumulateSet) const
{
	TBox<T, d> MBox(MyNode.MMin, MyNode.MMax);
	if (!MBox.Intersects(ObjectBox))
	{
		return;
	}
	if (MyNode.MChildren.Num() == 0)
	{
		TSpecializeParticlesHelper<OBJECT_ARRAY>::AccumulateChildrenResults(AccumulateElements, AccumulateSet, MyNode.MObjects, ObjectBox, MWorldSpaceBoxes);
		return;
	}

	
	int32 NumChildren = MyNode.MChildren.Num();
	for (int32 Child = 0; Child < NumChildren; ++Child)
	{
		FindAllIntersectionsHelperRecursive(Elements[MyNode.MChildren[Child]], ObjectBox, AccumulateElements, AccumulateSet);
	}
}

int32 UseAccumulationArray = 1;
FAutoConsoleVariableRef CVarUseAccumulationArray(TEXT("p.UseAccumulationArray"), UseAccumulationArray, TEXT(""));

template<class OBJECT_ARRAY, class T, int d>
TArray<int32> TBoundingVolumeHierarchy<OBJECT_ARRAY, T, d>::FindAllIntersectionsHelper(const Node& MyNode, const TBox<T, d>& ObjectBox) const
{
		TArray<int32> IntersectionList;
		TSet<int32> IntersectionSet;
		FindAllIntersectionsHelperRecursive(MyNode, ObjectBox, IntersectionList, IntersectionSet);
		return IntersectionList;
}


template<class OBJECT_ARRAY, class T, int d>
TArray<int32> TBoundingVolumeHierarchy<OBJECT_ARRAY, T, d>::FindAllIntersections(const TGeometryParticles<T, d>& InParticles, const int32 i) const
{
	return FindAllIntersections(GetWorldSpaceBoundingBox(InParticles, i, MWorldSpaceBoxes));
}

template <int d>
struct FSplitCount
{
	FSplitCount()
	{
		for (int i = 0; i < d; ++i)
		{
			Neg[i] = 0;
			Pos[i] = 0;
		}
	}

	int32 Neg[d];
	int32 Pos[d];
};

template <typename T, int d>
void AccumulateNextLevelCount(const TBox<T, d>& Box, const TVector<T, d>& MidPoint, FSplitCount<d>& Counts)
{
	//todo(ocohen): particles min = max so avoid extra work
	for (int32 i = 0; i < d; ++i)
	{
		Counts.Neg[i] += (Box.Max()[i] < MidPoint[i] || Box.Min()[i] < MidPoint[i]) ? 1 : 0;
		Counts.Pos[i] += (Box.Min()[i] > MidPoint[i] || Box.Max()[i] > MidPoint[i]) ? 1 : 0;
	}
}
template<class OBJECT_ARRAY, class T, int d>
int32 TBoundingVolumeHierarchy<OBJECT_ARRAY, T, d>::GenerateNextLevel(const TVector<T, d>& GlobalMin, const TVector<T, d>& GlobalMax, const TArray<int32>& Objects, const int32 Axis, const int32 Level, const bool AllowMultipleSplitting)
{
	if (Axis == -1)
	{
		return GenerateNextLevel(GlobalMin, GlobalMax, Objects, Level);
	}

	
	FSplitCount<d> Counts[2];
	TArray<Node> LocalElements;
	LocalElements.SetNum(2);
	TBox<T, d> GlobalBox(GlobalMin, GlobalMax);
	const TVector<T, d> WorldCenter = GlobalBox.Center();
	const TVector<T, d> MinCenterSearch = TBox<T, d>(GlobalMin, WorldCenter).Center();
	const TVector<T, d> MaxCenterSearch = TBox<T, d>(WorldCenter, GlobalMax).Center();

	for (int32 i = 0; i < Objects.Num(); ++i)
	{
		check(Objects[i] >= 0 && Objects[i] < GetObjectCount(*MObjects));
		const TBox<T, d>& ObjectBox = GetWorldSpaceBoundingBox(*MObjects, Objects[i], MWorldSpaceBoxes);
		const TVector<T, d> ObjectCenter = ObjectBox.Center();
		bool MinA = false, MaxA = false;
		if (ObjectBox.Min()[Axis] < WorldCenter[Axis])
		{
			MinA = true;
		}
		if (ObjectBox.Max()[Axis] >= WorldCenter[Axis])
		{
			MaxA = true;
		}
		check(MinA || MaxA);
		if (MinA)
		{
			LocalElements[0].MObjects.Add(Objects[i]);
			AccumulateNextLevelCount(ObjectBox, MinCenterSearch, Counts[0]);
		}
		if (MaxA)
		{
			LocalElements[1].MObjects.Add(Objects[i]);
			AccumulateNextLevelCount(ObjectBox, MaxCenterSearch, Counts[1]);
		}
	}
	PhysicsParallelFor(2, [&](int32 i) {
		TVector<T, d> Min = GlobalBox.Min();
		TVector<T, d> Max = GlobalBox.Max();
		if (i == 0)
			Max[Axis] = WorldCenter[Axis];
		else
			Min[Axis] = WorldCenter[Axis];
		LocalElements[i].MMin = Min;
		LocalElements[i].MMax = Max;
		LocalElements[i].MAxis = -1;
		if (LocalElements[i].MObjects.Num() > MIN_NUM_OBJECTS && Level < MMaxLevels && LocalElements[i].MObjects.Num() < Objects.Num())
		{
			//we pick the axis that gives us the most culled even in the case when it goes in the wrong direction (i.e the biggest min)
			int32 BestAxis = 0;
			int32 MaxCulled = 0;
			for (int32 LocalAxis = 0; LocalAxis < d; ++LocalAxis)
			{
				int32 CulledWorstCase = FMath::Min(Counts[i].Neg[LocalAxis], Counts[i].Pos[LocalAxis]);
				if (CulledWorstCase > MaxCulled)
				{
					MaxCulled = CulledWorstCase;
					BestAxis = LocalAxis;
				}
			}

			//todo(ocohen): use multi split when counts are very close
			int32 NextAxis = BestAxis;
			LocalElements[i].MAxis = NextAxis;
			int32 StartIndex = GenerateNextLevel(LocalElements[i].MMin, LocalElements[i].MMax, LocalElements[i].MObjects, NextAxis, Level + 1, AllowMultipleSplitting);
			for (int32 j = 0; j < (NextAxis == -1 ? 8 : 2); j++)
			{
				LocalElements[i].MChildren.Add(StartIndex + j);
			}
		}
	});
	CriticalSection.Lock();
	int32 MinElem = Elements.Num();
	Elements.Append(LocalElements);
	CriticalSection.Unlock();
	return MinElem;
}

template<class OBJECT_ARRAY, class T, int d>
int32 TBoundingVolumeHierarchy<OBJECT_ARRAY, T, d>::GenerateNextLevel(const TVector<T, d>& GlobalMin, const TVector<T, d>& GlobalMax, const TArray<int32>& Objects, const int32 Level)
{
	TArray<Node> LocalElements;
	LocalElements.SetNum(8);
	TBox<T, d> GlobalBox(GlobalMin, GlobalMax);
	const TVector<T, d> WorldCenter = GlobalBox.Center();
	for (int32 i = 0; i < Objects.Num(); ++i)
	{
		check(Objects[i] >= 0 && Objects[i] < GetObjectCount(*MObjects));
		const TBox<T, d>& ObjectBox = GetWorldSpaceBoundingBox(*MObjects, Objects[i], MWorldSpaceBoxes);
		const TVector<T, d> ObjectCenter = ObjectBox.Center();
		bool MinX = false, MaxX = false, MinY = false, MaxY = false, MinZ = false, MaxZ = false;
		if (ObjectBox.Min()[0] < WorldCenter[0])
		{
			MinX = true;
		}
		if (ObjectBox.Max()[0] >= WorldCenter[0])
		{
			MaxX = true;
		}
		if (ObjectBox.Min()[1] < WorldCenter[1])
		{
			MinY = true;
		}
		if (ObjectBox.Max()[1] >= WorldCenter[1])
		{
			MaxY = true;
		}
		if (ObjectBox.Min()[2] < WorldCenter[2])
		{
			MinZ = true;
		}
		if (ObjectBox.Max()[2] >= WorldCenter[2])
		{
			MaxZ = true;
		}
		check(MinX || MaxX);
		check(MinY || MaxY);
		check(MinZ || MaxZ);
		if (MinX && MinY && MinZ)
		{
			LocalElements[0].MObjects.Add(Objects[i]);
		}
		if (MaxX && MinY && MinZ)
		{
			LocalElements[1].MObjects.Add(Objects[i]);
		}
		if (MinX && MaxY && MinZ)
		{
			LocalElements[2].MObjects.Add(Objects[i]);
		}
		if (MaxX && MaxY && MinZ)
		{
			LocalElements[3].MObjects.Add(Objects[i]);
		}
		if (MinX && MinY && MaxZ)
		{
			LocalElements[4].MObjects.Add(Objects[i]);
		}
		if (MaxX && MinY && MaxZ)
		{
			LocalElements[5].MObjects.Add(Objects[i]);
		}
		if (MinX && MaxY && MaxZ)
		{
			LocalElements[6].MObjects.Add(Objects[i]);
		}
		if (MaxX && MaxY && MaxZ)
		{
			LocalElements[7].MObjects.Add(Objects[i]);
		}
	}
	PhysicsParallelFor(8, [&](int32 i) {
		TVector<T, d> Min = GlobalBox.Min();
		TVector<T, d> Max = GlobalBox.Max();
		if (i % 2 == 0)
			Max[0] = WorldCenter[0];
		else
			Min[0] = WorldCenter[0];
		if ((i / 2) % 2 == 0)
			Max[1] = WorldCenter[1];
		else
			Min[1] = WorldCenter[1];
		if (i < 4)
			Max[2] = WorldCenter[2];
		else
			Min[2] = WorldCenter[2];
		LocalElements[i].MMin = Min;
		LocalElements[i].MMax = Max;
		LocalElements[i].MAxis = -1;
		if (LocalElements[i].MObjects.Num() > MIN_NUM_OBJECTS && Level < MMaxLevels && LocalElements[i].MObjects.Num() < Objects.Num())
		{
			TBox<T, d> LocalBox(Min, Max);
			int32 NextAxis = 0;
			TVector<T, d> LocalExtents = LocalBox.Extents();
			if (LocalExtents[2] > LocalExtents[0] && LocalExtents[2] > LocalExtents[1])
			{
				NextAxis = 2;
			}
			else if (LocalExtents[1] > LocalExtents[0])
			{
				NextAxis = 1;
			}
			if (LocalExtents[NextAxis] < LocalExtents[(NextAxis + 1) % 3] * 1.25 && LocalExtents[NextAxis] < LocalExtents[(NextAxis + 2) % 3] * 1.25 && LocalElements[i].MObjects.Num() > 4 * MIN_NUM_OBJECTS)
			{
				NextAxis = -1;
			}
			LocalElements[i].MAxis = NextAxis;
			int32 StartIndex = GenerateNextLevel(LocalElements[i].MMin, LocalElements[i].MMax, LocalElements[i].MObjects, NextAxis, Level + 1, true);
			for (int32 j = 0; j < (NextAxis == -1 ? 8 : 2); j++)
			{
				LocalElements[i].MChildren.Add(StartIndex + j);
			}
		}
	});
	CriticalSection.Lock();
	int32 MinElem = Elements.Num();
	Elements.Append(LocalElements);

	CriticalSection.Unlock();
	return MinElem;
}

template class Chaos::TBoundingVolumeHierarchy<TArray<Chaos::TSphere<float, 3>*>, float, 3>;
template class Chaos::TBoundingVolumeHierarchy<Chaos::TPBDRigidParticles<float, 3>, float, 3>;
template class Chaos::TBoundingVolumeHierarchy<Chaos::TParticles<float, 3>, float, 3>;
template class Chaos::TBoundingVolumeHierarchy<Chaos::TGeometryParticles<float, 3>, float, 3>;