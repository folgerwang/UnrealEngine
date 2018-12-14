// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Framework/Parallel.h"
#include "Chaos/ParticleRule.h"

namespace Chaos
{
template<class T, int d>
class TPBDChainUpdateFromDeltaPosition : public TParticleRule<T, d>
{
  public:
	TPBDChainUpdateFromDeltaPosition(TArray<TArray<int32>>&& Constraints, const T Damping)
	    : MConstraints(MoveTemp(Constraints)), MDamping(Damping) {}
	virtual ~TPBDChainUpdateFromDeltaPosition() {}

	inline void Apply(TPBDParticles<T, d>& InParticles, const T Dt) const override //-V762
	{
		PhysicsParallelFor(MConstraints.Num(), [&](int32 index) {
			{
				int32 i = MConstraints[index][0];
				InParticles.V(i) = (InParticles.P(i) - InParticles.X(i)) / Dt;
				InParticles.X(i) = InParticles.P(i);
			}
			for (int i = 2; i < MConstraints[index].Num(); ++i)
			{
				int32 p = MConstraints[index][i];
				int32 pm1 = MConstraints[index][i - 1];
				InParticles.V(pm1) = (InParticles.P(pm1) - InParticles.X(pm1)) / Dt - MDamping * (InParticles.P(p) - InParticles.X(p)) / Dt;
				InParticles.X(pm1) = InParticles.P(pm1);
			}
			{
				int32 i = MConstraints[index][MConstraints[index].Num() - 1];
				InParticles.V(i) = (InParticles.P(i) - InParticles.X(i)) / Dt;
				InParticles.X(i) = InParticles.P(i);
			}
		});
	}

  private:
	TArray<TArray<int32>> MConstraints;
	T MDamping;
};
}
