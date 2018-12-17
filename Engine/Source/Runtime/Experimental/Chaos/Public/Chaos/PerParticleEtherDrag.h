// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Matrix.h"
#include "Chaos/PerParticleRule.h"

namespace Chaos
{
template<class T, int d>
class TPerParticleEtherDrag : public TPerParticleRule<T, d>
{
  public:
	TPerParticleEtherDrag(const T Coefficient = 0.01, const T AngularCoefficient = 0.01)
	    : MCoefficient(Coefficient), MAngularCoefficient(AngularCoefficient)
	{
	}
	virtual ~TPerParticleEtherDrag() {}

	template<class T_PARTICLES>
	inline void ApplyHelper(T_PARTICLES& InParticles, const T Dt, const int32 Index) const
	{
		InParticles.V(Index) -= MCoefficient * InParticles.V(Index);
	}

	inline void Apply(TDynamicParticles<T, d>& InParticles, const T Dt, const int32 Index) const override //-V762
	{
		if (InParticles.InvM(Index) == 0)
		{
			return; // Do not damp kinematic particles
		}
		ApplyHelper(InParticles, Dt, Index);
	}

	inline void Apply(TRigidParticles<T, d>& InParticles, const T Dt, const int32 Index) const override //-V762
	{
		if (InParticles.InvM(Index) == 0)
		{
			return; // Do not damp kinematic rigid bodies
		}
		ApplyHelper(InParticles, Dt, Index);
		InParticles.W(Index) -= MAngularCoefficient * InParticles.W(Index);
	}

  private:
	T MCoefficient;
	T MAngularCoefficient;
};
}
