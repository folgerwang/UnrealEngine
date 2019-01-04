// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"

struct FColor;

class MATERIALBAKING_API FMaterialBakingHelpers
{
public:
	/** 
	 * Applies a box blur to magenta pixels found in given texture represented by InBMP using non-magenta pixels, this creates a smear across the magenta/filled pixels 
	 * @param	InOutPixels		The image to apply the smear to
	 * @param	ImageWidth		The width of the image in pixels
	 * @param	ImageHeight		The height of the image in pixels
	 * @param	MaxIterations	The max distance in pixels to smear the edges of the texture. When set to -1 this value is the max of the width and height of the buffer
	 */
	static void PerformUVBorderSmear(TArray<FColor>& InOutPixels, int32 ImageWidth, int32 ImageHeight, int32 MaxIterations = -1);
};