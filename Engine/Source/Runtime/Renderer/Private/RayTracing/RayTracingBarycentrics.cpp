// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "RHI.h"

#if RHI_RAYTRACING

#include "DeferredShadingRenderer.h"
#include "GlobalShader.h"
#include "SceneRenderTargets.h"
#include "RenderGraphBuilder.h"

class FRayTracingBarycentricsRG : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingBarycentricsRG)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingBarycentricsRG, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, Output)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && IsRayTracingSupportedForThisProject();
	}
};
IMPLEMENT_GLOBAL_SHADER(FRayTracingBarycentricsRG, "/Engine/Private/RayTracing/RayTracingBarycentrics.usf", "RayTracingBarycentricsMainRG", SF_RayGen);

// Example ray miss shader
class FRayTracingBarycentricsMS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRayTracingBarycentricsMS, Global);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && IsRayTracingSupportedForThisProject();
	}

	FRayTracingBarycentricsMS() = default;
	FRayTracingBarycentricsMS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
};
IMPLEMENT_SHADER_TYPE(, FRayTracingBarycentricsMS, TEXT("/Engine/Private/RayTracing/RayTracingBarycentrics.usf"), TEXT("RayTracingBarycentricsMainMS"), SF_RayMiss);


// Example closest hit shader
class FRayTracingBarycentricsCHS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRayTracingBarycentricsCHS, Global);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && IsRayTracingSupportedForThisProject();
	}

	FRayTracingBarycentricsCHS() = default;
	FRayTracingBarycentricsCHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
};
IMPLEMENT_SHADER_TYPE(, FRayTracingBarycentricsCHS, TEXT("/Engine/Private/RayTracing/RayTracingBarycentrics.usf"), TEXT("RayTracingBarycentricsMainCHS"), SF_RayHitGroup);

void FDeferredShadingSceneRenderer::RenderRayTracedBarycentrics(FRHICommandListImmediate& RHICmdList, const FViewInfo& View)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	FRDGBuilder GraphBuilder(RHICmdList);

	TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(FeatureLevel);

	auto RayGenShader = ShaderMap->GetShader<FRayTracingBarycentricsRG>();
	auto ClosestHitShader = ShaderMap->GetShader<FRayTracingBarycentricsCHS>();
	auto MissShader = ShaderMap->GetShader<FRayTracingBarycentricsMS>();

	FRayTracingPipelineStateInitializer Initializer;
	Initializer.RayGenShaderRHI = RayGenShader->GetRayTracingShader();
	Initializer.DefaultClosestHitShaderRHI = ClosestHitShader->GetRayTracingShader();
	Initializer.MissShaderRHI = MissShader->GetRayTracingShader();

	FRHIRayTracingPipelineState* Pipeline = PipelineStateCache::GetAndOrCreateRayTracingPipelineState(Initializer); // #dxr_todo: this should be done once at load-time and cached

	FRayTracingSceneRHIParamRef RayTracingSceneRHI = View.PerViewRayTracingScene.RayTracingSceneRHI;

	FRayTracingBarycentricsRG::FParameters* RayGenParameters = GraphBuilder.AllocParameters<FRayTracingBarycentricsRG::FParameters>();

	RayGenParameters->TLAS = RHIGetAccelerationStructureShaderResourceView(RayTracingSceneRHI);
	RayGenParameters->ViewUniformBuffer = View.ViewUniformBuffer;
	RayGenParameters->Output = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(SceneContext.GetSceneColor()));

	FIntRect ViewRect = View.ViewRect;

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("Barycentrics"),
		RayGenParameters,
		ERenderGraphPassFlags::Compute,
		[this, RayGenParameters, RayGenShader, &SceneContext, RayTracingSceneRHI, Pipeline, ViewRect](FRHICommandList& RHICmdList)
	{
		FRayTracingShaderBindingsWriter GlobalResources;
		SetShaderParameters(GlobalResources, RayGenShader, *RayGenParameters);

		// Dispatch rays using default shader binding table
		RHICmdList.RayTraceDispatch(Pipeline, GlobalResources, ViewRect.Size().X, ViewRect.Size().Y);
	});

	GraphBuilder.Execute();
}

#endif RHI_RAYTRACING
