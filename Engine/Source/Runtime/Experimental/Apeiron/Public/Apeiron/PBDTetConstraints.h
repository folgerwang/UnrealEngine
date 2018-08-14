// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/Array.h"
#include "Apeiron/Map.h"
#include "Apeiron/PBDParticles.h"
#include "Apeiron/PBDTetConstraintsBase.h"
#include "Apeiron/ParticleRule.h"

namespace Apeiron
{
template<class T>
class PBDTetConstraints : public TParticleRule<T, 3>, public PBDTetConstraintsBase<T>
{
	typedef PBDTetConstraintsBase<T> Base;
	using Base::MConstraints;

  public:
	PBDTetConstraints(const TDynamicParticles<T, 3>& InParticles, TArray<TVector<int32, 4>>&& Constraints, const T Stiffness = (T)1)
	    : Base(InParticles, MoveTemp(Constraints), Stiffness) {}
	virtual ~PBDTetConstraints() {}

	void Apply(TPBDParticles<T, 3>& InParticles, const T dt) const override //-V762
	{
		for (int i = 0; i < MConstraints.Num(); ++i)
		{
			const auto& Constraint = MConstraints[i];
			const int32 i1 = Constraint[0];
			const int32 i2 = Constraint[1];
			const int32 i3 = Constraint[2];
			const int32 i4 = Constraint[3];
			auto Grads = Base::GetGradients(InParticles, i);
			auto S = Base::GetScalingFactor(InParticles, i, Grads);
			InParticles.P(i1) -= S * InParticles.InvM(i1) * Grads[0];
			InParticles.P(i2) -= S * InParticles.InvM(i2) * Grads[1];
			InParticles.P(i3) -= S * InParticles.InvM(i3) * Grads[2];
			InParticles.P(i4) -= S * InParticles.InvM(i4) * Grads[3];
		}
	}
};
}
