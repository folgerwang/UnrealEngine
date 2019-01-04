// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/**
 * Coord system utilities
 *
 * Translates between Unreal and Recast coords.
 * Unreal: x, y, z
 * Recast: -x, z, -y
 */

#pragma once

#include "CoreMinimal.h"

extern NAVIGATIONSYSTEM_API FVector Unreal2RecastPoint(const float* UnrealPoint);
extern NAVIGATIONSYSTEM_API FVector Unreal2RecastPoint(const FVector& UnrealPoint);
extern NAVIGATIONSYSTEM_API FBox Unreal2RecastBox(const FBox& UnrealBox);
extern NAVIGATIONSYSTEM_API FMatrix Unreal2RecastMatrix();

extern NAVIGATIONSYSTEM_API FVector Recast2UnrealPoint(const float* RecastPoint);
extern NAVIGATIONSYSTEM_API FVector Recast2UnrealPoint(const FVector& RecastPoint);
extern NAVIGATIONSYSTEM_API FBox Recast2UnrealBox(const float* RecastMin, const float* RecastMax);
extern NAVIGATIONSYSTEM_API FBox Recast2UnrealBox(const FBox& RecastBox);
extern NAVIGATIONSYSTEM_API FMatrix Recast2UnrealMatrix();
