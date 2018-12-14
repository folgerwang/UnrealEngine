// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#if !COMPILE_WITHOUT_UNREAL_SUPPORT
#include "Chaos/Array.h"
#include "Chaos/BoundingVolumeHierarchy.h"
#include "Chaos/Framework/Parallel.h"
#include "Chaos/Map.h"
#include "Chaos/PBDParticles.h"
#include "Chaos/PerParticleRule.h"
#include "Chaos/Sphere.h"

#include <algorithm>

// This is an approximation but only collides with spheres in the velocity direction which can hurt compared to all directions when it comes to thickness
namespace Chaos
{
template<class T, int d>
class TPBDCollisionSphereConstraints : public TPerParticleRule<T, d>
{
  public:
	TPBDCollisionSphereConstraints(const TPBDParticles<T, d>& InParticles, const TSet<TVector<int32, 2>>& DisabledCollisionElements, const T Dt, const T Height = (T)0)
	    : MH(Height)
	{
		for (int32 i = 0; i < InParticles.Size(); ++i)
		{
			MObjects.Add(TUniquePtr<TImplicitObject<T, d>>(new TSphere<T, d>(InParticles.P(i), Height)));
		}
		TBoundingVolumeHierarchy<TArray<TUniquePtr<TImplicitObject<T, d>>>, T, d> Hierarchy(MObjects);
		FCriticalSection CriticalSection;
		PhysicsParallelFor(InParticles.Size(), [&](int32 Index) {
			TArray<int32> PotentialIntersections = Hierarchy.FindAllIntersections(InParticles.P(Index));
			for (int32 i = 0; i < PotentialIntersections.Num(); ++i)
			{
				int32 Index2 = PotentialIntersections[i];
				if (Index == Index2 || DisabledCollisionElements.Contains(TVector<int32, 2>(Index, Index2)))
					continue;
				if ((InParticles.P(Index2) - InParticles.P(Index)).Size() < Height)
				{
					CriticalSection.Lock();
					MConstraints.FindOrAdd(Index).Add(Index2);
					CriticalSection.Unlock();
				}
			}
		});
	}
	virtual ~TPBDCollisionSphereConstraints() {}

	void Apply(TPBDParticles<T, d>& InParticles, const T Dt, const int32 Index) const override //-V762
	{
		if (InParticles.InvM(Index) == 0 || !MConstraints.Contains(Index))
			return;
		for (int32 i = 0; i < MConstraints[Index].Num(); ++i)
		{
			TVector<T, d> Normal;
			T Phi = MObjects[MConstraints[Index][i]]->PhiWithNormal(InParticles.P(Index), Normal);
			if (Phi < 0)
			{
				InParticles.P(Index) += -Phi * Normal;
			}
		}
	}

  private:
	T MH;
	TMap<int32, TArray<int32>> MConstraints;
	TArray<TUniquePtr<TImplicitObject<T, d>>> MObjects;
};
}
#endif
