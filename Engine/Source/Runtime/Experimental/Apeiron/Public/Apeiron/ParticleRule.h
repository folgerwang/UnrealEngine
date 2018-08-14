// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/Defines.h"
#include "Apeiron/PBDParticles.h"
#include "Apeiron/PBDRigidParticles.h"

namespace Apeiron
{
template<class T, int d>
class TParticleRule
{
  public:
	virtual void Apply(TParticles<T, d>& InParticles, const T Dt) const { check(0); }
	virtual void Apply(TDynamicParticles<T, d>& InParticles, const T Dt) const { Apply(static_cast<TParticles<T, d>&>(InParticles), Dt); }
	virtual void Apply(TPBDParticles<T, d>& InParticles, const T Dt) const { Apply(static_cast<TDynamicParticles<T, d>&>(InParticles), Dt); }
	virtual void Apply(TRigidParticles<T, d>& InParticles, const T Dt) const { Apply(static_cast<TParticles<T, d>&>(InParticles), Dt); }
	virtual void Apply(TPBDRigidParticles<T, d>& InParticles, const T Dt) const { Apply(static_cast<TRigidParticles<T, d>&>(InParticles), Dt); }
};
}
