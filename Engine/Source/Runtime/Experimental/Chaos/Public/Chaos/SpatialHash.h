// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Box.h"
#include "Chaos/Vector.h"

namespace Chaos
{
template<class T>
class CHAOS_API TSpatialHash
{
 public:
	TSpatialHash(const TArray<TVector<T, 3>>& Particles, const T Radius)
	: MParticles(Particles)
	{
		Init(Radius);
	}

	TSpatialHash(const TArray<TVector<T, 3>>& Particles)
	: MParticles(Particles)
	{
		Init();
	}

	~TSpatialHash() {}

	void Update(const TArray<TVector<T, 3>>& Particles, const T Radius);
	void Update(const TArray<TVector<T, 3>>& Particles);
	void Update(const T Radius);

	// Returns all the points in MaxRadius, result not sorted
	TArray<int32> GetClosestPoints(const TVector<T, 3>& Particle, const T MaxRadius);
	// Returns all the points in MaxRadius, no more than MaxCount, result always sorted
	TArray<int32> GetClosestPoints(const TVector<T, 3>& Particle, const T MaxRadius, const int32 MaxPoints);
	int32 GetClosestPoint(const TVector<T, 3>& Particle);
	
private:
	void Init(const T Radius);
	void Init();
	
	int32 ComputeMaxN(const TVector<T, 3>& Particle, const T Radius);
	TSet<int32> GetNRing(const TVector<T, 3>& Particle, const int32 N);
	void ComputeGridXYZ(const TVector<T, 3>& Particle, int32& XIndex, int32& YIndex, int32& ZIndex);

	int32 HashFunction(const TVector<T, 3>& Particle);
	int32 HashFunction(int32& XIndex, int32& YIndex, int32& ZIndex);

private:
	TArray<TVector<T, 3>> MParticles;
	T MCellSize;
	TBox<T, 3> MBoundingBox;
	int32 MNumberOfCellsX, MNumberOfCellsY, MNumberOfCellsZ;
	TMap<int32, TArray<int32>> MHashTable;
};
}
