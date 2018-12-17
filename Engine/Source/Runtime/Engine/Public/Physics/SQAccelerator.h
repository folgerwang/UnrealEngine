// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/BoxSphereBounds.h"

#if PHYSICS_INTERFACE_PHYSX
#include "PhysXInterfaceWrapper.h"	//todo remove physx specific include once refactor is done
#elif PHYSICS_INTERFACE_LLIMMEDIATE
#include "Physics/Experimental/LLImmediateInterfaceWrapper.h"
#elif WITH_CHAOS
#include "Physics/Experimental/PhysInterface_Chaos.h"
#endif

class FSQAccelerator;
struct FCollisionFilterData;
struct FCollisionQueryParams;
struct FCollisionQueryParams;
class FCollisionQueryFilterCallback;

class FSQAcceleratorEntry

{
public:
	bool Intersect(const FBoxSphereBounds& Other) const
	{
		//return FBoxSphereBounds::BoxesIntersect(Other, Bounds);
		return true;
	}

	void* GetPayload() const
	{
		return Payload;
	}
private:
	FSQAcceleratorEntry(void* InPayload)
		: Payload(InPayload) {}

	void* Payload;
	friend FSQAccelerator;
};

struct FSQNode
{
	TArray<FSQAcceleratorEntry*> Entries;
};

class ENGINE_API ISQAccelerator
{
public:
	virtual ~ISQAccelerator() {};
	virtual void Raycast(const FVector& Start, const FVector& Dir, FPhysicsHitCallback<FHitRaycast>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& QueryFilter, FCollisionQueryFilterCallback& QueryCallback) const = 0;
	virtual void Sweep(const FPhysicsGeometry& QueryGeom, const FTransform& StartTM, const FVector& Dir, FPhysicsHitCallback<FHitSweep>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& QueryFilter, FCollisionQueryFilterCallback& QueryCallback) const = 0;
	virtual void Overlap(const FPhysicsGeometry& QueryGeom, const FTransform& GeomPose, FPhysicsHitCallback<FHitOverlap>& HitBuffer, FQueryFlags QueryFlags, const FCollisionFilterData& QueryFilter, FCollisionQueryFilterCallback& QueryCallback) const = 0;
};

class ENGINE_API FSQAccelerator : public ISQAccelerator
{
public:
	FSQAcceleratorEntry* AddEntry(void* Payload);
	void RemoveEntry(FSQAcceleratorEntry* Entry);
	void GetNodes(TArray<const FSQNode*>& NodesFound) const;

	virtual void Raycast(const FVector& Start, const FVector& Dir, FPhysicsHitCallback<FHitRaycast>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& QueryFilter, FCollisionQueryFilterCallback& QueryCallback) const override;
	virtual void Sweep(const FPhysicsGeometry& QueryGeom, const FTransform& StartTM, const FVector& Dir, FPhysicsHitCallback<FHitSweep>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& QueryFilter, FCollisionQueryFilterCallback& QueryCallback) const override;
	virtual void Overlap(const FPhysicsGeometry& QueryGeom, const FTransform& GeomPose, FPhysicsHitCallback<FHitOverlap>& HitBuffer, FQueryFlags QueryFlags, const FCollisionFilterData& QueryFilter, FCollisionQueryFilterCallback& QueryCallback) const override;

	virtual ~FSQAccelerator() override;
private:
	TArray<FSQNode*> Nodes;
};

class ENGINE_API FSQAcceleratorUnion : public ISQAccelerator
{
public:

	virtual void Raycast(const FVector& Start, const FVector& Dir, FPhysicsHitCallback<FHitRaycast>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& QueryFilter, FCollisionQueryFilterCallback& QueryCallback) const override;
	virtual void Sweep(const FPhysicsGeometry& QueryGeom, const FTransform& StartTM, const FVector& Dir, FPhysicsHitCallback<FHitSweep>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& QueryFilter, FCollisionQueryFilterCallback& QueryCallback) const override;
	virtual void Overlap(const FPhysicsGeometry& QueryGeom, const FTransform& GeomPose, FPhysicsHitCallback<FHitOverlap>& HitBuffer, FQueryFlags QueryFlags, const FCollisionFilterData& QueryFilter, FCollisionQueryFilterCallback& QueryCallback) const override;

	void AddSQAccelerator(ISQAccelerator* InAccelerator);
	void RemoveSQAccelerator(ISQAccelerator* AcceleratorToRemove);

private:
	TArray<ISQAccelerator*> Accelerators;
};