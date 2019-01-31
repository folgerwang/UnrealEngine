// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "RenderResource.h"
#include "ShaderParameters.h"
#include "UniformBuffer.h"

class FLightCacheInterface;

/** The maximum value between NUM_LQ_LIGHTMAP_COEF and NUM_HQ_LIGHTMAP_COEF. */
static const int32 MAX_NUM_LIGHTMAP_COEF = 2;

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FPrecomputedLightingUniformParameters, ENGINE_API)
SHADER_PARAMETER(FVector4, StaticShadowMapMasks) // TDistanceFieldShadowsAndLightMapPolicy
SHADER_PARAMETER(FVector4, InvUniformPenumbraSizes) // TDistanceFieldShadowsAndLightMapPolicy
SHADER_PARAMETER(FVector4, LightMapCoordinateScaleBias) // TLightMapPolicy
SHADER_PARAMETER(FVector4, ShadowMapCoordinateScaleBias) // TDistanceFieldShadowsAndLightMapPolicy
SHADER_PARAMETER_ARRAY_EX(FVector4, LightMapScale, [MAX_NUM_LIGHTMAP_COEF], EShaderPrecisionModifier::Half) // TLightMapPolicy
SHADER_PARAMETER_ARRAY_EX(FVector4, LightMapAdd, [MAX_NUM_LIGHTMAP_COEF], EShaderPrecisionModifier::Half) // TLightMapPolicy
END_GLOBAL_SHADER_PARAMETER_STRUCT()

ENGINE_API void GetDefaultPrecomputedLightingParameters(FPrecomputedLightingUniformParameters& Parameters);

ENGINE_API void GetPrecomputedLightingParameters(
	ERHIFeatureLevel::Type FeatureLevel,
	FPrecomputedLightingUniformParameters& Parameters,
	const FLightCacheInterface* LCI);

struct FLightmapSceneShaderData
{
	// Must match usf
	enum { LightmapDataStrideInFloat4s = 8 };

	FVector4 Data[LightmapDataStrideInFloat4s];

	FLightmapSceneShaderData()
	{
		FPrecomputedLightingUniformParameters ShaderParameters;
		GetDefaultPrecomputedLightingParameters(ShaderParameters);
		Setup(ShaderParameters);
	}

	explicit FLightmapSceneShaderData(const FPrecomputedLightingUniformParameters& ShaderParameters)
	{
		Setup(ShaderParameters);
	}

	ENGINE_API FLightmapSceneShaderData(const class FLightCacheInterface* LCI, ERHIFeatureLevel::Type FeatureLevel);

	ENGINE_API void Setup(const FPrecomputedLightingUniformParameters& ShaderParameters);
};
