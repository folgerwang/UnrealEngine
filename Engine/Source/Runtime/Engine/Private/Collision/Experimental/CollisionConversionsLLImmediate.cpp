// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if PHYSICS_INTERFACE_LLIMMEDIATE

#include "CollisionConversionsLLImmediate.h"

FVector FindBoxOpposingNormal(const FPhysTypeDummy& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal)
{
	return FVector(0.0f, 0.0f, 1.0f);
}

FVector FindHeightFieldOpposingNormal(const FPhysTypeDummy& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal)
{
	return FVector(0.0f, 0.0f, 1.0f);
}

FVector FindConvexMeshOpposingNormal(const FPhysTypeDummy& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal)
{
	return FVector(0.0f, 0.0f, 1.0f);
}

FVector FindTriMeshOpposingNormal(const FPhysTypeDummy& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal)
{
	return FVector(0.0f, 0.0f, 1.0f);
}

void DrawOverlappingTris(const UWorld* World, const FPhysTypeDummy& Hit, const PxGeometry& Geom, const FTransform& QueryTM)
{

}

void ComputeZeroDistanceImpactNormalAndPenetration(const UWorld* World, const FPhysTypeDummy& Hit, const PxGeometry& Geom, const FTransform& QueryTM, FHitResult& OutResult)
{

}

#endif