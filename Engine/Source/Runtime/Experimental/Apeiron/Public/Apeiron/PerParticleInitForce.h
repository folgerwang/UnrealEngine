// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/PerParticleRule.h"

namespace Apeiron
{
template<class T, int d>
class TPerParticleInitForce : public TPerParticleRule<T, d>
{
  public:
	TPerParticleInitForce() {}
	virtual ~TPerParticleInitForce() {}

	inline void Apply(TDynamicParticles<T, d>& InParticles, const T Dt, const int Index) const override //-V762
	{
		InParticles.F(Index) *= 0;
	}

	inline void Apply(TRigidParticles<T, d>& InParticles, const T Dt, const int Index) const override //-V762
	{
		InParticles.F(Index) *= 0;
		InParticles.Torque(Index) *= 0;
	}
};
}
