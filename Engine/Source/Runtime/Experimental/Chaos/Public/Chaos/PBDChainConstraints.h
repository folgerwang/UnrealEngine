// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDParticles.h"
#include "Chaos/Framework/Parallel.h"
#include "Chaos/ParticleRule.h"

namespace Chaos
{
template<class T, int d>
class TPBDChainConstraints : public TParticleRule<T, d>
{
  public:
	TPBDChainConstraints(const TDynamicParticles<T, d>& InParticles, TArray<TArray<int32>>&& Constraints, const T Coefficient = (T)1)
	    : MConstraints(Constraints), MCoefficient(Coefficient)
	{
		MDists.SetNum(MConstraints.Num());
		PhysicsParallelFor(MConstraints.Num(), [&](int32 Index) {
			TArray<float> singledists;
			for (int i = 1; i < Constraints[Index].Num(); ++i)
			{
				const TVector<T, d>& P1 = InParticles.X(Constraints[Index][i - 1]);
				const TVector<T, d>& P2 = InParticles.X(Constraints[Index][i]);
				float Distance = (P1 - P2).Size();
				singledists.Add(Distance);
			}
			MDists[Index] = singledists;
		});
	}
	virtual ~TPBDChainConstraints() {}

	void Apply(TPBDParticles<T, d>& InParticles, const T Dt) const override //-V762
	{
		PhysicsParallelFor(MConstraints.Num(), [&](int32 Index) {
			for (int i = 1; i < MConstraints[Index].Num(); ++i)
			{
				int32 P = MConstraints[Index][i];
				int32 PM1 = MConstraints[Index][i - 1];
				const TVector<T, d>& P1 = InParticles.P(PM1);
				const TVector<T, d>& P2 = InParticles.P(P);
				TVector<T, d> Difference = P1 - P2;
				float Distance = Difference.Size();
				TVector<T, d> Direction = Difference / Distance;
				TVector<T, d> Delta = (Distance - MDists[Index][i - 1]) * Direction;
				if (i == 1)
				{
					InParticles.P(P) += Delta;
				}
				else
				{
					InParticles.P(P) += MCoefficient * Delta;
					InParticles.P(PM1) -= (1 - MCoefficient) * Delta;
				}
			}
		});
	}

  private:
	TArray<TArray<int32>> MConstraints;
	TArray<TArray<T>> MDists;
	T MCoefficient;
};
}
