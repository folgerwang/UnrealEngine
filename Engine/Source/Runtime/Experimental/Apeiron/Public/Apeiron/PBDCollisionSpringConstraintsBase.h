// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#if !COMPILE_WITHOUT_UNREAL_SUPPORT
#include "Apeiron/Array.h"
#include "Apeiron/PBDParticles.h"
#include "Apeiron/PerParticleRule.h"

#include <unordered_set>

namespace Apeiron
{
// This is an invertible spring class, typical springs are not invertible aware
template<class T, int d>
class PBDCollisionSpringConstraintsBase
{
  public:
	PBDCollisionSpringConstraintsBase(const TDynamicParticles<T, d>& InParticles, const TArray<TVector<int32, 3>>& Elements, const TSet<TVector<int32, 2>>& DisabledCollisionElements, const T Dt, const T Height = (T)0, const T Stiffness = (T)1);
	virtual ~PBDCollisionSpringConstraintsBase() {}

	TVector<T, d> GetDelta(const TPBDParticles<T, d>& InParticles, const int32 i) const;

  protected:
	TArray<TVector<int32, 4>> MConstraints;
	TArray<TVector<T, 3>> MBarys;
	T MH;

  private:
	TArray<TVector<T, d>> MNormals; // per constraint, sign changes depending on orientation of colliding particle
	T MStiffness;
};
}
#endif
