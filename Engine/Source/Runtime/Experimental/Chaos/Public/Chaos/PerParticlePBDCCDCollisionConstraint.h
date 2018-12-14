// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/OrientedGeometryParticles.h"
#include "Chaos/PerParticleRule.h"
#include "Chaos/Transform.h"

#include <memory>

namespace Chaos
{
template<class T, int d>
class PerParticlePBDCCDCollisionConstraint : public TPerParticleRule<T, d>
{
  public:
	PerParticlePBDCCDCollisionConstraint(const OrientedGeometryParticles<T, d>& InParticles, TArrayCollectionArray<bool>& Collided, const T Thickness = (T)0)
	    : MParticles(InParticles), MCollided(Collided), MThickness(Thickness)
	{
		for (int32 i = 0; i < MParticles.Size(); ++i)
		{
			MFrames.Add(TRigidTransform<T, d>(MParticles.X(i), MParticles.R(i)));
		}
	}
	virtual ~PerParticlePBDCCDCollisionConstraint() {}

	inline void Apply(TPBDParticles<T, d>& InParticles, const T Dt, const int32 Index) const override //-V762
	{
		if (InParticles.InvM(Index) == 0)
			return;
		for (int32 i = 0; i < MParticles.Size(); ++i)
		{
			TRigidTransform<T, d> Frame(MParticles.X(i), MParticles.R(i));
			auto PointPair = MParticles.Geometry(i)->FindClosestIntersection(
			    MFrames[i].InverseTransformPosition(InParticles.X(Index)), Frame.InverseTransformPosition(InParticles.P(Index)), MThickness);
			if (PointPair.second)
			{
				MCollided[i] = true;
				Vector<T, d> Normal = Frame.TransformVector(MParticles.Geometry(i)->Normal(PointPair.first));
				InParticles.P(Index) += 2 * Vector<T, d>::DotProduct(Normal, Frame.TransformPosition(PointPair.first) - InParticles.P(Index)) * Normal;
			}
		}
	}

  private:
	// TODO(mlentine): Need a bb hierarchy
	const OrientedGeometryParticles<T, d>& MParticles;
	TArray<TRigidTransform<T, d>> MFrames;
	TArrayCollectionArray<bool>& MCollided;
	const T MThickness;
};
}
