// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/PBDParticles.h"
#include "Apeiron/PBDRigidParticles.h"
#include "Apeiron/PerParticleRule.h"

namespace Apeiron
{
template<class T, int d>
class TPerParticlePBDUpdateFromDeltaPosition : public TPerParticleRule<T, d>
{
  public:
	TPerParticlePBDUpdateFromDeltaPosition() {}
	virtual ~TPerParticlePBDUpdateFromDeltaPosition() {}

	template<class T_PARTICLES>
	inline void ApplyHelper(T_PARTICLES& InParticles, const T Dt, const int32 Index) const
	{
		InParticles.V(Index) = (InParticles.P(Index) - InParticles.X(Index)) / Dt;
		InParticles.X(Index) = InParticles.P(Index);
	}

	inline void Apply(TPBDParticles<T, d>& InParticles, const T Dt, const int32 Index) const override //-V762
	{
		ApplyHelper(InParticles, Dt, Index);
	}

	inline void Apply(TPBDRigidParticles<T, d>& InParticles, const T Dt, const int32 Index) const override //-V762
	{
		ApplyHelper(InParticles, Dt, Index);
		TRotation<T, d> Delta = InParticles.Q(Index) * InParticles.R(Index).Inverse();
		TVector<T, d> Axis;
		T Angle;
		Delta.ToAxisAndAngle(Axis, Angle);
		InParticles.W(Index) = Axis * Angle / Dt;
		InParticles.R(Index) = InParticles.Q(Index);
	}
};
}
