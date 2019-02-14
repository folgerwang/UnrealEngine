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
#include "RenderGraph.h"

static int32 GRayTracingReflectionsMaxBounces = 1;
static FAutoConsoleVariableRef CVarRayTracingReflectionsMaxBounces(
	TEXT("r.RayTracing.Reflections.MaxBounces"),
	GRayTracingReflectionsMaxBounces,
	TEXT("Sets the maximum number of ray tracing reflection bounces (default = 1)")
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

static int32 GRayTracingReflectionsShadows = 1;
static FAutoConsoleVariableRef CVarRayTracingReflectionsShadows(
	TEXT("r.RayTracing.Reflections.Shadows"),
	GRayTracingReflectionsShadows,
	TEXT("Enables shadows in ray tracing reflections (default = 1)")
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
	32,
	TEXT("Size of pixel tiles for sorted reflections\n")
	TEXT("  Default 32\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRayTracingReflectionsSortSize(
	TEXT("r.RayTracing.Reflections.SortSize"),
	3,
	TEXT("Size of horizon for material ID sort\n")
	TEXT("0: Disabled\n")
	TEXT("1: 256 Elements\n")
	TEXT("2: 512 Elements\n")
	TEXT("3: 1024 Elements (Default)\n"),
	ECVF_RenderThreadSafe);

static const int32 GReflectionLightCountMaximum = 64;

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FReflectionsLightData, )
SHADER_PARAMETER(uint32, Count)
SHADER_PARAMETER_ARRAY(uint32, Type, [GReflectionLightCountMaximum])
SHADER_PARAMETER_ARRAY(FVector, LightPosition, [GReflectionLightCountMaximum])
SHADER_PARAMETER_ARRAY(float, LightInvRadius, [GReflectionLightCountMaximum])
SHADER_PARAMETER_ARRAY(FVector, LightColor, [GReflectionLightCountMaximum])
SHADER_PARAMETER_ARRAY(float, LightFalloffExponent, [GReflectionLightCountMaximum])
SHADER_PARAMETER_ARRAY(FVector, Direction, [GReflectionLightCountMaximum])
SHADER_PARAMETER_ARRAY(FVector, Tangent, [GReflectionLightCountMaximum])
SHADER_PARAMETER_ARRAY(FVector2D, SpotAngles, [GReflectionLightCountMaximum])
SHADER_PARAMETER_ARRAY(float, SpecularScale, [GReflectionLightCountMaximum])
SHADER_PARAMETER_ARRAY(float, SourceRadius, [GReflectionLightCountMaximum])
SHADER_PARAMETER_ARRAY(float, SourceLength, [GReflectionLightCountMaximum])
SHADER_PARAMETER_ARRAY(float, SoftSourceRadius, [GReflectionLightCountMaximum])
SHADER_PARAMETER_ARRAY(FVector2D, DistanceFadeMAD, [GReflectionLightCountMaximum])
SHADER_PARAMETER_TEXTURE(Texture2D, DummyRectLightTexture) //#dxr_todo: replace with an array of textures when there is support for SHADER_PARAMETER_TEXTURE_ARRAY 
END_GLOBAL_SHADER_PARAMETER_STRUCT()


IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FReflectionsLightData, "ReflectionLightsData");


void SetupReflectionsLightData(
	const TSparseArray<FLightSceneInfoCompact>& Lights,
	const FViewInfo& View,
	FReflectionsLightData* LightData)
{
	LightData->Count = 0;

	for (auto Light : Lights)
	{
		const bool bHasStaticLighting = Light.LightSceneInfo->Proxy->HasStaticLighting() && Light.LightSceneInfo->IsPrecomputedLightingValid();
		const bool bAffectReflection = Light.LightSceneInfo->Proxy->AffectReflection();
		if (bHasStaticLighting || !bAffectReflection) continue;

		FLightShaderParameters LightParameters;
		Light.LightSceneInfo->Proxy->GetLightShaderParameters(LightParameters);

		if (Light.LightSceneInfo->Proxy->IsInverseSquared())
		{
			LightParameters.FalloffExponent = 0;
		}

		LightData->Type[LightData->Count] = Light.LightType;
		LightData->LightPosition[LightData->Count] = LightParameters.Position;
		LightData->LightInvRadius[LightData->Count] = LightParameters.InvRadius;
		LightData->LightColor[LightData->Count] = LightParameters.Color;
		LightData->LightFalloffExponent[LightData->Count] = LightParameters.FalloffExponent;
		LightData->Direction[LightData->Count] = LightParameters.Direction;
		LightData->Tangent[LightData->Count] = LightParameters.Tangent;
		LightData->SpotAngles[LightData->Count] = LightParameters.SpotAngles;
		LightData->SpecularScale[LightData->Count] = LightParameters.SpecularScale;
		LightData->SourceRadius[LightData->Count] = LightParameters.SourceRadius;
		LightData->SourceLength[LightData->Count] = LightParameters.SourceLength;
		LightData->SoftSourceRadius[LightData->Count] = LightParameters.SoftSourceRadius;

		const FVector2D FadeParams = Light.LightSceneInfo->Proxy->GetDirectionalLightDistanceFadeParameters(View.GetFeatureLevel(), Light.LightSceneInfo->IsPrecomputedLightingValid(), View.MaxShadowCascades);
		LightData->DistanceFadeMAD[LightData->Count] = FVector2D(FadeParams.Y, -FadeParams.X * FadeParams.Y);

		LightData->Count++;

		if (LightData->Count >= GReflectionLightCountMaximum) break;
	}

	LightData->DummyRectLightTexture = GWhiteTexture->TextureRHI; //#dxr_todo: replace with valid textures per rect light
}


class FRayTracingReflectionsRG : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingReflectionsRG)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingReflectionsRG, FGlobalShader)

	class FDenoiserOutput : SHADER_PERMUTATION_BOOL("DIM_DENOISER_OUTPUT");

	class FDeferredMaterialMode : SHADER_PERMUTATION_ENUM_CLASS("DIM_DEFERRED_MATERIAL_MODE", EDeferredMaterialMode);

	using FPermutationDomain = TShaderPermutationDomain<FDenoiserOutput, FDeferredMaterialMode>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int32, SamplesPerPixel)
		SHADER_PARAMETER(int32, MaxBounces)
		SHADER_PARAMETER(int32, HeightFog)
		SHADER_PARAMETER(int32, ShouldDoDirectLighting)
		SHADER_PARAMETER(int32, ShouldDoReflectedShadows)
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

		SHADER_PARAMETER_TEXTURE(Texture2D, LTCMatTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, LTCMatSampler)
		SHADER_PARAMETER_TEXTURE(Texture2D, LTCAmpTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, LTCAmpSampler)

		SHADER_PARAMETER_TEXTURE(Texture2D, PreIntegratedGF)
		SHADER_PARAMETER_SAMPLER(SamplerState, PreIntegratedGFSampler)

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_STRUCT_REF(FSceneTexturesUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_STRUCT_REF(FReflectionsLightData, LightData)
		SHADER_PARAMETER_STRUCT_REF(FReflectionUniformParameters, ReflectionStruct)
		SHADER_PARAMETER_STRUCT_REF(FFogUniformParameters, FogUniformParameters)

		// Optional indirection buffer used for sorted materials
		SHADER_PARAMETER_RDG_BUFFER_UAV(StructuredBuffer<FDeferredMaterialPayload>, MaterialBuffer)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, ColorOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RayHitDistanceOutput)
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

IMPLEMENT_GLOBAL_SHADER(FRayTracingReflectionsRG, "/Engine/Private/RayTracing/RayTracingReflections.usf", "RayTracingReflectionsRGS", SF_RayGen);
IMPLEMENT_GLOBAL_SHADER(FRayTracingReflectionsCHS, "/Engine/Private/RayTracing/RayTracingReflections.usf", "RayTracingReflectionsMainCHS", SF_RayHitGroup);
IMPLEMENT_GLOBAL_SHADER(FRayTracingReflectionsMS, "/Engine/Private/RayTracing/RayTracingReflections.usf", "RayTracingReflectionsMainMS", SF_RayMiss);

#endif // RHI_RAYTRACING

void FDeferredShadingSceneRenderer::RayTraceReflections(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FRDGTextureRef* OutColorTexture,
	FRDGTextureRef* OutRayHitDistanceTexture,
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

		*OutColorTexture = GraphBuilder.CreateTexture(Desc, TEXT("RayTracingReflections"));
		
		Desc.Format = PF_R16F;
		*OutRayHitDistanceTexture = GraphBuilder.CreateTexture(Desc, TEXT("RayTracingReflectionsHitDistance"));
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

	FRayTracingReflectionsRG::FParameters CommonParameters;

	CommonParameters.SamplesPerPixel = SamplePerPixel;
	CommonParameters.MaxBounces = GRayTracingReflectionsMaxBounces;
	CommonParameters.HeightFog = HeightFog;
	CommonParameters.ShouldDoDirectLighting = GRayTracingReflectionsDirectLighting;
	CommonParameters.ShouldDoReflectedShadows = GRayTracingReflectionsShadows;
	CommonParameters.ShouldDoEmissiveAndIndirectLighting = GRayTracingReflectionsEmissiveAndIndirectLighting;
	CommonParameters.UpscaleFactor = UpscaleFactor;
	CommonParameters.ReflectionMinRayDistance = FMath::Min(GRayTracingReflectionsMinRayDistance, GRayTracingReflectionsMaxRayDistance);
	CommonParameters.ReflectionMaxRayDistance = GRayTracingReflectionsMaxRayDistance;
	CommonParameters.ReflectionMaxRoughness = FMath::Clamp(View.FinalPostProcessSettings.ScreenSpaceReflectionMaxRoughness, 0.01f, 1.0f);
	CommonParameters.ReflectionMaxNormalBias = GetRaytracingOcclusionMaxNormalBias();
	CommonParameters.LTCMatTexture = GSystemTextures.LTCMat->GetRenderTargetItem().ShaderResourceTexture;
	CommonParameters.LTCMatSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	CommonParameters.LTCAmpTexture = GSystemTextures.LTCAmp->GetRenderTargetItem().ShaderResourceTexture;
	CommonParameters.LTCAmpSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	CommonParameters.PreIntegratedGF = GSystemTextures.PreintegratedGF->GetRenderTargetItem().ShaderResourceTexture;
	CommonParameters.PreIntegratedGFSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	CommonParameters.RayTracingResolution = RayTracingResolution;
	CommonParameters.TileAlignedResolution = TileAlignedResolution;

	CommonParameters.TLAS = View.PerViewRayTracingScene.RayTracingSceneRHI->GetShaderResourceView();
	CommonParameters.ViewUniformBuffer = View.ViewUniformBuffer;
	{
		FReflectionsLightData LightData;
		SetupReflectionsLightData(Scene->Lights, View, &LightData);
		CommonParameters.LightData = CreateUniformBufferImmediate(LightData, EUniformBufferUsage::UniformBuffer_SingleFrame);
	}
	{ // TODO: use FSceneViewFamilyBlackboard.
		FSceneTexturesUniformParameters SceneTextures;
		SetupSceneTextureUniformParameters(SceneContext, FeatureLevel, ESceneTextureSetupMode::All, SceneTextures);
		CommonParameters.SceneTexturesStruct = CreateUniformBufferImmediate(SceneTextures, EUniformBufferUsage::UniformBuffer_SingleFrame);
	}
	{
		FReflectionUniformParameters ReflectionStruct;
		SetupReflectionUniformParameters(View, ReflectionStruct);
		CommonParameters.ReflectionStruct = CreateUniformBufferImmediate(ReflectionStruct, EUniformBufferUsage::UniformBuffer_SingleFrame);
	}
	{
		FFogUniformParameters FogStruct;
		SetupFogUniformParameters(View, FogStruct);
		CommonParameters.FogUniformParameters = CreateUniformBufferImmediate(FogStruct, EUniformBufferUsage::UniformBuffer_SingleFrame);
	}
	CommonParameters.ColorOutput = GraphBuilder.CreateUAV(*OutColorTexture);
	CommonParameters.RayHitDistanceOutput = GraphBuilder.CreateUAV(*OutRayHitDistanceTexture);
	CommonParameters.SortTileSize = SortTileSize;

	for (uint32 PassIndex = 0; PassIndex < NumPasses; ++PassIndex)
	{
		FRayTracingReflectionsRG::FParameters* PassParameters = GraphBuilder.AllocParameters<FRayTracingReflectionsRG::FParameters>();
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

		FRayTracingReflectionsRG::FPermutationDomain PermutationVector;
		PermutationVector.Set<FRayTracingReflectionsRG::FDeferredMaterialMode>(DeferredMaterialMode);

		auto RayGenShader = View.ShaderMap->GetShader<FRayTracingReflectionsRG>(PermutationVector);
		ClearUnusedGraphResources(RayGenShader, PassParameters);

		if (DeferredMaterialMode == EDeferredMaterialMode::Gather)
		{
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ReflectionRayTracingGatherMaterials %dx%d", RayTracingResolution.X, RayTracingResolution.Y),
				PassParameters,
				ERenderGraphPassFlags::Compute,
				[PassParameters, this, &View, RayGenShader, RayTracingResolution](FRHICommandList& RHICmdList)
			{
				FRHIRayTracingPipelineState* Pipeline = BindRayTracingPipelineForDeferredMaterialGather(RHICmdList, View, RayGenShader->GetRayTracingShader());

				FRayTracingShaderBindingsWriter GlobalResources;
				SetShaderParameters(GlobalResources, RayGenShader, *PassParameters);

				FRayTracingSceneRHIParamRef RayTracingSceneRHI = View.PerViewRayTracingScene.RayTracingSceneRHI;
				const uint32 RayGenShaderIndex = 0;
				RHICmdList.RayTraceDispatch(Pipeline, RayGenShaderIndex, RayTracingSceneRHI, GlobalResources, RayTracingResolution.X, RayTracingResolution.Y);
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
				auto ClosestHitShader = View.ShaderMap->GetShader<FRayTracingReflectionsCHS>();
				auto MissShader = View.ShaderMap->GetShader<FRayTracingReflectionsMS>();

				FRHIRayTracingPipelineState* Pipeline = BindRayTracingPipeline(
					RHICmdList, View,
					RayGenShader->GetRayTracingShader(),
					MissShader->GetRayTracingShader(),
					ClosestHitShader->GetRayTracingShader()); // #dxr_todo: this should be done once at load-time and cached

				FRayTracingShaderBindingsWriter GlobalResources;
				SetShaderParameters(GlobalResources, RayGenShader, *PassParameters);

				FRayTracingSceneRHIParamRef RayTracingSceneRHI = View.PerViewRayTracingScene.RayTracingSceneRHI;
				const uint32 RayGenShaderIndex = 0;

				if (DeferredMaterialMode == EDeferredMaterialMode::Shade)
				{
					// Shading pass for sorted materials uses 1D dispatch over all elements in the material buffer.
					// This can be reduced to the number of output pixels if sorting pass guarantees that all invalid entries are moved to the end.
					RHICmdList.RayTraceDispatch(Pipeline, RayGenShaderIndex, RayTracingSceneRHI, GlobalResources, DeferredMaterialBufferNumElements, 1);
				}
				else
				{
					RHICmdList.RayTraceDispatch(Pipeline, RayGenShaderIndex, RayTracingSceneRHI, GlobalResources, RayTracingResolution.X, RayTracingResolution.Y);
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
