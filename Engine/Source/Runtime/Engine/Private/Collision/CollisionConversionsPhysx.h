// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
// Utilities to convert from PhysX result structs to Unreal ones

#pragma once 

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "EngineDefines.h"

#if PHYSICS_INTERFACE_PHYSX
#include "PhysicsPublic.h"
#include "PhysXIncludes.h"
#include "PhysXInterfaceWrapper.h"

FVector FindBoxOpposingNormal(const PxLocationHit& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal);
FVector FindHeightFieldOpposingNormal(const PxLocationHit& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal);
FVector FindConvexMeshOpposingNormal(const PxLocationHit& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal);
FVector FindTriMeshOpposingNormal(const PxLocationHit& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal);

void DrawOverlappingTris(const UWorld* World, const PxLocationHit& Hit, const PxGeometry& Geom, const FTransform& QueryTM);
void ComputeZeroDistanceImpactNormalAndPenetration(const UWorld* World, const FHitLocation& Hit, const PxGeometry& Geom, const FTransform& QueryTM, FHitResult& OutResult);


#endif //PHYSICS_INTERFACE_PHYSX