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

	template<class T_PARTICLES>
	void ApplyHelper(T_PARTICLES& InParticles, const T Dt) const
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

	void Apply(TPBDParticles<T, d>& InParticles, const T Dt) const override //-V762
	{
		ApplyHelper(InParticles, Dt);
	}

	void Apply(TPBDRigidParticles<T, d>& InParticles, const T Dt) const override //-V762
	{
		ApplyHelper(InParticles, Dt);
	}
};
}
