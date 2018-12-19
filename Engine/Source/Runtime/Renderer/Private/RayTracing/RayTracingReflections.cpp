// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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

static int32 GRayTracingReflectionsSamplesPerPixel = 1;
static FAutoConsoleVariableRef CVarRayTracingReflectionsSamplesPerPixel(
	TEXT("r.RayTracing.Reflections.SamplesPerPixel"),
	GRayTracingReflectionsSamplesPerPixel,
	TEXT("Sets the samples-per-pixel for reflections (default = 1)")
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

static float GRayTracingReflectionsMaxRayDistance = 1.0e27;
static FAutoConsoleVariableRef CVarRayTracingReflectionsMaxRayDistance(
	TEXT("r.RayTracing.Reflections.MaxRayDistance"),
	GRayTracingReflectionsMaxRayDistance,
	TEXT("Sets the maximum ray distance for ray traced reflection rays (default = 1.0e27)")
);

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
	// Fill light buffer data
	LightData->Count = FMath::Min(Lights.Num(), GReflectionLightCountMaximum);

	TSparseArray<FLightSceneInfoCompact>::TConstIterator LightItr = Lights.CreateConstIterator();
	for (uint32 LightIndex = 0; LightIndex < LightData->Count; ++LightIndex, ++LightItr)
	{
		FLightSceneProxy* LightSceneProxy = LightItr->LightSceneInfo->Proxy;
		FLightShaderParameters LightParameters;
		LightSceneProxy->GetLightShaderParameters(LightParameters);

		if (LightSceneProxy->IsInverseSquared())
		{
			LightParameters.FalloffExponent = 0;
		}

		LightData->Type[LightIndex] = LightItr->LightType;
		LightData->LightPosition[LightIndex] = LightParameters.Position;
		LightData->LightInvRadius[LightIndex] = LightParameters.InvRadius;
		LightData->LightColor[LightIndex] = LightParameters.Color;
		LightData->LightFalloffExponent[LightIndex] = LightParameters.FalloffExponent;
		LightData->Direction[LightIndex] = LightParameters.Direction;
		LightData->Tangent[LightIndex] = LightParameters.Tangent;
		LightData->SpotAngles[LightIndex] = LightParameters.SpotAngles;
		LightData->SpecularScale[LightIndex] = LightParameters.SpecularScale;
		LightData->SourceRadius[LightIndex] = LightParameters.SourceRadius;
		LightData->SourceLength[LightIndex] = LightParameters.SourceLength;
		LightData->SoftSourceRadius[LightIndex] = LightParameters.SoftSourceRadius;

		const FVector2D FadeParams = LightSceneProxy->GetDirectionalLightDistanceFadeParameters(View.GetFeatureLevel(), LightItr->LightSceneInfo->IsPrecomputedLightingValid(), View.MaxShadowCascades);
		LightData->DistanceFadeMAD[LightIndex] = FVector2D(FadeParams.Y, -FadeParams.X * FadeParams.Y);
	}

	LightData->DummyRectLightTexture = GWhiteTexture->TextureRHI; //#dxr_todo: replace with valid textures per rect light
}


class FRayTracingReflectionsRG : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingReflectionsRG)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingReflectionsRG, FGlobalShader)

	class FDenoiserOutput : SHADER_PERMUTATION_BOOL("DIM_DENOISER_OUTPUT");
	using FPermutationDomain = TShaderPermutationDomain<FDenoiserOutput>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int32, SamplesPerPixel)
		SHADER_PARAMETER(int32, ShouldDoDirectLighting)
		SHADER_PARAMETER(int32, ShouldDoEmissiveAndIndirectLighting)
		SHADER_PARAMETER(float, ReflectionMaxRayDistance)

		SHADER_PARAMETER_SRV(RaytracingAccelerationStructure, TLAS)

		SHADER_PARAMETER_TEXTURE(Texture2D, LTCMatTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, LTCMatSampler)
		SHADER_PARAMETER_TEXTURE(Texture2D, LTCAmpTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, LTCAmpSampler)

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_STRUCT_REF(FSceneTexturesUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_STRUCT_REF(FReflectionsLightData, LightData)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, ColorOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RayHitDistanceOutput)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return RHISupportsRayTracing(Parameters.Platform);
	}
};

class FRayTracingReflectionsCHS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingReflectionsCHS);
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingReflectionsCHS, FGlobalShader)
		
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return RHISupportsRayTracing(Parameters.Platform);
	}
	
	using FParameters = FEmptyShaderParameters;
};

class FRayTracingReflectionsMS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingReflectionsMS);
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingReflectionsMS, FGlobalShader)
		
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return RHISupportsRayTracing(Parameters.Platform);
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
	FRDGTextureRef* OutRayHitDistanceTexture)
#if RHI_RAYTRACING
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(GraphBuilder.RHICmdList);
	FPooledRenderTargetDesc Desc = SceneContext.GetSceneColor()->GetDesc();
	Desc.Format = PF_FloatRGBA;
	Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);

	*OutColorTexture = GraphBuilder.CreateTexture(Desc, TEXT("RayTracingReflections"));
	*OutRayHitDistanceTexture = GraphBuilder.CreateTexture(Desc, TEXT("RayTracingReflections"));

	FRayTracingReflectionsRG::FParameters* PassParameters = GraphBuilder.AllocParameters<FRayTracingReflectionsRG::FParameters>();

	PassParameters->SamplesPerPixel = GRayTracingReflectionsSamplesPerPixel;
	PassParameters->ShouldDoDirectLighting = GRayTracingReflectionsDirectLighting;
	PassParameters->ShouldDoEmissiveAndIndirectLighting = GRayTracingReflectionsEmissiveAndIndirectLighting;
	PassParameters->ReflectionMaxRayDistance = GRayTracingReflectionsMaxRayDistance;
	PassParameters->LTCMatTexture = GSystemTextures.LTCMat->GetRenderTargetItem().ShaderResourceTexture;
	PassParameters->LTCMatSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->LTCAmpTexture = GSystemTextures.LTCAmp->GetRenderTargetItem().ShaderResourceTexture;
	PassParameters->LTCAmpSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	PassParameters->TLAS = RHIGetAccelerationStructureShaderResourceView(View.PerViewRayTracingScene.RayTracingSceneRHI);
	PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
	{
		FReflectionsLightData LightData;
		SetupReflectionsLightData(Scene->Lights, View, &LightData);
		PassParameters->LightData = CreateUniformBufferImmediate(LightData, EUniformBufferUsage::UniformBuffer_SingleDraw);
	}
	{ // TODO: use FSceneViewFamilyBlackboard.
		FSceneTexturesUniformParameters SceneTextures;
		SetupSceneTextureUniformParameters(SceneContext, FeatureLevel, ESceneTextureSetupMode::All, SceneTextures);
		PassParameters->SceneTexturesStruct = CreateUniformBufferImmediate(SceneTextures, EUniformBufferUsage::UniformBuffer_SingleDraw);
	}
	PassParameters->ColorOutput = GraphBuilder.CreateUAV(*OutColorTexture);
	PassParameters->RayHitDistanceOutput = GraphBuilder.CreateUAV(*OutRayHitDistanceTexture);

	auto RayGenShader = View.ShaderMap->GetShader<FRayTracingReflectionsRG>();
	ClearUnusedGraphResources(RayGenShader, PassParameters);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("ReflectionRayTracing %dx%d", View.ViewRect.Width(), View.ViewRect.Height()),
		PassParameters,
		ERenderGraphPassFlags::Compute,
		[PassParameters, this, &View, RayGenShader](FRHICommandList& RHICmdList)
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
		RHICmdList.RayTraceDispatch(Pipeline, RayTracingSceneRHI, GlobalResources, View.ViewRect.Width(), View.ViewRect.Height());
	});
}
#else // !RHI_RAYTRACING
{
	check(0);
}
#endif
