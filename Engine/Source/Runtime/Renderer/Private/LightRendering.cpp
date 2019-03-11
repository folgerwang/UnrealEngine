// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LightRendering.cpp: Light rendering implementation.
=============================================================================*/

#include "LightRendering.h"
#include "DeferredShadingRenderer.h"
#include "LightPropagationVolume.h"
#include "ScenePrivate.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"
#include "Engine/SubsurfaceProfile.h"
#include "ShowFlags.h"
#include "VisualizeTexture.h"
#include "RayTracing/RaytracingOptions.h"
#include "SceneViewFamilyBlackboard.h"

DECLARE_GPU_STAT(Lights);

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FDeferredLightUniformStruct, "DeferredLightUniforms");

extern int32 GUseTranslucentLightingVolumes;
ENGINE_API const IPooledRenderTarget* GetSubsufaceProfileTexture_RT(FRHICommandListImmediate& RHICmdList);


static int32 GAllowDepthBoundsTest = 1;
static FAutoConsoleVariableRef CVarAllowDepthBoundsTest(
	TEXT("r.AllowDepthBoundsTest"),
	GAllowDepthBoundsTest,
	TEXT("If true, use enable depth bounds test when rendering defered lights.")
	);

static int32 bAllowSimpleLights = 1;
static FAutoConsoleVariableRef CVarAllowSimpleLights(
	TEXT("r.AllowSimpleLights"),
	bAllowSimpleLights,
	TEXT("If true, we allow simple (ie particle) lights")
);

static int32 GRayTracingShadows = 1;
static FAutoConsoleVariableRef CVarRayTracingOcclusion(
	TEXT("r.RayTracing.Shadows"),
	GRayTracingShadows,
	TEXT("0: use traditional rasterized shadow map\n")
	TEXT("1: use ray tracing shadows (default)")
);

static int32 GShadowRayTracingSamplesPerPixel = 1;
static FAutoConsoleVariableRef CVarShadowRayTracingSamplesPerPixel(
	TEXT("r.RayTracing.Shadow.SamplesPerPixel"),
	GShadowRayTracingSamplesPerPixel,
	TEXT("Sets the samples-per-pixel for directional light occlusion (default = 1)"));

static TAutoConsoleVariable<int32> CVarShadowUseDenoiser(
	TEXT("r.Shadow.Denoiser"),
	2,
	TEXT("Choose the denoising algorithm.\n")
	TEXT(" 0: Disabled (default);\n")
	TEXT(" 1: Forces the default denoiser of the renderer;\n")
	TEXT(" 2: GScreenSpaceDenoiser witch may be overriden by a third party plugin.\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMaxShadowDenoisingBatchSize(
	TEXT("r.Shadow.Denoiser.MaxBatchSize"), 4,
	TEXT("Maximum number of shadow to denoise at the same time."),
	ECVF_RenderThreadSafe);

FLightOcclusionType GetLightOcclusionType(const FLightSceneProxy& Proxy)
{
#if RHI_RAYTRACING
	const bool bCastRaytracedShadow = IsRayTracingEnabled() && GRayTracingShadows == 1 && Proxy.CastsRaytracedShadow();
	return bCastRaytracedShadow ? FLightOcclusionType::Raytraced : FLightOcclusionType::Shadowmap;
#else
	return FLightOcclusionType::Shadowmap;
#endif
}

FLightOcclusionType GetLightOcclusionType(const FLightSceneInfoCompact& LightInfo)
{
#if RHI_RAYTRACING
	const bool bCastRaytracedShadow = IsRayTracingEnabled() && GRayTracingShadows == 1 && LightInfo.bCastRaytracedShadow;
	return bCastRaytracedShadow ? FLightOcclusionType::Raytraced : FLightOcclusionType::Shadowmap;
#else
	return FLightOcclusionType::Shadowmap;
#endif
}

float GetLightFadeFactor(const FSceneView& View, const FLightSceneProxy* Proxy)
{
	// Distance fade
	FSphere Bounds = Proxy->GetBoundingSphere();

	const float DistanceSquared = (Bounds.Center - View.ViewMatrices.GetViewOrigin()).SizeSquared();
	extern float GMinScreenRadiusForLights;
	float SizeFade = FMath::Square(FMath::Min(0.0002f, GMinScreenRadiusForLights / Bounds.W) * View.LODDistanceFactor) * DistanceSquared;
	SizeFade = FMath::Clamp(6.0f - 6.0f * SizeFade, 0.0f, 1.0f);

	extern float GLightMaxDrawDistanceScale;
	float MaxDist = Proxy->GetMaxDrawDistance() * GLightMaxDrawDistanceScale;
	float Range = Proxy->GetFadeRange();
	float DistanceFade = MaxDist ? (MaxDist - FMath::Sqrt(DistanceSquared)) / Range : 1.0f;
	DistanceFade = FMath::Clamp(DistanceFade, 0.0f, 1.0f);
	return SizeFade * DistanceFade;
}

void StencilingGeometry::DrawSphere(FRHICommandList& RHICmdList)
{
	RHICmdList.SetStreamSource(0, StencilingGeometry::GStencilSphereVertexBuffer.VertexBufferRHI, 0);
	RHICmdList.DrawIndexedPrimitive(StencilingGeometry::GStencilSphereIndexBuffer.IndexBufferRHI, 0, 0,
		StencilingGeometry::GStencilSphereVertexBuffer.GetVertexCount(), 0, 
		StencilingGeometry::GStencilSphereIndexBuffer.GetIndexCount() / 3, 1);
}
		
void StencilingGeometry::DrawVectorSphere(FRHICommandList& RHICmdList)
{
	RHICmdList.SetStreamSource(0, StencilingGeometry::GStencilSphereVectorBuffer.VertexBufferRHI, 0);
	RHICmdList.DrawIndexedPrimitive(StencilingGeometry::GStencilSphereIndexBuffer.IndexBufferRHI, 0, 0,
									StencilingGeometry::GStencilSphereVectorBuffer.GetVertexCount(), 0,
									StencilingGeometry::GStencilSphereIndexBuffer.GetIndexCount() / 3, 1);
}

void StencilingGeometry::DrawCone(FRHICommandList& RHICmdList)
{
	// No Stream Source needed since it will generate vertices on the fly
	RHICmdList.SetStreamSource(0, StencilingGeometry::GStencilConeVertexBuffer.VertexBufferRHI, 0);

	RHICmdList.DrawIndexedPrimitive(StencilingGeometry::GStencilConeIndexBuffer.IndexBufferRHI, 0, 0,
		FStencilConeIndexBuffer::NumVerts, 0, StencilingGeometry::GStencilConeIndexBuffer.GetIndexCount() / 3, 1);
}

/** The stencil sphere vertex buffer. */
TGlobalResource<StencilingGeometry::TStencilSphereVertexBuffer<18, 12, FVector4> > StencilingGeometry::GStencilSphereVertexBuffer;
TGlobalResource<StencilingGeometry::TStencilSphereVertexBuffer<18, 12, FVector> > StencilingGeometry::GStencilSphereVectorBuffer;

/** The stencil sphere index buffer. */
TGlobalResource<StencilingGeometry::TStencilSphereIndexBuffer<18, 12> > StencilingGeometry::GStencilSphereIndexBuffer;

TGlobalResource<StencilingGeometry::TStencilSphereVertexBuffer<4, 4, FVector4> > StencilingGeometry::GLowPolyStencilSphereVertexBuffer;
TGlobalResource<StencilingGeometry::TStencilSphereIndexBuffer<4, 4> > StencilingGeometry::GLowPolyStencilSphereIndexBuffer;

/** The (dummy) stencil cone vertex buffer. */
TGlobalResource<StencilingGeometry::FStencilConeVertexBuffer> StencilingGeometry::GStencilConeVertexBuffer;

/** The stencil cone index buffer. */
TGlobalResource<StencilingGeometry::FStencilConeIndexBuffer> StencilingGeometry::GStencilConeIndexBuffer;


// Implement a version for directional lights, and a version for point / spot lights
IMPLEMENT_SHADER_TYPE(template<>,TDeferredLightVS<false>,TEXT("/Engine/Private/DeferredLightVertexShaders.usf"),TEXT("DirectionalVertexMain"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(template<>,TDeferredLightVS<true>,TEXT("/Engine/Private/DeferredLightVertexShaders.usf"),TEXT("RadialVertexMain"),SF_Vertex);


enum class ELightSourceShape
{
	Directional,
	Capsule,
	Rect,

	MAX
};

/** A pixel shader for rendering the light in a deferred pass. */
class FDeferredLightPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDeferredLightPS)

	class FSourceShapeDim		: SHADER_PERMUTATION_ENUM_CLASS("LIGHT_SOURCE_SHAPE", ELightSourceShape);
	class FSourceTextureDim		: SHADER_PERMUTATION_BOOL("USE_SOURCE_TEXTURE");
	class FIESProfileDim		: SHADER_PERMUTATION_BOOL("USE_IES_PROFILE");
	class FInverseSquaredDim	: SHADER_PERMUTATION_BOOL("INVERSE_SQUARED_FALLOFF");
	class FVisualizeCullingDim	: SHADER_PERMUTATION_BOOL("VISUALIZE_LIGHT_CULLING");
	class FLightingChannelsDim	: SHADER_PERMUTATION_BOOL("USE_LIGHTING_CHANNELS");
	class FTransmissionDim		: SHADER_PERMUTATION_BOOL("USE_TRANSMISSION");

	using FPermutationDomain = TShaderPermutationDomain<
		FSourceShapeDim,
		FSourceTextureDim,
		FIESProfileDim,
		FInverseSquaredDim,
		FVisualizeCullingDim,
		FLightingChannelsDim,
		FTransmissionDim >;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if( PermutationVector.Get< FSourceShapeDim >() == ELightSourceShape::Directional && (
			PermutationVector.Get< FIESProfileDim >() ||
			PermutationVector.Get< FInverseSquaredDim >() ) )
		{
			return false;
		}

		if( PermutationVector.Get< FSourceShapeDim >() == ELightSourceShape::Rect )
		{
			if(	!PermutationVector.Get< FInverseSquaredDim >() )
			{
				return false;
			}
		}
		else
		{
			if( PermutationVector.Get< FSourceTextureDim >() )
			{
				return false;
			}
		}

		/*if( PermutationVector.Get< FVisualizeCullingDim >() && (
			PermutationVector.Get< FSourceShapeDim >() == ELightSourceShape::Rect ||
			PermutationVector.Get< FIESProfileDim >() ||
			PermutationVector.Get< FInverseSquaredDim >() ) )
		{
			return false;
		}*/

		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM4);
	}

	FDeferredLightPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	:	FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer);
		LightAttenuationTexture.Bind(Initializer.ParameterMap, TEXT("LightAttenuationTexture"));
		LightAttenuationTextureSampler.Bind(Initializer.ParameterMap, TEXT("LightAttenuationTextureSampler"));
		LTCMatTexture.Bind(Initializer.ParameterMap, TEXT("LTCMatTexture"));
		LTCMatSampler.Bind(Initializer.ParameterMap, TEXT("LTCMatSampler"));
		LTCAmpTexture.Bind(Initializer.ParameterMap, TEXT("LTCAmpTexture"));
		LTCAmpSampler.Bind(Initializer.ParameterMap, TEXT("LTCAmpSampler"));
		IESTexture.Bind(Initializer.ParameterMap, TEXT("IESTexture"));
		IESTextureSampler.Bind(Initializer.ParameterMap, TEXT("IESTextureSampler"));
		LightingChannelsTexture.Bind(Initializer.ParameterMap, TEXT("LightingChannelsTexture"));
		LightingChannelsSampler.Bind(Initializer.ParameterMap, TEXT("LightingChannelsSampler"));
		TransmissionProfilesTexture.Bind(Initializer.ParameterMap, TEXT("SSProfilesTexture"));
		TransmissionProfilesLinearSampler.Bind(Initializer.ParameterMap, TEXT("TransmissionProfilesLinearSampler"));		
	}

	FDeferredLightPS()
	{}

public:
	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FLightSceneInfo* LightSceneInfo, IPooledRenderTarget* ScreenShadowMaskTexture)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		SetParametersBase(RHICmdList, ShaderRHI, View, ScreenShadowMaskTexture, LightSceneInfo->Proxy->GetIESTextureResource());
		SetDeferredLightParameters(RHICmdList, ShaderRHI, GetUniformBufferParameter<FDeferredLightUniformStruct>(), LightSceneInfo, View);
	}

	void SetParametersSimpleLight(FRHICommandList& RHICmdList, const FSceneView& View, const FSimpleLightEntry& SimpleLight, const FSimpleLightPerViewEntry& SimpleLightPerViewData)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		SetParametersBase(RHICmdList, ShaderRHI, View, NULL, NULL);
		SetSimpleDeferredLightParameters(RHICmdList, ShaderRHI, GetUniformBufferParameter<FDeferredLightUniformStruct>(), SimpleLight, SimpleLightPerViewData, View);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SceneTextureParameters;
		Ar << LightAttenuationTexture;
		Ar << LightAttenuationTextureSampler;
		Ar << LTCMatTexture;
		Ar << LTCMatSampler;
		Ar << LTCAmpTexture;
		Ar << LTCAmpSampler;
		Ar << IESTexture;
		Ar << IESTextureSampler;
		Ar << LightingChannelsTexture;
		Ar << LightingChannelsSampler;
		Ar << TransmissionProfilesTexture;
		Ar << TransmissionProfilesLinearSampler;
		return bShaderHasOutdatedParameters;
	}

private:

	void SetParametersBase(FRHICommandList& RHICmdList, const FPixelShaderRHIParamRef ShaderRHI, const FSceneView& View, IPooledRenderTarget* ScreenShadowMaskTexture, FTexture* IESTextureResource)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI,View.ViewUniformBuffer);
		SceneTextureParameters.Set(RHICmdList, ShaderRHI, View.FeatureLevel, ESceneTextureSetupMode::All);

		FSceneRenderTargets& SceneRenderTargets = FSceneRenderTargets::Get(RHICmdList);

		if(LightAttenuationTexture.IsBound())
		{
			SetTextureParameter(
				RHICmdList, 
				ShaderRHI,
				LightAttenuationTexture,
				LightAttenuationTextureSampler,
				TStaticSamplerState<SF_Point,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI(),
				ScreenShadowMaskTexture ? ScreenShadowMaskTexture->GetRenderTargetItem().ShaderResourceTexture : GWhiteTexture->TextureRHI
				);
		}

		SetTextureParameter(
			RHICmdList, 
			ShaderRHI,
			LTCMatTexture,
			LTCMatSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			GSystemTextures.LTCMat->GetRenderTargetItem().ShaderResourceTexture
			);

		SetTextureParameter(
			RHICmdList, 
			ShaderRHI,
			LTCAmpTexture,
			LTCAmpSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			GSystemTextures.LTCAmp->GetRenderTargetItem().ShaderResourceTexture
			);

		{
			FTextureRHIParamRef TextureRHI = IESTextureResource ? IESTextureResource->TextureRHI : GSystemTextures.WhiteDummy->GetRenderTargetItem().TargetableTexture;

			SetTextureParameter(
				RHICmdList, 
				ShaderRHI,
				IESTexture,
				IESTextureSampler,
				TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				TextureRHI
				);
		}

		if( LightingChannelsTexture.IsBound() )
		{
			FTextureRHIParamRef LightingChannelsTextureRHI = SceneRenderTargets.LightingChannels ? SceneRenderTargets.LightingChannels->GetRenderTargetItem().ShaderResourceTexture : GSystemTextures.WhiteDummy->GetRenderTargetItem().TargetableTexture;

			SetTextureParameter(
				RHICmdList, 
				ShaderRHI,
				LightingChannelsTexture,
				LightingChannelsSampler,
				TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				LightingChannelsTextureRHI
				);
		}

		if( TransmissionProfilesTexture.IsBound() )
		{
			FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
			const IPooledRenderTarget* PooledRT = GetSubsufaceProfileTexture_RT((FRHICommandListImmediate&)RHICmdList);

			if (!PooledRT)
			{
				// no subsurface profile was used yet
				PooledRT = GSystemTextures.BlackDummy;
			}

			const FSceneRenderTargetItem& Item = PooledRT->GetRenderTargetItem();

			SetTextureParameter(RHICmdList,
				ShaderRHI,
				TransmissionProfilesTexture,
				TransmissionProfilesLinearSampler,
				TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(),
				Item.ShaderResourceTexture);
		}
	}

	FSceneTextureShaderParameters SceneTextureParameters;
	FShaderResourceParameter LightAttenuationTexture;
	FShaderResourceParameter LightAttenuationTextureSampler;
	FShaderResourceParameter LTCMatTexture;
	FShaderResourceParameter LTCMatSampler;
	FShaderResourceParameter LTCAmpTexture;
	FShaderResourceParameter LTCAmpSampler;
	FShaderResourceParameter IESTexture;
	FShaderResourceParameter IESTextureSampler;
	FShaderResourceParameter LightingChannelsTexture;
	FShaderResourceParameter LightingChannelsSampler;
	FShaderResourceParameter TransmissionProfilesTexture;
	FShaderResourceParameter TransmissionProfilesLinearSampler;
};

IMPLEMENT_GLOBAL_SHADER(FDeferredLightPS, "/Engine/Private/DeferredLightPixelShaders.usf", "DeferredLightPixelMain", SF_Pixel);


/** Shader used to visualize stationary light overlap. */
template<bool bRadialAttenuation>
class TDeferredLightOverlapPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDeferredLightOverlapPS,Global)
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("RADIAL_ATTENUATION"), (uint32)bRadialAttenuation);
	}

	TDeferredLightOverlapPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	:	FGlobalShader(Initializer)
	{
		HasValidChannel.Bind(Initializer.ParameterMap, TEXT("HasValidChannel"));
		SceneTextureParameters.Bind(Initializer);
	}

	TDeferredLightOverlapPS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FLightSceneInfo* LightSceneInfo)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI,View.ViewUniformBuffer);
		const float HasValidChannelValue = LightSceneInfo->Proxy->GetPreviewShadowMapChannel() == INDEX_NONE ? 0.0f : 1.0f;
		SetShaderValue(RHICmdList, ShaderRHI, HasValidChannel, HasValidChannelValue);
		SceneTextureParameters.Set(RHICmdList, ShaderRHI, View.FeatureLevel, ESceneTextureSetupMode::All);
		SetDeferredLightParameters(RHICmdList, ShaderRHI, GetUniformBufferParameter<FDeferredLightUniformStruct>(), LightSceneInfo, View);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << HasValidChannel;
		Ar << SceneTextureParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter HasValidChannel;
	FSceneTextureShaderParameters SceneTextureParameters;
};

IMPLEMENT_SHADER_TYPE(template<>, TDeferredLightOverlapPS<true>, TEXT("/Engine/Private/StationaryLightOverlapShaders.usf"), TEXT("OverlapRadialPixelMain"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TDeferredLightOverlapPS<false>, TEXT("/Engine/Private/StationaryLightOverlapShaders.usf"), TEXT("OverlapDirectionalPixelMain"), SF_Pixel);

void FSceneRenderer::SplitSimpleLightsByView(const FSceneViewFamily& ViewFamily, const TArray<FViewInfo>& Views, const FSimpleLightArray& SimpleLights, FSimpleLightArray* SimpleLightsByView)
{
	for (int32 LightIndex = 0; LightIndex < SimpleLights.InstanceData.Num(); ++LightIndex)
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			FSimpleLightPerViewEntry PerViewEntry = SimpleLights.GetViewDependentData(LightIndex, ViewIndex, Views.Num());
			SimpleLightsByView[ViewIndex].InstanceData.Add(SimpleLights.InstanceData[LightIndex]);
			SimpleLightsByView[ViewIndex].PerViewData.Add(PerViewEntry);
		}
	}
}

/** Gathers simple lights from visible primtives in the passed in views. */
void FSceneRenderer::GatherSimpleLights(const FSceneViewFamily& ViewFamily, const TArray<FViewInfo>& Views, FSimpleLightArray& SimpleLights)
{
	TArray<const FPrimitiveSceneInfo*, SceneRenderingAllocator> PrimitivesWithSimpleLights;

	// Gather visible primitives from all views that might have simple lights
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];
		for (int32 PrimitiveIndex = 0; PrimitiveIndex < View.VisibleDynamicPrimitivesWithSimpleLights.Num(); PrimitiveIndex++)
		{
			const FPrimitiveSceneInfo* PrimitiveSceneInfo = View.VisibleDynamicPrimitivesWithSimpleLights[PrimitiveIndex];

			// TArray::AddUnique is slow, but not expecting many entries in PrimitivesWithSimpleLights
			PrimitivesWithSimpleLights.AddUnique(PrimitiveSceneInfo);
		}
	}

	// Gather simple lights from the primitives
	for (int32 PrimitiveIndex = 0; PrimitiveIndex < PrimitivesWithSimpleLights.Num(); PrimitiveIndex++)
	{
		const FPrimitiveSceneInfo* Primitive = PrimitivesWithSimpleLights[PrimitiveIndex];
		Primitive->Proxy->GatherSimpleLights(ViewFamily, SimpleLights);
	}
}

/** Gets a readable light name for use with a draw event. */
void FSceneRenderer::GetLightNameForDrawEvent(const FLightSceneProxy* LightProxy, FString& LightNameWithLevel)
{
#if WANTS_DRAW_MESH_EVENTS
	if (GetEmitDrawEvents())
	{
		FString FullLevelName = LightProxy->GetLevelName().ToString();
		const int32 LastSlashIndex = FullLevelName.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);

		if (LastSlashIndex != INDEX_NONE)
		{
			// Trim the leading path before the level name to make it more readable
			// The level FName was taken directly from the Outermost UObject, otherwise we would do this operation on the game thread
			FullLevelName = FullLevelName.Mid(LastSlashIndex + 1, FullLevelName.Len() - (LastSlashIndex + 1));
		}

		LightNameWithLevel = FullLevelName + TEXT(".") + LightProxy->GetComponentName().ToString();
	}
#endif
}

extern int32 GbEnableAsyncComputeTranslucencyLightingVolumeClear;

uint32 GetShadowQuality();

static bool LightRequiresDenosier(const FLightSceneInfo& LightSceneInfo)
{
	ELightComponentType LightType = ELightComponentType(LightSceneInfo.Proxy->GetLightType());
	if (LightType == LightType_Directional)
	{
		return LightSceneInfo.Proxy->GetLightSourceAngle() > 0;
	}
	else if (LightType == LightType_Point || LightType == LightType_Spot)
	{
		return LightSceneInfo.Proxy->GetSourceRadius() > 0;
	}
	else if (LightType == LightType_Rect)
	{
		return true;
	}
	else
	{
		check(0);
	}
	return false;
}


/** Renders the scene's lighting. */
void FDeferredShadingSceneRenderer::RenderLights(FRHICommandListImmediate& RHICmdList)
{
	check(RHICmdList.IsOutsideRenderPass());

	SCOPED_NAMED_EVENT(FDeferredShadingSceneRenderer_RenderLights, FColor::Emerald);
	SCOPED_DRAW_EVENT(RHICmdList, DirectLighting);
	SCOPED_GPU_STAT(RHICmdList, Lights);


	bool bStencilBufferDirty = false;	// The stencil buffer should've been cleared to 0 already

	SCOPE_CYCLE_COUNTER(STAT_LightingDrawTime);
	SCOPE_CYCLE_COUNTER(STAT_LightRendering);

	FSimpleLightArray SimpleLights;
	if (bAllowSimpleLights)
	{
		GatherSimpleLights(ViewFamily, Views, SimpleLights);
	}

	TArray<FSortedLightSceneInfo, SceneRenderingAllocator> SortedLights;
	SortedLights.Empty(Scene->Lights.Num());

	bool bDynamicShadows = ViewFamily.EngineShowFlags.DynamicShadows && GetShadowQuality() > 0;
	
	// Build a list of visible lights.
	for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
	{
		const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
		const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

		if (LightSceneInfo->ShouldRenderLightViewIndependent()
			// Reflection override skips direct specular because it tends to be blindingly bright with a perfectly smooth surface
			&& !ViewFamily.EngineShowFlags.ReflectionOverride)
		{
			// Check if the light is visible in any of the views.
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				if (LightSceneInfo->ShouldRenderLight(Views[ViewIndex]))
				{
					FSortedLightSceneInfo* SortedLightInfo = new(SortedLights) FSortedLightSceneInfo(LightSceneInfo);

					// Check for shadows and light functions.
					SortedLightInfo->SortKey.Fields.LightType = LightSceneInfoCompact.LightType;
					SortedLightInfo->SortKey.Fields.bTextureProfile = ViewFamily.EngineShowFlags.TexturedLightProfiles && LightSceneInfo->Proxy->GetIESTextureResource();
					SortedLightInfo->SortKey.Fields.bShadowed = bDynamicShadows && CheckForProjectedShadows(LightSceneInfo);
					SortedLightInfo->SortKey.Fields.bLightFunction = ViewFamily.EngineShowFlags.LightFunctions && CheckForLightFunction(LightSceneInfo);
					SortedLightInfo->SortKey.Fields.bUsesLightingChannels = Views[ViewIndex].bUsesLightingChannels && LightSceneInfo->Proxy->GetLightingChannelMask() != GetDefaultLightingChannelMask();

					// tiled deferred lighting only supported for certain lights that don't use any additional features
					const bool bTiledDeferredSupported = LightSceneInfo->Proxy->IsTiledDeferredLightingSupported() &&
						!SortedLightInfo->SortKey.Fields.bTextureProfile &&
						!SortedLightInfo->SortKey.Fields.bShadowed &&
						!SortedLightInfo->SortKey.Fields.bLightFunction &&
						!SortedLightInfo->SortKey.Fields.bUsesLightingChannels;
					SortedLightInfo->SortKey.Fields.bTiledDeferredNotSupported = !bTiledDeferredSupported;
					break;
				}
			}
		}
	}

	// Sort non-shadowed, non-light function lights first to avoid render target switches.
	struct FCompareFSortedLightSceneInfo
	{
		FORCEINLINE bool operator()( const FSortedLightSceneInfo& A, const FSortedLightSceneInfo& B ) const
		{
			return A.SortKey.Packed < B.SortKey.Packed;
		}
	};
	SortedLights.Sort( FCompareFSortedLightSceneInfo() );

	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		int32 AttenuationLightStart = SortedLights.Num();
		int32 SupportedByTiledDeferredLightEnd = SortedLights.Num();

		// Iterate over all lights to be rendered and build ranges for tiled deferred and unshadowed lights
		for (int32 LightIndex = 0; LightIndex < SortedLights.Num(); LightIndex++)
		{
			const FSortedLightSceneInfo& SortedLightInfo = SortedLights[LightIndex];
			const bool bDrawShadows = SortedLightInfo.SortKey.Fields.bShadowed;
			const bool bDrawLightFunction = SortedLightInfo.SortKey.Fields.bLightFunction;
			const bool bTextureLightProfile = SortedLightInfo.SortKey.Fields.bTextureProfile;
			const bool bLightingChannels = SortedLightInfo.SortKey.Fields.bUsesLightingChannels;

			if (SortedLightInfo.SortKey.Fields.bTiledDeferredNotSupported && SupportedByTiledDeferredLightEnd == SortedLights.Num())
			{
				// Mark the first index to not support tiled deferred
				SupportedByTiledDeferredLightEnd = LightIndex;
			}

			if (bDrawShadows || bDrawLightFunction || bLightingChannels)
			{
				// Once we find a shadowed light, we can exit the loop, these lights should never support tiled deferred rendering either
				check(SortedLightInfo.SortKey.Fields.bTiledDeferredNotSupported);
				AttenuationLightStart = LightIndex;
				break;
			}
		}
		
		if (GbEnableAsyncComputeTranslucencyLightingVolumeClear && GSupportsEfficientAsyncCompute)
		{
			//Gfx pipe must wait for the async compute clear of the translucency volume clear.
			RHICmdList.WaitComputeFence(TranslucencyLightingVolumeClearEndFence);
		}

		if(ViewFamily.EngineShowFlags.DirectLighting)
		{
			SCOPED_DRAW_EVENT(RHICmdList, NonShadowedLights);
			INC_DWORD_STAT_BY(STAT_NumUnshadowedLights, AttenuationLightStart);

			int32 StandardDeferredStart = 0;

			bool bRenderSimpleLightsStandardDeferred = SimpleLights.InstanceData.Num() > 0;

			if (CanUseTiledDeferred())
			{
				bool bAnyViewIsStereo = false;
				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
				{
					if (Views[ViewIndex].StereoPass != eSSP_FULL)
					{
						bAnyViewIsStereo = true;
						break;
					}
				}

				// Use tiled deferred shading on any unshadowed lights without a texture light profile
				if (ShouldUseTiledDeferred(SupportedByTiledDeferredLightEnd, SimpleLights.InstanceData.Num()) && !bAnyViewIsStereo)
				{
					// Update the range that needs to be processed by standard deferred to exclude the lights done with tiled
					StandardDeferredStart = SupportedByTiledDeferredLightEnd;
					bRenderSimpleLightsStandardDeferred = false;
					RenderTiledDeferredLighting(RHICmdList, SortedLights, SupportedByTiledDeferredLightEnd, SimpleLights);
				}
			}
			
			if (bRenderSimpleLightsStandardDeferred)
			{
				SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite);
				RenderSimpleLightsStandardDeferred(RHICmdList, SimpleLights);
				SceneContext.FinishRenderingSceneColor(RHICmdList);
			}

			{
				SCOPED_DRAW_EVENT(RHICmdList, StandardDeferredLighting);

				// make sure we don't clear the depth
				SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, true);

				// Draw non-shadowed non-light function lights without changing render targets between them
				for (int32 LightIndex = StandardDeferredStart; LightIndex < AttenuationLightStart; LightIndex++)
				{
					const FSortedLightSceneInfo& SortedLightInfo = SortedLights[LightIndex];
					const FLightSceneInfo* const LightSceneInfo = SortedLightInfo.LightSceneInfo;

					// Render the light to the scene color buffer, using a 1x1 white texture as input 
					RenderLight(RHICmdList, LightSceneInfo, NULL, false, false);
				}

				SceneContext.FinishRenderingSceneColor(RHICmdList);
			}

			if (GUseTranslucentLightingVolumes && GSupportsVolumeTextureRendering)
			{
				if (AttenuationLightStart)
				{
					// Inject non-shadowed, non-light function lights in to the volume.
					SCOPED_DRAW_EVENT(RHICmdList, InjectNonShadowedTranslucentLighting);
					InjectTranslucentVolumeLightingArray(RHICmdList, SortedLights, AttenuationLightStart);
				}

				if(SimpleLights.InstanceData.Num() > 0)
				{
					FSimpleLightArray* SimpleLightsByView = new FSimpleLightArray[Views.Num()];

					SplitSimpleLightsByView(ViewFamily, Views, SimpleLights, SimpleLightsByView);

					for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
					{
						if (SimpleLightsByView[ViewIndex].InstanceData.Num() > 0)
						{
							SCOPED_DRAW_EVENT(RHICmdList, InjectSimpleLightsTranslucentLighting);
							InjectSimpleTranslucentVolumeLightingArray(RHICmdList, SimpleLightsByView[ViewIndex], Views[ViewIndex], ViewIndex);
						}
					}

					delete[] SimpleLightsByView;
				}
			}
		}

		EShaderPlatform ShaderPlatformForFeatureLevel = GShaderPlatformForFeatureLevel[FeatureLevel]; 

		if ( IsFeatureLevelSupported(ShaderPlatformForFeatureLevel, ERHIFeatureLevel::SM5) )
		{
			SCOPED_DRAW_EVENT(RHICmdList, IndirectLighting);
			bool bRenderedRSM = false;
			// Render Reflective shadow maps
			// Draw shadowed and light function lights
			for (int32 LightIndex = AttenuationLightStart; LightIndex < SortedLights.Num(); LightIndex++)
			{
				const FSortedLightSceneInfo& SortedLightInfo = SortedLights[LightIndex];
				const FLightSceneInfo& LightSceneInfo = *SortedLightInfo.LightSceneInfo;
				// Render any reflective shadow maps (if necessary)
				if ( LightSceneInfo.Proxy && LightSceneInfo.Proxy->NeedsLPVInjection() )
				{
					if ( LightSceneInfo.Proxy->HasReflectiveShadowMap() )
					{
						INC_DWORD_STAT(STAT_NumReflectiveShadowMapLights);
						InjectReflectiveShadowMaps(RHICmdList, &LightSceneInfo);
						bRenderedRSM = true;
					}
				}
			}

			// LPV Direct Light Injection
			if ( bRenderedRSM )
			{
				for (int32 LightIndex = 0; LightIndex < SortedLights.Num(); LightIndex++)
				{
					const FSortedLightSceneInfo& SortedLightInfo = SortedLights[LightIndex];
					const FLightSceneInfo* const LightSceneInfo = SortedLightInfo.LightSceneInfo;

					// Render any reflective shadow maps (if necessary)
					if ( LightSceneInfo && LightSceneInfo->Proxy && LightSceneInfo->Proxy->NeedsLPVInjection() )
					{
						if ( !LightSceneInfo->Proxy->HasReflectiveShadowMap() )
						{
							// Inject the light directly into all relevant LPVs
							for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
							{
								FViewInfo& View = Views[ViewIndex];

								if (LightSceneInfo->ShouldRenderLight(View))
								{
									FSceneViewState* ViewState = (FSceneViewState*)View.State;
									if (ViewState)
									{
										FLightPropagationVolume* Lpv = ViewState->GetLightPropagationVolume(View.GetFeatureLevel());
										if (Lpv && LightSceneInfo->Proxy)
										{
											Lpv->InjectLightDirect(RHICmdList, *LightSceneInfo->Proxy, View);
										}
									}
								}
							}					
						}
					}
				}
			}

			// Kickoff the LPV update (asynchronously if possible)
			UpdateLPVs(RHICmdList);
		}

		{
			SCOPED_DRAW_EVENT(RHICmdList, ShadowedLights);

			const int32 DenoiserMode = CVarShadowUseDenoiser.GetValueOnRenderThread();

			const IScreenSpaceDenoiser* DefaultDenoiser = IScreenSpaceDenoiser::GetDefaultDenoiser();
			const IScreenSpaceDenoiser* DenoiserToUse = DenoiserMode == 1 ? DefaultDenoiser : GScreenSpaceDenoiser;

			TArray<TRefCountPtr<IPooledRenderTarget>> PreprocessedShadowMaskTextures;

			const int32 MaxDenoisingBatchSize = FMath::Clamp(CVarMaxShadowDenoisingBatchSize.GetValueOnRenderThread(), 1, IScreenSpaceDenoiser::kMaxBatchSize);
			const bool bDoShadowDenoisingBatching = DenoiserMode != 0 && MaxDenoisingBatchSize > 1;

			// Optimisations: batches all shadow ray tracing denoising. Definitely could be smarter to avoid high VGPR pressure if this entire
			// function was converted to render graph, and want least intrusive change as possible. So right not it trades render target memory pressure
			// for denoising perf.
			if (RHI_RAYTRACING && bDoShadowDenoisingBatching)
			{
				TStaticArray<IScreenSpaceDenoiser::FShadowParameters, IScreenSpaceDenoiser::kMaxBatchSize> DenoisingQueue;
				TStaticArray<int32, IScreenSpaceDenoiser::kMaxBatchSize> LightIndices;

				FRDGBuilder GraphBuilder(RHICmdList);

				FSceneViewFamilyBlackboard SceneBlackboard;
				SetupSceneViewFamilyBlackboard(GraphBuilder, &SceneBlackboard);

				FViewInfo& View = Views[0];

				// Allocate PreprocessedShadowMaskTextures once so QueueTextureExtraction can deferred write.
				{
					if (!View.bViewStateIsReadOnly)
					{
						View.ViewState->PrevFrameViewInfo.ShadowHistories.Empty();
						View.ViewState->PrevFrameViewInfo.ShadowHistories.Reserve(SortedLights.Num());
					}
					PreprocessedShadowMaskTextures.Reserve(SortedLights.Num());
					for (int32 LightIndex = AttenuationLightStart; LightIndex < SortedLights.Num(); LightIndex++)
					{
						const FSortedLightSceneInfo& SortedLightInfo = SortedLights[LightIndex];
						const FLightSceneInfo& LightSceneInfo = *SortedLightInfo.LightSceneInfo;

						PreprocessedShadowMaskTextures.Add(TRefCountPtr<IPooledRenderTarget>());

						if (!View.bViewStateIsReadOnly)
						{
							View.ViewState->PrevFrameViewInfo.ShadowHistories.Emplace(LightSceneInfo.Proxy->GetLightComponent());
						}
					}
				}

				TMap<const FLightSceneInfo*, FRDGTextureRef> ShadowMaskTextures;

				// Lambda to share the code quicking of the shadow denoiser.
				auto QuickOffDenoisingBatch = [&](){
					int32 InputParameterCount = 0;
					for (int32 i = 0; i < IScreenSpaceDenoiser::kMaxBatchSize; i++)
					{
						InputParameterCount += DenoisingQueue[i].LightSceneInfo != nullptr ? 1 : 0;
					}

					check(InputParameterCount >= 1);

					TStaticArray<IScreenSpaceDenoiser::FShadowPenumbraOutputs, IScreenSpaceDenoiser::kMaxBatchSize> Outputs;

					RDG_EVENT_SCOPE(GraphBuilder, "%s%s(Shadow BatchSize=%d) %dx%d",
						DenoiserToUse != DefaultDenoiser ? TEXT("ThirdParty ") : TEXT(""),
						DenoiserToUse->GetDebugName(),
						InputParameterCount,
						View.ViewRect.Width(), View.ViewRect.Height());

					DenoiserToUse->DenoiseShadows(
						GraphBuilder,
						View,
						&View.PrevViewInfo,
						SceneBlackboard,
						DenoisingQueue,
						InputParameterCount,
						Outputs);

					for (int32 i = 0; i < InputParameterCount; i++)
					{
						const FLightSceneInfo* LightSceneInfo = DenoisingQueue[i].LightSceneInfo;

						int32 LightIndex = LightIndices[i];
						TRefCountPtr<IPooledRenderTarget>* RefDestination = &PreprocessedShadowMaskTextures[LightIndex - AttenuationLightStart];
						check(*RefDestination == nullptr);

						GraphBuilder.QueueTextureExtraction(Outputs[i].DiffusePenumbra, RefDestination);
						DenoisingQueue[i].LightSceneInfo = nullptr;
					}
				}; // QuickOffDenoisingBatch

				// Ray trace shadows of light that needs, and quick off denoising batch.
				for (int32 LightIndex = AttenuationLightStart; LightIndex < SortedLights.Num(); LightIndex++)
				{
					const FSortedLightSceneInfo& SortedLightInfo = SortedLights[LightIndex];
					const FLightSceneInfo& LightSceneInfo = *SortedLightInfo.LightSceneInfo;

					// Denoiser do not support texture rect light important sampling.
					const bool bDrawShadows = SortedLightInfo.SortKey.Fields.bShadowed && !ShouldRenderRayTracingStochasticRectLight(LightSceneInfo);

					if (!bDrawShadows)
						continue;

					INC_DWORD_STAT(STAT_NumShadowedLights);

					const FLightOcclusionType OcclusionType = GetLightOcclusionType(*LightSceneInfo.Proxy);
					if (OcclusionType != FLightOcclusionType::Raytraced)
						continue;

					if (!LightRequiresDenosier(LightSceneInfo))
						continue;

					IScreenSpaceDenoiser::FShadowRayTracingConfig RayTracingConfig;
					RayTracingConfig.RayCountPerPixel = LightSceneInfo.Proxy->GetSamplesPerPixel();

					IScreenSpaceDenoiser::EShadowRequirements DenoiserRequirements = DenoiserToUse->GetShadowRequirements(
						View, LightSceneInfo, RayTracingConfig);

					// Not worth batching and increase memory pressure if the denoiser do not support this ray tracing config.
					// TODO: add suport for batch with multiple SPP.
					if (DenoiserRequirements != IScreenSpaceDenoiser::EShadowRequirements::PenumbraAndClosestOccluder)
					{
						continue;
					}

					// Ray trace the shadow.
					FRDGTextureRef ShadowMask;
					FRDGTextureRef RayHitDistance;
					{
						FString LightNameWithLevel;
						GetLightNameForDrawEvent(LightSceneInfo.Proxy, LightNameWithLevel);
						RDG_EVENT_SCOPE(GraphBuilder, "%s", *LightNameWithLevel);

						RenderRayTracingShadows(
							GraphBuilder,
							SceneBlackboard,
							View,
							LightSceneInfo,
							RayTracingConfig,
							DenoiserRequirements,
							&ShadowMask,
							&RayHitDistance);
					}

					// Queue the ray tracing output for shadow denoising.
					for (int32 i = 0; i < IScreenSpaceDenoiser::kMaxBatchSize; i++)
					{
						if (DenoisingQueue[i].LightSceneInfo == nullptr)
						{
							DenoisingQueue[i].LightSceneInfo = &LightSceneInfo;
							DenoisingQueue[i].RayTracingConfig = RayTracingConfig;
							DenoisingQueue[i].InputTextures.Penumbra = ShadowMask;
							DenoisingQueue[i].InputTextures.ClosestOccluder = RayHitDistance;
							LightIndices[i] = LightIndex;

							// If queue for this light type is full, quick of the batch.
							if ((i + 1) == MaxDenoisingBatchSize)
							{
								QuickOffDenoisingBatch();
							}
							break;
						}
						else
						{
							check((i - 1) < IScreenSpaceDenoiser::kMaxBatchSize);
						}
					}

				} // for (int32 LightIndex = AttenuationLightStart; LightIndex < SortedLights.Num(); LightIndex++)

				// Ensures all denoising queues are processed.
				if (DenoisingQueue[0].LightSceneInfo)
				{
					QuickOffDenoisingBatch();
				}

				GraphBuilder.Execute();
			} // if (RHI_RAYTRACING)

			bool bDirectLighting = ViewFamily.EngineShowFlags.DirectLighting;

			TRefCountPtr<IPooledRenderTarget> ScreenShadowMaskTexture;

			// Draw shadowed and light function lights
			for (int32 LightIndex = AttenuationLightStart; LightIndex < SortedLights.Num(); LightIndex++)
			{
				const FSortedLightSceneInfo& SortedLightInfo = SortedLights[LightIndex];
				const FLightSceneInfo& LightSceneInfo = *SortedLightInfo.LightSceneInfo;

				// Note: Skip shadow mask generation for rect light if direct illumination is computed 
				//		 stochastically (rather than analytically + shadow mask)
				const bool bDrawShadows = SortedLightInfo.SortKey.Fields.bShadowed && !ShouldRenderRayTracingStochasticRectLight(LightSceneInfo);
				bool bDrawLightFunction = SortedLightInfo.SortKey.Fields.bLightFunction;
				bool bDrawPreviewIndicator = ViewFamily.EngineShowFlags.PreviewShadowsIndicator && !LightSceneInfo.IsPrecomputedLightingValid() && LightSceneInfo.Proxy->HasStaticShadowing();
				bool bInjectedTranslucentVolume = false;
				bool bUsedShadowMaskTexture = false;
				
				FScopeCycleCounter Context(LightSceneInfo.Proxy->GetStatId());

				if ((bDrawShadows || bDrawLightFunction || bDrawPreviewIndicator) && !ScreenShadowMaskTexture.IsValid())
				{
					SceneContext.AllocateScreenShadowMask(RHICmdList, ScreenShadowMaskTexture);
				}

				FString LightNameWithLevel;
				GetLightNameForDrawEvent(LightSceneInfo.Proxy, LightNameWithLevel);
				SCOPED_DRAW_EVENTF(RHICmdList, EventLightPass, *LightNameWithLevel);

				if (bDrawShadows)
				{
					INC_DWORD_STAT(STAT_NumShadowedLights);

					const FLightOcclusionType OcclusionType = GetLightOcclusionType(*LightSceneInfo.Proxy);
					if (RHI_RAYTRACING && PreprocessedShadowMaskTextures.Num() > 0 && PreprocessedShadowMaskTextures[LightIndex - AttenuationLightStart])
					{
						ScreenShadowMaskTexture = PreprocessedShadowMaskTextures[LightIndex - AttenuationLightStart];
						PreprocessedShadowMaskTextures[LightIndex - AttenuationLightStart] = nullptr;
					}
					else if (OcclusionType == FLightOcclusionType::Raytraced)
					{
						FViewInfo& View = Views[0];

						IScreenSpaceDenoiser::FShadowRayTracingConfig RayTracingConfig;
						RayTracingConfig.RayCountPerPixel = LightSceneInfo.Proxy->GetSamplesPerPixel();

						IScreenSpaceDenoiser::EShadowRequirements DenoiserRequirements = IScreenSpaceDenoiser::EShadowRequirements::Bailout;
						if (DenoiserMode != 0 && LightRequiresDenosier(LightSceneInfo))
						{
							DenoiserRequirements = DenoiserToUse->GetShadowRequirements(View, LightSceneInfo, RayTracingConfig);
						}

						FRDGBuilder GraphBuilder(RHICmdList);

						FSceneViewFamilyBlackboard SceneBlackboard;
						SetupSceneViewFamilyBlackboard(GraphBuilder, &SceneBlackboard);

						FRDGTextureRef ShadowMask;
						FRDGTextureRef RayHitDistance;
						RenderRayTracingShadows(
							GraphBuilder,
							SceneBlackboard,
							View,
							LightSceneInfo,
							RayTracingConfig,
							DenoiserRequirements,
							&ShadowMask,
							&RayHitDistance);

						if (DenoiserRequirements != IScreenSpaceDenoiser::EShadowRequirements::Bailout)
						{
							TStaticArray<IScreenSpaceDenoiser::FShadowParameters, IScreenSpaceDenoiser::kMaxBatchSize> InputParameters;
							TStaticArray<IScreenSpaceDenoiser::FShadowPenumbraOutputs, IScreenSpaceDenoiser::kMaxBatchSize> Outputs;

							InputParameters[0].InputTextures.Penumbra = ShadowMask;
							InputParameters[0].InputTextures.ClosestOccluder = RayHitDistance;
							InputParameters[0].LightSceneInfo = &LightSceneInfo;
							InputParameters[0].RayTracingConfig = RayTracingConfig;

							int32 InputParameterCount = 1;

							RDG_EVENT_SCOPE(GraphBuilder, "%s%s(Shadow BatchSize=%d) %dx%d",
								DenoiserToUse != DefaultDenoiser ? TEXT("ThirdParty ") : TEXT(""),
								DenoiserToUse->GetDebugName(),
								InputParameterCount,
								View.ViewRect.Width(), View.ViewRect.Height());

							DenoiserToUse->DenoiseShadows(
								GraphBuilder,
								View,
								&View.PrevViewInfo,
								SceneBlackboard,
								InputParameters,
								InputParameterCount,
								Outputs);

							GraphBuilder.QueueTextureExtraction(Outputs[0].DiffusePenumbra, &ScreenShadowMaskTexture);
						}
						else
						{
							GraphBuilder.QueueTextureExtraction(ShadowMask, &ScreenShadowMaskTexture);
						}

						GraphBuilder.Execute();
					}
					else // (OcclusionType == FOcclusionType::Shadowmap)
					{
						for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
						{
							const FViewInfo& View = Views[ViewIndex];
							View.HeightfieldLightingViewInfo.ClearShadowing(View, RHICmdList, LightSceneInfo);
						}

						// Clear light attenuation for local lights with a quad covering their extents
						const bool bClearLightScreenExtentsOnly = SortedLightInfo.SortKey.Fields.LightType != LightType_Directional;
						// All shadows render with min blending
						bool bClearToWhite = !bClearLightScreenExtentsOnly;

						FRHIRenderPassInfo RPInfo(ScreenShadowMaskTexture->GetRenderTargetItem().TargetableTexture, ERenderTargetActions::Load_Store);
						RPInfo.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(ERenderTargetActions::Load_DontStore, ERenderTargetActions::Load_Store);
						RPInfo.DepthStencilRenderTarget.DepthStencilTarget = SceneContext.GetSceneDepthSurface();
						RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthRead_StencilWrite;
						if (bClearToWhite)
						{
							RPInfo.ColorRenderTargets[0].Action = ERenderTargetActions::Clear_Store;
						}

						TransitionRenderPassTargets(RHICmdList, RPInfo);
						RHICmdList.BeginRenderPass(RPInfo, TEXT("ClearScreenShadowMask"));
						if (bClearLightScreenExtentsOnly)
						{
							SCOPED_DRAW_EVENT(RHICmdList, ClearQuad);

							for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
							{
								const FViewInfo& View = Views[ViewIndex];
								FIntRect ScissorRect;

								if (!LightSceneInfo.Proxy->GetScissorRect(ScissorRect, View, View.ViewRect))
								{
									ScissorRect = View.ViewRect;
								}

								if (ScissorRect.Min.X < ScissorRect.Max.X && ScissorRect.Min.Y < ScissorRect.Max.Y)
								{
									RHICmdList.SetViewport(ScissorRect.Min.X, ScissorRect.Min.Y, 0.0f, ScissorRect.Max.X, ScissorRect.Max.Y, 1.0f);
									DrawClearQuad(RHICmdList, true, FLinearColor(1, 1, 1, 1), false, 0, false, 0);
								}
								else
								{
									LightSceneInfo.Proxy->GetScissorRect(ScissorRect, View, View.ViewRect);
								}
							}
						}

						RHICmdList.EndRenderPass();

						RenderShadowProjections(RHICmdList, &LightSceneInfo, ScreenShadowMaskTexture, bInjectedTranslucentVolume);
					}

					bUsedShadowMaskTexture = true;					
				}

				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					const FViewInfo& View = Views[ViewIndex];
					View.HeightfieldLightingViewInfo.ComputeLighting(View, RHICmdList, LightSceneInfo);
				}
			
				// Render light function to the attenuation buffer.
				if (bDirectLighting)
				{
					if (bDrawLightFunction)
					{
						const bool bLightFunctionRendered = RenderLightFunction(RHICmdList, &LightSceneInfo, ScreenShadowMaskTexture, bDrawShadows, false);
						bUsedShadowMaskTexture |= bLightFunctionRendered;
					}

					if (bDrawPreviewIndicator)
					{
						RenderPreviewShadowsIndicator(RHICmdList, &LightSceneInfo, ScreenShadowMaskTexture, bUsedShadowMaskTexture);
					}

					if (!bDrawShadows)
					{
						INC_DWORD_STAT(STAT_NumLightFunctionOnlyLights);
					}
				}

				if (bUsedShadowMaskTexture)
				{
					RHICmdList.CopyToResolveTarget(ScreenShadowMaskTexture->GetRenderTargetItem().TargetableTexture, ScreenShadowMaskTexture->GetRenderTargetItem().ShaderResourceTexture, FResolveParams(FResolveRect()));
				}
			
				if(bDirectLighting && !bInjectedTranslucentVolume)
				{
					for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
					{
						SCOPED_DRAW_EVENT(RHICmdList, InjectTranslucentVolume);
						// Accumulate this light's unshadowed contribution to the translucency lighting volume
						InjectTranslucentVolumeLighting(RHICmdList, LightSceneInfo, NULL, Views[ViewIndex], ViewIndex);
					}
				}

				GVisualizeTexture.SetCheckPoint(RHICmdList, ScreenShadowMaskTexture);

				if (ShouldRenderRayTracingStochasticRectLight(LightSceneInfo))
				{
					TRefCountPtr<IPooledRenderTarget> RectLightRT;
					TRefCountPtr<IPooledRenderTarget> HitDistanceRT;
					RenderRayTracingStochasticRectLight(RHICmdList, LightSceneInfo, RectLightRT, HitDistanceRT);
					// #dxr_todo: Denoise RectLight
					CompositeRayTracingSkyLight(RHICmdList, RectLightRT, HitDistanceRT);
				}
				else
				{
					SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite);

					// Render the light to the scene color buffer, conditionally using the attenuation buffer or a 1x1 white texture as input 
					if (bDirectLighting)
					{
						RenderLight(RHICmdList, &LightSceneInfo, ScreenShadowMaskTexture, false, true);
					}

					SceneContext.FinishRenderingSceneColor(RHICmdList);
				}
			}
		}
	}
}

void FDeferredShadingSceneRenderer::RenderLightArrayForOverlapViewmode(FRHICommandListImmediate& RHICmdList, const TSparseArray<FLightSceneInfoCompact>& LightArray)
{
	for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(LightArray); LightIt; ++LightIt)
	{
		const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
		const FLightSceneInfo* LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

		// Nothing to do for black lights.
		if(LightSceneInfoCompact.Color.IsAlmostBlack())
		{
			continue;
		}

		bool bShouldRender = false;

		// Check if the light is visible in any of the views.
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			bShouldRender |= LightSceneInfo->ShouldRenderLight(Views[ViewIndex]);
		}

		if (bShouldRender 
			// Only render shadow casting stationary lights
			&& LightSceneInfo->Proxy->HasStaticShadowing() 
			&& !LightSceneInfo->Proxy->HasStaticLighting()
			&& LightSceneInfo->Proxy->CastsStaticShadow())
		{
			RenderLight(RHICmdList, LightSceneInfo, NULL, true, false);
		}
	}
}

void FDeferredShadingSceneRenderer::RenderStationaryLightOverlap(FRHICommandListImmediate& RHICmdList)
{
	if (Scene->bIsEditorScene)
	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EUninitializedColorExistingDepth, FExclusiveDepthStencil::DepthRead_StencilWrite);

		// Clear to discard base pass values in scene color since we didn't skip that, to have valid scene depths
		DrawClearQuad(RHICmdList, FLinearColor::Black);

		RenderLightArrayForOverlapViewmode(RHICmdList, Scene->Lights);

		//Note: making use of FScene::InvisibleLights, which contains lights that haven't been added to the scene in the same way as visible lights
		// So code called by RenderLightArrayForOverlapViewmode must be careful what it accesses
		RenderLightArrayForOverlapViewmode(RHICmdList, Scene->InvisibleLights);

		SceneContext.FinishRenderingSceneColor(RHICmdList);
	}
}

/** Sets up rasterizer and depth state for rendering bounding geometry in a deferred pass. */
void SetBoundingGeometryRasterizerAndDepthState(FGraphicsPipelineStateInitializer& GraphicsPSOInit, const FViewInfo& View, const FSphere& LightBounds)
{
	const bool bCameraInsideLightGeometry = ((FVector)View.ViewMatrices.GetViewOrigin() - LightBounds.Center).SizeSquared() < FMath::Square(LightBounds.W * 1.05f + View.NearClippingDistance * 2.0f)
		// Always draw backfaces in ortho
		//@todo - accurate ortho camera / light intersection
		|| !View.IsPerspectiveProjection();

	if (bCameraInsideLightGeometry)
	{
		// Render backfaces with depth tests disabled since the camera is inside (or close to inside) the light geometry
		GraphicsPSOInit.RasterizerState = View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI();
	}
	else
	{
		// Render frontfaces with depth tests on to get the speedup from HiZ since the camera is outside the light geometry
		GraphicsPSOInit.RasterizerState = View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI();
	}

	GraphicsPSOInit.DepthStencilState =
		bCameraInsideLightGeometry
		? TStaticDepthStencilState<false, CF_Always>::GetRHI()
		: TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI();
}

template<bool bUseIESProfile, bool bRadialAttenuation, bool bInverseSquaredFalloff>
static void SetShaderTemplLightingSimple(
	FRHICommandList& RHICmdList,
	FGraphicsPipelineStateInitializer& GraphicsPSOInit,
	const FViewInfo& View,
	FShader* VertexShader,
	const FSimpleLightEntry& SimpleLight,
	const FSimpleLightPerViewEntry& SimpleLightPerViewData)
{
	FDeferredLightPS::FPermutationDomain PermutationVector;
	PermutationVector.Set< FDeferredLightPS::FSourceShapeDim >( ELightSourceShape::Capsule );
	PermutationVector.Set< FDeferredLightPS::FIESProfileDim >( bUseIESProfile );
	PermutationVector.Set< FDeferredLightPS::FInverseSquaredDim >( bInverseSquaredFalloff );
	PermutationVector.Set< FDeferredLightPS::FVisualizeCullingDim >( View.Family->EngineShowFlags.VisualizeLightCulling );
	PermutationVector.Set< FDeferredLightPS::FLightingChannelsDim >( false );
	PermutationVector.Set< FDeferredLightPS::FTransmissionDim >( false );

	TShaderMapRef< FDeferredLightPS > PixelShader( View.ShaderMap, PermutationVector );
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
	PixelShader->SetParametersSimpleLight(RHICmdList, View, SimpleLight, SimpleLightPerViewData);
}

// Use DBT to allow work culling on shadow lights
void CalculateLightNearFarDepthFromBounds(const FViewInfo& View, const FSphere &LightBounds, float &NearDepth, float &FarDepth)
{
	const FMatrix ViewProjection = View.ViewMatrices.GetViewProjectionMatrix();
	const FVector ViewDirection = View.GetViewDirection();
	
	// push camera relative bounds center along view vec by its radius
	const FVector FarPoint = LightBounds.Center + LightBounds.W * ViewDirection;
	const FVector4 FarPoint4 = FVector4(FarPoint, 1.f);
	const FVector4 FarPoint4Clip = ViewProjection.TransformFVector4(FarPoint4);
	FarDepth = FarPoint4Clip.Z / FarPoint4Clip.W;

	// pull camera relative bounds center along -view vec by its radius
	const FVector NearPoint = LightBounds.Center - LightBounds.W * ViewDirection;
	const FVector4 NearPoint4 = FVector4(NearPoint, 1.f);
	const FVector4 NearPoint4Clip = ViewProjection.TransformFVector4(NearPoint4);
	NearDepth = NearPoint4Clip.Z / NearPoint4Clip.W;

	// negative means behind view, but we use a NearClipPlane==1.f depth	

	if (NearPoint4Clip.W < 0)
		NearDepth = 1;

	if (FarPoint4Clip.W < 0)
		FarDepth = 1;

	NearDepth = FMath::Clamp(NearDepth, 0.0f, 1.0f);
	FarDepth = FMath::Clamp(FarDepth, 0.0f, 1.0f);

}

/**
 * Used by RenderLights to render a light to the scene color buffer.
 *
 * @param LightSceneInfo Represents the current light
 * @param LightIndex The light's index into FScene::Lights
 * @return true if anything got rendered
 */
void FDeferredShadingSceneRenderer::RenderLight(FRHICommandList& RHICmdList, const FLightSceneInfo* LightSceneInfo, IPooledRenderTarget* ScreenShadowMaskTexture, bool bRenderOverlap, bool bIssueDrawEvent)
{
	SCOPE_CYCLE_COUNTER(STAT_DirectLightRenderingTime);
	INC_DWORD_STAT(STAT_NumLightsUsingStandardDeferred);
	SCOPED_CONDITIONAL_DRAW_EVENT(RHICmdList, StandardDeferredLighting, bIssueDrawEvent);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	// Use additive blending for color
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	const FSphere LightBounds = LightSceneInfo->Proxy->GetBoundingSphere();
	const bool bTransmission = LightSceneInfo->Proxy->Transmission();

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		// Ensure the light is valid for this view
		if (!LightSceneInfo->ShouldRenderLight(View))
		{
			continue;
		}

		bool bUseIESTexture = false;

		if(View.Family->EngineShowFlags.TexturedLightProfiles)
		{
			bUseIESTexture = (LightSceneInfo->Proxy->GetIESTextureResource() != 0);
		}

		// Set the device viewport for the view.
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

		if (LightSceneInfo->Proxy->GetLightType() == LightType_Directional)
		{
			// Turn DBT back off
			GraphicsPSOInit.bDepthBounds = false;
			TShaderMapRef<TDeferredLightVS<false> > VertexShader(View.ShaderMap);

			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

			if (bRenderOverlap)
			{
				TShaderMapRef<TDeferredLightOverlapPS<false> > PixelShader(View.ShaderMap);
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
				PixelShader->SetParameters(RHICmdList, View, LightSceneInfo);
			}
			else
			{
				FDeferredLightPS::FPermutationDomain PermutationVector;
				PermutationVector.Set< FDeferredLightPS::FSourceShapeDim >( ELightSourceShape::Directional );
				PermutationVector.Set< FDeferredLightPS::FIESProfileDim >( false );
				PermutationVector.Set< FDeferredLightPS::FInverseSquaredDim >( false );
				PermutationVector.Set< FDeferredLightPS::FVisualizeCullingDim >( View.Family->EngineShowFlags.VisualizeLightCulling );
				PermutationVector.Set< FDeferredLightPS::FLightingChannelsDim >( View.bUsesLightingChannels );
				PermutationVector.Set< FDeferredLightPS::FTransmissionDim >( bTransmission );

				TShaderMapRef< FDeferredLightPS > PixelShader( View.ShaderMap, PermutationVector );
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);

				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
				PixelShader->SetParameters(RHICmdList, View, LightSceneInfo, ScreenShadowMaskTexture);
			}

			VertexShader->SetParameters(RHICmdList, View, LightSceneInfo);

			// Apply the directional light as a full screen quad
			DrawRectangle( 
				RHICmdList,
				0, 0,
				View.ViewRect.Width(), View.ViewRect.Height(),
				View.ViewRect.Min.X, View.ViewRect.Min.Y, 
				View.ViewRect.Width(), View.ViewRect.Height(),
				View.ViewRect.Size(),
				FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY(),
				*VertexShader,
				EDRF_UseTriangleOptimization);
		}
		else
		{
			// Use DBT to allow work culling on shadow lights
			GraphicsPSOInit.bDepthBounds = (GSupportsDepthBoundsTest && GAllowDepthBoundsTest != 0);

			TShaderMapRef<TDeferredLightVS<true> > VertexShader(View.ShaderMap);

			SetBoundingGeometryRasterizerAndDepthState(GraphicsPSOInit, View, LightBounds);

			if (bRenderOverlap)
			{
				TShaderMapRef<TDeferredLightOverlapPS<true> > PixelShader(View.ShaderMap);
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);

				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
				PixelShader->SetParameters(RHICmdList, View, LightSceneInfo);
			}
			else
			{
				FDeferredLightPS::FPermutationDomain PermutationVector;
				PermutationVector.Set< FDeferredLightPS::FSourceShapeDim >( LightSceneInfo->Proxy->IsRectLight() ? ELightSourceShape::Rect : ELightSourceShape::Capsule );
				PermutationVector.Set< FDeferredLightPS::FSourceTextureDim >( LightSceneInfo->Proxy->IsRectLight() && LightSceneInfo->Proxy->HasSourceTexture() );
				PermutationVector.Set< FDeferredLightPS::FIESProfileDim >( bUseIESTexture );
				PermutationVector.Set< FDeferredLightPS::FInverseSquaredDim >( LightSceneInfo->Proxy->IsInverseSquared() );
				PermutationVector.Set< FDeferredLightPS::FVisualizeCullingDim >( View.Family->EngineShowFlags.VisualizeLightCulling );
				PermutationVector.Set< FDeferredLightPS::FLightingChannelsDim >( View.bUsesLightingChannels );
				PermutationVector.Set< FDeferredLightPS::FTransmissionDim >( bTransmission );

				TShaderMapRef< FDeferredLightPS > PixelShader( View.ShaderMap, PermutationVector );
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);

				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
				PixelShader->SetParameters(RHICmdList, View, LightSceneInfo, ScreenShadowMaskTexture);
			}

			VertexShader->SetParameters(RHICmdList, View, LightSceneInfo);

			// Use DBT to allow work culling on shadow lights
			if (GSupportsDepthBoundsTest && GAllowDepthBoundsTest != 0)
			{
				// Can use the depth bounds test to skip work for pixels which won't be touched by the light (i.e outside the depth range)
				float NearDepth = 1.f;
				float FarDepth = 0.f;
				CalculateLightNearFarDepthFromBounds(View,LightBounds,NearDepth,FarDepth);

				if (NearDepth <= FarDepth)
				{
					NearDepth = 1.0f;
					FarDepth = 0.0f;
				}

				// UE4 uses reversed depth, so far < near
				RHICmdList.SetDepthBounds(FarDepth, NearDepth);
			}

			if( LightSceneInfo->Proxy->GetLightType() == LightType_Point ||
				LightSceneInfo->Proxy->GetLightType() == LightType_Rect )
			{
				// Apply the point or spot light with some approximate bounding geometry, 
				// So we can get speedups from depth testing and not processing pixels outside of the light's influence.
				StencilingGeometry::DrawSphere(RHICmdList);
			}
			else if (LightSceneInfo->Proxy->GetLightType() == LightType_Spot)
			{
				StencilingGeometry::DrawCone(RHICmdList);
			}
		}
	}
}

void FDeferredShadingSceneRenderer::RenderSimpleLightsStandardDeferred(FRHICommandListImmediate& RHICmdList, const FSimpleLightArray& SimpleLights)
{
	SCOPE_CYCLE_COUNTER(STAT_DirectLightRenderingTime);
	INC_DWORD_STAT_BY(STAT_NumLightsUsingStandardDeferred, SimpleLights.InstanceData.Num());
	SCOPED_DRAW_EVENT(RHICmdList, StandardDeferredSimpleLights);
	
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	// Use additive blending for color
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	const int32 NumViews = Views.Num();
	for (int32 LightIndex = 0; LightIndex < SimpleLights.InstanceData.Num(); LightIndex++)
	{
		const FSimpleLightEntry& SimpleLight = SimpleLights.InstanceData[LightIndex];

		for (int32 ViewIndex = 0; ViewIndex < NumViews; ViewIndex++)
		{
			const FSimpleLightPerViewEntry& SimpleLightPerViewData = SimpleLights.GetViewDependentData(LightIndex, ViewIndex, NumViews);
			const FSphere LightBounds(SimpleLightPerViewData.Position, SimpleLight.Radius);

			FViewInfo& View = Views[ViewIndex];

			// Set the device viewport for the view.
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

			TShaderMapRef<TDeferredLightVS<true> > VertexShader(View.ShaderMap);

			SetBoundingGeometryRasterizerAndDepthState(GraphicsPSOInit, View, LightBounds);

			if (SimpleLight.Exponent == 0)
			{
				// inverse squared
				SetShaderTemplLightingSimple<false, true, true>(RHICmdList, GraphicsPSOInit, View, *VertexShader, SimpleLight, SimpleLightPerViewData);
			}
			else
			{
				// light's exponent, not inverse squared
				SetShaderTemplLightingSimple<false, true, false>(RHICmdList, GraphicsPSOInit, View, *VertexShader, SimpleLight, SimpleLightPerViewData);
			}

			VertexShader->SetSimpleLightParameters(RHICmdList, View, LightBounds);

			// Apply the point or spot light with some approximately bounding geometry, 
			// So we can get speedups from depth testing and not processing pixels outside of the light's influence.
			StencilingGeometry::DrawSphere(RHICmdList);
		}
	}
}
