// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Array.h"
#include "Chaos/Map.h"
#include "Chaos/PBDParticles.h"
#include "Chaos/PBDVolumeConstraintBase.h"
#include "Chaos/ParticleRule.h"

namespace Chaos
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
