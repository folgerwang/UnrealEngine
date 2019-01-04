// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "EngineDefines.h"

#if PHYSICS_INTERFACE_LLIMMEDIATE
#include "PhysicsPublic.h"
#include "PhysXIncludes.h"
#include "Physics/Experimental/LLImmediateInterfaceWrapper.h"

FVector FindBoxOpposingNormal(const FPhysTypeDummy& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal);
FVector FindHeightFieldOpposingNormal(const FPhysTypeDummy& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal);
FVector FindConvexMeshOpposingNormal(const FPhysTypeDummy& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal);
FVector FindTriMeshOpposingNormal(const FPhysTypeDummy& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal);

void DrawOverlappingTris(const UWorld* World, const FPhysTypeDummy& Hit, const PxGeometry& Geom, const FTransform& QueryTM);
void ComputeZeroDistanceImpactNormalAndPenetration(const UWorld* World, const FHitLocation& Hit, const PxGeometry& Geom, const FTransform& QueryTM, FHitResult& OutResult);

#endif //PHYSICS_INTERFACE_PHYSX