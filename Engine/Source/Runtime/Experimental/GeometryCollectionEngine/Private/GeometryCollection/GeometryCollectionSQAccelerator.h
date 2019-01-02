// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Physics/SQAccelerator.h"

#if INCLUDE_CHAOS

class UGeometryCollectionComponent;

class FGeometryCollectionSQAccelerator : public ISQAccelerator
{
public:
	virtual void Raycast(const FVector& Start, const FVector& Dir, FPhysicsHitCallback<FHitRaycast>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& QueryFilter, FCollisionQueryFilterCallback& QueryCallback) const override;
	virtual void Sweep(const FPhysicsGeometry& QueryGeom, const FTransform& StartTM, const FVector& Dir, FPhysicsHitCallback<FHitSweep>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& QueryFilter, FCollisionQueryFilterCallback& QueryCallback) const override;
	virtual void Overlap(const FPhysicsGeometry& QueryGeom, const FTransform& GeomPose, FPhysicsHitCallback<FHitOverlap>& HitBuffer, FQueryFlags QueryFlags, const FCollisionFilterData& QueryFilter, FCollisionQueryFilterCallback& QueryCallback) const override;
	virtual ~FGeometryCollectionSQAccelerator() {}

	void AddComponent(UGeometryCollectionComponent* Component);
	void RemoveComponent(UGeometryCollectionComponent* Component);

private:
	TSet<UGeometryCollectionComponent*> Components;
};
#endif