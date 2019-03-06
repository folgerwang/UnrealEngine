// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RHI.h"

#if RHI_RAYTRACING

#include "DeferredShadingRenderer.h"
#include "GlobalShader.h"
#include "SceneRenderTargets.h"
#include "RenderGraphBuilder.h"
#include "SceneUtils.h"
#include "RayTracingDebugDefinitions.h"

#define LOCTEXT_NAMESPACE "RayTracingDebugVisualizationMenuCommands"

DECLARE_GPU_STAT(RayTracingDebug);

class FRayTracingDebugRGS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingDebugRGS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingDebugRGS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, VisualizationMode)
		SHADER_PARAMETER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, Output)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}
};
IMPLEMENT_GLOBAL_SHADER(FRayTracingDebugRGS, "/Engine/Private/RayTracing/RayTracingDebug.usf", "RayTracingDebugMainRGS", SF_RayGen);

class FRayTracingDebugMS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRayTracingDebugMS, Global);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	FRayTracingDebugMS() = default;
	FRayTracingDebugMS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
};
IMPLEMENT_SHADER_TYPE(, FRayTracingDebugMS, TEXT("/Engine/Private/RayTracing/RayTracingDebug.usf"), TEXT("RayTracingDebugMainMS"), SF_RayMiss);


class FRayTracingDebugCHS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRayTracingDebugCHS, Global);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	FRayTracingDebugCHS() = default;
	FRayTracingDebugCHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
};

// Dummy shader permutations to test hit group API
IMPLEMENT_SHADER_TYPE(, FRayTracingDebugCHS, TEXT("/Engine/Private/RayTracing/RayTracingDebug.usf"), TEXT("RayTracingDebugMainCHS"), SF_RayHitGroup);

void FDeferredShadingSceneRenderer::PrepareRayTracingDebug(const FViewInfo& View, TArray<FRayTracingShaderRHIParamRef>& OutRayGenShaders)
{
	// Declare all RayGen shaders that require material closest hit shaders to be bound

	auto RayGenShader = View.ShaderMap->GetShader<FRayTracingDebugRGS>();
	OutRayGenShaders.Add(RayGenShader->GetRayTracingShader());
}

void FDeferredShadingSceneRenderer::RenderRayTracingDebug(FRHICommandListImmediate& RHICmdList, const FViewInfo& View)
{
	static TMap<FName, uint32> RayTracingDebugVisualizationModes;
	if (RayTracingDebugVisualizationModes.Num() == 0)
	{
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Radiance", "Radiance").ToString()),											RAY_TRACING_DEBUG_VIZ_RADIANCE);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("World Normal", "World Normal").ToString()),									RAY_TRACING_DEBUG_VIZ_WORLD_NORMAL);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("BaseColor", "BaseColor").ToString()),											RAY_TRACING_DEBUG_VIZ_BASE_COLOR);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("DiffuseColor", "DiffuseColor").ToString()),									RAY_TRACING_DEBUG_VIZ_DIFFUSE_COLOR);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("SpecularColor", "SpecularColor").ToString()),									RAY_TRACING_DEBUG_VIZ_SPECULAR_COLOR);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Opacity", "Opacity").ToString()),												RAY_TRACING_DEBUG_VIZ_OPACITY);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Metallic", "Metallic").ToString()),											RAY_TRACING_DEBUG_VIZ_METALLIC);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Specular", "Specular").ToString()),											RAY_TRACING_DEBUG_VIZ_SPECULAR);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Roughness", "Roughness").ToString()),											RAY_TRACING_DEBUG_VIZ_ROUGHNESS);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Ior", "Ior").ToString()),														RAY_TRACING_DEBUG_VIZ_IOR);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("ShadingModelID", "ShadingModelID").ToString()),								RAY_TRACING_DEBUG_VIZ_SHADING_MODEL);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("BlendingMode", "BlendingMode").ToString()),									RAY_TRACING_DEBUG_VIZ_BLENDING_MODE);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("PrimitiveLightingChannelMask", "PrimitiveLightingChannelMask").ToString()),	RAY_TRACING_DEBUG_VIZ_LIGHTING_CHANNEL_MASK);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("CustomData", "CustomData").ToString()),										RAY_TRACING_DEBUG_VIZ_CUSTOM_DATA);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("GBufferAO", "GBufferAO").ToString()),											RAY_TRACING_DEBUG_VIZ_GBUFFER_AO);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("IndirectIrradiance", "IndirectIrradiance").ToString()),						RAY_TRACING_DEBUG_VIZ_INDIRECT_IRRADIANCE);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("World Position", "World Position").ToString()),								RAY_TRACING_DEBUG_VIZ_WORLD_POSITION);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("HitKind", "HitKind").ToString()),												RAY_TRACING_DEBUG_VIZ_HITKIND);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Barycentrics", "Barycentrics").ToString()),									RAY_TRACING_DEBUG_VIZ_BARYCENTRICS);
	}

	uint32 DebugVisualizationMode = RayTracingDebugVisualizationModes.FindRef(View.CurrentRayTracingDebugVisualizationMode);

	if (DebugVisualizationMode == RAY_TRACING_DEBUG_VIZ_BARYCENTRICS)
	{
		return RenderRayTracingBarycentrics(RHICmdList, View);
	}

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	FRDGBuilder GraphBuilder(RHICmdList);

	TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(FeatureLevel);

	auto RayGenShader = ShaderMap->GetShader<FRayTracingDebugRGS>();
	auto ClosestHitShader = ShaderMap->GetShader<FRayTracingDebugCHS>();
	auto MissShader = ShaderMap->GetShader<FRayTracingDebugMS>();

	FRHIRayTracingPipelineState* Pipeline = View.RayTracingMaterialPipeline;

	FRayTracingSceneRHIParamRef RayTracingSceneRHI = View.RayTracingScene.RayTracingSceneRHI;

	FRayTracingDebugRGS::FParameters* RayGenParameters = GraphBuilder.AllocParameters<FRayTracingDebugRGS::FParameters>();

	RayGenParameters->VisualizationMode = DebugVisualizationMode;
	RayGenParameters->TLAS = RayTracingSceneRHI->GetShaderResourceView();
	RayGenParameters->ViewUniformBuffer = View.ViewUniformBuffer;
	RayGenParameters->Output = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(SceneContext.GetSceneColor()));

	FIntRect ViewRect = View.ViewRect;

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("RayTracingDebug"),
		RayGenParameters,
		ERenderGraphPassFlags::Compute,
		[this, RayGenParameters, RayGenShader, &SceneContext, RayTracingSceneRHI, Pipeline, ViewRect](FRHICommandList& RHICmdList)
	{
		SCOPED_GPU_STAT(RHICmdList, RayTracingDebug);

		FRayTracingShaderBindingsWriter GlobalResources;
		SetShaderParameters(GlobalResources, RayGenShader, *RayGenParameters);

		RHICmdList.RayTraceDispatch(Pipeline, RayGenShader->GetRayTracingShader(), RayTracingSceneRHI, GlobalResources, ViewRect.Size().X, ViewRect.Size().Y);
	});

	GraphBuilder.Execute();
}

#undef LOCTEXT_NAMESPACE
#endif