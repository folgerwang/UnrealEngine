// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/Array.h"
#include "Apeiron/Map.h"
#include "Apeiron/PBDParticles.h"
#include "Apeiron/ParticleRule.h"

namespace Apeiron
{
template<class T>
class TPBDVolumeConstraintBase
{
  public:
	TPBDVolumeConstraintBase(const TDynamicParticles<T, 3>& InParticles, TArray<TVector<int32, 3>>&& constraints, const T stiffness = (T)1)
	    : MConstraints(constraints), Stiffness(stiffness)
	{
		TVector<T, 3> Com = TVector<T, 3>(0, 0, 0);
		for (uint32 i = 0; i < InParticles.Size(); ++i)
		{
			Com += InParticles.X(i);
		}
		Com /= InParticles.Size();
		MVolume = 0;
		for (auto constraint : MConstraints)
		{
			const TVector<T, 3>& P1 = InParticles.X(constraint[0]);
			const TVector<T, 3>& P2 = InParticles.X(constraint[1]);
			const TVector<T, 3>& P3 = InParticles.X(constraint[2]);
			MVolume += GetVolume(P1, P2, P3, Com);
		}
		MVolume /= (T)9;
	}
	virtual ~TPBDVolumeConstraintBase() {}

	TArray<T> GetWeights(const TPBDParticles<T, 3>& InParticles, const T Alpha) const
	{
		TArray<T> W;
		W.SetNum(InParticles.Size());
		T oneminusAlpha = 1 - Alpha;
		T Wg = (T)1 / (T)InParticles.Size();
		T WlDenom = 0;
		for (uint32 i = 0; i < InParticles.Size(); ++i)
		{
			WlDenom += (InParticles.P(i) - InParticles.X(i)).Size();
		}
		for (uint32 i = 0; i < InParticles.Size(); ++i)
		{
			T Wl = (InParticles.P(i) - InParticles.X(i)).Size() / WlDenom;
			W[i] = oneminusAlpha * Wl + Alpha * Wg;
		}
		return W;
	}

	TArray<TVector<T, 3>> GetGradients(const TPBDParticles<T, 3>& InParticles) const
	{
		TVector<T, 3> Com = TVector<T, 3>(0, 0, 0);
		for (uint32 i = 0; i < InParticles.Size(); ++i)
		{
			Com += InParticles.P(i);
		}
		Com /= InParticles.Size();
		TArray<TVector<T, 3>> Grads;
		Grads.SetNum(InParticles.Size());
		for (auto& Elem : Grads)
		{
			Elem = TVector<T, 3>(0, 0, 0);
		}
		for (int32 i = 0; i < MConstraints.Num(); ++i)
		{
			auto constraint = MConstraints[i];
			const int32 i1 = constraint[0];
			const int32 i2 = constraint[1];
			const int32 i3 = constraint[2];
			const TVector<T, 3>& P1 = InParticles.P(i1);
			const TVector<T, 3>& P2 = InParticles.P(i2);
			const TVector<T, 3>& P3 = InParticles.P(i3);
			auto area = GetArea(P1, P2, P3);
			auto Normal = GetNormal(P1, P2, P3, Com);
			Grads[i1] += area * Normal;
			Grads[i2] += area * Normal;
			Grads[i3] += area * Normal;
		}
		for (auto& Elem : Grads)
		{
			Elem *= (T)1 / (T)3;
		}
		return Grads;
	}

	T GetScalingFactor(const TPBDParticles<T, 3>& InParticles, const TArray<TVector<T, 3>>& Grads, const TArray<T>& W) const
	{
		TVector<T, 3> Com = TVector<T, 3>(0, 0, 0);
		for (uint32 i = 0; i < InParticles.Size(); ++i)
		{
			Com += InParticles.P(i);
		}
		Com /= InParticles.Size();
		T Volume = 0;
		for (auto constraint : MConstraints)
		{
			const TVector<T, 3>& P1 = InParticles.P(constraint[0]);
			const TVector<T, 3>& P2 = InParticles.P(constraint[1]);
			const TVector<T, 3>& P3 = InParticles.P(constraint[2]);
			Volume += GetVolume(P1, P2, P3, Com);
		}
		Volume /= (T)9;
		T Denom = 0;
		for (uint32 i = 0; i < InParticles.Size(); ++i)
		{
			Denom += W[i] * Grads[i].SizeSquared();
		}
		T S = (Volume - MVolume) / Denom;
		return Stiffness * S;
	}

  protected:
	TArray<TVector<int32, 3>> MConstraints;

  private:
	// Utility functions for the triangle concept
	TVector<T, 3> GetNormal(const TVector<T, 3> P1, const TVector<T, 3>& P2, const TVector<T, 3>& P3, const TVector<T, 3>& Com) const
	{
		auto Normal = TVector<T, 3>::CrossProduct(P2 - P1, P3 - P1).GetSafeNormal();
		if (TVector<T, 3>::DotProduct((P1 + P2 + P3) / (T)3 - Com, Normal) < 0)
			return -Normal;
		return Normal;
	}

	T GetArea(const TVector<T, 3>& P1, const TVector<T, 3>& P2, const TVector<T, 3>& P3) const
	{
		TVector<T, 3> B = (P2 - P1).GetSafeNormal();
		TVector<T, 3> H = TVector<T, 3>::DotProduct(B, P3 - P1) * B + P1;
		return (T)0.5 * (P2 - P1).Size() * (P3 - H).Size();
	}

	T GetVolume(const TVector<T, 3>& P1, const TVector<T, 3>& P2, const TVector<T, 3>& P3, const TVector<T, 3>& Com) const
	{
		return GetArea(P1, P2, P3) * TVector<T, 3>::DotProduct(P1 + P2 + P3, GetNormal(P1, P2, P3, Com));
	}

	T MVolume;
	T Stiffness;
};
}
