// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#include "Apeiron/TriangleMesh.h"

#include "Apeiron/Defines.h"

using namespace Apeiron;

template<class T>
TTriangleMesh<T>::TTriangleMesh(TArray<TVector<int32, 3>>&& Elements)
    : MElements(MoveTemp(Elements))
{
	for (int i = 0; i < MElements.Num(); ++i)
	{
		MPointToTriangleMap.FindOrAdd(MElements[i][0]).Add(i);
		MPointToTriangleMap.FindOrAdd(MElements[i][1]).Add(i);
		MPointToTriangleMap.FindOrAdd(MElements[i][2]).Add(i);
		MSurfaceIndices.Add(MElements[i][1]);
		MSurfaceIndices.Add(MElements[i][1]);
		MSurfaceIndices.Add(MElements[i][2]);
		check(MElements[i][0] != MElements[i][1]);
		check(MElements[i][1] != MElements[i][2]);
		MPointToNeighborsMap.FindOrAdd(MElements[i][0]).Add(MElements[i][1]);
		MPointToNeighborsMap.FindOrAdd(MElements[i][0]).Add(MElements[i][2]);
		MPointToNeighborsMap.FindOrAdd(MElements[i][1]).Add(MElements[i][0]);
		MPointToNeighborsMap.FindOrAdd(MElements[i][1]).Add(MElements[i][2]);
		MPointToNeighborsMap.FindOrAdd(MElements[i][2]).Add(MElements[i][0]);
		MPointToNeighborsMap.FindOrAdd(MElements[i][2]).Add(MElements[i][1]);
	}
}

template<class T>
TArray<TVector<int32, 2>> TTriangleMesh<T>::GetUniqueAdjacentPoints() const
{
	TArray<TVector<int32, 2>> BendingConstraints;
	auto BendingElements = GetUniqueAdjacentElements();
	for (const auto& Element : BendingElements)
	{
		BendingConstraints.Add(TVector<int32, 2>(Element[2], Element[3]));
	}
	return BendingConstraints;
}

template<class T>
TArray<TVector<int32, 4>> TTriangleMesh<T>::GetUniqueAdjacentElements() const
{
	TArray<TVector<int32, 4>> BendingConstraints;
	TSet<TArray<int32>> BendingElements;
	for (auto SurfaceIndex : MSurfaceIndices)
	{
		TMap<int32, TArray<int32>> SubPointToTriangleMap;
		for (auto TriangleIndex : MPointToTriangleMap[SurfaceIndex])
		{
			SubPointToTriangleMap.FindOrAdd(MElements[TriangleIndex][0]).Add(TriangleIndex);
			SubPointToTriangleMap.FindOrAdd(MElements[TriangleIndex][1]).Add(TriangleIndex);
			SubPointToTriangleMap.FindOrAdd(MElements[TriangleIndex][2]).Add(TriangleIndex);
		}
		for (auto OtherIndex : SubPointToTriangleMap)
		{
			if (SurfaceIndex == OtherIndex.Key)
				continue;
			if (OtherIndex.Value.Num() == 1)
				continue; // We are at an edge
			check(OtherIndex.Value.Num() == 2);
			int32 Tri1 = OtherIndex.Value[0];
			int32 Tri2 = OtherIndex.Value[1];
			int32 Tri1Pt = -1, Tri2Pt = -1;
			if (MElements[Tri1][0] != SurfaceIndex && MElements[Tri1][0] != OtherIndex.Key)
			{
				Tri1Pt = MElements[Tri1][0];
			}
			else if (MElements[Tri1][1] != SurfaceIndex && MElements[Tri1][1] != OtherIndex.Key)
			{
				Tri1Pt = MElements[Tri1][1];
			}
			else if (MElements[Tri1][2] != SurfaceIndex && MElements[Tri1][2] != OtherIndex.Key)
			{
				Tri1Pt = MElements[Tri1][2];
			}
			check(Tri1Pt != -1);
			if (MElements[Tri2][0] != SurfaceIndex && MElements[Tri2][0] != OtherIndex.Key)
			{
				Tri2Pt = MElements[Tri2][0];
			}
			else if (MElements[Tri2][1] != SurfaceIndex && MElements[Tri2][1] != OtherIndex.Key)
			{
				Tri2Pt = MElements[Tri2][1];
			}
			else if (MElements[Tri2][2] != SurfaceIndex && MElements[Tri2][2] != OtherIndex.Key)
			{
				Tri2Pt = MElements[Tri2][2];
			}
			check(Tri2Pt != -1);
			auto BendingArray = TArray<int32>({SurfaceIndex, OtherIndex.Key, Tri1Pt, Tri2Pt});
			BendingArray.Sort();
			if (BendingElements.Contains(BendingArray))
			{
				continue;
			}
			BendingElements.Add(BendingArray);
			BendingConstraints.Add(TVector<int32, 4>({SurfaceIndex, OtherIndex.Key, Tri1Pt, Tri2Pt}));
		}
	}
	return BendingConstraints;
}

template<class T>
TArray<TVector<T, 3>> TTriangleMesh<T>::GetFaceNormals(const TParticles<T, 3>& InParticles) const
{
	TArray<TVector<T, 3>> Normals;
	for (int32 i = 0; i < MElements.Num(); ++i)
	{
		TVector<T, 3> p10 = InParticles.X(MElements[i][1]) - InParticles.X(MElements[i][0]);
		TVector<T, 3> p20 = InParticles.X(MElements[i][2]) - InParticles.X(MElements[i][0]);
		Normals.Add(TVector<T, 3>::CrossProduct(p10, p20).GetSafeNormal());
	}
	return Normals;
}

template<class T>
TArray<TVector<T, 3>> TTriangleMesh<T>::GetPointNormals(const TParticles<T, 3>& InParticles) const
{
	TArray<TVector<T, 3>> FaceNormals = GetFaceNormals(InParticles);
	TArray<TVector<T, 3>> PointNormals;
	PointNormals.SetNum(InParticles.Size());
	for (auto Element : MPointToTriangleMap)
	{
		if (PointNormals.Num() <= Element.Key)
		{
			PointNormals.SetNum(Element.Key);
		}
		TVector<T, 3> Normal(0);
		for (int32 k = 0; k < Element.Value.Num(); ++k)
		{
			Normal += FaceNormals[Element.Value[k]];
		}
		PointNormals[Element.Key] = Normal.GetSafeNormal();
	}
	return PointNormals;
}

template class Apeiron::TTriangleMesh<float>;
