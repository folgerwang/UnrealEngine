// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "PerParticleRule.h"

namespace Chaos
{
template<class T, int d>
class PerParticlePBDGroundConstraint : public TPerParticleRule<T, d>
{
  public:
	PerParticlePBDGroundConstraint(const T Height = 0)
	    : MHeight(Height) {}
	virtual ~PerParticlePBDGroundConstraint() {}

	inline void Apply(TPBDParticles<T, d>& InParticles, const T Dt, const int32 Index) const override //-V762
	{
		if (InParticles.P(Index)[1] >= MHeight || InParticles.InvM(Index) == 0)
			return;
		InParticles.P(Index)[1] = MHeight;
	}

  private:
	T MHeight;
};
}
