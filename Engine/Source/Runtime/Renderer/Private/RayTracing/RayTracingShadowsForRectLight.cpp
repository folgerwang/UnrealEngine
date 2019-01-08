// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DeferredShadingRenderer.h"

#if RHI_RAYTRACING

#include "ClearQuad.h"
#include "SceneRendering.h"
#include "SceneRenderTargets.h"
#include "RenderTargetPool.h"
#include "RHIResources.h"
#include "UniformBuffer.h"
#include "VisualizeTexture.h"

#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"
#include "RectLightSceneProxy.h"

static int32 GRayTracingRectLight = 1;
static FAutoConsoleVariableRef CVarRayTracingRectLight(
	TEXT("r.RayTracing.RectLight"),
	GRayTracingRectLight,
	TEXT("0: use traditional rasterized shadow map\n")
	TEXT("1: use ray gen shader (default)\n")
);

static int32 GRayTracingRectLightSamplesPerPixel = 1;
static FAutoConsoleVariableRef CVarRayTracingRecLightSamplesPerPixel(
	TEXT("r.RayTracing.RectLight.SamplesPerPixel"),
	GRayTracingRectLightSamplesPerPixel,
	TEXT("Sets the samples-per-pixel for directional light occlusion (default = 1)")
);

static int32 GRayTracingRectLightIsTextureImportanceSampling = 1;
static FAutoConsoleVariableRef CVarRayTracingRecLightIsTextureImportanceSampling(
	TEXT("r.RayTracing.RectLight.IsTextureImportanceSampling"),
	GRayTracingRectLightIsTextureImportanceSampling,
	TEXT("Sets the samples-per-pixel for directional light occlusion (default = 1)")
);

bool IsRayTracingRectLightSelected()
{
	return IsRayTracingEnabled() && GRayTracingRectLight == 1;
}

bool ShouldRenderRayTracingStaticOrStationaryRectLight(const FLightSceneInfo& LightSceneInfo)
{
	return IsRayTracingRectLightSelected()
		&& LightSceneInfo.Proxy->GetLightType() == LightType_Rect
		&& !LightSceneInfo.Proxy->IsMovable();
}

bool ShouldRenderRayTracingDynamicRectLight(const FLightSceneInfo& LightSceneInfo)
{
	return IsRayTracingRectLightSelected()
		&& LightSceneInfo.Proxy->GetLightType() == LightType_Rect
		&& LightSceneInfo.Proxy->IsMovable();
}

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FRectLightData, )
// Pass settings
SHADER_PARAMETER(int, SamplesPerPixel)
SHADER_PARAMETER(int, bIsTextureImportanceSampling)
// Light data
SHADER_PARAMETER(FVector, Position)
SHADER_PARAMETER(FVector, Normal)
SHADER_PARAMETER(FVector, dPdu)
SHADER_PARAMETER(FVector, dPdv)
SHADER_PARAMETER(FVector, Color)
SHADER_PARAMETER(float, Width)
SHADER_PARAMETER(float, Height)
SHADER_PARAMETER(FIntVector, MipTreeDimensions)
SHADER_PARAMETER_TEXTURE(Texture2D, Texture)
SHADER_PARAMETER_SAMPLER(SamplerState, TextureSampler)
// Sampling data
SHADER_PARAMETER_SRV(Buffer<float>, MipTree)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

DECLARE_GPU_STAT_NAMED(RayTracingRectLight, TEXT("Ray Tracing RectLight"));
DECLARE_GPU_STAT_NAMED(RayTracingRectLightOcclusion, TEXT("Ray Tracing RectLight Occlusion"));
DECLARE_GPU_STAT_NAMED(BuildRectLightMipTree, TEXT("Build RectLight Mip Tree"));

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FRectLightData, "RectLight");

template <int CalcDirectLighting, int EncodeVisibility, int TextureImportanceSampling>
class FRectLightOcclusionRGS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRectLightOcclusionRGS, Global)

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("CALC_DIRECT_LIGHTING"), CalcDirectLighting);
		OutEnvironment.SetDefine(TEXT("ENCODE_VISIBILITY"), EncodeVisibility);
		OutEnvironment.SetDefine(TEXT("TEXTURE_IMPORTANCE_SAMPLING"), TextureImportanceSampling);
	}

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	FRectLightOcclusionRGS() {}
	virtual ~FRectLightOcclusionRGS() {}

	FRectLightOcclusionRGS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ViewParameter.Bind(Initializer.ParameterMap, TEXT("View"));
		SceneTexturesParameter.Bind(Initializer.ParameterMap, TEXT("SceneTexturesStruct"));
		RectLightParameter.Bind(Initializer.ParameterMap, TEXT("RectLight"));
		TLASParameter.Bind(Initializer.ParameterMap, TEXT("TLAS"));

		OcclusionMaskUAVParameter.Bind(Initializer.ParameterMap, TEXT("RWOcclusionMaskUAV"));
		RayDistanceUAVParameter.Bind(Initializer.ParameterMap, TEXT("RWRayDistanceUAV"));
	}

	bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ViewParameter;
		Ar << SceneTexturesParameter;
		Ar << RectLightParameter;
		Ar << TLASParameter;
		Ar << OcclusionMaskUAVParameter;
		Ar << RayDistanceUAVParameter;
		return bShaderHasOutdatedParameters;
	}

	void Dispatch(
		FRHICommandListImmediate& RHICmdList,
		const FRayTracingScene& RayTracingScene,
		FUniformBufferRHIParamRef ViewUniformBuffer,
		FUniformBufferRHIParamRef SceneTexturesUniformBuffer,
		FUniformBufferRHIParamRef RectLightUniformBuffer,
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
		GlobalResources.Set(RectLightParameter, RectLightUniformBuffer);
		GlobalResources.Set(OcclusionMaskUAVParameter, OcclusionMaskUAV);
		GlobalResources.Set(RayDistanceUAVParameter, RayDistanceUAV);

		RHICmdList.RayTraceDispatch(Pipeline, GlobalResources, Width, Height);
	}

private:
	// Input
	FShaderResourceParameter TLASParameter;
	FShaderUniformBufferParameter ViewParameter;
	FShaderUniformBufferParameter SceneTexturesParameter;
	FShaderUniformBufferParameter RectLightParameter;

	// Output
	FShaderResourceParameter OcclusionMaskUAVParameter;
	FShaderResourceParameter RayDistanceUAVParameter;
};

#define IMPLEMENT_RECT_LIGHT_OCCLUSION_TYPE(CalcDirectLightingType, EncodeVisbilityType, TextureImportanceSampling) \
	typedef FRectLightOcclusionRGS<CalcDirectLightingType, EncodeVisbilityType, TextureImportanceSampling> FRectLightOcclusionRGS##CalcDirectLightingType##EncodeVisbilityType##TextureImportanceSampling; \
	IMPLEMENT_SHADER_TYPE(template<>, FRectLightOcclusionRGS##CalcDirectLightingType##EncodeVisbilityType##TextureImportanceSampling, TEXT("/Engine/Private/RayTracing/RayTracingRectLightOcclusionRGS.usf"), TEXT("RectLightOcclusionRGS"), SF_RayGen);

IMPLEMENT_RECT_LIGHT_OCCLUSION_TYPE(0, 0, 0);
IMPLEMENT_RECT_LIGHT_OCCLUSION_TYPE(0, 0, 1);
IMPLEMENT_RECT_LIGHT_OCCLUSION_TYPE(0, 1, 0);
IMPLEMENT_RECT_LIGHT_OCCLUSION_TYPE(0, 1, 1);
IMPLEMENT_RECT_LIGHT_OCCLUSION_TYPE(1, 0, 0);
IMPLEMENT_RECT_LIGHT_OCCLUSION_TYPE(1, 0, 1);

class FVisualizeRectLightMipTreePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FVisualizeRectLightMipTreePS, Global);

public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	FVisualizeRectLightMipTreePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DimensionsParameter.Bind(Initializer.ParameterMap, TEXT("Dimensions"));
		MipTreeParameter.Bind(Initializer.ParameterMap, TEXT("MipTree"));
	}

	FVisualizeRectLightMipTreePS() {}

	template<typename TRHICommandList>
	void SetParameters(
		TRHICommandList& RHICmdList,
		const FViewInfo& View,
		const FRWBuffer& MipTree,
		const FIntVector Dimensions)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		SetShaderValue(RHICmdList, ShaderRHI, DimensionsParameter, Dimensions);
		SetSRVParameter(RHICmdList, ShaderRHI, MipTreeParameter, MipTree.SRV);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DimensionsParameter;
		Ar << MipTreeParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter DimensionsParameter;
	FShaderResourceParameter MipTreeParameter;
};

IMPLEMENT_SHADER_TYPE(, FVisualizeRectLightMipTreePS, TEXT("/Engine/Private/PathTracing/VisualizeMipTreePixelShader.usf"), TEXT("VisualizeMipTreePS"), SF_Pixel)

void FDeferredShadingSceneRenderer::VisualizeRectLightMipTree(
	FRHICommandListImmediate& RHICmdList,
	const FViewInfo& View,
	const FRWBuffer& RectLightMipTree,
	const FIntVector& RectLightMipTreeDimensions
)
{
	// Allocate render target
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	FPooledRenderTargetDesc Desc = SceneContext.GetSceneColor()->GetDesc();
	Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
	TRefCountPtr<IPooledRenderTarget> RectLightMipTreeRT;
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, RectLightMipTreeRT, TEXT("RectLightMipTreeRT"));

	// Define shaders
	const auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef<FPostProcessVS> VertexShader(ShaderMap);
	TShaderMapRef<FVisualizeRectLightMipTreePS> PixelShader(ShaderMap);
	FTextureRHIParamRef RenderTargets[2] =
	{
		SceneContext.GetSceneColor()->GetRenderTargetItem().TargetableTexture,
		RectLightMipTreeRT->GetRenderTargetItem().TargetableTexture
	};
	SetRenderTargets(RHICmdList, 2, RenderTargets, SceneContext.GetSceneDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilNop);

	// PSO definition
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, true);
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	// Transition to graphics
	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, RectLightMipTree.UAV);

	// Draw
	RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
	PixelShader->SetParameters(RHICmdList, View, RectLightMipTree, RectLightMipTreeDimensions);
	DrawRectangle(
		RHICmdList,
		0, 0,
		View.ViewRect.Width(), View.ViewRect.Height(),
		View.ViewRect.Min.X, View.ViewRect.Min.Y,
		View.ViewRect.Width(), View.ViewRect.Height(),
		FIntPoint(View.ViewRect.Width(), View.ViewRect.Height()),
		SceneContext.GetBufferSizeXY(),
		*VertexShader);
	ResolveSceneColor(RHICmdList);
	GVisualizeTexture.SetCheckPoint(RHICmdList, RectLightMipTreeRT);

	// Transition to compute
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, RectLightMipTree.UAV);

	ResolveSceneColor(RHICmdList);
	SceneContext.FinishRenderingSceneColor(RHICmdList);
}

void FDeferredShadingSceneRenderer::RenderRayTracingRectLight(
	FRHICommandListImmediate& RHICmdList,
	const FLightSceneInfo& RectLightSceneInfo,
	TRefCountPtr<IPooledRenderTarget>& RectLightRT,
	TRefCountPtr<IPooledRenderTarget>& HitDistanceRT
)
{
	SCOPED_DRAW_EVENT(RHICmdList, RayTracingRectLight);
	SCOPED_GPU_STAT(RHICmdList, RayTracingRectLight);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	FPooledRenderTargetDesc Desc = SceneContext.GetSceneColor()->GetDesc();
	Desc.Format = PF_FloatRGBA;
	Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, RectLightRT, TEXT("RayTracingRectLight"));
	//ClearUAV(RHICmdList, RectLightRT->GetRenderTargetItem(), FLinearColor::Black);

	Desc.Format = PF_R16F;
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, HitDistanceRT, TEXT("RayTracingRectLightDistance"));
	//ClearUAV(RHICmdList, HitDistanceRT->GetRenderTargetItem(), FLinearColor::Black);

	if (RectLightSceneInfo.Proxy->HasSourceTexture())
	{
		RenderRayTracingRectLightInternal<1, 0, 1>(RHICmdList, RectLightSceneInfo, RectLightRT, HitDistanceRT);
	}
	else
	{
		RenderRayTracingRectLightInternal<1, 0, 0>(RHICmdList, RectLightSceneInfo, RectLightRT, HitDistanceRT);
	}
}

void FDeferredShadingSceneRenderer::RenderRayTracingOcclusionForRectLight(
	FRHICommandListImmediate& RHICmdList,
	const FLightSceneInfo& RectLightSceneInfo,
	TRefCountPtr<IPooledRenderTarget>& ScreenShadowMaskTexture
)
{
	SCOPED_DRAW_EVENT(RHICmdList, RayTracingRectLightOcclusion);
	SCOPED_GPU_STAT(RHICmdList, RayTracingRectLightOcclusion);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	FPooledRenderTargetDesc Desc = SceneContext.GetSceneColor()->GetDesc();
	Desc.Format = PF_FloatRGBA;
	Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, ScreenShadowMaskTexture, TEXT("RayTracingRectLightOcclusion"));
	//ClearUAV(RHICmdList, ScreenShadowMaskTexture->GetRenderTargetItem(), FLinearColor::Black);

	TRefCountPtr<IPooledRenderTarget> HitDistanceTexture;
	Desc.Format = PF_R16F;
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, HitDistanceTexture, TEXT("RayTracingRectLightDistance"));
	//ClearUAV(RHICmdList, HitDistanceTexture->GetRenderTargetItem(), FLinearColor::Black);

	if (RectLightSceneInfo.Proxy->HasSourceTexture())
	{
		RenderRayTracingRectLightInternal<0, 1, 1>(RHICmdList, RectLightSceneInfo, ScreenShadowMaskTexture, HitDistanceTexture);
	}
	else
	{
		RenderRayTracingRectLightInternal<0, 1, 0>(RHICmdList, RectLightSceneInfo, ScreenShadowMaskTexture, HitDistanceTexture);
	}
}

template <int CalcDirectLighting, int EncodeVisibility, int TextureImportanceSampling>
void FDeferredShadingSceneRenderer::RenderRayTracingRectLightInternal(
	FRHICommandListImmediate& RHICmdList,
	const FLightSceneInfo& RectLightSceneInfo,
	TRefCountPtr<IPooledRenderTarget>& ScreenShadowMaskTexture,
	TRefCountPtr<IPooledRenderTarget>& RayDistanceTexture
)
{
	check(RectLightSceneInfo.Proxy);
	check(RectLightSceneInfo.Proxy->IsRectLight());
	FRectLightSceneProxy* RectLightSceneProxy = (FRectLightSceneProxy*) RectLightSceneInfo.Proxy;

	FLightShaderParameters LightShaderParameters;
	RectLightSceneProxy->GetLightShaderParameters(LightShaderParameters);

	FRectLightData RectLightData;
	RectLightData.SamplesPerPixel = GRayTracingRectLightSamplesPerPixel;
	RectLightData.bIsTextureImportanceSampling = GRayTracingRectLightIsTextureImportanceSampling;
	RectLightData.Position = RectLightSceneInfo.Proxy->GetOrigin();
	RectLightData.Normal = RectLightSceneInfo.Proxy->GetDirection();
	const FMatrix& WorldToLight = RectLightSceneInfo.Proxy->GetWorldToLight();
	RectLightData.dPdu = FVector(WorldToLight.M[0][1], WorldToLight.M[1][1], WorldToLight.M[2][1]);
	RectLightData.dPdv = FVector(WorldToLight.M[0][2], WorldToLight.M[1][2], WorldToLight.M[2][2]);
	RectLightData.Color = LightShaderParameters.Color / 2.0;

	// #dxr_todo: Ray traced textured area lights are 1.5X brighter than those in lit mode.
	if (RectLightSceneProxy->HasSourceTexture())
	{
		RectLightData.Color *= 2.0 / 3.0;
	}

	RectLightData.Width = 2.0f * LightShaderParameters.SourceRadius;
	RectLightData.Height = 2.0f * LightShaderParameters.SourceLength;
	RectLightData.Texture = LightShaderParameters.SourceTexture;
	RectLightData.TextureSampler = RHICreateSamplerState(FSamplerStateInitializerRHI(SF_Bilinear, AM_Border, AM_Border, AM_Border));
	RectLightData.MipTree = RectLightSceneProxy->RectLightMipTree.SRV;
	RectLightData.MipTreeDimensions = RectLightSceneProxy->RectLightMipTreeDimensions;
	FUniformBufferRHIRef RectLightUniformBuffer = RHICreateUniformBuffer(&RectLightData, FRectLightData::StaticStructMetadata.GetLayout(), EUniformBufferUsage::UniformBuffer_SingleDraw);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		FViewInfo& View = Views[ViewIndex];
		FIntPoint ViewSize = View.ViewRect.Size();

		TShaderMapRef<FRectLightOcclusionRGS<CalcDirectLighting, EncodeVisibility, TextureImportanceSampling>> RectLightOcclusionRayGenerationShader(GetGlobalShaderMap(FeatureLevel));

		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		FSceneTexturesUniformParameters SceneTextures;
		SetupSceneTextureUniformParameters(SceneContext, FeatureLevel, ESceneTextureSetupMode::All, SceneTextures);
		FUniformBufferRHIRef SceneTexturesUniformBuffer = RHICreateUniformBuffer(&SceneTextures, FSceneTexturesUniformParameters::StaticStructMetadata.GetLayout(), EUniformBufferUsage::UniformBuffer_SingleDraw);

		// Dispatch
		RectLightOcclusionRayGenerationShader->Dispatch(
			RHICmdList,
			View.PerViewRayTracingScene,
			View.ViewUniformBuffer,
			SceneTexturesUniformBuffer,
			RectLightUniformBuffer,
			ScreenShadowMaskTexture->GetRenderTargetItem().UAV,
			RayDistanceTexture->GetRenderTargetItem().UAV,
			ViewSize.X, ViewSize.Y
		);
	}

	// Transition out to graphics pipeline
	FComputeFenceRHIRef Fence = RHICmdList.CreateComputeFence(TEXT("RayTracingRectLight"));
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToGfx, ScreenShadowMaskTexture->GetRenderTargetItem().UAV, Fence);
	GVisualizeTexture.SetCheckPoint(RHICmdList, ScreenShadowMaskTexture);

	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToGfx, RayDistanceTexture->GetRenderTargetItem().UAV);
	GVisualizeTexture.SetCheckPoint(RHICmdList, RayDistanceTexture);
}

#endif