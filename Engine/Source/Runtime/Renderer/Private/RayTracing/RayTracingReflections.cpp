// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RendererPrivate.h"
#include "GlobalShader.h"
#include "DeferredShadingRenderer.h"

#if RHI_RAYTRACING

#include "ClearQuad.h"
#include "SceneRendering.h"
#include "SceneRenderTargets.h"
#include "RenderTargetPool.h"
#include "RHIResources.h"
#include "UniformBuffer.h"
#include "VisualizeTexture.h"
#include "LightRendering.h"
#include "SystemTextures.h"
#include "RayTracing/RaytracingOptions.h"
#include "RayTracing/RayTracingDeferredMaterials.h"
#include "RayTracing/RayTracingIESLightProfiles.h"
#include "RayTracing/RayTracingLighting.h"
#include "RenderGraph.h"
#include "RayTracing/RayTracingLighting.h"

static float GRayTracingReflectionsMaxRoughness = -1;
static FAutoConsoleVariableRef CVarRayTracingReflectionsMaxRoughness(
	TEXT("r.RayTracing.Reflections.MaxRoughness"),
	GRayTracingReflectionsMaxRoughness,
	TEXT("Sets the maximum roughness until which ray tracing reflections will be visible (default = -1 (max roughness driven by postprocessing volume))")
);

static int32 GRayTracingReflectionsMaxBounces = -1;
static FAutoConsoleVariableRef CVarRayTracingReflectionsMaxBounces(
	TEXT("r.RayTracing.Reflections.MaxBounces"),
	GRayTracingReflectionsMaxBounces,
	TEXT("Sets the maximum number of ray tracing reflection bounces (default = -1 (max bounces driven by postprocessing volume))")
);

static int32 GRayTracingReflectionsEmissiveAndIndirectLighting = 1;
static FAutoConsoleVariableRef CVarRayTracingReflectionsEmissiveAndIndirectLighting(
	TEXT("r.RayTracing.Reflections.EmissiveAndIndirectLighting"),
	GRayTracingReflectionsEmissiveAndIndirectLighting,
	TEXT("Enables ray tracing reflections emissive and indirect lighting (default = 1)")
);

static int32 GRayTracingReflectionsDirectLighting = 1;
static FAutoConsoleVariableRef CVarRayTracingReflectionsDirectLighting(
	TEXT("r.RayTracing.Reflections.DirectLighting"),
	GRayTracingReflectionsDirectLighting,
	TEXT("Enables ray tracing reflections direct lighting (default = 1)")
);

static int32 GRayTracingReflectionsShadows = -1;
static FAutoConsoleVariableRef CVarRayTracingReflectionsShadows(
	TEXT("r.RayTracing.Reflections.Shadows"),
	GRayTracingReflectionsShadows,
	TEXT("Enables shadows in ray tracing reflections)")
	TEXT(" -1: Shadows driven by postprocessing volume (default)")
	TEXT(" 0: Shadows disabled ")
	TEXT(" 1: Hard shadows")
	TEXT(" 2: Soft area shadows")
);

static float GRayTracingReflectionsMinRayDistance = -1;
static FAutoConsoleVariableRef CVarRayTracingReflectionsMinRayDistance(
	TEXT("r.RayTracing.Reflections.MinRayDistance"),
	GRayTracingReflectionsMinRayDistance,
	TEXT("Sets the minimum ray distance for ray traced reflection rays. Actual reflection ray length is computed as Lerp(MaxRayDistance, MinRayDistance, Roughness), i.e. reflection rays become shorter when traced from rougher surfaces. (default = -1 (infinite rays))")
);

static float GRayTracingReflectionsMaxRayDistance = -1;
static FAutoConsoleVariableRef CVarRayTracingReflectionsMaxRayDistance(
	TEXT("r.RayTracing.Reflections.MaxRayDistance"),
	GRayTracingReflectionsMaxRayDistance,
	TEXT("Sets the maximum ray distance for ray traced reflection rays. When ray shortening is used, skybox will not be sampled in RT reflection pass and will be composited later, together with local reflection captures. Negative values turn off this optimization. (default = -1 (infinite rays))")
);

static TAutoConsoleVariable<int32> CVarRayTracingReflectionsSortMaterials(
	TEXT("r.RayTracing.Reflections.SortMaterials"),
	0,
	TEXT("Sets whether refected materials will be sorted before shading\n")
	TEXT("0: Disabled (Default)\n ")
	TEXT("1: Enabled, using Trace->Sort->Trace\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRayTracingReflectionsSortTileSize(
	TEXT("r.RayTracing.Reflections.SortTileSize"),
	64,
	TEXT("Size of pixel tiles for sorted reflections\n")
	TEXT("  Default 64\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRayTracingReflectionsSortSize(
	TEXT("r.RayTracing.Reflections.SortSize"),
	5,
	TEXT("Size of horizon for material ID sort\n")
	TEXT("0: Disabled\n")
	TEXT("1: 256 Elements\n")
	TEXT("2: 512 Elements\n")
	TEXT("3: 1024 Elements\n")
	TEXT("4: 2048 Elements\n")
	TEXT("5: 4096 Elements (Default)\n"),
	ECVF_RenderThreadSafe);

static const int32 GReflectionLightCountMaximum = 64;

class FRayTracingReflectionsRGS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingReflectionsRGS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingReflectionsRGS, FGlobalShader)

	class FDenoiserOutput : SHADER_PERMUTATION_BOOL("DIM_DENOISER_OUTPUT");

	class FDeferredMaterialMode : SHADER_PERMUTATION_ENUM_CLASS("DIM_DEFERRED_MATERIAL_MODE", EDeferredMaterialMode);

	using FPermutationDomain = TShaderPermutationDomain<FDenoiserOutput, FDeferredMaterialMode>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int32, SamplesPerPixel)
		SHADER_PARAMETER(int32, MaxBounces)
		SHADER_PARAMETER(int32, HeightFog)
		SHADER_PARAMETER(int32, ShouldDoDirectLighting)
		SHADER_PARAMETER(int32, ReflectedShadowsType)
		SHADER_PARAMETER(int32, ShouldDoEmissiveAndIndirectLighting)
		SHADER_PARAMETER(int32, UpscaleFactor)
		SHADER_PARAMETER(int32, SortTileSize)
		SHADER_PARAMETER(FIntPoint, RayTracingResolution)
		SHADER_PARAMETER(FIntPoint, TileAlignedResolution)
		SHADER_PARAMETER(float, ReflectionMinRayDistance)
		SHADER_PARAMETER(float, ReflectionMaxRayDistance)
		SHADER_PARAMETER(float, ReflectionMaxRoughness)
		SHADER_PARAMETER(float, ReflectionMaxNormalBias)

		SHADER_PARAMETER_SRV(RaytracingAccelerationStructure, TLAS)

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_STRUCT_REF(FSceneTexturesUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_STRUCT_REF(FRaytracingLightDataPacked, LightDataPacked)
		SHADER_PARAMETER_STRUCT_REF(FReflectionUniformParameters, ReflectionStruct)
		SHADER_PARAMETER_STRUCT_REF(FFogUniformParameters, FogUniformParameters)
		SHADER_PARAMETER_STRUCT_REF(FIESLightProfileParameters, IESLightProfileParameters)

		// Optional indirection buffer used for sorted materials
		SHADER_PARAMETER_RDG_BUFFER_UAV(StructuredBuffer<FDeferredMaterialPayload>, MaterialBuffer)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, ColorOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RayHitDistanceOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RayImaginaryDepthOutput)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}
};

class FRayTracingReflectionsCHS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingReflectionsCHS);
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingReflectionsCHS, FGlobalShader)
		
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}
	
	using FParameters = FEmptyShaderParameters;
};

class FRayTracingReflectionsMS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingReflectionsMS);
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingReflectionsMS, FGlobalShader)
		
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	using FParameters = FEmptyShaderParameters;
};

IMPLEMENT_GLOBAL_SHADER(FRayTracingReflectionsRGS, "/Engine/Private/RayTracing/RayTracingReflections.usf", "RayTracingReflectionsRGS", SF_RayGen);
IMPLEMENT_GLOBAL_SHADER(FRayTracingReflectionsCHS, "/Engine/Private/RayTracing/RayTracingReflections.usf", "RayTracingReflectionsMainCHS", SF_RayHitGroup);
IMPLEMENT_GLOBAL_SHADER(FRayTracingReflectionsMS, "/Engine/Private/RayTracing/RayTracingReflections.usf", "RayTracingReflectionsMainMS", SF_RayMiss);

void FDeferredShadingSceneRenderer::PrepareRayTracingReflections(const FViewInfo& View, TArray<FRayTracingShaderRHIParamRef>& OutRayGenShaders)
{
	// Declare all RayGen shaders that require material closest hit shaders to be bound

	const bool bSortMaterials = CVarRayTracingReflectionsSortMaterials.GetValueOnRenderThread();

	const EDeferredMaterialMode DeferredMaterialMode = bSortMaterials ? EDeferredMaterialMode::Shade : EDeferredMaterialMode::None;

	FRayTracingReflectionsRGS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FRayTracingReflectionsRGS::FDeferredMaterialMode>(DeferredMaterialMode);

	auto RayGenShader = View.ShaderMap->GetShader<FRayTracingReflectionsRGS>(PermutationVector);

	OutRayGenShaders.Add(RayGenShader->GetRayTracingShader());
}

#endif // RHI_RAYTRACING

void FDeferredShadingSceneRenderer::RenderRayTracingReflections(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FRDGTextureRef* OutColorTexture,
	FRDGTextureRef* OutRayHitDistanceTexture,
	FRDGTextureRef* OutRayImaginaryDepthTexture,
	int32 SamplePerPixel,
	int32 HeightFog,
	float ResolutionFraction)
#if RHI_RAYTRACING
{
	const uint32 SortTileSize = CVarRayTracingReflectionsSortTileSize.GetValueOnRenderThread();
	const bool bSortMaterials = CVarRayTracingReflectionsSortMaterials.GetValueOnRenderThread();

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(GraphBuilder.RHICmdList);

	int32 UpscaleFactor = int32(1.0f / ResolutionFraction);
	ensure(ResolutionFraction == 1.0 / UpscaleFactor);
	ensureMsgf(FComputeShaderUtils::kGolden2DGroupSize % UpscaleFactor == 0, TEXT("Reflection ray tracing will have uv misalignement."));
	FIntPoint RayTracingResolution = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), UpscaleFactor);

	{
		FPooledRenderTargetDesc Desc = SceneContext.GetSceneColor()->GetDesc();
		Desc.Format = PF_FloatRGBA;
		Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
		Desc.Extent /= UpscaleFactor;
		Desc.TargetableFlags |= TexCreate_UAV;

		*OutColorTexture = GraphBuilder.CreateTexture(Desc, TEXT("RayTracingReflections"));
		
		Desc.Format = PF_R16F;
		*OutRayHitDistanceTexture = GraphBuilder.CreateTexture(Desc, TEXT("RayTracingReflectionsHitDistance"));
		*OutRayImaginaryDepthTexture = GraphBuilder.CreateTexture(Desc, TEXT("RayTracingReflectionsImaginaryDepth"));
	}

	// When deferred materials are used, we need to dispatch the reflection shader twice:
	// - First pass gathers reflected ray hit data and sorts it by hit shader ID.
	// - Second pass re-traces the reflected ray and performs full shading.
	// When deferred materials are not used, everything is done in a single pass.
	const uint32 NumPasses = bSortMaterials ? 2 : 1;
	const EDeferredMaterialMode DeferredMaterialModes[2] = 
	{
		bSortMaterials ? EDeferredMaterialMode::Gather : EDeferredMaterialMode::None,
		bSortMaterials ? EDeferredMaterialMode::Shade : EDeferredMaterialMode::None,
	};

	FRDGBufferRef DeferredMaterialBuffer = nullptr;

	FIntPoint TileAlignedResolution = RayTracingResolution;
	if (SortTileSize)
	{
		TileAlignedResolution = FIntPoint::DivideAndRoundUp(RayTracingResolution, SortTileSize) * SortTileSize;
	}

	const uint32 DeferredMaterialBufferNumElements = TileAlignedResolution.X * TileAlignedResolution.Y;

	FRayTracingReflectionsRGS::FParameters CommonParameters;

	CommonParameters.SamplesPerPixel = SamplePerPixel;
	CommonParameters.MaxBounces = GRayTracingReflectionsMaxBounces > -1? GRayTracingReflectionsMaxBounces : View.FinalPostProcessSettings.RayTracingReflectionsMaxBounces;
	CommonParameters.HeightFog = HeightFog;
	CommonParameters.ShouldDoDirectLighting = GRayTracingReflectionsDirectLighting;
	CommonParameters.ReflectedShadowsType = GRayTracingReflectionsShadows > -1 ? GRayTracingReflectionsShadows : (int32)View.FinalPostProcessSettings.RayTracingReflectionsShadows;
	CommonParameters.ShouldDoEmissiveAndIndirectLighting = GRayTracingReflectionsEmissiveAndIndirectLighting;
	CommonParameters.UpscaleFactor = UpscaleFactor;
	CommonParameters.ReflectionMinRayDistance = FMath::Min(GRayTracingReflectionsMinRayDistance, GRayTracingReflectionsMaxRayDistance);
	CommonParameters.ReflectionMaxRayDistance = GRayTracingReflectionsMaxRayDistance;
	CommonParameters.ReflectionMaxRoughness = FMath::Clamp(GRayTracingReflectionsMaxRoughness >= 0 ? GRayTracingReflectionsMaxRoughness : View.FinalPostProcessSettings.RayTracingReflectionsMaxRoughness, 0.01f, 1.0f);
	CommonParameters.ReflectionMaxNormalBias = GetRaytracingMaxNormalBias();
	CommonParameters.RayTracingResolution = RayTracingResolution;
	CommonParameters.TileAlignedResolution = TileAlignedResolution;

	CommonParameters.TLAS = View.RayTracingScene.RayTracingSceneRHI->GetShaderResourceView();
	CommonParameters.ViewUniformBuffer = View.ViewUniformBuffer;
	CommonParameters.LightDataPacked = CreateLightDataPackedUniformBuffer(Scene->Lights, View, EUniformBufferUsage::UniformBuffer_SingleFrame);
	CommonParameters.SceneTexturesStruct = CreateSceneTextureUniformBuffer( SceneContext, FeatureLevel, ESceneTextureSetupMode::All, EUniformBufferUsage::UniformBuffer_SingleFrame);
	CommonParameters.ReflectionStruct = CreateReflectionUniformBuffer(View, EUniformBufferUsage::UniformBuffer_SingleFrame);
	CommonParameters.FogUniformParameters = CreateFogUniformBuffer(View, EUniformBufferUsage::UniformBuffer_SingleFrame);
	CommonParameters.IESLightProfileParameters = CreateIESLightProfilesUniformBuffer(View, EUniformBufferUsage::UniformBuffer_SingleFrame);
	CommonParameters.ColorOutput = GraphBuilder.CreateUAV(*OutColorTexture);
	CommonParameters.RayHitDistanceOutput = GraphBuilder.CreateUAV(*OutRayHitDistanceTexture);
	CommonParameters.RayImaginaryDepthOutput = GraphBuilder.CreateUAV(*OutRayImaginaryDepthTexture);
	CommonParameters.SortTileSize = SortTileSize;

	for (uint32 PassIndex = 0; PassIndex < NumPasses; ++PassIndex)
	{
		FRayTracingReflectionsRGS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRayTracingReflectionsRGS::FParameters>();
		*PassParameters = CommonParameters;

		const EDeferredMaterialMode DeferredMaterialMode = DeferredMaterialModes[PassIndex];

		if (DeferredMaterialMode != EDeferredMaterialMode::None)
		{
			if (DeferredMaterialMode == EDeferredMaterialMode::Gather)
			{
				FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FDeferredMaterialPayload), DeferredMaterialBufferNumElements);
				DeferredMaterialBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("RayTracingReflectionsMaterialBuffer"));
			}

			PassParameters->MaterialBuffer = GraphBuilder.CreateUAV(DeferredMaterialBuffer);
		}

		FRayTracingReflectionsRGS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FRayTracingReflectionsRGS::FDeferredMaterialMode>(DeferredMaterialMode);

		auto RayGenShader = View.ShaderMap->GetShader<FRayTracingReflectionsRGS>(PermutationVector);
		ClearUnusedGraphResources(RayGenShader, PassParameters);

		if (DeferredMaterialMode == EDeferredMaterialMode::Gather)
		{
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ReflectionRayTracingGatherMaterials %dx%d", TileAlignedResolution.X, TileAlignedResolution.Y),
				PassParameters,
				ERenderGraphPassFlags::Compute,
				[PassParameters, this, &View, RayGenShader, TileAlignedResolution](FRHICommandList& RHICmdList)
			{
				FRHIRayTracingPipelineState* Pipeline = BindRayTracingDeferredMaterialGatherPipeline(RHICmdList, View, RayGenShader->GetRayTracingShader());

				FRayTracingShaderBindingsWriter GlobalResources;
				SetShaderParameters(GlobalResources, RayGenShader, *PassParameters);

				FRayTracingSceneRHIParamRef RayTracingSceneRHI = View.RayTracingScene.RayTracingSceneRHI;
				RHICmdList.RayTraceDispatch(Pipeline, RayGenShader->GetRayTracingShader(), RayTracingSceneRHI, GlobalResources, TileAlignedResolution.X, TileAlignedResolution.Y);
			});

			// A material sorting pass
			const uint32 SortSize = CVarRayTracingReflectionsSortSize.GetValueOnRenderThread();
			if (SortSize)
			{
				SortDeferredMaterials(GraphBuilder, View, SortSize, DeferredMaterialBufferNumElements, DeferredMaterialBuffer);
			}
		}
		else
		{
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ReflectionRayTracing %dx%d", RayTracingResolution.X, RayTracingResolution.Y),
				PassParameters,
				ERenderGraphPassFlags::Compute,
				[PassParameters, this, &View, RayGenShader, RayTracingResolution, DeferredMaterialBufferNumElements, DeferredMaterialMode](FRHICommandList& RHICmdList)
			{
				FRayTracingShaderBindingsWriter GlobalResources;
				SetShaderParameters(GlobalResources, RayGenShader, *PassParameters);

				FRayTracingSceneRHIParamRef RayTracingSceneRHI = View.RayTracingScene.RayTracingSceneRHI;

				if (DeferredMaterialMode == EDeferredMaterialMode::Shade)
				{
					// Shading pass for sorted materials uses 1D dispatch over all elements in the material buffer.
					// This can be reduced to the number of output pixels if sorting pass guarantees that all invalid entries are moved to the end.
					RHICmdList.RayTraceDispatch(View.RayTracingMaterialPipeline, RayGenShader->GetRayTracingShader(), RayTracingSceneRHI, GlobalResources, DeferredMaterialBufferNumElements, 1);
				}
				else // EDeferredMaterialMode::None
				{
					RHICmdList.RayTraceDispatch(View.RayTracingMaterialPipeline, RayGenShader->GetRayTracingShader(), RayTracingSceneRHI, GlobalResources, RayTracingResolution.X, RayTracingResolution.Y);
				}
			});
		}
	}

}
#else // !RHI_RAYTRACING
{
	check(0);
}
#endif
