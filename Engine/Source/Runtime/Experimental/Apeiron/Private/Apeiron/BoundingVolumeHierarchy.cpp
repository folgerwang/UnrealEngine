// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Apeiron/BoundingVolumeHierarchy.h"
#include "Apeiron/BoundingVolumeUtilities.h"
#include "Async/ParallelFor.h"

#define MIN_NUM_OBJECTS 5

using namespace Apeiron;

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
	if (!AllObjects.Num())
	{
		return;
	}
	ComputeAllWorldSpaceBoundingBoxes(*MObjects, AllObjects, MWorldSpaceBoxes);
	TBox<T, d> GlobalBox = GetWorldSpaceBoundingBox(*MObjects, AllObjects[0], MWorldSpaceBoxes);
	for (int32 i = 1; i < AllObjects.Num(); ++i)
	{
		GlobalBox.GrowToInclude(GetWorldSpaceBoundingBox(*MObjects, AllObjects[i], MWorldSpaceBoxes));
	}
	int32 Axis = 0;
	TVector<T, d> GlobalExtents = GlobalBox.Extents();
	if (GlobalExtents[2] > GlobalExtents[0] && GlobalExtents[2] > GlobalExtents[1])
	{
		Axis = 2;
	}
	else if (GlobalExtents[1] > GlobalExtents[0])
	{
		Axis = 1;
	}
	if (bAllowMultipleSplitting && GlobalExtents[Axis] < GlobalExtents[(Axis + 1) % 3] * 1.25 && GlobalExtents[Axis] < GlobalExtents[(Axis + 2) % 3] * 1.25 && AllObjects.Num() > 4 * MIN_NUM_OBJECTS)
	{
		Axis = -1;
	}
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
	UE_LOG(LogApeiron, Verbose, TEXT("Generated Tree with %d Nodes"), Elements.Num());
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

template<class OBJECT_ARRAY, class T, int d>
TArray<int32> TBoundingVolumeHierarchy<OBJECT_ARRAY, T, d>::FindAllIntersectionsHelper(const Node& MyNode, const TBox<T, d>& ObjectBox) const
{
	TBox<T, d> MBox(MyNode.MMin, MyNode.MMax);
	if (!MBox.Intersects(ObjectBox))
	{
		return TArray<int32>();
	}
	if (MyNode.MChildren.Num() == 0)
	{
		return MyNode.MObjects;
	}
	const TVector<T, d> ObjectCenter = MBox.Center();
	TArray<int32> IntersectionList;
	FCriticalSection SearchCriticalSection;
	TSet<int32> IntersectionSet;
	ParallelFor(MyNode.MChildren.Num(), [&](int32 Child) {
		const auto ChildList = FindAllIntersectionsHelper(Elements[MyNode.MChildren[Child]], ObjectBox);
		if (ChildList.Num())
		{
			SearchCriticalSection.Lock();
			for (auto ChildIndex : ChildList)
			{
				if (!IntersectionSet.Contains(ChildIndex))
				{
					IntersectionSet.Add(ChildIndex);
					IntersectionList.Add(ChildIndex);
				}
			}
			SearchCriticalSection.Unlock();
		}
	});
	return IntersectionList;
}

template<class OBJECT_ARRAY, class T, int d>
TArray<int32> TBoundingVolumeHierarchy<OBJECT_ARRAY, T, d>::FindAllIntersections(const TGeometryParticles<T, d>& InParticles, const int32 i) const
{
	return FindAllIntersections(GetWorldSpaceBoundingBox(InParticles, i, MWorldSpaceBoxes));
}

template<class OBJECT_ARRAY, class T, int d>
int32 TBoundingVolumeHierarchy<OBJECT_ARRAY, T, d>::GenerateNextLevel(const TVector<T, d>& GlobalMin, const TVector<T, d>& GlobalMax, const TArray<int32>& Objects, const int32 Axis, const int32 Level, const bool AllowMultipleSplitting)
{
	if (Axis == -1)
	{
		return GenerateNextLevel(GlobalMin, GlobalMax, Objects, Level);
	}
	TArray<Node> LocalElements;
	LocalElements.SetNum(2);
	TBox<T, d> GlobalBox(GlobalMin, GlobalMax);
	const TVector<T, d> WorldCenter = GlobalBox.Center();
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
		}
		if (MaxA)
		{
			LocalElements[1].MObjects.Add(Objects[i]);
		}
	}
	ParallelFor(2, [&](int32 i) {
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
			if (AllowMultipleSplitting && LocalExtents[NextAxis] < LocalExtents[(NextAxis + 1) % 3] * 1.25 && LocalExtents[NextAxis] < LocalExtents[(NextAxis + 2) % 3] * 1.25 && LocalElements[i].MObjects.Num() > 4 * MIN_NUM_OBJECTS)
			{
				NextAxis = -1;
			}
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
	ParallelFor(8, [&](int32 i) {
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

template class Apeiron::TBoundingVolumeHierarchy<TArray<Apeiron::TSphere<float, 3>*>, float, 3>;
template class Apeiron::TBoundingVolumeHierarchy<Apeiron::TPBDRigidParticles<float, 3>, float, 3>;
template class Apeiron::TBoundingVolumeHierarchy<Apeiron::TParticles<float, 3>, float, 3>;