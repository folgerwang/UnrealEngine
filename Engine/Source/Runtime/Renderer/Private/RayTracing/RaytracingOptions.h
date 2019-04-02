// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RaytracingOptions.h declares ray tracing options for use in rendering
=============================================================================*/

#pragma once

#include "RHIDefinitions.h"


extern bool ShouldRenderRayTracingSkyLight(const FSkyLightSceneProxy* SkyLightSceneProxy);

#if RHI_RAYTRACING
extern bool ShouldRenderRayTracingAmbientOcclusion();
extern bool ShouldRenderRayTracingGlobalIllumination(const TArray<FViewInfo>& Views);
extern bool ShouldRenderRayTracingStochasticRectLight(const FLightSceneInfo& LightSceneInfo);

extern float GetRaytracingMaxNormalBias();

#else

FORCEINLINE bool ShouldRenderRayTracingAmbientOcclusion()
{
	return false;
}

FORCEINLINE bool ShouldRenderRayTracingGlobalIllumination(const TArray<FViewInfo>& Views)
{
	return false;
}

FORCEINLINE bool ShouldRenderRayTracingStochasticRectLight(const FLightSceneInfo& LightSceneInfo)
{
	return false;
}


#endif
