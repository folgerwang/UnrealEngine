// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Array.h"
#include "Chaos/Map.h"
#include "Chaos/PBDParticles.h"
#include "Chaos/PBDShapeConstraintsBase.h"
#include "Chaos/PerParticleRule.h"

namespace Chaos
{
template<class T, int d>
class TPerParticlePBDShapeConstraints : public TPerParticleRule<T, d>, public TPBDShapeConstraintsBase<T, d>
{
	typedef TPBDShapeConstraintsBase<T, d> Base;

  public:
	TPerParticlePBDShapeConstraints(const T Stiffness = (T)1)
	    : Base(Stiffness)
	{
	}
	TPerParticlePBDShapeConstraints(const TDynamicParticles<T, d>& InParticles, const TArray<TVector<float, 3>>& TargetPositions, const T Stiffness = (T)1)
	    : Base(InParticles, TargetPositions, Stiffness)
	{
	}
	virtual ~TPerParticlePBDShapeConstraints() {}

	// TODO(mlentine): We likely need to use time n positions here
	void Apply(TPBDParticles<T, d>& InParticles, const T Dt, const int32 Index) const override //-V762
	{
		if (InParticles.InvM(Index) > 0)
		{
			InParticles.P(Index) -= InParticles.InvM(Index) * Base::GetDelta(InParticles, Index);
		}
	}

	void Apply(TPBDParticles<T, d>& InParticles, const T Dt) const override //-V762
	{
		PhysicsParallelFor(InParticles.Size(), [&](int32 Index) {
			Apply(InParticles, Dt, Index);
		});
	}
};
}
