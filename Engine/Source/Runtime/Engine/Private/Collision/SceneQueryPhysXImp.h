// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_PHYSX && PHYSICS_INTERFACE_PHYSX

#include "PhysXPublic.h"

FORCEINLINE bool LowLevelRaycastImp(const PxVec3& Start, const PxVec3& Dir, float DeltaMag, const PxShape& Shape, const PxTransform ActorTM, PxHitFlags OutputFlags, PxRaycastHit& Hit)
{
	const PxGeometryHolder GeomHolder = Shape.getGeometry();
	const PxGeometry& Geom = GeomHolder.any();
	const PxTransform GeomTM = ActorTM * Shape.getLocalPose();

	return !!PxGeometryQuery::raycast(Start, Dir, Geom, GeomTM, DeltaMag, OutputFlags, 1, &Hit);
							
}

FORCEINLINE bool LowLevelSweepImp(const PxTransform& StartTM, const PxVec3& Dir, float DeltaMag, const PxGeometry& SweepGeom, const PxShape& Shape, const PxTransform ActorTM, PxHitFlags OutputFlags, PxSweepHit& Hit)
{
	const PxGeometryHolder GeomHolder = Shape.getGeometry();
	const PxGeometry& ShapeGeom = GeomHolder.any();
	const PxTransform ShapeGeomTM = ActorTM * Shape.getLocalPose();

	return PxGeometryQuery::sweep(Dir, DeltaMag, SweepGeom, StartTM, ShapeGeom, ShapeGeomTM, Hit, OutputFlags);
}

FORCEINLINE bool LowLevelOverlapImp(const PxTransform& GeomPose, const PxGeometry& OverlapGeom, const PxShape& Shape, const PxTransform ActorTM, PxOverlapHit& Overlap)
{
	const PxGeometryHolder GeomHolder = Shape.getGeometry();
	const PxGeometry& ShapeGeom = GeomHolder.any();
	const PxTransform ShapeGeomTM = ActorTM * Shape.getLocalPose();

	return PxGeometryQuery::overlap(OverlapGeom, GeomPose, ShapeGeom, ShapeGeomTM);
}
#endif // WITH_PHYSX 