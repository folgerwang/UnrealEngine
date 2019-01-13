// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHI.h"

class FWmfMediaHardwareVideoDecodingTextureSample;

class FWmfMediaHardwareVideoDecodingParameters
{
public:
	static bool ConvertTextureFormat_RenderThread(FWmfMediaHardwareVideoDecodingTextureSample* InSample, FTexture2DRHIRef InDstTexture);
};
