// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if PHYSICS_INTERFACE_LLIMMEDIATE

#include "Physics/PhysicsInterfaceDeclares.h"

inline bool LowLevelRaycastImp(const FVector& Start, const FVector& Dir, float DeltaMag, const FPhysTypeDummy& Shape, const FTransform ActorTM, EHitFlags OutputFlags, FPhysTypeDummy& Hit)
{
	return false;			
}

inline bool LowLevelSweepImp(const FTransform& StartTM, const FVector& Dir, float DeltaMag, const FPhysicsGeometry& SweepGeom, const FPhysTypeDummy& Shape, const FTransform ActorTM, EHitFlags OutputFlags, FPhysTypeDummy& Hit)
{
	return false;
}

inline bool LowLevelOverlapImp(const FTransform& GeomPose, const FPhysicsGeometry& OverlapGeom, const FPhysTypeDummy& Shape, const FTransform ActorTM, FPhysTypeDummy& Overlap)
{
	return false;
}
#endif // WITH_PHYSX 