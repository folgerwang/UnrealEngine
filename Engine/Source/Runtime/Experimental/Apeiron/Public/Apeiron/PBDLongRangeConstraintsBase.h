// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/Array.h"
#include "Apeiron/PBDParticles.h"
#include "Apeiron/PerParticleRule.h"
#include "Apeiron/TriangleMesh.h"

namespace Apeiron
{
template<class T, int d>
class APEIRON_API TPBDLongRangeConstraintsBase
{
  public:
	TPBDLongRangeConstraintsBase(const TDynamicParticles<T, d>& InParticles, const TTriangleMesh<T>& Mesh, const int32 NumberOfAttachments = 1, const T Stiffness = (T)1);
	virtual ~TPBDLongRangeConstraintsBase() {}

	TVector<T, d> GetDelta(const TPBDParticles<T, d>& InParticles, const int32 i) const;

  private:
	static TArray<TArray<uint32>> ComputeIslands(const TDynamicParticles<T, d>& InParticles, const TTriangleMesh<T>& Mesh, const TArray<uint32>& KinematicParticles);
	void ComputeEuclidianConstraints(const TDynamicParticles<T, d>& InParticles, const TTriangleMesh<T>& Mesh, const int32 NumberOfAttachments);
	void ComputeGeodesicConstraints(const TDynamicParticles<T, d>& InParticles, const TTriangleMesh<T>& Mesh, const int32 NumberOfAttachments);

	static T ComputeDistance(const TParticles<T, d>& InParticles, const uint32 i, const uint32 j) { return (InParticles.X(i) - InParticles.X(j)).Size(); }
	static T ComputeDistance(const TPBDParticles<T, d>& InParticles, const uint32 i, const uint32 j) { return (InParticles.P(i) - InParticles.P(j)).Size(); }
	static T ComputeGeodesicDistance(const TParticles<T, d>& InParticles, const TArray<uint32>& Path)
	{
		T distance = 0;
		for (int i = 0; i < Path.Num() - 1; ++i)
		{
			distance += (InParticles.X(Path[i]) - InParticles.X(Path[i + 1])).Size();
		}
		return distance;
	}
	static T ComputeGeodesicDistance(const TPBDParticles<T, d>& InParticles, const TArray<uint32>& Path)
	{
		T distance = 0;
		for (int i = 0; i < Path.Num() - 1; ++i)
		{
			distance += (InParticles.P(Path[i]) - InParticles.P(Path[i + 1])).Size();
		}
		return distance;
	}

  protected:
	TArray<TArray<uint32>> MConstraints;

  private:
	TArray<T> MDists;
	T MStiffness;
};
}
