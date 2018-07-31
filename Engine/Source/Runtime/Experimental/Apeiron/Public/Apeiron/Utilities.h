// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Apeiron/DynamicParticles.h"
#include "Apeiron/PBDParticles.h"
#include "Apeiron/PerParticleGravity.h"
#include "Apeiron/Vector.h"

namespace Apeiron
{
namespace Utilities
{
template<class T_PARTICLES, class T_PARTICLES_BASE, class T, int d>
inline TFunction<void(T_PARTICLES&, const T, const int32)> GetGravityFunction(const TVector<T, d>& Direction, const T Magnitude)
{
	return [Gravity = PerParticleGravity<T, d>(Direction, Magnitude)](T_PARTICLES_BASE& InParticles, const T Dt, const int Index) {
		Gravity.Apply(InParticles, Dt, Index);
	};
}
template<class T, int d>
inline TFunction<void(TPBDParticles<T, d>&, const T, const int32)> GetDeformablesGravityFunction(const TVector<T, d>& Direction, const T Magnitude)
{
	return GetGravityFunction<TPBDParticles<T, d>, TDynamicParticles<T, d>, T, d>(Direction, Magnitude);
}
template<class T, int d>
inline TFunction<void(TPBDRigidParticles<T, d>&, const T, const int32)> GetRigidsGravityFunction(const TVector<T, d>& Direction, const T Magnitude)
{
	return GetGravityFunction<TPBDRigidParticles<T, d>, TRigidParticles<T, d>, T, d>(Direction, Magnitude);
}
}
}