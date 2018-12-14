// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Chaos/PBDRigidSpringConstraintsBase.h"

using namespace Chaos;

template<class T, int d>
void TPBDRigidSpringConstraintsBase<T, d>::UpdateDistances(const TRigidParticles<T, d>& InParticles, const TArray<TVector<T, d>>& Locations0, const TArray<TVector<T, d>>& Locations1)
{
	Distances.SetNum(Constraints.Num());
	SpringDistances.SetNum(Constraints.Num());

	const int32 NumConstraints = Constraints.Num();
	for (int32 ConstraintIndex = 0; ConstraintIndex < NumConstraints; ++ConstraintIndex)
	{
		int32 ConstraintInnerIndex1 = Constraints[ConstraintIndex][0];
		int32 ConstraintInnerIndex2 = Constraints[ConstraintIndex][1];

		Distances[ConstraintIndex][0] = InParticles.R(ConstraintInnerIndex1).Inverse().RotateVector(Locations0[ConstraintIndex] - InParticles.X(ConstraintInnerIndex1));
		Distances[ConstraintIndex][1] = InParticles.R(ConstraintInnerIndex2).Inverse().RotateVector(Locations1[ConstraintIndex] - InParticles.X(ConstraintInnerIndex2));
		SpringDistances[ConstraintIndex] = (Locations0[ConstraintIndex] - Locations1[ConstraintIndex]).Size();
	}
}

template<class T, int d>
TVector<T, d> TPBDRigidSpringConstraintsBase<T, d>::GetDelta(const TPBDRigidParticles<T, d>& InParticles, const TVector<T, d>& WorldSpaceX1, const TVector<T, d>& WorldSpaceX2, int32 i) const
{
	const TVector<int32, 2>& Constraint = Constraints[i];

	int32 i1 = Constraint[0];
	int32 i2 = Constraint[1];

	if (InParticles.InvM(i2) == 0 && InParticles.InvM(i1) == 0)
	{
		return TVector<T, d>(0);
	}

	TVector<T, d> Difference = WorldSpaceX2 - WorldSpaceX1;

	float Distance = Difference.Size();

	check(Distance > 1e-7);

	TVector<T, d> Direction = Difference / Distance;
	TVector<T, d> Delta = (Distance - SpringDistances[i]) * Direction;
	T CombinedMass = InParticles.InvM(i2) + InParticles.InvM(i1);
	return Stiffness * Delta / CombinedMass;
}

namespace Chaos
{
	template class TPBDRigidSpringConstraintsBase<float, 3>;
}
