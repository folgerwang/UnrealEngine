// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/Vector.h"

namespace Apeiron
{
/**/
template<class T, int d>
struct TRigidBodyContactConstraint
{
	int32 ParticleIndex, LevelsetIndex;
	TVector<T, d> Normal;
	TVector<T, d> Location;
	T Phi;
};
}
