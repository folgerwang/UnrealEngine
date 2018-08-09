// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/PerParticleRule.h"

namespace Apeiron
{
template<class T, int d>
class TPerParticlePBDEulerStep : public TPerParticleRule<T, d>
{
  public:
	TPerParticlePBDEulerStep() {}
	virtual ~TPerParticlePBDEulerStep() {}

	template<class T_PARTICLES>
	inline void ApplyHelper(T_PARTICLES& InParticles, const T Dt, const int32 Index) const
	{
		InParticles.P(Index) = InParticles.X(Index) + InParticles.V(Index) * Dt;
	}

	inline void Apply(TPBDParticles<T, d>& InParticles, const T Dt, const int32 Index) const override //-V762
	{
		ApplyHelper(InParticles, Dt, Index);
	}

	inline void Apply(TPBDRigidParticles<T, d>& InParticles, const T Dt, const int32 Index) const override //-V762
	{
		ApplyHelper(InParticles, Dt, Index);
		InParticles.Q(Index) = InParticles.R(Index) + TRotation<T, d>(InParticles.W(Index), 0.f) * InParticles.R(Index) * Dt * T(0.5);
		InParticles.Q(Index).Normalize();
	}
};
}
