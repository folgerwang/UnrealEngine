// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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

BEGIN_UNIFORM_BUFFER_STRUCT(FReflectionUniformParameters,)
	UNIFORM_MEMBER(FVector4, SkyLightParameters)
	UNIFORM_MEMBER(float, SkyLightCubemapBrightness)
	UNIFORM_MEMBER_TEXTURE(TextureCube, SkyLightCubemap)
	UNIFORM_MEMBER_SAMPLER(SamplerState, SkyLightCubemapSampler)
	UNIFORM_MEMBER_TEXTURE(TextureCube, SkyLightBlendDestinationCubemap)
	UNIFORM_MEMBER_SAMPLER(SamplerState, SkyLightBlendDestinationCubemapSampler)
	UNIFORM_MEMBER_TEXTURE(TextureCubeArray, ReflectionCubemap)
	UNIFORM_MEMBER_SAMPLER(SamplerState, ReflectionCubemapSampler)
END_UNIFORM_BUFFER_STRUCT(FReflectionUniformParameters)

extern void SetupReflectionUniformParameters(const class FViewInfo& View, FReflectionUniformParameters& OutParameters);