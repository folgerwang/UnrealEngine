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
#include "SceneViewFamilyBlackboard.h"
#include "ScreenSpaceDenoise.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PipelineStateCache.h"
#include "RayTracing/RaytracingOptions.h"

static int32 GRayTracingTranslucencyMaxRefractionRays = 4;
static FAutoConsoleVariableRef CVarRayTracingTranslucencyMaxRefractionRays(
	TEXT("r.RayTracing.Translucency.MaxRefractionRays"),
	GRayTracingTranslucencyMaxRefractionRays,
	TEXT("Sets the maximum number of refraction rays for ray traced translucency (default = 3)"));

static int32 GRayTracingTranslucencyEmissiveAndIndirectLighting = 1;
static FAutoConsoleVariableRef CVarRayTracingTranslucencyEmissiveAndIndirectLighting(
	TEXT("r.RayTracing.Translucency.EmissiveAndIndirectLighting"),
	GRayTracingTranslucencyEmissiveAndIndirectLighting,
	TEXT("Enables ray tracing translucency emissive and indirect lighting (default = 1)")
);

static int32 GRayTracingTranslucencyDirectLighting = 1;
static FAutoConsoleVariableRef CVarRayTracingTranslucencyDirectLighting(
	TEXT("r.RayTracing.Translucency.DirectLighting"),
	GRayTracingTranslucencyDirectLighting,
	TEXT("Enables ray tracing translucency direct lighting (default = 1)")
);

static int32 GRayTracingTranslucencyShadows = 1;
static FAutoConsoleVariableRef CVarRayTracingTranslucencyShadows(
	TEXT("r.RayTracing.Translucency.Shadows"),
	GRayTracingTranslucencyShadows,
	TEXT("Enables shadows in ray tracing Translucency (default = 1)")
);

static float GRayTracingTranslucencyMinRayDistance = -1;
static FAutoConsoleVariableRef CVarRayTracingTranslucencyMinRayDistance(
	TEXT("r.RayTracing.Translucency.MinRayDistance"),
	GRayTracingTranslucencyMinRayDistance,
	TEXT("Sets the minimum ray distance for ray traced translucency rays. Actual translucency ray length is computed as Lerp(MaxRayDistance, MinRayDistance, Roughness), i.e. translucency rays become shorter when traced from rougher surfaces. (default = -1 (infinite rays))")
);

static float GRayTracingTranslucencyMaxRayDistance = -1;
static FAutoConsoleVariableRef CVarRayTracingTranslucencyMaxRayDistance(
	TEXT("r.RayTracing.Translucency.MaxRayDistance"),
	GRayTracingTranslucencyMaxRayDistance,
	TEXT("Sets the maximum ray distance for ray traced translucency rays. When ray shortening is used, skybox will not be sampled in RT translucency pass and will be composited later, together with local reflection captures. Negative values turn off this optimization. (default = -1 (infinite rays))")
);

static int32 GRayTracingTranslucencySamplesPerPixel = 1;
static FAutoConsoleVariableRef CVarRayTracingTranslucencySamplesPerPixel(
	TEXT("r.RayTracing.Translucency.SamplesPerPixel"),
	GRayTracingTranslucencySamplesPerPixel,
	TEXT("Sets the samples-per-pixel for Translucency (default = 1)"));

static int32 GRayTracingTranslucencyHeightFog = 1;
static FAutoConsoleVariableRef CVarRayTracingTranslucencyHeightFog(
	TEXT("r.RayTracing.Translucency.HeightFog"),
	GRayTracingTranslucencyHeightFog,
	TEXT("Enables height fog in ray traced Translucency (default = 1)"));

DECLARE_GPU_STAT_NAMED(RayTracingTranslucency, TEXT("Ray Tracing Translucency"));


//#dxr_todo: factor out with the light structure in RayTracingReflections.cpp
static const int32 GTranslucencyLightCountMaximum = 64;

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FTranslucencyLightData, )
SHADER_PARAMETER(uint32, Count)
SHADER_PARAMETER_ARRAY(uint32, Type, [GTranslucencyLightCountMaximum])
SHADER_PARAMETER_ARRAY(FVector, LightPosition, [GTranslucencyLightCountMaximum])
SHADER_PARAMETER_ARRAY(float, LightInvRadius, [GTranslucencyLightCountMaximum])
SHADER_PARAMETER_ARRAY(FVector, LightColor, [GTranslucencyLightCountMaximum])
SHADER_PARAMETER_ARRAY(float, LightFalloffExponent, [GTranslucencyLightCountMaximum])
SHADER_PARAMETER_ARRAY(FVector, Direction, [GTranslucencyLightCountMaximum])
SHADER_PARAMETER_ARRAY(FVector, Tangent, [GTranslucencyLightCountMaximum])
SHADER_PARAMETER_ARRAY(FVector2D, SpotAngles, [GTranslucencyLightCountMaximum])
SHADER_PARAMETER_ARRAY(float, SpecularScale, [GTranslucencyLightCountMaximum])
SHADER_PARAMETER_ARRAY(float, SourceRadius, [GTranslucencyLightCountMaximum])
SHADER_PARAMETER_ARRAY(float, SourceLength, [GTranslucencyLightCountMaximum])
SHADER_PARAMETER_ARRAY(float, SoftSourceRadius, [GTranslucencyLightCountMaximum])
SHADER_PARAMETER_ARRAY(FVector2D, DistanceFadeMAD, [GTranslucencyLightCountMaximum])
SHADER_PARAMETER_TEXTURE(Texture2D, DummyRectLightTexture) //#dxr_todo: replace with an array of textures when there is support for SHADER_PARAMETER_TEXTURE_ARRAY 
END_GLOBAL_SHADER_PARAMETER_STRUCT()


IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FTranslucencyLightData, "ReflectionLightsData");


void SetupTranslucencyLightData(
	const TSparseArray<FLightSceneInfoCompact>& Lights,
	const FViewInfo& View,
	FTranslucencyLightData* LightData)
{
	LightData->Count = 0;

	for (auto Light : Lights)
	{
		if (Light.LightSceneInfo->Proxy->HasStaticLighting() && Light.LightSceneInfo->IsPrecomputedLightingValid()) continue;

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

		if (LightData->Count >= GTranslucencyLightCountMaximum) break;
	}

	LightData->DummyRectLightTexture = GWhiteTexture->TextureRHI; //#dxr_todo: replace with valid textures per rect light
}


class FRayTracingTranslucencyRG : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingTranslucencyRG)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingTranslucencyRG, FGlobalShader)

		class FDenoiserOutput : SHADER_PERMUTATION_BOOL("DIM_DENOISER_OUTPUT");
	using FPermutationDomain = TShaderPermutationDomain<FDenoiserOutput>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int32, SamplesPerPixel)
		SHADER_PARAMETER(int32, MaxRefractionRays)
		SHADER_PARAMETER(int32, HeightFog)
		SHADER_PARAMETER(int32, ShouldDoDirectLighting)
		SHADER_PARAMETER(int32, ShouldDoReflectedShadows)
		SHADER_PARAMETER(int32, ShouldDoEmissiveAndIndirectLighting)
		SHADER_PARAMETER(int32, UpscaleFactor)
		SHADER_PARAMETER(float, TranslucencyMinRayDistance)
		SHADER_PARAMETER(float, TranslucencyMaxRayDistance)
		SHADER_PARAMETER(float, TranslucencyMaxRoughness)
		SHADER_PARAMETER(float, MaxNormalBias)

		SHADER_PARAMETER_SRV(RaytracingAccelerationStructure, TLAS)

		SHADER_PARAMETER_TEXTURE(Texture2D, LTCMatTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, LTCMatSampler)
		SHADER_PARAMETER_TEXTURE(Texture2D, LTCAmpTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, LTCAmpSampler)

		SHADER_PARAMETER_TEXTURE(Texture2D, PreIntegratedGF)
		SHADER_PARAMETER_SAMPLER(SamplerState, PreIntegratedGFSampler)

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_STRUCT_REF(FSceneTexturesUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_STRUCT_REF(FTranslucencyLightData, LightData)
		SHADER_PARAMETER_STRUCT_REF(FReflectionUniformParameters, ReflectionStruct)
		SHADER_PARAMETER_STRUCT_REF(FFogUniformParameters, FogUniformParameters)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, ColorOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RayHitDistanceOutput)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}
};

class FRayTracingTranslucencyCHS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingTranslucencyCHS);
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingTranslucencyCHS, FGlobalShader)

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	using FParameters = FEmptyShaderParameters;
};

class FRayTracingTranslucencyMS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingTranslucencyMS);
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingTranslucencyMS, FGlobalShader)

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	using FParameters = FEmptyShaderParameters;
};

IMPLEMENT_GLOBAL_SHADER(FRayTracingTranslucencyRG, "/Engine/Private/RayTracing/RayTracingTranslucency.usf", "RayTracingTranslucencyRGS", SF_RayGen);
IMPLEMENT_GLOBAL_SHADER(FRayTracingTranslucencyCHS, "/Engine/Private/RayTracing/RayTracingTranslucency.usf", "RayTracingTranslucencyMainCHS", SF_RayHitGroup);
IMPLEMENT_GLOBAL_SHADER(FRayTracingTranslucencyMS, "/Engine/Private/RayTracing/RayTracingTranslucency.usf", "RayTracingTranslucencyMainMS", SF_RayMiss);


//#dxr-todo: should we unify it with the composition happening in the non raytraced translucency pass? In that case it should use FCopySceneColorPS
// Probably, but the architecture depends on the denoiser -> discuss

class FCompositeTranslucencyPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCompositeTranslucencyPS, Global);

public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return ShouldCompileRayTracingShadersForProject(Platform);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	FCompositeTranslucencyPS() {}
	virtual ~FCompositeTranslucencyPS() {}

	FCompositeTranslucencyPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer);
		TranslucencyTextureParameter.Bind(Initializer.ParameterMap, TEXT("TranslucencyTexture"));
		TranslucencyTextureSamplerParameter.Bind(Initializer.ParameterMap, TEXT("TranslucencyTextureSampler"));
	}

	template<typename TRHICommandList>
	void SetParameters(
		TRHICommandList& RHICmdList,
		const FViewInfo& View,
		FTextureRHIParamRef TranslucencyTexture,
		FTextureRHIParamRef HitDistanceTexture)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		SceneTextureParameters.Set(RHICmdList, ShaderRHI, View.FeatureLevel, ESceneTextureSetupMode::All);

		SetTextureParameter(RHICmdList, ShaderRHI, TranslucencyTextureParameter, TranslucencyTextureSamplerParameter, TStaticSamplerState<SF_Bilinear>::GetRHI(), TranslucencyTexture);
		// #dxr_todo: Use hit-distance texture for denoising
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << TranslucencyTextureParameter;
		Ar << TranslucencyTextureSamplerParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FSceneTextureShaderParameters SceneTextureParameters;
	FShaderResourceParameter TranslucencyTextureParameter;
	FShaderResourceParameter TranslucencyTextureSamplerParameter;
};

IMPLEMENT_SHADER_TYPE(, FCompositeTranslucencyPS, TEXT("/Engine/Private/RayTracing/CompositeTranslucencyPS.usf"), TEXT("CompositeTranslucencyPS"), SF_Pixel)


void FDeferredShadingSceneRenderer::RayTraceTranslucency(FRHICommandListImmediate& RHICmdList)
{
	//#dxr_todo: check TPT_StandardTranslucency/TPT_TranslucencyAfterDOF support
	ETranslucencyPass::Type TranslucencyPass(ETranslucencyPass::TPT_AllTranslucency);

	if (!ShouldRenderTranslucency(TranslucencyPass))
	{
		return; // Early exit if nothing needs to be done.
	}

	for (int32 ViewIndex = 0, Num = Views.Num(); ViewIndex < Num; ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		SCOPED_DRAW_EVENT(RHICmdList, RayTracingTranslucency);
		SCOPED_GPU_STAT(RHICmdList, RayTracingTranslucency);

		FRDGBuilder GraphBuilder(RHICmdList); //#dxr_todo: convert the entire translucency pass to render graph.
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(GraphBuilder.RHICmdList);

		FSceneViewFamilyBlackboard SceneBlackboard;
		SetupSceneViewFamilyBlackboard(GraphBuilder, &SceneBlackboard);

		//#dxr_todo: do not use reflections denoiser structs but separated ones
		IScreenSpaceDenoiser::FReflectionsInputs DenoiserInputs;
		float ResolutionFraction = 1.0f;

		RayTraceTranslucencyView(
			GraphBuilder,
			View, &DenoiserInputs.Color, &DenoiserInputs.RayHitDistance,
			GRayTracingTranslucencySamplesPerPixel, GRayTracingTranslucencyHeightFog, ResolutionFraction);

		//#dxr_todo: denoise: replace DenoiserInputs with DenoiserOutputs in the following lines!
		TRefCountPtr<IPooledRenderTarget> TranslucencyColor = GSystemTextures.BlackDummy;
		TRefCountPtr<IPooledRenderTarget> TranslucencyHitDistanceColor = GSystemTextures.BlackDummy;

		GraphBuilder.QueueTextureExtraction(DenoiserInputs.Color, &TranslucencyColor);
		GraphBuilder.QueueTextureExtraction(DenoiserInputs.RayHitDistance, &TranslucencyHitDistanceColor);

		GraphBuilder.Execute();

		// Compositing result with the scene color
		//#dxr-todo: should we unify it with the composition happening in the non raytraced translucency pass? In that case it should use FCopySceneColorPS
		// Probably, but the architecture depends on the denoiser -> discuss
		{
			const auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
			TShaderMapRef<FPostProcessVS> VertexShader(ShaderMap);
			TShaderMapRef<FCompositeTranslucencyPS> PixelShader(ShaderMap);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, true);
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			PixelShader->SetParameters(RHICmdList, View, TranslucencyColor->GetRenderTargetItem().ShaderResourceTexture, TranslucencyHitDistanceColor->GetRenderTargetItem().ShaderResourceTexture);

			DrawRectangle(
				RHICmdList,
				0, 0,
				View.ViewRect.Width(), View.ViewRect.Height(),
				View.ViewRect.Min.X, View.ViewRect.Min.Y,
				View.ViewRect.Width(), View.ViewRect.Height(),
				FIntPoint(View.ViewRect.Width(), View.ViewRect.Height()),
				SceneContext.GetBufferSizeXY(),
				*VertexShader);
		}

		ResolveSceneColor(RHICmdList);
		SceneContext.FinishRenderingSceneColor(RHICmdList);
	}
}

void FDeferredShadingSceneRenderer::RayTraceTranslucencyView(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FRDGTextureRef* OutColorTexture,
	FRDGTextureRef* OutRayHitDistanceTexture,
	int32 SamplePerPixel,
	int32 HeightFog,
	float ResolutionFraction)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(GraphBuilder.RHICmdList);

	int32 UpscaleFactor = int32(1.0f / ResolutionFraction);
	ensure(ResolutionFraction == 1.0 / UpscaleFactor);
	ensureMsgf(FComputeShaderUtils::kGolden2DGroupSize % UpscaleFactor == 0, TEXT("Translucency ray tracing will have uv misalignement."));
	FIntPoint RayTracingResolution = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), UpscaleFactor);

	{
		FPooledRenderTargetDesc Desc = SceneContext.GetSceneColor()->GetDesc();
		Desc.Format = PF_FloatRGBA;
		Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
		Desc.Extent /= UpscaleFactor;

		*OutColorTexture = GraphBuilder.CreateTexture(Desc, TEXT("RayTracingTranslucency"));

		Desc.Format = PF_R16F;
		*OutRayHitDistanceTexture = GraphBuilder.CreateTexture(Desc, TEXT("RayTracingTranslucencyHitDistance"));
	}

	FRayTracingTranslucencyRG::FParameters* PassParameters = GraphBuilder.AllocParameters<FRayTracingTranslucencyRG::FParameters>();

	PassParameters->SamplesPerPixel = SamplePerPixel;
	PassParameters->MaxRefractionRays = GRayTracingTranslucencyMaxRefractionRays;
	PassParameters->HeightFog = HeightFog;
	PassParameters->ShouldDoDirectLighting = GRayTracingTranslucencyDirectLighting;
	PassParameters->ShouldDoReflectedShadows = GRayTracingTranslucencyShadows;
	PassParameters->ShouldDoEmissiveAndIndirectLighting = GRayTracingTranslucencyEmissiveAndIndirectLighting;
	PassParameters->UpscaleFactor = UpscaleFactor;
	PassParameters->TranslucencyMinRayDistance = FMath::Min(GRayTracingTranslucencyMinRayDistance, GRayTracingTranslucencyMaxRayDistance);
	PassParameters->TranslucencyMaxRayDistance = GRayTracingTranslucencyMaxRayDistance;
	//#dxr-todo: do we want to use SSR parameter here?
	PassParameters->TranslucencyMaxRoughness = FMath::Clamp(View.FinalPostProcessSettings.ScreenSpaceReflectionMaxRoughness, 0.01f, 1.0f); 
	PassParameters->MaxNormalBias = GetRaytracingOcclusionMaxNormalBias();
	PassParameters->LTCMatTexture = GSystemTextures.LTCMat->GetRenderTargetItem().ShaderResourceTexture;
	PassParameters->LTCMatSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->LTCAmpTexture = GSystemTextures.LTCAmp->GetRenderTargetItem().ShaderResourceTexture;
	PassParameters->LTCAmpSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->PreIntegratedGF = GSystemTextures.PreintegratedGF->GetRenderTargetItem().ShaderResourceTexture;
	PassParameters->PreIntegratedGFSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	PassParameters->TLAS = View.PerViewRayTracingScene.RayTracingSceneRHI->GetShaderResourceView();
	PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
	{
		FTranslucencyLightData LightData;
		SetupTranslucencyLightData(Scene->Lights, View, &LightData);
		PassParameters->LightData = CreateUniformBufferImmediate(LightData, EUniformBufferUsage::UniformBuffer_SingleDraw);
	}
	{ // TODO: use FSceneViewFamilyBlackboard.
		FSceneTexturesUniformParameters SceneTextures;
		SetupSceneTextureUniformParameters(SceneContext, FeatureLevel, ESceneTextureSetupMode::All, SceneTextures);
		PassParameters->SceneTexturesStruct = CreateUniformBufferImmediate(SceneTextures, EUniformBufferUsage::UniformBuffer_SingleDraw);
	}
	{
		FReflectionUniformParameters ReflectionStruct;
		SetupReflectionUniformParameters(View, ReflectionStruct);
		PassParameters->ReflectionStruct = CreateUniformBufferImmediate(ReflectionStruct, EUniformBufferUsage::UniformBuffer_SingleDraw);
	}
	{
		FFogUniformParameters FogStruct;
		SetupFogUniformParameters(View, FogStruct);
		PassParameters->FogUniformParameters = CreateUniformBufferImmediate(FogStruct, EUniformBufferUsage::UniformBuffer_SingleDraw);
	}
	PassParameters->ColorOutput = GraphBuilder.CreateUAV(*OutColorTexture);
	PassParameters->RayHitDistanceOutput = GraphBuilder.CreateUAV(*OutRayHitDistanceTexture);

	auto RayGenShader = View.ShaderMap->GetShader<FRayTracingTranslucencyRG>();
	ClearUnusedGraphResources(RayGenShader, PassParameters);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("TranslucencyRayTracing %dx%d", RayTracingResolution.X, RayTracingResolution.Y),
		PassParameters,
		ERenderGraphPassFlags::Compute,
		[PassParameters, this, &View, RayGenShader, RayTracingResolution](FRHICommandList& RHICmdList)
	{
		auto ClosestHitShader = View.ShaderMap->GetShader<FRayTracingTranslucencyCHS>();
		auto MissShader = View.ShaderMap->GetShader<FRayTracingTranslucencyMS>();

		FRHIRayTracingPipelineState* Pipeline = BindRayTracingPipeline(
			RHICmdList, View,
			RayGenShader->GetRayTracingShader(),
			MissShader->GetRayTracingShader(),
			ClosestHitShader->GetRayTracingShader()); // #dxr_todo: this should be done once at load-time and cached

		FRayTracingShaderBindingsWriter GlobalResources;
		SetShaderParameters(GlobalResources, RayGenShader, *PassParameters);

		FRayTracingSceneRHIParamRef RayTracingSceneRHI = View.PerViewRayTracingScene.RayTracingSceneRHI;
		const uint32 RayGenShaderIndex = 0;
		RHICmdList.RayTraceDispatch(Pipeline, RayGenShaderIndex, RayTracingSceneRHI, GlobalResources, RayTracingResolution.X, RayTracingResolution.Y);
	});
}


#endif // RHI_RAYTRACING
