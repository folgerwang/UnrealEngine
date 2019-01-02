// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#include "Chaos/PBDJointConstraintsBase.h"

using namespace Chaos;

// @todo(mlentine): This should be in a utility class somewhere
template<class T>
PMatrix<T, 3, 3> ComputeJointFactorMatrix(const TVector<T, 3>& V, const PMatrix<T, 3, 3>& M, const T& Im)
{
	// Rigid objects rotational contribution to the impulse.
	// Vx*M*VxT+Im
	check(Im > FLT_MIN)
	return PMatrix<T, 3, 3>(
		-V[2] * (-V[2] * M.M[1][1] + V[1] * M.M[2][1]) + V[1] * (-V[2] * M.M[2][1] + V[1] * M.M[2][2]) + Im,
		V[2] * (-V[2] * M.M[1][0] + V[1] * M.M[2][0]) - V[0] * (-V[2] * M.M[2][1] + V[1] * M.M[2][2]),
		-V[1] * (-V[2] * M.M[1][0] + V[1] * M.M[2][0]) + V[0] * (-V[2] * M.M[1][1] + V[1] * M.M[2][1]),
		V[2] * (V[2] * M.M[0][0] - V[0] * M.M[2][0]) - V[0] * (V[2] * M.M[2][0] - V[0] * M.M[2][2]) + Im,
		-V[1] * (V[2] * M.M[0][0] - V[0] * M.M[2][0]) + V[0] * (V[2] * M.M[1][0] - V[0] * M.M[2][1]),
		-V[1] * (-V[1] * M.M[0][0] + V[0] * M.M[1][0]) + V[0] * (-V[1] * M.M[1][0] + V[0] * M.M[1][1]) + Im);
}

template<class T, int d>
void TPBDJointConstraintsBase<T, d>::UpdateDistances(const TRigidParticles<T, d>& InParticles, const TArray<TVector<T, d>>& Locations)
{
	Distances.SetNum(Constraints.Num());
	const int32 NumConstraints = Constraints.Num();
	for (int32 ConstraintIndex = 0; ConstraintIndex < NumConstraints; ++ConstraintIndex)
	{
		int32 ConstraintInnerIndex1 = Constraints[ConstraintIndex][0];
		int32 ConstraintInnerIndex2 = Constraints[ConstraintIndex][1];
		Distances[ConstraintIndex][0] = InParticles.R(ConstraintInnerIndex1).Inverse().RotateVector(Locations[ConstraintIndex] - InParticles.X(ConstraintInnerIndex1));
		Distances[ConstraintIndex][1] = InParticles.R(ConstraintInnerIndex2).Inverse().RotateVector(Locations[ConstraintIndex] - InParticles.X(ConstraintInnerIndex2));
	}
}

template<class T, int d>
TVector<T, d> TPBDJointConstraintsBase<T, d>::GetDelta(const TPBDRigidParticles<T, d>& InParticles, const TVector<T, d>& WorldSpaceX1, const TVector<T, d>& WorldSpaceX2, const PMatrix<T, d, d>& WorldSpaceInvI1, const PMatrix<T, d, d>& WorldSpaceInvI2, int32 ConstraintIndex) const
{
	const TVector<int32, 2>& Constraint = Constraints[ConstraintIndex];
	int32 ConstraintInnerIndex1 = Constraint[0];
	int32 ConstraintInnerIndex2 = Constraint[1];

	if (InParticles.InvM(ConstraintInnerIndex2) == 0 && InParticles.InvM(ConstraintInnerIndex1) == 0)
	{
		return TVector<T, d>(0);
	}

	PMatrix<T, d, d> Factor =
		(InParticles.InvM(ConstraintInnerIndex1) > FLT_MIN ? ComputeJointFactorMatrix(WorldSpaceX1 - InParticles.P(ConstraintInnerIndex1), WorldSpaceInvI1, InParticles.InvM(ConstraintInnerIndex1)) : PMatrix<T, d, d>(0)) +
		(InParticles.InvM(ConstraintInnerIndex2) > FLT_MIN ? ComputeJointFactorMatrix(WorldSpaceX2 - InParticles.P(ConstraintInnerIndex2), WorldSpaceInvI2, InParticles.InvM(ConstraintInnerIndex2)) : PMatrix<T, d, d>(0));
	PMatrix<T, d, d> FactorInv = Factor.Inverse();
	FactorInv.M[3][3] = 1;
	TVector<T, d> Delta = WorldSpaceX2 - WorldSpaceX1;
	return FactorInv * Delta;
}

namespace Chaos
{
	template class TPBDJointConstraintsBase<float, 3>;
}
