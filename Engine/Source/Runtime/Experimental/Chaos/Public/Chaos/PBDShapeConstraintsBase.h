// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Array.h"
#include "Chaos/Map.h"
#include "Chaos/PBDParticles.h"
#include "Chaos/PerParticleRule.h"

#include <functional>

namespace Chaos
{
template<class T, int d>
class TPBDShapeConstraintsBase
{
  public:
	TPBDShapeConstraintsBase(const TDynamicParticles<T, d>& InParticles, const TArray<TVector<T, 3>>& TargetPositions, const T Stiffness = (T)1)
	    : MTargetPositions(TargetPositions), MStiffness(Stiffness)
	{
		for (uint32 i = 0; i < InParticles.Size(); ++i)
		{
			const TVector<T, d>& P1 = InParticles.X(i);
			const TVector<T, d>& P2 = MTargetPositions[i];
			MDists.Add((P1 - P2).Size());
		}
	}
	virtual ~TPBDShapeConstraintsBase() {}

	TVector<T, d> GetDelta(const TPBDParticles<T, d>& InParticles, const int32 i) const
	{
		if (InParticles.InvM(i) == 0)
			return TVector<T, d>(0);
		const TVector<T, d>& P1 = InParticles.P(i);
		const TVector<T, d>& P2 = MTargetPositions[i];
		TVector<T, d> Difference = P1 - P2;
		float Distance = Difference.Size();
		TVector<T, d> Direction = Difference / Distance;
		TVector<T, d> Delta = (Distance - MDists[i]) * Direction;
		return MStiffness * Delta / InParticles.InvM(i);
	}

  protected:
	const TArray<TVector<float, 3>>& MTargetPositions;

  private:
	TArray<T> MDists;
	T MStiffness;
};
}
