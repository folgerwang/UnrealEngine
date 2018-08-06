// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/DynamicParticles.h"
#include "Apeiron/PerParticleRule.h"

namespace Apeiron
{
template<class T, int d>
class PerParticleGravity : public TPerParticleRule<T, d>
{
  public:
	PerParticleGravity(const TVector<T, d>& Direction = TVector<T, d>(0, -1, 0), const T Magnitude = (T)9.8)
	    : MAcceleration(Direction * Magnitude) {}
	virtual ~PerParticleGravity() {}

	template<class T_PARTICLES>
	inline void ApplyHelper(T_PARTICLES& InParticles, const T Dt, const int Index) const
	{
		InParticles.F(Index) += MAcceleration * InParticles.M(Index);
	}

	inline void Apply(TDynamicParticles<T, d>& InParticles, const T Dt, const int Index) const override //-V762
	{
		ApplyHelper(InParticles, Dt, Index);
	}

	inline void Apply(TRigidParticles<T, d>& InParticles, const T Dt, const int Index) const override //-V762
	{
		ApplyHelper(InParticles, Dt, Index);
	}

	void SetAcceleration(const TVector<T, d>& Acceleration)
	{
		MAcceleration = Acceleration;
	}

  private:
	TVector<T, d> MAcceleration;
};
}
