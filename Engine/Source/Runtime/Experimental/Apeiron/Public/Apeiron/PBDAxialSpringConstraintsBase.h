// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/Array.h"
#include "Apeiron/Map.h"
#include "Apeiron/PBDParticles.h"
#include "Apeiron/PerParticleRule.h"

#include <cmath>
#include <functional>

namespace Apeiron
{
template<class T, int d>
class TPBDAxialSpringConstraintsBase
{
  public:
	TPBDAxialSpringConstraintsBase(const TDynamicParticles<T, d>& InParticles, TArray<TVector<int32, 3>>&& Constraints, const T Stiffness = (T)1)
	    : MConstraints(MoveTemp(Constraints)), MStiffness(Stiffness)
	{
		for (auto& Constraint : MConstraints)
		{
			int32 i1 = Constraint[0];
			int32 i2 = Constraint[1];
			int32 i3 = Constraint[2];
			// Find Bary closest to 0.5
			T Bary1 = FindBary(InParticles, i1, i2, i3);
			T Bary2 = FindBary(InParticles, i2, i3, i1);
			T Bary3 = FindBary(InParticles, i3, i1, i2);
			T Bary = Bary1;
			T Bary1dist = FGenericPlatformMath::Abs(Bary1 - 0.5);
			T Bary2dist = FGenericPlatformMath::Abs(Bary2 - 0.5);
			T Bary3dist = FGenericPlatformMath::Abs(Bary3 - 0.5);
			if (Bary3dist < Bary2dist && Bary3dist < Bary1dist)
			{
				Constraint[0] = i3;
				Constraint[1] = i1;
				Constraint[2] = i2;
				Bary = Bary3;
			}
			else if (Bary2dist < Bary1dist && Bary2dist < Bary3dist)
			{
				Constraint[0] = i2;
				Constraint[1] = i3;
				Constraint[2] = i1;
				Bary = Bary2;
			}
			// Reset as they may have changed
			i1 = Constraint[0];
			i2 = Constraint[1];
			i3 = Constraint[2];
			const TVector<T, d>& P1 = InParticles.X(i1);
			const TVector<T, d>& P2 = InParticles.X(i2);
			const TVector<T, d>& P3 = InParticles.X(i3);
			const TVector<T, d> P = (P2 - P3) * Bary + P3;
			MBarys.Add(Bary);
			MDists.Add((P1 - P).Size());
		}
	}
	virtual ~TPBDAxialSpringConstraintsBase() {}

	TVector<T, d> GetDelta(const TPBDParticles<T, d>& InParticles, const int32 i) const
	{
		const auto& Constraint = MConstraints[i];
		int32 i1 = Constraint[0];
		int32 i2 = Constraint[1];
		int32 i3 = Constraint[2];
		T PInvMass = InParticles.InvM(i3) * (1 - MBarys[i]) + InParticles.InvM(i2) * MBarys[i];
		if (InParticles.InvM(i1) == 0 && PInvMass == 0)
			return TVector<T, d>(0);
		const TVector<T, d>& P1 = InParticles.P(i1);
		const TVector<T, d>& P2 = InParticles.P(i2);
		const TVector<T, d>& P3 = InParticles.P(i3);
		const TVector<T, d> P = (P2 - P3) * MBarys[i] + P3;
		TVector<T, d> Difference = P1 - P;
		float Distance = Difference.Size();
		check(Distance > 1e-7);
		TVector<T, d> Direction = Difference / Distance;
		TVector<T, d> Delta = (Distance - MDists[i]) * Direction;
		T CombinedMass = PInvMass + InParticles.InvM(i1);
		check(CombinedMass > 1e-7);
		return MStiffness * Delta / CombinedMass;
	}

  private:
	T FindBary(const TDynamicParticles<T, d>& InParticles, const int32 i1, const int32 i2, const int32 i3)
	{
		const TVector<T, d>& P1 = InParticles.X(i1);
		const TVector<T, d>& P2 = InParticles.X(i2);
		const TVector<T, d>& P3 = InParticles.X(i3);
		const TVector<T, d>& P32 = P3 - P2;
		T Bary = TVector<T, d>::DotProduct(P32, P3 - P1) / P32.SizeSquared();
		if (Bary > (T)1)
			Bary = (T)1;
		if (Bary < (T)0)
			Bary = (T)0;
		return Bary;
	}

  protected:
	TArray<TVector<int32, 3>> MConstraints;
	TArray<T> MBarys;

  private:
	TArray<T> MDists;
	T MStiffness;
};
}
