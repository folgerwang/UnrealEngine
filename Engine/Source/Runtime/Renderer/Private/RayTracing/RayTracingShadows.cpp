// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DeferredShadingRenderer.h"

#if RHI_RAYTRACING

#include "ClearQuad.h"
#include "PathTracingUniformBuffers.h"
#include "SceneRendering.h"
#include "SceneRenderTargets.h"
#include "RenderGraphBuilder.h"
#include "RenderTargetPool.h"
#include "RHIResources.h"
#include "UniformBuffer.h"
#include "VisualizeTexture.h"
#include "RayGenShaderUtils.h"

#include "Containers/DynamicRHIResourceArray.h"

#include "SceneViewFamilyBlackboard.h"
#include "ScreenSpaceDenoise.h"

static int32 GRayTracingOcclusionSamplesPerPixel = 1;
static FAutoConsoleVariableRef CVarRayTracingOcclusionSamplesPerPixel(
	TEXT("r.Shadow.RayTracing.SamplesPerPixel"),
	GRayTracingOcclusionSamplesPerPixel,
	TEXT("Sets the samples-per-pixel for directional light occlusion (default = 1)")
);

static TAutoConsoleVariable<int32> CVarShadowUseDenoiser(
	TEXT("r.Shadow.Denoiser"),
	0, // TODO: change default to 2.
	TEXT("Choose the denoising algorithm.\n")
	TEXT(" 0: Disabled (default);\n")
	TEXT(" 1: Forces the default denoiser of the renderer;\n")
	TEXT(" 2: GScreenSpaceDenoiser witch may be overriden by a third party plugin.\n"),
	ECVF_RenderThreadSafe);

class FOcclusionRGS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOcclusionRGS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FOcclusionRGS, FGlobalShader)

	class FLightTypeDim : SHADER_PERMUTATION_INT("LIGHT_TYPE", LightType_MAX);

	using FPermutationDomain = TShaderPermutationDomain<FLightTypeDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, SamplesPerPixel)

		SHADER_PARAMETER_STRUCT(FLightShaderParameters, Light)

		SHADER_PARAMETER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWOcclusionMaskUAV)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWRayDistanceUAV)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_STRUCT_REF(FSceneTexturesUniformParameters, SceneTexturesStruct)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FOcclusionRGS, "/Engine/Private/RayTracing/RayTracingOcclusionRGS.usf", "OcclusionRGS", SF_RayGen);

void FDeferredShadingSceneRenderer::RenderRayTracingOcclusion(
	FRHICommandListImmediate& RHICmdList,
	const FLightSceneInfo* LightSceneInfo,
	TRefCountPtr<IPooledRenderTarget>& OutScreenShadowMaskTexture
)
{
	check(LightSceneInfo);
	FLightSceneProxy* LightSceneProxy = LightSceneInfo->Proxy;
	check(LightSceneProxy);

	const FViewInfo& View = Views[0]; // #dxr_todo: what about multi-view case?

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	FRDGBuilder GraphBuilder(RHICmdList);

	// Render targets
	FRDGTextureRef ScreenShadowMaskTexture;
	{
		FRDGTextureDesc Desc = SceneContext.GetSceneColor()->GetDesc();
		Desc.Format = PF_FloatRGBA;
		Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
		ScreenShadowMaskTexture = GraphBuilder.CreateTexture(Desc, TEXT("RayTracingOcclusion"));
	}

	FRDGTextureRef RayDistanceTexture;
	{
		FRDGTextureDesc Desc = SceneContext.GetSceneColor()->GetDesc();
		Desc.Format = PF_R16F;
		Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
		RayDistanceTexture = GraphBuilder.CreateTexture(Desc, TEXT("RayTracingOcclusionDistance"));
	}

	// Ray generation pass for shadow occlusion.
	{
		// Uniform buffer data
		// TODO(RDG): use FSceneViewFamilyBlackboard.
		FSceneTexturesUniformParameters SceneTextures;
		SetupSceneTextureUniformParameters(SceneContext, FeatureLevel, ESceneTextureSetupMode::All, SceneTextures);

		FOcclusionRGS::FParameters* PassParameters = GraphBuilder.AllocParameters<FOcclusionRGS::FParameters>();
		PassParameters->RWOcclusionMaskUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenShadowMaskTexture));
		PassParameters->RWRayDistanceUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(RayDistanceTexture));
		PassParameters->SamplesPerPixel = GRayTracingOcclusionSamplesPerPixel;
		LightSceneProxy->GetLightShaderParameters(PassParameters->Light);
		PassParameters->TLAS = RHIGetAccelerationStructureShaderResourceView(View.PerViewRayTracingScene.RayTracingSceneRHI);
		PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
		PassParameters->SceneTexturesStruct = CreateUniformBufferImmediate(SceneTextures, EUniformBufferUsage::UniformBuffer_SingleDraw);

		FOcclusionRGS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FOcclusionRGS::FLightTypeDim>(LightSceneProxy->GetLightType());

		TShaderMapRef<FOcclusionRGS> RayGenerationShader(GetGlobalShaderMap(FeatureLevel), PermutationVector);

		FRayGenShaderUtils::AddRayTraceDispatchPass(
			GraphBuilder,
			RDG_EVENT_NAME("RayTracedShadow %dx%d", View.ViewRect.Width(), View.ViewRect.Height()),
			*RayGenerationShader,
			PassParameters,
			View.ViewRect.Size());
	}

	if (int32 DenoiserMode = CVarShadowUseDenoiser.GetValueOnRenderThread())
	{
		FSceneViewFamilyBlackboard SceneBlackboard;
		SetupSceneViewFamilyBlackboard(GraphBuilder, &SceneBlackboard);

		IScreenSpaceDenoiser::FShadowPenumbraInputs DenoiserInputs;
		DenoiserInputs.Penumbra = ScreenShadowMaskTexture;
		DenoiserInputs.ClosestOccluder = RayDistanceTexture;

		const IScreenSpaceDenoiser* DefaultDenoiser = IScreenSpaceDenoiser::GetDefaultDenoiser();
		const IScreenSpaceDenoiser* DenoiserToUse = DenoiserMode == 1 ? DefaultDenoiser : GScreenSpaceDenoiser;

		// Standard event scope for denoiser to have all profiling information not matter what, and with explicit detection of third party.
		RDG_EVENT_SCOPE(GraphBuilder, "%s%s(Shadow) %dx%d", 
			DenoiserToUse != DefaultDenoiser ? TEXT("ThirdParty ") : TEXT(""),
			DenoiserToUse->GetDebugName(),
			View.ViewRect.Width(), View.ViewRect.Height());

		IScreenSpaceDenoiser::FShadowPenumbraOutputs DenoiserOutputs = GScreenSpaceDenoiser->DenoiseShadowPenumbra(
			GraphBuilder,
			View,
			*LightSceneInfo,
			SceneBlackboard,
			DenoiserInputs);

		GraphBuilder.QueueTextureExtraction(DenoiserOutputs.DiffusePenumbra, &OutScreenShadowMaskTexture);
	}
	else
	{
		GraphBuilder.QueueTextureExtraction(ScreenShadowMaskTexture, &OutScreenShadowMaskTexture);
	}

	GraphBuilder.Execute();
}

#endif // RHI_RAYTRACING