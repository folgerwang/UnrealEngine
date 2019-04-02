// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RHI.h"

#if RHI_RAYTRACING

#include "DeferredShadingRenderer.h"
#include "GlobalShader.h"
#include "SceneRenderTargets.h"
#include "RenderGraphBuilder.h"
#include "RHI/Public/PipelineStateCache.h"

class FRayTracingBarycentricsRGS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingBarycentricsRGS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingBarycentricsRGS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, Output)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}
};
IMPLEMENT_GLOBAL_SHADER(FRayTracingBarycentricsRGS, "/Engine/Private/RayTracing/RayTracingBarycentrics.usf", "RayTracingBarycentricsMainRGS", SF_RayGen);

// Example ray miss shader
class FRayTracingBarycentricsMS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRayTracingBarycentricsMS, Global);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
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
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	FRayTracingBarycentricsCHS() = default;
	FRayTracingBarycentricsCHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
};
IMPLEMENT_SHADER_TYPE(, FRayTracingBarycentricsCHS, TEXT("/Engine/Private/RayTracing/RayTracingBarycentrics.usf"), TEXT("RayTracingBarycentricsMainCHS"), SF_RayHitGroup);

void FDeferredShadingSceneRenderer::RenderRayTracingBarycentrics(FRHICommandListImmediate& RHICmdList, const FViewInfo& View)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	FRDGBuilder GraphBuilder(RHICmdList);

	TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(FeatureLevel);

	auto RayGenShader = ShaderMap->GetShader<FRayTracingBarycentricsRGS>();
	auto ClosestHitShader = ShaderMap->GetShader<FRayTracingBarycentricsCHS>();
	auto MissShader = ShaderMap->GetShader<FRayTracingBarycentricsMS>();

	FRayTracingPipelineStateInitializer Initializer;

	FRayTracingShaderRHIParamRef RayGenShaderTable[] = { RayGenShader->GetRayTracingShader() };
	Initializer.SetRayGenShaderTable(RayGenShaderTable);

	FRayTracingShaderRHIParamRef MissShaderTable[] = { MissShader->GetRayTracingShader() };
	Initializer.SetMissShaderTable(MissShaderTable);

	FRayTracingShaderRHIParamRef HitGroupTable[] = { ClosestHitShader->GetRayTracingShader() };
	Initializer.SetHitGroupTable(HitGroupTable);
	Initializer.bAllowHitGroupIndexing = false; // Use the same hit shader for all geometry in the scene by disabling SBT indexing.

	FRHIRayTracingPipelineState* Pipeline = PipelineStateCache::GetAndOrCreateRayTracingPipelineState(Initializer); // #dxr_todo: this should be done once at load-time and cached

	FRayTracingSceneRHIParamRef RayTracingSceneRHI = View.RayTracingScene.RayTracingSceneRHI;

	FRayTracingBarycentricsRGS::FParameters* RayGenParameters = GraphBuilder.AllocParameters<FRayTracingBarycentricsRGS::FParameters>();

	RayGenParameters->TLAS = RayTracingSceneRHI->GetShaderResourceView();
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
		RHICmdList.RayTraceDispatch(Pipeline, RayGenShader->GetRayTracingShader(), RayTracingSceneRHI, GlobalResources, ViewRect.Size().X, ViewRect.Size().Y);
	});

	GraphBuilder.Execute();
}
#endif