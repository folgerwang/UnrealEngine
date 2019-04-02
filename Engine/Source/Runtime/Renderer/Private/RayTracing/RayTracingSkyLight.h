// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
RaytracingOptions.h declares ray tracing options for use in rendering
=============================================================================*/

#pragma once

#include "UniformBuffer.h"

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FSkyLightData, )
SHADER_PARAMETER(int, SamplesPerPixel)
SHADER_PARAMETER(int, SamplingStopLevel)
SHADER_PARAMETER(float, MaxRayDistance)
SHADER_PARAMETER(FVector, Color)
SHADER_PARAMETER(FIntVector, MipDimensions)
SHADER_PARAMETER(float, MaxNormalBias)
SHADER_PARAMETER_TEXTURE(TextureCube, Texture)
SHADER_PARAMETER_SAMPLER(SamplerState, TextureSampler)
SHADER_PARAMETER_SRV(Buffer<float>, MipTreePosX)
SHADER_PARAMETER_SRV(Buffer<float>, MipTreeNegX)
SHADER_PARAMETER_SRV(Buffer<float>, MipTreePosY)
SHADER_PARAMETER_SRV(Buffer<float>, MipTreeNegY)
SHADER_PARAMETER_SRV(Buffer<float>, MipTreePosZ)
SHADER_PARAMETER_SRV(Buffer<float>, MipTreeNegZ)
SHADER_PARAMETER_SRV(Buffer<float>, MipTreePdfPosX)
SHADER_PARAMETER_SRV(Buffer<float>, MipTreePdfNegX)
SHADER_PARAMETER_SRV(Buffer<float>, MipTreePdfPosY)
SHADER_PARAMETER_SRV(Buffer<float>, MipTreePdfNegY)
SHADER_PARAMETER_SRV(Buffer<float>, MipTreePdfPosZ)
SHADER_PARAMETER_SRV(Buffer<float>, MipTreePdfNegZ)
SHADER_PARAMETER_SRV(Buffer<float>, SolidAnglePdf)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

#if RHI_RAYTRACING

extern void SetupSkyLightParameters(const FScene& Scene, FSkyLightData* SkyLight);

#endif // RHI_RAYTRACING
