// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/Array.h"
#include "Apeiron/Map.h"
#include "Apeiron/PBDParticles.h"
#include "Apeiron/PBDSpringConstraintsBase.h"
#include "Apeiron/PerParticleRule.h"

namespace Apeiron
{
template<class T, int d>
class PerParticlePBDSpringConstraints : public TPerParticleRule<T, d>, public TPBDSpringConstraintsBase<T, d>
{
	typedef TPBDSpringConstraintsBase<T, d> Base;
	using Base::Constraints_;

  public:
	PerParticlePBDSpringConstraints(const TDynamicParticles<T, d>& InParticles, TArray<TVector<int32, 2>>&& Constraints, const T Stiffness = (T)1)
	    : Base(InParticles, MoveTemp(Constraints), Stiffness)
	{
		for (int32 i = 0; i < Constraints_.Num(); ++i)
		{
			const auto& Constraint = Constraints_[i];
			int32 i1 = Constraint[0];
			int32 i2 = Constraint[1];
			if (i1 >= MParticleToConstraints.Num())
			{
				MParticleToConstraints.SetNum(i1 + 1);
			}
			if (i2 >= MParticleToConstraints.Num())
			{
				MParticleToConstraints.SetNum(i2 + 1);
			}
			MParticleToConstraints[i1].Add(i);
			MParticleToConstraints[i2].Add(i);
		}
	}
	virtual ~PerParticlePBDSpringConstraints() {}

	// TODO(mlentine): We likely need to use time n positions here
	void Apply(TPBDParticles<T, d>& InParticles, const T Dt, const int32 Index) const override //-V762
	{
		for (int32 i = 0; i < MParticleToConstraints[Index].Num(); ++i)
		{
			int32 CIndex = MParticleToConstraints[Index][i];
			const auto& Constraint = Constraints_[CIndex];
			int32 i1 = Constraint[0];
			int32 i2 = Constraint[1];
			if (Index == i1 && InParticles.InvM(i1) > 0)
			{
				InParticles.P(i1) -= InParticles.InvM(i1) * Base::GetDelta(InParticles, CIndex);
			}
			else if (InParticles.InvM(i2) > 0)
			{
				check(Index == i2);
				InParticles.P(i2) += InParticles.InvM(i2) * Base::GetDelta(InParticles, CIndex);
			}
		}
	}

  private:
	TArray<TArray<int32>> MParticleToConstraints;
};
}
