// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_PHYSX
#include "PhysXPublic.h"
#endif

namespace ImmediatePhysics
{
/** Holds shape data*/
struct FImmediateKinematicTarget
{
#if WITH_PHYSX
	PxTransform BodyToWorld;
#endif
	bool bTargetSet;
	
	FImmediateKinematicTarget()
		: bTargetSet(false)
	{
	}
};

}