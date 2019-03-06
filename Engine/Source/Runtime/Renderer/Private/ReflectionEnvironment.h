// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Reflection Environment common declarations
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHIDefinitions.h"
#include "ShaderParameters.h"
#include "UniformBuffer.h"

extern bool IsReflectionEnvironmentAvailable(ERHIFeatureLevel::Type InFeatureLevel);
extern bool IsReflectionCaptureAvailable();

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FReflectionUniformParameters,)
	SHADER_PARAMETER(FVector4, SkyLightParameters)
	SHADER_PARAMETER(float, SkyLightCubemapBrightness)
	SHADER_PARAMETER_TEXTURE(TextureCube, SkyLightCubemap)
	SHADER_PARAMETER_SAMPLER(SamplerState, SkyLightCubemapSampler)
	SHADER_PARAMETER_TEXTURE(TextureCube, SkyLightBlendDestinationCubemap)
	SHADER_PARAMETER_SAMPLER(SamplerState, SkyLightBlendDestinationCubemapSampler)
	SHADER_PARAMETER_TEXTURE(TextureCubeArray, ReflectionCubemap)
	SHADER_PARAMETER_SAMPLER(SamplerState, ReflectionCubemapSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, PreIntegratedGF)
	SHADER_PARAMETER_SAMPLER(SamplerState, PreIntegratedGFSampler)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

extern void SetupReflectionUniformParameters(const class FViewInfo& View, FReflectionUniformParameters& OutParameters);
TUniformBufferRef<FReflectionUniformParameters> CreateReflectionUniformBuffer(const class FViewInfo& View, EUniformBufferUsage Usage);