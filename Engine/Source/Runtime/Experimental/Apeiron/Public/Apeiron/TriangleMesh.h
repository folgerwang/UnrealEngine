// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/Array.h"
#include "Apeiron/Map.h"
#include "Apeiron/Particles.h"

#include <unordered_set>

namespace Apeiron
{
template<class T>
class APEIRON_API TTriangleMesh
{
  public:
	TTriangleMesh(TArray<TVector<int32, 3>>&& Elements);
	TTriangleMesh(const TTriangleMesh& Other) = delete;
	TTriangleMesh(TTriangleMesh&& Other)
	    : MElements(MoveTemp(Other.MElements)), MSurfaceIndices(MoveTemp(Other.MSurfaceIndices)), MPointToTriangleMap(MoveTemp(Other.MPointToTriangleMap))
	{
	}
	~TTriangleMesh() {}

	const TArray<TVector<int32, 3>>& GetSurfaceElements() const
	{
		return MElements;
	}

	const TSet<uint32>& GetNeighbors(const int32 Element) const
	{
		return MPointToNeighborsMap[Element];
	}

	TSet<int32> GetNRing(const int32 Element, const int32 N) const
	{
		TSet<int32> Neighbors;
		TSet<uint32> LevelNeighbors, PrevLevelNeighbors;
		PrevLevelNeighbors = GetNeighbors(Element);
		for (auto SubElement : PrevLevelNeighbors)
		{
			check(SubElement != Element);
			Neighbors.Add(SubElement);
		}
		for (int32 i = 1; i < N; ++i)
		{
			for (auto SubElement : PrevLevelNeighbors)
			{
				const auto& SubNeighbors = GetNeighbors(SubElement);
				for (auto SubSubElement : SubNeighbors)
				{
					if (!Neighbors.Contains(SubSubElement) && SubSubElement != Element)
					{
						LevelNeighbors.Add(SubSubElement);
					}
				}
			}
			PrevLevelNeighbors = LevelNeighbors;
			LevelNeighbors.Reset();
			for (auto SubElement : PrevLevelNeighbors)
			{
				if (!Neighbors.Contains(SubElement))
				{
					check(SubElement != Element);
					Neighbors.Add(SubElement);
				}
			}
		}
		return Neighbors;
	}

	TArray<Apeiron::TVector<int32, 2>> GetUniqueAdjacentPoints() const;
	TArray<Apeiron::TVector<int32, 4>> GetUniqueAdjacentElements() const;
	TArray<TVector<T, 3>> GetFaceNormals(const TParticles<T, 3>& InParticles) const;
	TArray<TVector<T, 3>> GetPointNormals(const TParticles<T, 3>& InParticles) const;

  private:
	TArray<TVector<int32, 3>> MElements;
	TSet<int32> MSurfaceIndices; // Flattened and Dedupped from MElements

	TMap<int32, TArray<int32>> MPointToTriangleMap;
	TMap<int32, TSet<uint32>> MPointToNeighborsMap;
};
}
