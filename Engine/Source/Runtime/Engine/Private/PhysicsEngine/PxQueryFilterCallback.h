// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=======================================================================================
PhysXCollision.h: Collision related data structures/types specific to PhysX
=========================================================================================*/

#pragma once
#include "PhysicsEngine/CollisionQueryFilterCallback.h"
#include "PhysXPublic.h"

#if WITH_PHYSX

static PxQueryHitType::Enum HitTypeToPxQueryHitType(ECollisionQueryHitType HitType)
{
	return (PxQueryHitType::Enum)HitType;
}

/** Unreal PhysX scene query filter callback object */
class FPxQueryFilterCallback : public PxQueryFilterCallback, public FCollisionQueryFilterCallback
{
public:

	FPxQueryFilterCallback(const FCollisionQueryParams& InQueryParams, bool bIsSweep)
		: FCollisionQueryFilterCallback(InQueryParams, bIsSweep)
	{}

	virtual PxQueryHitType::Enum preFilter(const PxFilterData& filterData, const PxShape* shape, const PxRigidActor* actor, PxHitFlags& queryFlags) override;

	virtual PxQueryHitType::Enum postFilter(const PxFilterData& filterData, const PxQueryHit& hit) override;
};

#endif // WITH_PHYX
