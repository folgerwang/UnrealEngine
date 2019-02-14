// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DeferredShadingRenderer.h"

#if RHI_RAYTRACING

#include "RayTracingSkyLight.h"
#include "ScenePrivate.h"
#include "SceneRenderTargets.h"
#include "RenderGraphBuilder.h"
#include "RenderTargetPool.h"
#include "RHIResources.h"
#include "UniformBuffer.h"
#include "RayGenShaderUtils.h"
#include "PathTracingUniformBuffers.h"
#include "SceneViewFamilyBlackboard.h"
#include "ScreenSpaceDenoise.h"
#include "ClearQuad.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"
#include "Raytracing/RaytracingOptions.h"

static int32 GRayTracingGlobalIllumination = 0;
static FAutoConsoleVariableRef CVarRayTracingGlobalIllumination(
	TEXT("r.RayTracing.GlobalIllumination"),
	GRayTracingGlobalIllumination,
	TEXT("Enabled ray tracing global illumination (default = 0)")
);

static int32 GRayTracingGlobalIlluminationSamplesPerPixel = 1;
static FAutoConsoleVariableRef CVarRayTracingGlobalIlluminationSamplesPerPixel(
	TEXT("r.RayTracing.GlobalIllumination.SamplesPerPixel"),
	GRayTracingGlobalIlluminationSamplesPerPixel,
	TEXT("Samples per pixel (default = 1)")
);

static float GRayTracingGlobalIlluminationMaxRayDistance = 1.0e27;
static FAutoConsoleVariableRef CVarRayTracingGlobalIlluminationMaxRayDistance(
	TEXT("r.RayTracing.GlobalIllumination.MaxRayDistance"),
	GRayTracingGlobalIlluminationMaxRayDistance,
	TEXT("Max ray distance (default = 1.0e27)")
);

static int32 GRayTracingGlobalIlluminationMaxBounces = 1;
static FAutoConsoleVariableRef CVarRayTracingGlobalIlluminationMaxBounces(
	TEXT("r.RayTracing.GlobalIllumination.MaxBounces"),
	GRayTracingGlobalIlluminationMaxBounces,
	TEXT("Max bounces (default = 1)")
);

static int32 GRayTracingGlobalIlluminationDenoiser = 1;
static FAutoConsoleVariableRef CVarRayTracingGlobalIlluminationDenoiser(
	TEXT("r.RayTracing.GlobalIllumination.Denoiser"),
	GRayTracingGlobalIlluminationDenoiser,
	TEXT("Denoising options (default = 1)")
);

static int32 GRayTracingGlobalIlluminationEvalSkyLight = 0;
static FAutoConsoleVariableRef CVarRayTracingGlobalIlluminationEvalSkyLight(
	TEXT("r.RayTracing.GlobalIllumination.EvalSkyLight"),
	GRayTracingGlobalIlluminationEvalSkyLight,
	TEXT("Evaluate SkyLight multi-bounce contribution")
);

static float GRayTracingGlobalIlluminationScreenPercentage = 100.0;
static FAutoConsoleVariableRef CVarRayTracingGlobalIlluminationScreenPercentage(
	TEXT("r.RayTracing.GlobalIllumination.ScreenPercentage"),
	GRayTracingGlobalIlluminationScreenPercentage,
	TEXT("Screen percentage for ray tracing global illumination (default = 100)")
);

static const int32 GLightCountMax = 64;

DECLARE_GPU_STAT_NAMED(RayTracingGlobalIllumination, TEXT("Ray Tracing Global Illumination"));

void SetupLightParameters(
	const TSparseArray<FLightSceneInfoCompact>& Lights,
	const FViewInfo& View,
	FPathTracingLightData* LightParameters)
{
	LightParameters->Count = 0;
	for (auto Light : Lights)
	{
		if (LightParameters->Count >= GLightCountMax) break;

		if (Light.LightSceneInfo->Proxy->HasStaticLighting() && Light.LightSceneInfo->IsPrecomputedLightingValid()) continue;

		FLightShaderParameters LightShaderParameters;
		Light.LightSceneInfo->Proxy->GetLightShaderParameters(LightShaderParameters);

		ELightComponentType LightComponentType = (ELightComponentType)Light.LightSceneInfo->Proxy->GetLightType();
		switch (LightComponentType)
		{
		case LightType_Directional:
		{
			LightParameters->Type[LightParameters->Count] = 2;
			LightParameters->Normal[LightParameters->Count] = LightShaderParameters.Direction;
			LightParameters->Color[LightParameters->Count] = LightShaderParameters.Color;
			break;
		}
		case LightType_Rect:
		{
			LightParameters->Type[LightParameters->Count] = 3;
			LightParameters->Position[LightParameters->Count] = LightShaderParameters.Position;
			LightParameters->Normal[LightParameters->Count] = -LightShaderParameters.Direction;
			LightParameters->dPdu[LightParameters->Count] = FVector::CrossProduct(LightShaderParameters.Direction, LightShaderParameters.Tangent);
			LightParameters->dPdv[LightParameters->Count] = LightShaderParameters.Tangent;
			LightParameters->Color[LightParameters->Count] = LightShaderParameters.Color / 4.0;
			LightParameters->Dimensions[LightParameters->Count] = FVector(2.0f * LightShaderParameters.SourceRadius, 2.0f * LightShaderParameters.SourceLength, 0.0f);
			break;
		}
		case LightType_Point:
		default:
		{
			LightParameters->Type[LightParameters->Count] = 1;
			LightParameters->Position[LightParameters->Count] = LightShaderParameters.Position;
			LightParameters->Color[LightParameters->Count] = LightShaderParameters.Color;
			break;
		}
		case LightType_Spot:
		{
			LightParameters->Type[LightParameters->Count] = 4;
			LightParameters->Position[LightParameters->Count] = LightShaderParameters.Position;
			LightParameters->Normal[LightParameters->Count] = -LightShaderParameters.Direction;
			LightParameters->Color[LightParameters->Count] = 4.0 * PI * LightShaderParameters.Color;
			LightParameters->Dimensions[LightParameters->Count] = FVector(LightShaderParameters.SpotAngles, 0.0);
			break;
		}
		};

		LightParameters->Count++;
	}
}

void SetupSkyLightParameters(
	const FScene& Scene,
	FSkyLightData* SkyLight
)
{
	// dxr_todo: factor out these pass constants
	SkyLight->SamplesPerPixel = 0;
	SkyLight->SamplingStopLevel = 0;

	if (Scene.SkyLight)
	{
		SkyLight->Color = FVector(Scene.SkyLight->LightColor);
		SkyLight->Texture = Scene.SkyLight->ProcessedTexture->TextureRHI;
		SkyLight->TextureSampler = Scene.SkyLight->ProcessedTexture->SamplerStateRHI;
		SkyLight->MipDimensions = Scene.SkyLight->SkyLightMipDimensions;
	}
	else
	{
		SkyLight->Color = FVector(0.0);
		SkyLight->Texture = GBlackTextureCube->TextureRHI;
		SkyLight->TextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		SkyLight->MipDimensions = FIntVector(0);
	}

	// dxr_todo: Sky light importance sampling is currently disabled
	auto BlackTextureBuffer = RHICreateShaderResourceView(GBlackTexture->TextureRHI->GetTexture2D(), 0);
	SkyLight->MipTreePosX = BlackTextureBuffer;
	SkyLight->MipTreeNegX = BlackTextureBuffer;
	SkyLight->MipTreePosY = BlackTextureBuffer;
	SkyLight->MipTreeNegY = BlackTextureBuffer;
	SkyLight->MipTreePosZ = BlackTextureBuffer;
	SkyLight->MipTreeNegZ = BlackTextureBuffer;

	SkyLight->MipTreePdfPosX = BlackTextureBuffer;
	SkyLight->MipTreePdfNegX = BlackTextureBuffer;
	SkyLight->MipTreePdfPosY = BlackTextureBuffer;
	SkyLight->MipTreePdfNegY = BlackTextureBuffer;
	SkyLight->MipTreePdfPosZ = BlackTextureBuffer;
	SkyLight->MipTreePdfNegZ = BlackTextureBuffer;
	SkyLight->SolidAnglePdf = BlackTextureBuffer;
}

bool ShouldRenderRayTracingGlobalIllumination()
{
	return GRayTracingGlobalIllumination == 1;
}

class FGlobalIlluminationRGS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGlobalIlluminationRGS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FGlobalIlluminationRGS, FGlobalShader)

	using FPermutationDomain = TShaderPermutationDomain<>;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, SamplesPerPixel)
		SHADER_PARAMETER(uint32, MaxBounces)
		SHADER_PARAMETER(uint32, UpscaleFactor)
		SHADER_PARAMETER(float, MaxRayDistance)
		SHADER_PARAMETER(bool, EvalSkyLight)
		SHADER_PARAMETER(float, MaxNormalBias)

		SHADER_PARAMETER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWGlobalIlluminationUAV)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWRayDistanceUAV)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_STRUCT_REF(FSceneTexturesUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_STRUCT_REF(FPathTracingLightData, LightParameters)
		SHADER_PARAMETER_STRUCT_REF(FSkyLightData, SkyLight)
	END_SHADER_PARAMETER_STRUCT()
};

class FRayTracingGlobalIlluminationCompositePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingGlobalIlluminationCompositePS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingGlobalIlluminationCompositePS, FGlobalShader)

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RENDER_TARGET_BINDING_SLOTS()
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, GlobalIlluminationTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, GlobalIlluminationSampler)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_STRUCT_REF(FSceneTexturesUniformParameters, SceneTexturesStruct)
	END_SHADER_PARAMETER_STRUCT()
};

class FRayTracingGlobalIlluminationSceneColorCompositePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingGlobalIlluminationSceneColorCompositePS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingGlobalIlluminationSceneColorCompositePS, FGlobalShader)

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RENDER_TARGET_BINDING_SLOTS()
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, GlobalIlluminationTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, GlobalIlluminationSampler)
		SHADER_PARAMETER_STRUCT_REF(FSceneTexturesUniformParameters, SceneTexturesStruct)
		END_SHADER_PARAMETER_STRUCT()
};

class FRayTracingGlobalIlluminationCHS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingGlobalIlluminationCHS);
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingGlobalIlluminationCHS, FGlobalShader)

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	using FParameters = FEmptyShaderParameters;
};

class FRayTracingGlobalIlluminationMS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingGlobalIlluminationMS);
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingGlobalIlluminationMS, FGlobalShader)

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	using FParameters = FEmptyShaderParameters;
};

IMPLEMENT_GLOBAL_SHADER(FGlobalIlluminationRGS, "/Engine/Private/RayTracing/RayTracingGlobalIlluminationRGS.usf", "GlobalIlluminationRGS", SF_RayGen);
IMPLEMENT_GLOBAL_SHADER(FRayTracingGlobalIlluminationCHS, "/Engine/Private/RayTracing/RayTracingGlobalIlluminationRGS.usf", "RayTracingGlobalIlluminationCHS", SF_RayHitGroup);
IMPLEMENT_GLOBAL_SHADER(FRayTracingGlobalIlluminationMS, "/Engine/Private/RayTracing/RayTracingGlobalIlluminationRGS.usf", "RayTracingGlobalIlluminationMS", SF_RayMiss);
IMPLEMENT_GLOBAL_SHADER(FRayTracingGlobalIlluminationCompositePS, "/Engine/Private/RayTracing/RayTracingGlobalIlluminationCompositePS.usf", "GlobalIlluminationCompositePS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FRayTracingGlobalIlluminationSceneColorCompositePS, "/Engine/Private/RayTracing/RayTracingGlobalIlluminationCompositePS.usf", "GlobalIlluminationSceneColorCompositePS", SF_Pixel);

void FDeferredShadingSceneRenderer::RenderRayTracingGlobalIllumination(
	FRHICommandListImmediate& RHICmdList,
	const FViewInfo& View,
	TRefCountPtr<IPooledRenderTarget>& GlobalIlluminationRT,
	TRefCountPtr<IPooledRenderTarget>& AmbientOcclusionRT
)
{
	if (!GRayTracingGlobalIllumination) return;
	SCOPED_GPU_STAT(RHICmdList, RayTracingGlobalIllumination);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	{
		FPooledRenderTargetDesc Desc = SceneContext.GetSceneColor()->GetDesc();
		Desc.Format = PF_FloatRGBA;
		Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, GlobalIlluminationRT, TEXT("RayTracingGlobalIllumination"));
	}

	FRDGBuilder GraphBuilder(RHICmdList);

	IScreenSpaceDenoiser::FAmbientOcclusionRayTracingConfig RayTracingConfig;
	RayTracingConfig.ResolutionFraction = 1.0;
	if (GRayTracingGlobalIlluminationDenoiser != 0)
	{
		RayTracingConfig.ResolutionFraction = FMath::Clamp(GRayTracingGlobalIlluminationScreenPercentage / 100.0, 0.25, 1.0);
	}
	RayTracingConfig.RayCountPerPixel = GRayTracingGlobalIlluminationSamplesPerPixel;
	int32 UpscaleFactor = int32(1.0 / RayTracingConfig.ResolutionFraction);

	// Render targets
	FRDGTextureRef GlobalIlluminationTexture;
	{
		FRDGTextureDesc Desc = SceneContext.GetSceneColor()->GetDesc();
		Desc.Extent /= UpscaleFactor;
		Desc.Format = PF_FloatRGBA;
		Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
		GlobalIlluminationTexture = GraphBuilder.CreateTexture(Desc, TEXT("RayTracingGlobalIllumination"));
	}

	FRDGTextureRef RayDistanceTexture;
	{
		FRDGTextureDesc Desc = SceneContext.GetSceneColor()->GetDesc();
		Desc.Extent /= UpscaleFactor;
		Desc.Format = PF_G16R16;
		Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
		RayDistanceTexture = GraphBuilder.CreateTexture(Desc, TEXT("RayTracingGlobalIlluminationRayDistance"));
	}
	FRDGTextureRef ResultTexture;
	
	FSceneTexturesUniformParameters SceneTextures;
	SetupSceneTextureUniformParameters(SceneContext, FeatureLevel, ESceneTextureSetupMode::All, SceneTextures);
	// Ray generation
	{
		FPathTracingLightData LightParameters;
		SetupLightParameters(Scene->Lights, View, &LightParameters);

		FSkyLightData SkyLightParameters;
		SetupSkyLightParameters(*Scene, &SkyLightParameters);

		FGlobalIlluminationRGS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGlobalIlluminationRGS::FParameters>();
		PassParameters->SamplesPerPixel = GRayTracingGlobalIlluminationSamplesPerPixel;
		PassParameters->MaxBounces = GRayTracingGlobalIlluminationMaxBounces;
		PassParameters->MaxNormalBias = GetRaytracingOcclusionMaxNormalBias();
		PassParameters->MaxRayDistance = GRayTracingGlobalIlluminationMaxRayDistance;
		PassParameters->UpscaleFactor = UpscaleFactor;
		PassParameters->EvalSkyLight = GRayTracingGlobalIlluminationEvalSkyLight;
		PassParameters->TLAS = View.PerViewRayTracingScene.RayTracingSceneRHI->GetShaderResourceView();
		PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
		PassParameters->SceneTexturesStruct = CreateUniformBufferImmediate(SceneTextures, EUniformBufferUsage::UniformBuffer_SingleDraw);
		PassParameters->LightParameters = CreateUniformBufferImmediate(LightParameters, EUniformBufferUsage::UniformBuffer_SingleDraw);
		PassParameters->SkyLight = CreateUniformBufferImmediate(SkyLightParameters, EUniformBufferUsage::UniformBuffer_SingleDraw);
		PassParameters->RWGlobalIlluminationUAV = GraphBuilder.CreateUAV(GlobalIlluminationTexture);
		PassParameters->RWRayDistanceUAV = GraphBuilder.CreateUAV(RayDistanceTexture);

		auto RayGenerationShader = View.ShaderMap->GetShader<FGlobalIlluminationRGS>();
		ClearUnusedGraphResources(RayGenerationShader, PassParameters);

		FIntPoint RayTracingResolution = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), UpscaleFactor);
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("GlobalIlluminationRayTracing %dx%d", RayTracingResolution.X, RayTracingResolution.Y),
			PassParameters,
			ERenderGraphPassFlags::Compute,
			[PassParameters, this, &View, RayGenerationShader, RayTracingResolution](FRHICommandList& RHICmdList)
		{
			auto ClosestHitShader = View.ShaderMap->GetShader<FRayTracingGlobalIlluminationCHS>();
			auto MissShader = View.ShaderMap->GetShader<FRayTracingGlobalIlluminationMS>();

			FRHIRayTracingPipelineState* Pipeline = BindRayTracingPipeline(
				RHICmdList, View,
				RayGenerationShader->GetRayTracingShader(),
				MissShader->GetRayTracingShader(),
				ClosestHitShader->GetRayTracingShader()); // #dxr_todo: this should be done once at load-time and cached

			FRayTracingShaderBindingsWriter GlobalResources;
			SetShaderParameters(GlobalResources, RayGenerationShader, *PassParameters);

			FRayTracingSceneRHIParamRef RayTracingSceneRHI = View.PerViewRayTracingScene.RayTracingSceneRHI;
			uint32 RayGenShaderIndex = 0;
			RHICmdList.RayTraceDispatch(Pipeline, RayGenShaderIndex, RayTracingSceneRHI, GlobalResources, RayTracingResolution.X, RayTracingResolution.Y);
		});
	}

	// Denoising
	if (GRayTracingGlobalIlluminationDenoiser != 0)
	{
		FSceneViewFamilyBlackboard SceneBlackboard;
		SetupSceneViewFamilyBlackboard(GraphBuilder, &SceneBlackboard);

		const IScreenSpaceDenoiser* DefaultDenoiser = IScreenSpaceDenoiser::GetDefaultDenoiser();
		const IScreenSpaceDenoiser* DenoiserToUse = GRayTracingGlobalIlluminationDenoiser == 1 ? DefaultDenoiser : GScreenSpaceDenoiser;

		IScreenSpaceDenoiser::FGlobalIlluminationInputs DenoiserInputs;
		DenoiserInputs.Color = GlobalIlluminationTexture;
		DenoiserInputs.RayHitDistance = RayDistanceTexture;

		{
			RDG_EVENT_SCOPE(GraphBuilder, "%s%s(GlobalIllumination) %dx%d",
				DenoiserToUse != DefaultDenoiser ? TEXT("ThirdParty ") : TEXT(""),
				DenoiserToUse->GetDebugName(),
				View.ViewRect.Width(), View.ViewRect.Height());

			IScreenSpaceDenoiser::FGlobalIlluminationOutputs DenoiserOutputs = DenoiserToUse->DenoiseGlobalIllumination(
				GraphBuilder,
				View,
				SceneBlackboard,
				DenoiserInputs,
				RayTracingConfig);

			ResultTexture = DenoiserOutputs.Color;
		}
	}
	else
	{
		ResultTexture = GlobalIlluminationTexture;
	}

	// Compositing
	{
		FRayTracingGlobalIlluminationCompositePS::FParameters *PassParameters = GraphBuilder.AllocParameters<FRayTracingGlobalIlluminationCompositePS::FParameters>();
		PassParameters->GlobalIlluminationTexture = ResultTexture;
		PassParameters->GlobalIlluminationSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
		PassParameters->SceneTexturesStruct = CreateUniformBufferImmediate(SceneTextures, EUniformBufferUsage::UniformBuffer_SingleDraw);
		PassParameters->RenderTargets[0] = FRenderTargetBinding(GraphBuilder.RegisterExternalTexture(GlobalIlluminationRT), ERenderTargetLoadAction::ENoAction, ERenderTargetStoreAction::ENoAction);
		PassParameters->RenderTargets[1] = FRenderTargetBinding(GraphBuilder.RegisterExternalTexture(AmbientOcclusionRT), ERenderTargetLoadAction::ENoAction, ERenderTargetStoreAction::ENoAction);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("GlobalIlluminationComposite"),
			PassParameters,
			ERenderGraphPassFlags::None,
			[this, &SceneContext, &View, &SceneTextures, PassParameters](FRHICommandListImmediate& RHICmdList)
			{
				TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
				TShaderMapRef<FRayTracingGlobalIlluminationCompositePS> PixelShader(View.ShaderMap);
				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

				// Additive blending
				GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
				//GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();
				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

				SetShaderParameters(RHICmdList, *PixelShader, PixelShader->GetPixelShader(), *PassParameters);

				RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

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
		);
	}

	GraphBuilder.Execute();
	SceneContext.bScreenSpaceAOIsValid = true;
	GVisualizeTexture.SetCheckPoint(RHICmdList, GlobalIlluminationRT);
}

void FDeferredShadingSceneRenderer::CompositeGlobalIllumination(
	FRHICommandListImmediate& RHICmdList,
	const FViewInfo& View,
	TRefCountPtr<IPooledRenderTarget>& GlobalIlluminationRT
)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	FRDGBuilder GraphBuilder(RHICmdList);
	FRayTracingGlobalIlluminationSceneColorCompositePS::FParameters *PassParameters = GraphBuilder.AllocParameters<FRayTracingGlobalIlluminationSceneColorCompositePS::FParameters>();
	PassParameters->GlobalIlluminationTexture = GraphBuilder.RegisterExternalTexture(GlobalIlluminationRT);
	PassParameters->GlobalIlluminationSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->RenderTargets[0] = FRenderTargetBinding(GraphBuilder.RegisterExternalTexture(SceneContext.GetSceneColor()), ERenderTargetLoadAction::ENoAction, ERenderTargetStoreAction::ENoAction);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("GlobalIlluminationComposite"),
		PassParameters,
		ERenderGraphPassFlags::None,
		[this, &SceneContext, &View, PassParameters](FRHICommandListImmediate& RHICmdList)
		{
			TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
			TShaderMapRef<FRayTracingGlobalIlluminationSceneColorCompositePS> PixelShader(View.ShaderMap);
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			// Additive blending
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();
			//GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			SetShaderParameters(RHICmdList, *PixelShader, PixelShader->GetPixelShader(), *PassParameters);

			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

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
	);
	GraphBuilder.Execute();
}

#endif // RHI_RAYTRACING
