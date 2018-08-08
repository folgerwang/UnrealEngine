// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/Array.h"
#include "Apeiron/Map.h"
#include "Apeiron/PBDParticles.h"
#include "Apeiron/ParticleRule.h"
#include "GenericPlatform/GenericPlatformMath.h"

namespace Apeiron
{
template<class T>
class TPBDBendingConstraintsBase
{
  public:
	TPBDBendingConstraintsBase(const TDynamicParticles<T, 3>& InParticles, TArray<TVector<int32, 4>>&& Constraints, const T Stiffness = (T)1)
	    : MConstraints(MoveTemp(Constraints)), MStiffness(Stiffness)
	{
		for (auto Constraint : MConstraints)
		{
			const TVector<T, 3>& P1 = InParticles.X(Constraint[0]);
			const TVector<T, 3>& P2 = InParticles.X(Constraint[1]);
			const TVector<T, 3>& P3 = InParticles.X(Constraint[2]);
			const TVector<T, 3>& P4 = InParticles.X(Constraint[3]);
			MAngles.Add(GetAngle(P1, P2, P3, P4));
		}
	}
	virtual ~TPBDBendingConstraintsBase() {}

	TArray<TVector<T, 3>> GetGradients(const TPBDParticles<T, 3>& InParticles, const int32 i) const
	{
		TArray<TVector<T, 3>> Grads;
		Grads.SetNum(4);
		const auto& Constraint = MConstraints[i];
		const TVector<T, 3>& P1 = InParticles.P(Constraint[0]);
		const TVector<T, 3>& P2 = InParticles.P(Constraint[1]);
		const TVector<T, 3>& P3 = InParticles.P(Constraint[2]);
		const TVector<T, 3>& P4 = InParticles.P(Constraint[3]);
		auto Edge = P2 - P1;
		auto Normal1 = TVector<T, 3>::CrossProduct(P3 - P1, P3 - P2);
		SafeDivide(Normal1, Normal1.SizeSquared());
		auto Normal2 = TVector<T, 3>::CrossProduct(P4 - P2, P4 - P1);
		SafeDivide(Normal2, Normal2.SizeSquared());
		T EdgeSize = Edge.Size();
		Grads[0] = SafeDivide(TVector<T, 3>::DotProduct(Edge, P3 - P2), EdgeSize) * Normal1 + SafeDivide(TVector<T, 3>::DotProduct(Edge, P4 - P2), EdgeSize) * Normal2;
		Grads[1] = SafeDivide(TVector<T, 3>::DotProduct(Edge, P1 - P3), EdgeSize) * Normal1 + SafeDivide(TVector<T, 3>::DotProduct(Edge, P1 - P4), EdgeSize) * Normal2;
		Grads[2] = EdgeSize * Normal1;
		Grads[3] = EdgeSize * Normal2;
		return Grads;
	}

	T GetScalingFactor(const TPBDParticles<T, 3>& InParticles, const int32 i, const TArray<TVector<T, 3>>& Grads) const
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
		T Angle = GetAngle(P1, P2, P3, P4);
		T Denom = (InParticles.InvM(i1) * Grads[0].SizeSquared() + InParticles.InvM(i2) * Grads[1].SizeSquared() + InParticles.InvM(i3) * Grads[2].SizeSquared() + InParticles.InvM(i4) * Grads[3].SizeSquared());
		{
			auto Edge = P2 - P1;
			auto Normal1 = TVector<T, 3>::CrossProduct(P3 - P1, P3 - P2).GetSafeNormal();
			auto Normal2 = TVector<T, 3>::CrossProduct(P4 - P2, P4 - P1).GetSafeNormal();
			Denom = TVector<T, 3>::DotProduct(Edge, TVector<T, 3>::CrossProduct(Normal1, Normal2)) > (T)0 ? -Denom : Denom;
		}
		T Delta = Angle - MAngles[i];
		return MStiffness * SafeDivide(Delta, Denom);
	}

  private:
	template<class T2>
	inline T2 SafeDivide(const T2& Numerator, const T& Denominator) const
	{
		if (Denominator > 1e-7)
			return Numerator / Denominator;
		return T2(0);
	}

	inline T Clamp(const T& Value, const T& Low, const T& High) const
	{
		return Value < Low ? Low : (Value > High ? High : Value);
	}

	T GetAngle(const TVector<T, 3>& P1, const TVector<T, 3>& P2, const TVector<T, 3>& P3, const TVector<T, 3>& P4) const
	{
		auto Normal1 = TVector<T, 3>::CrossProduct(P3 - P1, P3 - P2).GetSafeNormal();
		auto Normal2 = TVector<T, 3>::CrossProduct(P4 - P2, P4 - P1).GetSafeNormal();
		auto Dot = TVector<T, 3>::DotProduct(Normal1, Normal2);
		return FGenericPlatformMath::Acos(Clamp(Dot, 1e-4, 1 - 1e-4));
	}

  protected:
	TArray<TVector<int32, 4>> MConstraints;

  private:
	TArray<T> MAngles;
	T MStiffness;
};
}
