// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RayGenShaderUtils.h: Utilities for ray generation shaders shaders.
=============================================================================*/

#pragma once

#include "RenderResource.h"
#include "RenderGraphUtils.h"
#include "PipelineStateCache.h"


/** All utils for ray generation shaders. */
struct RENDERCORE_API FRayGenShaderUtils
{
	/** Dispatch a ray generation shader to render graph builder with its parameters. */
	template<typename TShaderClass>
	static inline void AddRayTraceDispatchPass(
		FRDGBuilder& GraphBuilder,
		FRDGEventName&& PassName,
		const TShaderClass* RayGenerationShader,
		typename TShaderClass::FParameters* Parameters,
		FIntPoint Resolution)
	{
		ClearUnusedGraphResources(RayGenerationShader, Parameters);

		GraphBuilder.AddPass(
			Forward<FRDGEventName>(PassName),
			Parameters,
			ERenderGraphPassFlags::Compute,
			[RayGenerationShader, Parameters, Resolution](FRHICommandList& RHICmdList)
		{
			FRayTracingShaderBindingsWriter GlobalResources;
			SetShaderParameters(GlobalResources, RayGenerationShader, *Parameters);

			FRayTracingPipelineStateInitializer Initializer;
			FRayTracingShaderRHIParamRef RayGenShaderTable[] = { RayGenerationShader->GetRayTracingShader() };
			Initializer.SetRayGenShaderTable(RayGenShaderTable);

			FRHIRayTracingPipelineState* Pipeline = PipelineStateCache::GetAndOrCreateRayTracingPipelineState(Initializer);
			RHICmdList.RayTraceDispatch(Pipeline, RayGenerationShader->GetRayTracingShader(), GlobalResources, Resolution.X, Resolution.Y);
		});
	}
};
