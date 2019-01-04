// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DeferredShadingRenderer.h"

#if RHI_RAYTRACING

#include "ClearQuad.h"
#include "SceneRendering.h"
#include "SceneRenderTargets.h"
#include "SceneUtils.h"
#include "RenderTargetPool.h"
#include "RHIResources.h"
#include "UniformBuffer.h"

#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"

static int32 GRayTracingAmbientOcclusion = 1;
static FAutoConsoleVariableRef CVarRayTracingAmbientOcclusion(
	TEXT("r.RayTracing.AmbientOcclusion"),
	GRayTracingAmbientOcclusion,
	TEXT("Enables ray tracing ambient occlusion (default = 1)")
);

bool FDeferredShadingSceneRenderer::ShouldRenderRayTracingAmbientOcclusion() const
{
	return GRayTracingAmbientOcclusion != 0;
}

static int32 GRayTracingAmbientOcclusionSamplesPerPixel = 1;
static FAutoConsoleVariableRef CVarRayTracingAmbientOcclusionSamplesPerPixel(
	TEXT("r.RayTracing.AmbientOcclusion.SamplesPerPixel"),
	GRayTracingAmbientOcclusionSamplesPerPixel,
	TEXT("Sets the samples-per-pixel for ambient occlusion (default = 1)")
);

static float GRayTracingAmbientOcclusionMaxRayDistance = 1.0e27;
static FAutoConsoleVariableRef CVarRayTracingAmbientOcclusionMaxRayDistance(
	TEXT("r.RayTracing.AmbientOcclusion.MaxRayDistance"),
	GRayTracingAmbientOcclusionMaxRayDistance,
	TEXT("Sets the maximum ray distance for ambient occlusion rays (default = 1.0e27)")
);

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FAmbientOcclusionData, )
SHADER_PARAMETER(int, SamplesPerPixel)
SHADER_PARAMETER(float, MaxRayDistance)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FAmbientOcclusionData, "AmbientOcclusion");

DECLARE_GPU_STAT_NAMED(RayTracingAmbientOcclusion, TEXT("RTAO"));
DECLARE_GPU_STAT_NAMED(CompositeRayTracingAmbientOcclusion, TEXT("Denoising: RTAO"));

class FAmbientOcclusionRGS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FAmbientOcclusionRGS, Global)

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	FAmbientOcclusionRGS() {}
	virtual ~FAmbientOcclusionRGS() {}

	FAmbientOcclusionRGS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ViewParameter.Bind(Initializer.ParameterMap, TEXT("View"));
		TLASParameter.Bind(Initializer.ParameterMap, TEXT("TLAS"));
		SceneTexturesParameter.Bind(Initializer.ParameterMap, TEXT("SceneTexturesStruct"));
		AmbientOcclusionParameter.Bind(Initializer.ParameterMap, TEXT("AmbientOcclusion"));

		OcclusionMaskUAVParameter.Bind(Initializer.ParameterMap, TEXT("RWOcclusionMaskUAV"));
		RayDistanceUAVParameter.Bind(Initializer.ParameterMap, TEXT("RWRayDistanceUAV"));
	}

	bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ViewParameter;
		Ar << TLASParameter;
		Ar << SceneTexturesParameter;
		Ar << AmbientOcclusionParameter;
		Ar << OcclusionMaskUAVParameter;
		Ar << RayDistanceUAVParameter;
		return bShaderHasOutdatedParameters;
	}

	void Dispatch(
		FRHICommandListImmediate& RHICmdList,
		const FRayTracingScene& RayTracingScene,
		FUniformBufferRHIParamRef ViewUniformBuffer,
		FUniformBufferRHIParamRef SceneTexturesUniformBuffer,
		FUniformBufferRHIParamRef AmbientOcclusionUniformBuffer,
		FUnorderedAccessViewRHIParamRef OcclusionMaskUAV,
		FUnorderedAccessViewRHIParamRef RayDistanceUAV,
		uint32 Width, uint32 Height
	)
	{
		FRayTracingPipelineStateInitializer Initializer;
		Initializer.RayGenShaderRHI = GetRayTracingShader();

		FRHIRayTracingPipelineState* Pipeline = PipelineStateCache::GetAndOrCreateRayTracingPipelineState(Initializer); // #dxr_todo: this should be done once at load-time and cached

		FRayTracingShaderBindingsWriter GlobalResources;
		GlobalResources.Set(TLASParameter, RHIGetAccelerationStructureShaderResourceView(RayTracingScene.RayTracingSceneRHI));
		GlobalResources.Set(ViewParameter, ViewUniformBuffer);
		GlobalResources.Set(SceneTexturesParameter, SceneTexturesUniformBuffer);
		GlobalResources.Set(AmbientOcclusionParameter, AmbientOcclusionUniformBuffer);
		GlobalResources.Set(OcclusionMaskUAVParameter, OcclusionMaskUAV);
		GlobalResources.Set(RayDistanceUAVParameter, RayDistanceUAV);

		RHICmdList.RayTraceDispatch(Pipeline, GlobalResources, Width, Height);
	}

private:
	// Input
	FShaderResourceParameter TLASParameter;
	FShaderUniformBufferParameter ViewParameter;
	FShaderUniformBufferParameter SceneTexturesParameter;
	FShaderUniformBufferParameter AmbientOcclusionParameter;

	// Output
	FShaderResourceParameter OcclusionMaskUAVParameter;
	FShaderResourceParameter RayDistanceUAVParameter;
};

IMPLEMENT_SHADER_TYPE(, FAmbientOcclusionRGS, TEXT("/Engine/Private/RayTracing/RayTracingAmbientOcclusionRGS.usf"), TEXT("AmbientOcclusionRGS"), SF_RayGen)

void FDeferredShadingSceneRenderer::RenderRayTracingAmbientOcclusion(
	FRHICommandListImmediate& RHICmdList,
	const FLightSceneInfo* SkyLightSceneInfo,
	TRefCountPtr<IPooledRenderTarget>& AmbientOcclusionRT
)
{
	SCOPED_DRAW_EVENT(RHICmdList, RayTracingAmbientOcclusion);
	SCOPED_GPU_STAT(RHICmdList, RayTracingAmbientOcclusion);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	FPooledRenderTargetDesc Desc = SceneContext.GetSceneColor()->GetDesc();
	Desc.Format = PF_FloatRGBA;
	Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, AmbientOcclusionRT, TEXT("RayTracingAmbientOcclusion"));
	ClearUAV(RHICmdList, AmbientOcclusionRT->GetRenderTargetItem(), FLinearColor::Black);

	TRefCountPtr<IPooledRenderTarget> RayDistanceRT;
	Desc = SceneContext.GetSceneColor()->GetDesc();
	Desc.Format = PF_R16F;
	Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, RayDistanceRT, TEXT("RayTracingAmbientOcclusionDistance"));
	ClearUAV(RHICmdList, RayDistanceRT->GetRenderTargetItem(), FLinearColor::Black);

	// Add ambient occlusion parameters to uniform buffer
	FAmbientOcclusionData AmbientOcclusionData;
	AmbientOcclusionData.SamplesPerPixel = GRayTracingAmbientOcclusionSamplesPerPixel;
	AmbientOcclusionData.MaxRayDistance = GRayTracingAmbientOcclusionMaxRayDistance;
	FUniformBufferRHIRef AmbientOcclusionUniformBuffer = RHICreateUniformBuffer(&AmbientOcclusionData, FAmbientOcclusionData::StaticStructMetadata.GetLayout(), EUniformBufferUsage::UniformBuffer_SingleDraw);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		FViewInfo& View = Views[ViewIndex];
		FIntPoint ViewSize = View.ViewRect.Size();

		TShaderMapRef<FAmbientOcclusionRGS> AmbientOcclusionRayGenerationShader(GetGlobalShaderMap(FeatureLevel));
		FSceneTexturesUniformParameters SceneTextures;
		SetupSceneTextureUniformParameters(SceneContext, FeatureLevel, ESceneTextureSetupMode::All, SceneTextures);
		FUniformBufferRHIRef SceneTexturesUniformBuffer = RHICreateUniformBuffer(&SceneTextures, FSceneTexturesUniformParameters::StaticStructMetadata.GetLayout(), EUniformBufferUsage::UniformBuffer_SingleDraw);

		AmbientOcclusionRayGenerationShader->Dispatch(
			RHICmdList,
			View.PerViewRayTracingScene,
			View.ViewUniformBuffer,
			SceneTexturesUniformBuffer,
			AmbientOcclusionUniformBuffer,
			AmbientOcclusionRT->GetRenderTargetItem().UAV,
			RayDistanceRT->GetRenderTargetItem().UAV,
			ViewSize.X, ViewSize.Y
		);
	}

	// Transition to graphics pipeline
	FComputeFenceRHIRef Fence = RHICmdList.CreateComputeFence(TEXT("RayTracingAmbientOcclusion"));
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToGfx, AmbientOcclusionRT->GetRenderTargetItem().UAV, Fence);
	GVisualizeTexture.SetCheckPoint(RHICmdList, AmbientOcclusionRT);

	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToGfx, RayDistanceRT->GetRenderTargetItem().UAV);
	GVisualizeTexture.SetCheckPoint(RHICmdList, RayDistanceRT);
}

class FCompositeAmbientOcclusionPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCompositeAmbientOcclusionPS, Global)

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	FCompositeAmbientOcclusionPS() {}
	virtual ~FCompositeAmbientOcclusionPS() {}

	FCompositeAmbientOcclusionPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer);
		AmbientOcclusionTextureParameter.Bind(Initializer.ParameterMap, TEXT("AmbientOcclusionTexture"));
		AmbientOcclusionTextureSamplerParameter.Bind(Initializer.ParameterMap, TEXT("AmbientOcclusionTextureSampler"));
	}

	bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SceneTextureParameters;
		Ar << AmbientOcclusionTextureParameter;
		Ar << AmbientOcclusionTextureSamplerParameter;
		return bShaderHasOutdatedParameters;
	}

	template<typename TRHICommandList>
	void SetParameters(
		TRHICommandList& RHICmdList,
		const FViewInfo& View,
		FTextureRHIParamRef AmbientOcclusionTexture
	)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		SceneTextureParameters.Set(RHICmdList, ShaderRHI, View.FeatureLevel, ESceneTextureSetupMode::All);

		SetTextureParameter(RHICmdList, ShaderRHI, AmbientOcclusionTextureParameter, AmbientOcclusionTextureSamplerParameter, TStaticSamplerState<SF_Bilinear>::GetRHI(), AmbientOcclusionTexture);
	}

private:
	FSceneTextureShaderParameters SceneTextureParameters;

	FShaderResourceParameter AmbientOcclusionTextureParameter;
	FShaderResourceParameter AmbientOcclusionTextureSamplerParameter;
};

IMPLEMENT_SHADER_TYPE(, FCompositeAmbientOcclusionPS, TEXT("/Engine/Private/RayTracing/CompositeAmbientOcclusionPS.usf"), TEXT("CompositeAmbientOcclusionPS"), SF_Pixel)

void FDeferredShadingSceneRenderer::CompositeRayTracingAmbientOcclusion(
	FRHICommandListImmediate& RHICmdList,
	TRefCountPtr<IPooledRenderTarget>& AmbientOcclusionRT
)
{
	SCOPED_DRAW_EVENT(RHICmdList, CompositeRayTracingAmbientOcclusion);
	SCOPED_GPU_STAT(RHICmdList, CompositeRayTracingAmbientOcclusion);

	auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef<FPostProcessVS> VertexShader(ShaderMap);
	TShaderMapRef<FCompositeAmbientOcclusionPS> PixelShader(ShaderMap);
	FGraphicsPipelineStateInitializer GraphicsPSOInit;

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, true);
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	// Multiply scene color by ambient term
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_Zero, BF_SourceColor, BO_Add, BF_One, BF_One>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
		FTextureRHIParamRef AmbientOcclusionTexture = AmbientOcclusionRT->GetRenderTargetItem().ShaderResourceTexture;
		PixelShader->SetParameters(RHICmdList, View, AmbientOcclusionTexture);
		DrawRectangle(
			RHICmdList,
			0, 0,
			View.ViewRect.Width(), View.ViewRect.Height(),
			View.ViewRect.Min.X, View.ViewRect.Min.Y,
			View.ViewRect.Width(), View.ViewRect.Height(),
			FIntPoint(View.ViewRect.Width(), View.ViewRect.Height()),
			SceneContext.GetBufferSizeXY(),
			*VertexShader
		);
	}

	ResolveSceneColor(RHICmdList);
	SceneContext.FinishRenderingSceneColor(RHICmdList);
}

#endif // RHI_RAYTRACING
