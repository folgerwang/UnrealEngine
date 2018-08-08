// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PhysicsEngine/PxQueryFilterCallback.h"
#include "PhysxUserData.h"
#include "Components/PrimitiveComponent.h"
#include "Physics/PhysicsInterfaceUtils.h"
#include "Collision.h"

#if WITH_PHYSX

PxQueryHitType::Enum FPxQueryFilterCallback::preFilter(const PxFilterData& filterData, const PxShape* shape, const PxRigidActor* actor, PxHitFlags& queryFlags)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_PreFilter);

	ensureMsgf(shape, TEXT("Invalid shape encountered in FPxQueryFilterCallback::preFilter, actor: %p, filterData: %x %x %x %x"), actor, filterData.word0, filterData.word1, filterData.word2, filterData.word3);

	if (!shape)
	{
		// Early out to avoid crashing.
		return HitTypeToPxQueryHitType(PreFilterReturnValue = ECollisionQueryHitType::None);
	}

	FCollisionFilterData FilterData = P2UFilterData(filterData);
	FCollisionFilterData ShapeFilter = P2UFilterData(shape->getQueryFilterData());

	// We usually don't have ignore components so we try to avoid the virtual getSimulationFilterData() call below. 'word2' of shape sim filter data is componentID.
	uint32 ComponentID = 0;
	if (IgnoreComponents.Num() > 0)
	{
		ComponentID = shape->getSimulationFilterData().word2;
	}

	FBodyInstance* BodyInstance = nullptr;
#if ENABLE_PREFILTER_LOGGING || DETECT_SQ_HITCHES
	BodyInstance = FPhysxUserData::Get<FBodyInstance>(actor->userData);
#endif // ENABLE_PREFILTER_LOGGING || DETECT_SQ_HITCHES

	return HitTypeToPxQueryHitType(PreFilter(FilterData, ShapeFilter, ComponentID, BodyInstance));
}

PxQueryHitType::Enum FPxQueryFilterCallback::postFilter(const PxFilterData& filterData, const PxQueryHit& hit)
{
	// Unused in non-sweeps
	if (!bIsSweep)
	{
		return PxQueryHitType::eNONE;
	}

	FCollisionFilterData FilterData = P2UFilterData(filterData);

	PxSweepHit& SweepHit = (PxSweepHit&)hit;
	const bool bIsOverlap = SweepHit.hadInitialOverlap();

	const ECollisionQueryHitType HitType = PostFilter(FilterData, bIsOverlap);
	return HitTypeToPxQueryHitType(HitType);
}

#endif