// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RaytracingOptions.h declares ray tracing options for use in rendering
=============================================================================*/

#if RHI_RAYTRACING

extern bool IsRayTracingSkyLightSelected();
extern bool IsRayTracingRectLightSelected();

extern bool ShouldRenderRayTracingStaticOrStationaryRectLight(const FLightSceneInfo& LightSceneInfo);
extern bool ShouldRenderRayTracingDynamicRectLight(const FLightSceneInfo& LightSceneInfo);

#endif
