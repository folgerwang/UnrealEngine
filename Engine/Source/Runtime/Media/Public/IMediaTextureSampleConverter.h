// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIResources.h"

/**
 * Interface class to implement custom sample conversion
 */
class IMediaTextureSampleConverter
{
public:
	virtual void Convert(FTexture2DRHIRef InDstTexture) = 0;
};
