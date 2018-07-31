// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/Array.h"
#include "Apeiron/Map.h"
#include "Apeiron/PBDParticles.h"
#include "Apeiron/PerParticleRule.h"

#include <functional>

namespace Apeiron
{
template<class T, int d>
class TPBDSpringConstraintsBase
{
  public:
	TPBDSpringConstraintsBase(const T Stiffness = (T)1)
	    : MStiffness(Stiffness)
	{
	}
	TPBDSpringConstraintsBase(const TDynamicParticles<T, d>& InParticles, TArray<TVector<int32, 2>>&& Constraints, const T Stiffness = (T)1)
	    : MConstraints(MoveTemp(Constraints)), MStiffness(Stiffness)
	{
		UpdateDistances(InParticles);
	}
	TPBDSpringConstraintsBase(const TRigidParticles<T, d>& InParticles, TArray<TVector<int32, 2>>&& Constraints, const T Stiffness = (T)1)
	    : MConstraints(MoveTemp(Constraints)), MStiffness(Stiffness)
	{
		UpdateDistances(InParticles);
	}
	TPBDSpringConstraintsBase(const TDynamicParticles<T, d>& InParticles, const TArray<TVector<int32, 3>>& Constraints, const T Stiffness = (T)1)
	    : MStiffness(Stiffness)
	{
		for (auto Constraint : Constraints)
		{
			int32 i1 = Constraint[0];
			int32 i2 = Constraint[1];
			int32 i3 = Constraint[2];
			const TVector<T, d>& P1 = InParticles.X(i1);
			const TVector<T, d>& P2 = InParticles.X(i2);
			const TVector<T, d>& P3 = InParticles.X(i3);
			MConstraints.Add(TVector<int32, 2>({i1, i2}));
			MDists.Add((P1 - P2).Size());
			MConstraints.Add(TVector<int32, 2>({i1, i3}));
			MDists.Add((P1 - P3).Size());
			MConstraints.Add(TVector<int32, 2>({i2, i3}));
			MDists.Add((P2 - P3).Size());
		}
	}
	TPBDSpringConstraintsBase(const TDynamicParticles<T, d>& InParticles, const TArray<TVector<int32, 4>>& Constraints, const T Stiffness = (T)1)
	    : MStiffness(Stiffness)
	{
		static_assert(d == 3, "Tet spring Constraints are only valid in 3d.");
		for (auto Constraint : Constraints)
		{
			int32 i1 = Constraint[0];
			int32 i2 = Constraint[1];
			int32 i3 = Constraint[2];
			int32 i4 = Constraint[3];
			const TVector<T, d>& P1 = InParticles.X(i1);
			const TVector<T, d>& P2 = InParticles.X(i2);
			const TVector<T, d>& P3 = InParticles.X(i3);
			const TVector<T, d>& P4 = InParticles.X(i4);
			MConstraints.Add(TVector<int32, 2>({i1, i2}));
			MDists.Add((P1 - P2).Size());
			MConstraints.Add(TVector<int32, 2>({i1, i3}));
			MDists.Add((P1 - P3).Size());
			MConstraints.Add(TVector<int32, 2>({i1, i4}));
			MDists.Add((P1 - P4).Size());
			MConstraints.Add(TVector<int32, 2>({i2, i3}));
			MDists.Add((P2 - P3).Size());
			MConstraints.Add(TVector<int32, 2>({i2, i4}));
			MDists.Add((P2 - P4).Size());
			MConstraints.Add(TVector<int32, 2>({i3, i4}));
			MDists.Add((P3 - P4).Size());
		}
	}
	virtual ~TPBDSpringConstraintsBase() {}

	template<class T_PARTICLES>
	void UpdateDistances(const T_PARTICLES& InParticles, const int32 StartIndex = 0)
	{
		MDists.SetNum(MConstraints.Num());
		for (int32 i = StartIndex; i < MConstraints.Num(); ++i)
		{
			int32 i1 = MConstraints[i][0];
			int32 i2 = MConstraints[i][1];
			const TVector<T, d>& P1 = InParticles.X(i1);
			const TVector<T, d>& P2 = InParticles.X(i2);
			MDists[i] = (P1 - P2).Size();
		}
	}

	template<class T_PARTICLES>
	TVector<T, d> GetDelta(const T_PARTICLES& InParticles, const int32 i) const
	{
		const auto& Constraint = MConstraints[i];
		int32 i1 = Constraint[0];
		int32 i2 = Constraint[1];
		if (InParticles.InvM(i2) == 0 && InParticles.InvM(i1) == 0)
			return TVector<T, d>(0);
		const TVector<T, d>& P1 = InParticles.P(i1);
		const TVector<T, d>& P2 = InParticles.P(i2);
		TVector<T, d> Difference = P1 - P2;
		float Distance = Difference.Size();
		check(Distance > 1e-7);
		TVector<T, d> Direction = Difference / Distance;
		TVector<T, d> Delta = (Distance - MDists[i]) * Direction;
		T CombinedMass = InParticles.InvM(i2) + InParticles.InvM(i1);
		return MStiffness * Delta / CombinedMass;
	}

  protected:
	TArray<TVector<int32, 2>> MConstraints;

  private:
	TArray<T> MDists;
	T MStiffness;
};
}
