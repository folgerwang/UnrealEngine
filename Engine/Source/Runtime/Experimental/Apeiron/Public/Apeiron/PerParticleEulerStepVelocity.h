// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/PerParticleRule.h"

namespace Apeiron
{
template<class T, int d>
class TPerParticleEulerStepVelocity : public TPerParticleRule<T, d>
{
  public:
	TPerParticleEulerStepVelocity() {}
	virtual ~TPerParticleEulerStepVelocity() {}

	template<class T_PARTICLES>
	inline void ApplyHelper(T_PARTICLES& InParticles, const T Dt, const int32 Index) const
	{
		InParticles.V(Index) += InParticles.F(Index) * InParticles.InvM(Index) * Dt;
	}

	inline void Apply(TDynamicParticles<T, d>& InParticles, const T Dt, const int32 Index) const override //-V762
	{
		if (InParticles.InvM(Index) == 0)
			return;
		ApplyHelper(InParticles, Dt, Index);
	}

	inline void Apply(TRigidParticles<T, d>& InParticles, const T Dt, const int32 Index) const override //-V762
	{
		if (InParticles.InvM(Index) == 0 || InParticles.Disabled(Index) || InParticles.Sleeping(Index))
			return;
		ApplyHelper(InParticles, Dt, Index);
		TVector<T, d> L = InParticles.I(Index) * InParticles.W(Index);
		InParticles.W(Index) += InParticles.InvI(Index) * (InParticles.Torque(Index) - TVector<T, d>::CrossProduct(InParticles.W(Index), L)) * Dt;
	}
};
}
