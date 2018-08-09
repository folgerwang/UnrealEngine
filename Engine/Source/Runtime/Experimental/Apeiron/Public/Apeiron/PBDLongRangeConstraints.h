// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/PBDLongRangeConstraintsBase.h"
#include "Apeiron/PBDParticles.h"
#include "Apeiron/PerParticleRule.h"

namespace Apeiron
{
template<class T, int d>
class APEIRON_API PBDLongRangeConstraints : public TParticleRule<T, d>, public TPBDLongRangeConstraintsBase<T, d>
{
	typedef TPBDLongRangeConstraintsBase<T, d> Base;
	using Base::MConstraints;

  public:
	PBDLongRangeConstraints(const TDynamicParticles<T, d>& InParticles, const TTriangleMesh<T>& Mesh, const int32 NumberOfAttachments = 1, const T Stiffness = (T)1)
	    : TPBDLongRangeConstraintsBase<T, d>(InParticles, Mesh, NumberOfAttachments, Stiffness) {}
	virtual ~PBDLongRangeConstraints() {}

	void Apply(TPBDParticles<T, d>& InParticles, const T Dt) const override //-V762
	{
		for (int32 i = 0; i < MConstraints.Num(); ++i)
		{
			const auto& Constraint = MConstraints[i];
			int32 i2 = Constraint[Constraint.Num() - 1];
			check(InParticles.InvM(i2) > 0);
			InParticles.P(i2) += Base::GetDelta(InParticles, i);
		}
	}
};
}
