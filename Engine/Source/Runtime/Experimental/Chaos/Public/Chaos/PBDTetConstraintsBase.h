// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Array.h"
#include "Chaos/Map.h"
#include "Chaos/PBDParticles.h"
#include "Chaos/ParticleRule.h"

namespace Chaos
{
template<class T>
class PBDTetConstraintsBase
{
  public:
	PBDTetConstraintsBase(const TDynamicParticles<T, 3>& InParticles, TArray<TVector<int32, 4>>&& Constraints, const T Stiffness = (T)1)
	    : MConstraints(Constraints), MStiffness(Stiffness)
	{
		for (auto Constraint : MConstraints)
		{
			const TVector<T, 3>& P1 = InParticles.X(Constraint[0]);
			const TVector<T, 3>& P2 = InParticles.X(Constraint[1]);
			const TVector<T, 3>& P3 = InParticles.X(Constraint[2]);
			const TVector<T, 3>& P4 = InParticles.X(Constraint[3]);
			MVolumes.Add(TVector<T, 3>::DotProduct(TVector<T, 3>::CrossProduct(P2 - P1, P3 - P1), P4 - P1) / (T)6);
		}
	}
	virtual ~PBDTetConstraintsBase() {}

	TVector<TVector<T, 3>, 4> GetGradients(const TPBDParticles<T, 3>& InParticles, const int32 i) const
	{
		TVector<TVector<T, 3>, 4> Grads;
		const auto& Constraint = MConstraints[i];
		const TVector<T, 3>& P1 = InParticles.P(Constraint[0]);
		const TVector<T, 3>& P2 = InParticles.P(Constraint[1]);
		const TVector<T, 3>& P3 = InParticles.P(Constraint[2]);
		const TVector<T, 3>& P4 = InParticles.P(Constraint[3]);
		const TVector<T, 3> P2P1 = P2 - P1;
		const TVector<T, 3> P3P1 = P3 - P1;
		const TVector<T, 3> P4P1 = P4 - P1;
		Grads[1] = TVector<T, 3>::CrossProduct(P3P1, P4P1) / (T)6;
		Grads[2] = TVector<T, 3>::CrossProduct(P4P1, P2P1) / (T)6;
		Grads[3] = TVector<T, 3>::CrossProduct(P2P1, P3P1) / (T)6;
		Grads[0] = -1 * (Grads[1] + Grads[2] + Grads[3]);
		return Grads;
	}

	T GetScalingFactor(const TPBDParticles<T, 3>& InParticles, const int32 i, const TVector<TVector<T, 3>, 4>& Grads) const
	{
		const auto& Constraint = MConstraints[i];
		const int32 i1 = Constraint[0];
		const int32 i2 = Constraint[1];
		const int32 i3 = Constraint[2];
		const int32 i4 = Constraint[3];
		const TVector<T, 3>& P1 = InParticles.P(i1);
		const TVector<T, 3>& P2 = InParticles.P(i2);
		const TVector<T, 3>& P3 = InParticles.P(i3);
		const TVector<T, 3>& P4 = InParticles.P(i4);
		T Volume = TVector<T, 3>::DotProduct(TVector<T, 3>::CrossProduct(P2 - P1, P3 - P1), P4 - P1) / (T)6;
		T S = (Volume - MVolumes[i]) / (InParticles.InvM(i1) * Grads[0].SizeSquared() + 
			                            InParticles.InvM(i2) * Grads[1].SizeSquared() + 
			                            InParticles.InvM(i3) * Grads[2].SizeSquared() + 
			                            InParticles.InvM(i4) * Grads[3].SizeSquared());
		return MStiffness * S;
	}

  protected:
	TArray<TVector<int32, 4>> MConstraints;

  private:
	TArray<T> MVolumes;
	T MStiffness;
};
}
