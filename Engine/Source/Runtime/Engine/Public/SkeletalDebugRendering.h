// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

class FPrimitiveDrawInterface;

namespace SkeletalDebugRendering
{

/**
 * Draw a wireframe bone from InStart to InEnd 
 * @param	PDI					Primitive draw interface to use
 * @param	InStart				The start location of the bone
 * @param	InEnd				The end location of the bone
 * @param	InColor				The color to draw the bone with
 * @param	InDepthPriority		The scene depth priority group to use
 * @param	SphereRadius		SphereRadius
 */
ENGINE_API void DrawWireBone(FPrimitiveDrawInterface* PDI, const FVector& InStart, const FVector& InEnd, const FLinearColor& InColor, ESceneDepthPriorityGroup InDepthPriority, const float SphereRadius = 1.f);

/**
 * Draw a set of axes to represent a transform
 * @param	PDI					Primitive draw interface to use
 * @param	InTransform			The transform to represent
 * @param	InDepthPriority		The scene depth priority group to use
 * @param	Thickness			Thickness of Axes
 * @param	AxisLength			AxisLength
 */
ENGINE_API void DrawAxes(FPrimitiveDrawInterface* PDI, const FTransform& InTransform, ESceneDepthPriorityGroup InDepthPriority, const float Thickness = 0.f, const float AxisLength = 4.f);

}