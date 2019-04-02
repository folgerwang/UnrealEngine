// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Array.h"
#include "Chaos/Map.h"
#include "Chaos/PBDRigidSpringConstraintsBase.h"
#include "Chaos/PerParticleRule.h"

namespace Chaos
{
template<class T, int d>
class TPBDRigidSpringConstraints : public TParticleRule<T, d>, public TPBDRigidSpringConstraintsBase<T, d>
{
	typedef TPBDRigidSpringConstraintsBase<T, d> Base;

	using Base::Constraints;
	using Base::Distances;

  public:
	TPBDRigidSpringConstraints(const T InStiffness = (T)1)
	    : TPBDRigidSpringConstraintsBase<T, d>(InStiffness) 
	{}

	TPBDRigidSpringConstraints(const TRigidParticles<T, d>& InParticles, const TArray<TVector<T, 3>>& Locations0, const TArray<TVector<T, 3>>& Locations1, TArray<TVector<int32, 2>>&& InConstraints, const T InStiffness = (T)1)
	    : TPBDRigidSpringConstraintsBase<T, d>(InParticles, Locations0, Locations1, MoveTemp(InConstraints), InStiffness)
	{}

	virtual ~TPBDRigidSpringConstraints() {}

	void ApplyHelper(TPBDRigidParticles<T, d>& InParticles, const T Dt, const int32 Island) const
	{
		const int32 NumConstraints = Constraints.Num();
		for (int32 ConstraintIndex = 0; ConstraintIndex < NumConstraints; ++ConstraintIndex)
		{
			const TVector<int32, 2>& Constraint = Constraints[ConstraintIndex];

			int32 ConstraintInnerIndex1 = Constraint[0];
			int32 ConstraintInnerIndex2 = Constraint[1];

			check(InParticles.Island(ConstraintInnerIndex1) == InParticles.Island(ConstraintInnerIndex2) || InParticles.Island(ConstraintInnerIndex1) == -1 || InParticles.Island(ConstraintInnerIndex2) == -1);

			// @todo(mlentine): We should cache constraints per island somewhere
			if (InParticles.Island(ConstraintInnerIndex1) != Island && InParticles.Island(ConstraintInnerIndex2) != Island)
			{
				continue;
			}

			const TVector<T, d> WorldSpaceX1 = InParticles.Q(ConstraintInnerIndex1).RotateVector(Distances[ConstraintIndex][0]) + InParticles.P(ConstraintInnerIndex1);
			const TVector<T, d> WorldSpaceX2 = InParticles.Q(ConstraintInnerIndex2).RotateVector(Distances[ConstraintIndex][1]) + InParticles.P(ConstraintInnerIndex2);
			const PMatrix<T, d, d> WorldSpaceInvI1 = (InParticles.Q(ConstraintInnerIndex1) * FMatrix::Identity) * InParticles.InvI(ConstraintInnerIndex1) * (InParticles.Q(ConstraintInnerIndex1) * FMatrix::Identity).GetTransposed();
			const PMatrix<T, d, d> WorldSpaceInvI2 = (InParticles.Q(ConstraintInnerIndex2) * FMatrix::Identity) * InParticles.InvI(ConstraintInnerIndex2) * (InParticles.Q(ConstraintInnerIndex2) * FMatrix::Identity).GetTransposed();
			const TVector<T, d> Delta = Base::GetDelta(InParticles, WorldSpaceX1, WorldSpaceX2, ConstraintIndex);

			if (InParticles.InvM(ConstraintInnerIndex1) > 0)
			{
				const TVector<T, d> Radius = WorldSpaceX1 - InParticles.P(ConstraintInnerIndex1);
				InParticles.P(ConstraintInnerIndex1) += InParticles.InvM(ConstraintInnerIndex1) * Delta;
				InParticles.Q(ConstraintInnerIndex1) += TRotation<T, d>(WorldSpaceInvI1 * TVector<T, d>::CrossProduct(Radius, Delta), 0.f) * InParticles.Q(ConstraintInnerIndex1) * T(0.5);
				InParticles.Q(ConstraintInnerIndex1).Normalize();
			}

			if (InParticles.InvM(ConstraintInnerIndex2) > 0)
			{
				const TVector<T, d> Radius = WorldSpaceX2 - InParticles.P(ConstraintInnerIndex2);
				InParticles.P(ConstraintInnerIndex2) -= InParticles.InvM(ConstraintInnerIndex2) * Delta;
				InParticles.Q(ConstraintInnerIndex2) += TRotation<T, d>(WorldSpaceInvI2 * TVector<T, d>::CrossProduct(Radius, -Delta), 0.f) * InParticles.Q(ConstraintInnerIndex2) * T(0.5);
				InParticles.Q(ConstraintInnerIndex2).Normalize();
			}
		}
	}

	void Apply(TPBDRigidParticles<T, d>& InParticles, const T Dt, const int32 Island) const override //-V762
	{
		ApplyHelper(InParticles, Dt, Island);
	}
};
}
