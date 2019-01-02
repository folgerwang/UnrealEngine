// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Array.h"
#include "Chaos/Map.h"
#include "Chaos/PBDParticles.h"
#include "Chaos/PBDSpringConstraintsBase.h"
#include "Chaos/PerParticleRule.h"

namespace Chaos
{
template<class T, int d>
class TPBDSpringConstraints : public TParticleRule<T, d>, public TPBDSpringConstraintsBase<T, d>
{
	typedef TPBDSpringConstraintsBase<T, d> Base;
	using Base::MConstraints;

  public:
	TPBDSpringConstraints(const T Stiffness = (T)1)
	    : TPBDSpringConstraintsBase<T, d>(Stiffness) {}
	TPBDSpringConstraints(const TDynamicParticles<T, d>& InParticles, TArray<TVector<int32, 2>>&& Constraints, const T Stiffness = (T)1)
	    : TPBDSpringConstraintsBase<T, d>(InParticles, MoveTemp(Constraints), Stiffness) {}
	TPBDSpringConstraints(const TRigidParticles<T, d>& InParticles, TArray<TVector<int32, 2>>&& Constraints, const T Stiffness = (T)1)
	    : TPBDSpringConstraintsBase<T, d>(InParticles, MoveTemp(Constraints), Stiffness) {}
	TPBDSpringConstraints(const TDynamicParticles<T, d>& InParticles, const TArray<TVector<int32, 3>>& Constraints, const T Stiffness = (T)1)
	    : TPBDSpringConstraintsBase<T, d>(InParticles, Constraints, Stiffness) {}
	TPBDSpringConstraints(const TDynamicParticles<T, d>& InParticles, const TArray<TVector<int32, 4>>& Constraints, const T Stiffness = (T)1)
	    : TPBDSpringConstraintsBase<T, d>(InParticles, Constraints, Stiffness) {}
	virtual ~TPBDSpringConstraints() {}

	TArray<TVector<int32, 2>>& Constraints()
	{
		return MConstraints;
	}

	void Apply(TPBDParticles<T, d>& InParticles, const T Dt) const override //-V762
	{
		for (int32 i = 0; i < MConstraints.Num(); ++i)
		{
			const auto& Constraint = MConstraints[i];
			int32 i1 = Constraint[0];
			int32 i2 = Constraint[1];
			auto Delta = Base::GetDelta(InParticles, i);
			if (InParticles.InvM(i1) > 0)
			{
				InParticles.P(i1) -= InParticles.InvM(i1) * Delta;
			}
			if (InParticles.InvM(i2) > 0)
			{
				InParticles.P(i2) += InParticles.InvM(i2) * Delta;
			}
		}
	}

	void Apply(TPBDRigidParticles<T, d>& InParticles, const T Dt, const int32 Island) const override //-V762
	{
		for (int32 i = 0; i < MConstraints.Num(); ++i)
		{
			const auto& Constraint = MConstraints[i];
			int32 i1 = Constraint[0];
			int32 i2 = Constraint[1];
			check(InParticles.Island(i1) == InParticles.Island(i2) || InParticles.Island(i1) == -1 || InParticles.Island(i2) == -1);
			if (InParticles.Island(i1) != Island && InParticles.Island(i2) != Island)
			{
				continue;
			}
			auto Delta = Base::GetDelta(InParticles, i);
			if (InParticles.InvM(i1) > 0)
			{
				InParticles.P(i1) -= InParticles.InvM(i1) * Delta;
			}
			if (InParticles.InvM(i2) > 0)
			{
				InParticles.P(i2) += InParticles.InvM(i2) * Delta;
			}
		}
	}
};
}
