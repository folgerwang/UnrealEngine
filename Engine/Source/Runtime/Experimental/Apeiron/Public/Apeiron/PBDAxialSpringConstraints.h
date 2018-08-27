// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/Array.h"
#include "Apeiron/Map.h"
#include "Apeiron/PBDAxialSpringConstraintsBase.h"
#include "Apeiron/PBDParticles.h"
#include "Apeiron/PerParticleRule.h"

#include <algorithm>

namespace Apeiron
{
template<class T, int d>
class TPBDAxialSpringConstraints : public TParticleRule<T, d>, public TPBDAxialSpringConstraintsBase<T, d>
{
	typedef TPBDAxialSpringConstraintsBase<T, d> Base;
	using Base::MBarys;
	using Base::MConstraints;

  public:
	TPBDAxialSpringConstraints(const TDynamicParticles<T, d>& InParticles, TArray<TVector<int32, 3>>&& Constraints, const T Stiffness = (T)1)
	    : TPBDAxialSpringConstraintsBase<T, d>(InParticles, MoveTemp(Constraints), Stiffness) {}
	virtual ~TPBDAxialSpringConstraints() {}

	void Apply(TPBDParticles<T, d>& InParticles, const T Dt) const override //-V762
	{
		for (int32 i = 0; i < MConstraints.Num(); ++i)
		{
			const auto& constraint = MConstraints[i];
			int32 i1 = constraint[0];
			int32 i2 = constraint[1];
			int32 i3 = constraint[2];
			auto Delta = Base::GetDelta(InParticles, i);
			T Multiplier = 2 / (FMath::Max(MBarys[i], 1 - MBarys[i]) + 1);
			if (InParticles.InvM(i1) > 0)
			{
				InParticles.P(i1) -= Multiplier * InParticles.InvM(i1) * Delta;
			}
			if (InParticles.InvM(i2))
			{
				InParticles.P(i2) += Multiplier * InParticles.InvM(i2) * MBarys[i] * Delta;
			}
			if (InParticles.InvM(i3))
			{
				InParticles.P(i3) += Multiplier * InParticles.InvM(i3) * (1 - MBarys[i]) * Delta;
			}
		}
	}
};
}
