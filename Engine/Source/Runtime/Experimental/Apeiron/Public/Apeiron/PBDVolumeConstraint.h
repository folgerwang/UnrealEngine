// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/Array.h"
#include "Apeiron/Map.h"
#include "Apeiron/PBDParticles.h"
#include "Apeiron/PBDVolumeConstraintBase.h"
#include "Apeiron/ParticleRule.h"

namespace Apeiron
{
template<class T>
class TPBDVolumeConstraint : public TParticleRule<T, 3>, public TPBDVolumeConstraintBase<T>
{
	typedef TPBDVolumeConstraintBase<T> Base;

  public:
	TPBDVolumeConstraint(const TDynamicParticles<T, 3>& InParticles, TArray<TVector<int32, 3>>&& constraints, const T stiffness = (T)1)
	    : Base(InParticles, MoveTemp(constraints), stiffness) {}
	virtual ~TPBDVolumeConstraint() {}

	void Apply(TPBDParticles<T, 3>& InParticles, const T dt) const override //-V762
	{
		auto W = Base::GetWeights(InParticles, (T)1);
		auto Grads = Base::GetGradients(InParticles);
		auto S = Base::GetScalingFactor(InParticles, Grads, W);
		for (uint32 i = 0; i < InParticles.Size(); ++i)
		{
			InParticles.P(i) -= S * W[i] * Grads[i];
		}
	}
};
}
