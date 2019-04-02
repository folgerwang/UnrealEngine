// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Reflection Environment - feature that provides HDR glossy reflections on any surfaces, leveraging precomputation to prefilter cubemaps of the scene
=============================================================================*/

#include "ReflectionEnvironment.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "RendererInterface.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "RHIStaticStates.h"
#include "PostProcess/SceneRenderTargets.h"
#include "GlobalShader.h"
#include "SceneRenderTargetParameters.h"
#include "DeferredShadingRenderer.h"
#include "BasePassRendering.h"
#include "ScenePrivate.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessSubsurface.h"
#include "PostProcess/ScreenSpaceReflections.h"
#include "LightRendering.h"
#include "LightPropagationVolumeSettings.h"
#include "PipelineStateCache.h"
#include "DistanceFieldAmbientOcclusion.h"
#include "SceneViewFamilyBlackboard.h"
#include "ScreenSpaceDenoise.h"
#include "RayTracing/RaytracingOptions.h"

DECLARE_GPU_STAT_NAMED(ReflectionEnvironment, TEXT("Reflection Environment"));
DECLARE_GPU_STAT_NAMED(RayTracingReflections, TEXT("Ray Tracing Reflections"));
DECLARE_GPU_STAT(SkyLightDiffuse);

extern TAutoConsoleVariable<int32> CVarLPVMixing;

static TAutoConsoleVariable<int32> CVarReflectionEnvironment(
	TEXT("r.ReflectionEnvironment"),
	1,
	TEXT("Whether to render the reflection environment feature, which implements local reflections through Reflection Capture actors.\n")
	TEXT(" 0: off\n")
	TEXT(" 1: on and blend with scene (default)")
	TEXT(" 2: on and overwrite scene (only in non-shipping builds)"),
	ECVF_RenderThreadSafe | ECVF_Scalability);

int32 GReflectionEnvironmentLightmapMixing = 1;
FAutoConsoleVariableRef CVarReflectionEnvironmentLightmapMixing(
	TEXT("r.ReflectionEnvironmentLightmapMixing"),
	GReflectionEnvironmentLightmapMixing,
	TEXT("Whether to mix indirect specular from reflection captures with indirect diffuse from lightmaps for rough surfaces."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GReflectionEnvironmentLightmapMixBasedOnRoughness = 1;
FAutoConsoleVariableRef CVarReflectionEnvironmentLightmapMixBasedOnRoughness(
	TEXT("r.ReflectionEnvironmentLightmapMixBasedOnRoughness"),
	GReflectionEnvironmentLightmapMixBasedOnRoughness,
	TEXT("Whether to reduce lightmap mixing with reflection captures for very smooth surfaces.  This is useful to make sure reflection captures match SSR / planar reflections in brightness."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GReflectionEnvironmentBeginMixingRoughness = .1f;
FAutoConsoleVariableRef CVarReflectionEnvironmentBeginMixingRoughness(
	TEXT("r.ReflectionEnvironmentBeginMixingRoughness"),
	GReflectionEnvironmentBeginMixingRoughness,
	TEXT("Min roughness value at which to begin mixing reflection captures with lightmap indirect diffuse."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GReflectionEnvironmentEndMixingRoughness = .3f;
FAutoConsoleVariableRef CVarReflectionEnvironmentEndMixingRoughness(
	TEXT("r.ReflectionEnvironmentEndMixingRoughness"),
	GReflectionEnvironmentEndMixingRoughness,
	TEXT("Min roughness value at which to end mixing reflection captures with lightmap indirect diffuse."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GReflectionEnvironmentLightmapMixLargestWeight = 10000;
FAutoConsoleVariableRef CVarReflectionEnvironmentLightmapMixLargestWeight(
	TEXT("r.ReflectionEnvironmentLightmapMixLargestWeight"),
	GReflectionEnvironmentLightmapMixLargestWeight,
	TEXT("When set to 1 can be used to clamp lightmap mixing such that only darkening from lightmaps are applied to reflection captures."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarDoTiledReflections(
	TEXT("r.DoTiledReflections"),
	1,
	TEXT("Compute Reflection Environment with Tiled compute shader..\n")
	TEXT(" 0: off\n")
	TEXT(" 1: on (default)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarSkySpecularOcclusionStrength(
	TEXT("r.SkySpecularOcclusionStrength"),
	1,
	TEXT("Strength of skylight specular occlusion from DFAO (default is 1.0)"),
	ECVF_RenderThreadSafe);

static int32 GRayTracingReflections = -1;
static FAutoConsoleVariableRef CVarReflectionsMethod(
	TEXT("r.RayTracing.Reflections"),
	GRayTracingReflections,
	TEXT("-1: Value driven by postprocess volume (default) \n")
	TEXT("0: use traditional rasterized SSR\n")
	TEXT("1: use ray traced reflections\n")
);

static TAutoConsoleVariable<float> CVarReflectionScreenPercentage(
	TEXT("r.RayTracing.Reflections.ScreenPercentage"),
	100.0f,
	TEXT("Screen percentage the reflections should be ray traced at (default = 100)."),
	ECVF_RenderThreadSafe);

static int32 GRayTracingReflectionsSamplesPerPixel = -1;
static FAutoConsoleVariableRef CVarRayTracingReflectionsSamplesPerPixel(
	TEXT("r.RayTracing.Reflections.SamplesPerPixel"),
	GRayTracingReflectionsSamplesPerPixel,
	TEXT("Sets the samples-per-pixel for reflections (default = -1 (driven by postprocesing volume))"));

static int32 GRayTracingReflectionsHeightFog = 1;
static FAutoConsoleVariableRef CVarRayTracingReflectionsHeightFog(
	TEXT("r.RayTracing.Reflections.HeightFog"),
	GRayTracingReflectionsHeightFog,
	TEXT("Enables height fog in ray traced reflections (default = 1)"));

static TAutoConsoleVariable<int32> CVarUseReflectionDenoiser(
	TEXT("r.Reflections.Denoiser"),
	2,
	TEXT("Choose the denoising algorithm.\n")
	TEXT(" 0: Disabled;\n")
	TEXT(" 1: Forces the default denoiser of the renderer;\n")
	TEXT(" 2: GScreenSpaceDenoiser witch may be overriden by a third party plugin (default)."),
	ECVF_RenderThreadSafe);


// to avoid having direct access from many places
static int GetReflectionEnvironmentCVar()
{
	int32 RetVal = CVarReflectionEnvironment.GetValueOnAnyThread();

#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Disabling the debug part of this CVar when in shipping
	if (RetVal == 2)
	{
		RetVal = 1;
	}
#endif

	return RetVal;
}

FVector GetReflectionEnvironmentRoughnessMixingScaleBiasAndLargestWeight()
{
	float RoughnessMixingRange = 1.0f / FMath::Max(GReflectionEnvironmentEndMixingRoughness - GReflectionEnvironmentBeginMixingRoughness, .001f);

	if (GReflectionEnvironmentLightmapMixing == 0)
	{
		return FVector(0, 0, GReflectionEnvironmentLightmapMixLargestWeight);
	}

	if (GReflectionEnvironmentEndMixingRoughness == 0.0f && GReflectionEnvironmentBeginMixingRoughness == 0.0f)
	{
		// Make sure a Roughness of 0 results in full mixing when disabling roughness-based mixing
		return FVector(0, 1, GReflectionEnvironmentLightmapMixLargestWeight);
	}

	if (!GReflectionEnvironmentLightmapMixBasedOnRoughness)
	{
		return FVector(0, 1, GReflectionEnvironmentLightmapMixLargestWeight);
	}

	return FVector(RoughnessMixingRange, -GReflectionEnvironmentBeginMixingRoughness * RoughnessMixingRange, GReflectionEnvironmentLightmapMixLargestWeight);
}

bool IsReflectionEnvironmentAvailable(ERHIFeatureLevel::Type InFeatureLevel)
{
	return (InFeatureLevel >= ERHIFeatureLevel::SM4) && (GetReflectionEnvironmentCVar() != 0);
}

bool IsReflectionCaptureAvailable()
{
	static IConsoleVariable* AllowStaticLightingVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AllowStaticLighting"));
	return (!AllowStaticLightingVar || AllowStaticLightingVar->GetInt() != 0);
}

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FReflectionUniformParameters, "ReflectionStruct");

void SetupReflectionUniformParameters(const FViewInfo& View, FReflectionUniformParameters& OutParameters)
{
	FTexture* SkyLightTextureResource = GBlackTextureCube;
	FTexture* SkyLightBlendDestinationTextureResource = GBlackTextureCube;
	float ApplySkyLightMask = 0;
	float BlendFraction = 0;
	bool bSkyLightIsDynamic = false;
	float SkyAverageBrightness = 1.0f;

	const bool bApplySkyLight = View.Family->EngineShowFlags.SkyLighting;
	const FScene* Scene = (const FScene*)View.Family->Scene;

	if (Scene
		&& Scene->SkyLight 
		&& Scene->SkyLight->ProcessedTexture
		&& bApplySkyLight)
	{
		const FSkyLightSceneProxy& SkyLight = *Scene->SkyLight;
		SkyLightTextureResource = SkyLight.ProcessedTexture;
		BlendFraction = SkyLight.BlendFraction;

		if (SkyLight.BlendFraction > 0.0f && SkyLight.BlendDestinationProcessedTexture)
		{
			if (SkyLight.BlendFraction < 1.0f)
			{
				SkyLightBlendDestinationTextureResource = SkyLight.BlendDestinationProcessedTexture;
			}
			else
			{
				SkyLightTextureResource = SkyLight.BlendDestinationProcessedTexture;
				BlendFraction = 0;
			}
		}
		
		ApplySkyLightMask = 1;
		bSkyLightIsDynamic = !SkyLight.bHasStaticLighting && !SkyLight.bWantsStaticShadowing;
		SkyAverageBrightness = SkyLight.AverageBrightness;
	}

	const int32 CubemapWidth = SkyLightTextureResource->GetSizeX();
	const float SkyMipCount = FMath::Log2(CubemapWidth) + 1.0f;

	OutParameters.SkyLightCubemap = SkyLightTextureResource->TextureRHI;
	OutParameters.SkyLightCubemapSampler = SkyLightTextureResource->SamplerStateRHI;
	OutParameters.SkyLightBlendDestinationCubemap = SkyLightBlendDestinationTextureResource->TextureRHI;
	OutParameters.SkyLightBlendDestinationCubemapSampler = SkyLightBlendDestinationTextureResource->SamplerStateRHI;
	OutParameters.SkyLightParameters = FVector4(SkyMipCount - 1.0f, ApplySkyLightMask, bSkyLightIsDynamic ? 1.0f : 0.0f, BlendFraction);
	OutParameters.SkyLightCubemapBrightness = SkyAverageBrightness;

	// Note: GBlackCubeArrayTexture has an alpha of 0, which is needed to represent invalid data so the sky cubemap can still be applied
	FTextureRHIParamRef CubeArrayTexture = View.FeatureLevel >= ERHIFeatureLevel::SM5 ? GBlackCubeArrayTexture->TextureRHI : GBlackTextureCube->TextureRHI;

	if (View.Family->EngineShowFlags.ReflectionEnvironment 
		&& View.FeatureLevel >= ERHIFeatureLevel::SM5
		&& Scene
		&& Scene->ReflectionSceneData.CubemapArray.IsValid())
	{
		CubeArrayTexture = Scene->ReflectionSceneData.CubemapArray.GetRenderTarget().ShaderResourceTexture;
	}

	OutParameters.ReflectionCubemap = CubeArrayTexture;
	OutParameters.ReflectionCubemapSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	OutParameters.PreIntegratedGF = GSystemTextures.PreintegratedGF->GetRenderTargetItem().ShaderResourceTexture;
	OutParameters.PreIntegratedGFSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
}

TUniformBufferRef<FReflectionUniformParameters> CreateReflectionUniformBuffer(const class FViewInfo& View, EUniformBufferUsage Usage)
{
	FReflectionUniformParameters ReflectionStruct;
	SetupReflectionUniformParameters(View, ReflectionStruct);
	return CreateUniformBufferImmediate(ReflectionStruct, Usage);
}

void FReflectionEnvironmentCubemapArray::InitDynamicRHI()
{
	if (GetFeatureLevel() >= ERHIFeatureLevel::SM5)
	{
		const int32 NumReflectionCaptureMips = FMath::CeilLogTwo(CubemapSize) + 1;

		ReleaseCubeArray();

		FPooledRenderTargetDesc Desc(
			FPooledRenderTargetDesc::CreateCubemapDesc(
				CubemapSize,
				// Alpha stores sky mask
				PF_FloatRGBA, 
				FClearValueBinding::None,
				TexCreate_None,
				TexCreate_None,
				false, 
				// Cubemap array of 1 produces a regular cubemap, so guarantee it will be allocated as an array
				FMath::Max<uint32>(MaxCubemaps, 2),
				NumReflectionCaptureMips
				)
			);

		Desc.AutoWritable = false;
	
		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

		// Allocate TextureCubeArray for the scene's reflection captures
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, ReflectionEnvs, TEXT("ReflectionEnvs"));
	}
}

void FReflectionEnvironmentCubemapArray::ReleaseCubeArray()
{
	// it's unlikely we can reuse the TextureCubeArray so when we release it we want to really remove it
	GRenderTargetPool.FreeUnusedResource(ReflectionEnvs);
}

void FReflectionEnvironmentCubemapArray::ReleaseDynamicRHI()
{
	ReleaseCubeArray();
}

void FReflectionEnvironmentSceneData::ResizeCubemapArrayGPU(uint32 InMaxCubemaps, int32 InCubemapSize)
{
	check(IsInRenderingThread());

	// If the cubemap array isn't setup yet then no copying/reallocation is necessary. Just go through the old path
	if (!CubemapArray.IsInitialized())
	{
		CubemapArraySlotsUsed.Init(false, InMaxCubemaps);
		CubemapArray.UpdateMaxCubemaps(InMaxCubemaps, InCubemapSize);
		return;
	}

	// Generate a remapping table for the elements
	TArray<int32> IndexRemapping;
	int32 Count = 0;
	for (int i = 0; i < CubemapArray.GetMaxCubemaps(); i++)
	{
		bool bUsed = i < CubemapArraySlotsUsed.Num() ? CubemapArraySlotsUsed[i] : false;
		if (bUsed)
		{
			IndexRemapping.Add(Count);
			Count++;
		}
		else
		{
			IndexRemapping.Add(-1);
		}
	}

	// Reset the CubemapArraySlotsUsed array (we'll recompute it below)
	CubemapArraySlotsUsed.Init(false, InMaxCubemaps);

	// Spin through the AllocatedReflectionCaptureState map and remap the indices based on the LUT
	TArray<const UReflectionCaptureComponent*> Components;
	AllocatedReflectionCaptureState.GetKeys(Components);
	int32 UsedCubemapCount = 0;
	for (int32 i=0; i<Components.Num(); i++ )
	{
		FCaptureComponentSceneState* ComponentStatePtr = AllocatedReflectionCaptureState.Find(Components[i]);
		check(ComponentStatePtr->CubemapIndex < IndexRemapping.Num());
		int32 NewIndex = IndexRemapping[ComponentStatePtr->CubemapIndex];
		CubemapArraySlotsUsed[NewIndex] = true; 
		ComponentStatePtr->CubemapIndex = NewIndex;
		check(ComponentStatePtr->CubemapIndex > -1);
		UsedCubemapCount = FMath::Max(UsedCubemapCount, ComponentStatePtr->CubemapIndex + 1);
	}

	// Clear elements in the remapping array which are outside the range of the used components (these were allocated but not used)
	for (int i = 0; i < IndexRemapping.Num(); i++)
	{
		if (IndexRemapping[i] >= UsedCubemapCount)
		{
			IndexRemapping[i] = -1;
		}
	}

	CubemapArray.ResizeCubemapArrayGPU(InMaxCubemaps, InCubemapSize, IndexRemapping);
}

void FReflectionEnvironmentCubemapArray::ResizeCubemapArrayGPU(uint32 InMaxCubemaps, int32 InCubemapSize, const TArray<int32>& IndexRemapping)
{
	check(IsInRenderingThread());
	check(GetFeatureLevel() >= ERHIFeatureLevel::SM5);
	check(IsInitialized());
	check(InCubemapSize == CubemapSize);

	// Take a reference to the old cubemap array and then release it to prevent it getting destroyed during InitDynamicRHI
	TRefCountPtr<IPooledRenderTarget> OldReflectionEnvs = ReflectionEnvs;
	ReflectionEnvs = nullptr;
	int OldMaxCubemaps = MaxCubemaps;
	MaxCubemaps = InMaxCubemaps;

	InitDynamicRHI();

	FTextureRHIRef TexRef = OldReflectionEnvs->GetRenderTargetItem().TargetableTexture;
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	const int32 NumMips = FMath::CeilLogTwo(InCubemapSize) + 1;

	{
		SCOPED_DRAW_EVENT(RHICmdList, ReflectionEnvironment_ResizeCubemapArray);
		SCOPED_GPU_STAT(RHICmdList, ReflectionEnvironment)

		// Copy the cubemaps, remapping the elements as necessary
		FResolveParams ResolveParams;
		ResolveParams.Rect = FResolveRect();
		for (int32 SourceCubemapIndex = 0; SourceCubemapIndex < OldMaxCubemaps; SourceCubemapIndex++)
		{
			int32 DestCubemapIndex = IndexRemapping[SourceCubemapIndex];
			if (DestCubemapIndex != -1)
			{
				ResolveParams.SourceArrayIndex = SourceCubemapIndex;
				ResolveParams.DestArrayIndex = DestCubemapIndex;

				check(SourceCubemapIndex < OldMaxCubemaps);
				check(DestCubemapIndex < (int32)MaxCubemaps);

				for (int Face = 0; Face < 6; Face++)
				{
					ResolveParams.CubeFace = (ECubeFace)Face;
					for (int Mip = 0; Mip < NumMips; Mip++)
					{
						ResolveParams.MipIndex = Mip;
						//@TODO: We should use an explicit copy method for this rather than CopyToResolveTarget, but that doesn't exist right now. 
						// For now, we'll just do this on RHIs where we know CopyToResolveTarget does the right thing. In future we should look to 
						// add a a new RHI method
						check(GRHISupportsResolveCubemapFaces);
						RHICmdList.CopyToResolveTarget(OldReflectionEnvs->GetRenderTargetItem().ShaderResourceTexture, ReflectionEnvs->GetRenderTargetItem().ShaderResourceTexture, ResolveParams);
					}
				}
			}
		}
	}
	GRenderTargetPool.FreeUnusedResource(OldReflectionEnvs);
}

void FReflectionEnvironmentCubemapArray::UpdateMaxCubemaps(uint32 InMaxCubemaps, int32 InCubemapSize)
{
	MaxCubemaps = InMaxCubemaps;
	CubemapSize = InCubemapSize;

	// Reallocate the cubemap array
	if (IsInitialized())
	{
		UpdateRHI();
	}
	else
	{
		InitResource();
	}
}

class FSkyLightParameters
{
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		ApplyBentNormalAO.Bind(ParameterMap, TEXT("ApplyBentNormalAO"));
		InvSkySpecularOcclusionStrength.Bind(ParameterMap, TEXT("InvSkySpecularOcclusionStrength"));
		OcclusionTintAndMinOcclusion.Bind(ParameterMap, TEXT("OcclusionTintAndMinOcclusion"));
		ContrastAndNormalizeMulAdd.Bind(ParameterMap, TEXT("ContrastAndNormalizeMulAdd"));
		OcclusionExponent.Bind(ParameterMap, TEXT("OcclusionExponent"));
		OcclusionCombineMode.Bind(ParameterMap, TEXT("OcclusionCombineMode"));
	}

	template<typename ShaderRHIParamRef, typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const ShaderRHIParamRef& ShaderRHI, bool bApplyBentNormalAO, float SkySpecularOcclusionStrength, const FSkyLightSceneProxy* SkyLight)
	{
		float SkyLightContrast = 0.01f;
		float SkyLightOcclusionExponent = 1.0f;
		FVector4 SkyLightOcclusionTintAndMinOcclusion(0.0f, 0.0f, 0.0f, 0.0f);
		EOcclusionCombineMode SkyLightOcclusionCombineMode = EOcclusionCombineMode::OCM_MAX;
		if (SkyLight)
		{
			FDistanceFieldAOParameters Parameters(SkyLight->OcclusionMaxDistance, SkyLight->Contrast);
			SkyLightContrast = Parameters.Contrast;
			SkyLightOcclusionExponent = SkyLight->OcclusionExponent;
			SkyLightOcclusionTintAndMinOcclusion = FVector4(SkyLight->OcclusionTint);
			SkyLightOcclusionTintAndMinOcclusion.W = SkyLight->MinOcclusion;
			SkyLightOcclusionCombineMode = SkyLight->OcclusionCombineMode;
		}

		// Scale and bias to remap the contrast curve to [0,1]
		const float Min = 1 / (1 + FMath::Exp(-SkyLightContrast * (0 * 10 - 5)));
		const float Max = 1 / (1 + FMath::Exp(-SkyLightContrast * (1 * 10 - 5)));
		const float Mul = 1.0f / (Max - Min);
		const float Add = -Min / (Max - Min);

		SetShaderValue(RHICmdList, ShaderRHI, ContrastAndNormalizeMulAdd, FVector(SkyLightContrast, Mul, Add));

		SetShaderValue(RHICmdList, ShaderRHI, OcclusionExponent, SkyLightOcclusionExponent);
		SetShaderValue(RHICmdList, ShaderRHI, OcclusionTintAndMinOcclusion, SkyLightOcclusionTintAndMinOcclusion);
		SetShaderValue(RHICmdList, ShaderRHI, OcclusionCombineMode, SkyLightOcclusionCombineMode == OCM_Minimum ? 0.0f : 1.0f);
		SetShaderValue(RHICmdList, ShaderRHI, ApplyBentNormalAO, bApplyBentNormalAO ? 1.0f : 0.0f);
		SetShaderValue(RHICmdList, ShaderRHI, InvSkySpecularOcclusionStrength, 1.0f / FMath::Max(SkySpecularOcclusionStrength, .1f));
	}

	friend FArchive& operator<<(FArchive& Ar, FSkyLightParameters& P)
	{
		Ar << P.ApplyBentNormalAO;
		Ar << P.InvSkySpecularOcclusionStrength;
		Ar << P.OcclusionTintAndMinOcclusion;
		Ar << P.ContrastAndNormalizeMulAdd;
		Ar << P.OcclusionExponent;
		Ar << P.OcclusionCombineMode;

		return Ar;
	}

private:
	FShaderParameter ApplyBentNormalAO;
	FShaderParameter InvSkySpecularOcclusionStrength;
	FShaderParameter OcclusionTintAndMinOcclusion;
	FShaderParameter ContrastAndNormalizeMulAdd;
	FShaderParameter OcclusionExponent;
	FShaderParameter OcclusionCombineMode;
};

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FReflectionCaptureShaderData, "ReflectionCapture");

/** Pixel shader that does tiled deferred culling of reflection captures, then sorts and composites them. */
class FReflectionEnvironmentSkyLightingPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FReflectionEnvironmentSkyLightingPS);

	class FHasBoxCaptures			: SHADER_PERMUTATION_BOOL("REFLECTION_COMPOSITE_HAS_BOX_CAPTURES");
	class FHasSphereCaptures		: SHADER_PERMUTATION_BOOL("REFLECTION_COMPOSITE_HAS_SPHERE_CAPTURES");
	class FDFAOIndirectOcclusion	: SHADER_PERMUTATION_BOOL("SUPPORT_DFAO_INDIRECT_OCCLUSION");
	class FSpecularBounce			: SHADER_PERMUTATION_BOOL("SPECULAR_BOUNCE");
	class FSkyLight					: SHADER_PERMUTATION_BOOL("ENABLE_SKY_LIGHT");
	class FDynamicSkyLight			: SHADER_PERMUTATION_BOOL("ENABLE_DYNAMIC_SKY_LIGHT");
	class FSkyShadowing				: SHADER_PERMUTATION_BOOL("APPLY_SKY_SHADOWING");
	class FRayTracedReflections		: SHADER_PERMUTATION_BOOL("RAY_TRACED_REFLECTIONS");

	using FPermutationDomain = TShaderPermutationDomain<
		FHasBoxCaptures,
		FHasSphereCaptures,
		FDFAOIndirectOcclusion,
		FSpecularBounce,
		FSkyLight,
		FDynamicSkyLight,
		FSkyShadowing,
		FRayTracedReflections>;

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		// Environment captures have simple specular bounce without reflection captures.
		if (PermutationVector.Get<FSpecularBounce>())
		{
			PermutationVector.Set<FSkyLight>(false);
			PermutationVector.Set<FDFAOIndirectOcclusion>(false);
			PermutationVector.Set<FHasBoxCaptures>(false);
			PermutationVector.Set<FHasSphereCaptures>(false);
		}

		// FSkyLightingDynamicSkyLight requires FSkyLightingSkyLight.
		if (!PermutationVector.Get<FSkyLight>())
		{
			PermutationVector.Set<FDynamicSkyLight>(false);
		}

		// FSkyLightingSkyShadowing requires FSkyLightingDynamicSkyLight.
		if (!PermutationVector.Get<FDynamicSkyLight>())
		{
			PermutationVector.Set<FSkyShadowing>(false);
		}

		return PermutationVector;
	}

	static FPermutationDomain BuildPermutationVector(const FViewInfo& View, bool bBoxCapturesOnly, bool bSphereCapturesOnly, bool bSupportDFAOIndirectOcclusion, bool bSpecularBounce, bool bEnableSkyLight, bool bEnableDynamicSkyLight, bool bApplySkyShadowing, bool bRayTracedReflections)
	{
		FPermutationDomain PermutationVector;

		PermutationVector.Set<FHasBoxCaptures>(bBoxCapturesOnly);
		PermutationVector.Set<FHasSphereCaptures>(bSphereCapturesOnly);
		PermutationVector.Set<FDFAOIndirectOcclusion>(bSupportDFAOIndirectOcclusion);
		PermutationVector.Set<FSpecularBounce>(bSpecularBounce);
		PermutationVector.Set<FSkyLight>(bEnableSkyLight);
		PermutationVector.Set<FDynamicSkyLight>(bEnableDynamicSkyLight);
		PermutationVector.Set<FSkyShadowing>(bApplySkyShadowing);
		PermutationVector.Set<FRayTracedReflections>(bRayTracedReflections);

		return RemapPermutation(PermutationVector);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM4))
		{
			return false;
		}

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		return PermutationVector == RemapPermutation(PermutationVector);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("MAX_CAPTURES"), GMaxNumReflectionCaptures);
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
		FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
	}


public:

	FReflectionEnvironmentSkyLightingPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer);
		ReflectionCubemap.Bind(Initializer.ParameterMap,TEXT("ReflectionCubemap"));
		ReflectionCubemapSampler.Bind(Initializer.ParameterMap,TEXT("ReflectionCubemapSampler"));
		ScreenSpaceReflectionsTexture.Bind(Initializer.ParameterMap, TEXT("ScreenSpaceReflectionsTexture"));
		ScreenSpaceReflectionsSampler.Bind(Initializer.ParameterMap, TEXT("ScreenSpaceReflectionsSampler"));
		PreIntegratedGF.Bind(Initializer.ParameterMap, TEXT("PreIntegratedGF"));
		PreIntegratedGFSampler.Bind(Initializer.ParameterMap, TEXT("PreIntegratedGFSampler"));
		DFAOUpsampleParameters.Bind(Initializer.ParameterMap);
		SkyLightParameters.Bind(Initializer.ParameterMap);
	}

	FReflectionEnvironmentSkyLightingPS()
	{
	}

	template<typename TRHICommandList>
	void SetParameters(
		TRHICommandList& RHICmdList,
		const FViewInfo& View,
		FTextureRHIParamRef SSRTexture,
		const TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO
	)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		SceneTextureParameters.Set(RHICmdList, ShaderRHI, View.FeatureLevel, ESceneTextureSetupMode::All);

		FScene* Scene = (FScene*)View.Family->Scene;

		FTextureRHIParamRef CubemapArray;
		if (Scene->ReflectionSceneData.CubemapArray.IsValid() &&
			Scene->ReflectionSceneData.CubemapArray.GetRenderTarget().IsValid())
		{
			CubemapArray = Scene->ReflectionSceneData.CubemapArray.GetRenderTarget().ShaderResourceTexture;
		}
		else
		{
			CubemapArray = GBlackCubeArrayTexture->TextureRHI;
		}

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			ReflectionCubemap,
			ReflectionCubemapSampler,
			TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(),
			CubemapArray);

		SetTextureParameter(RHICmdList, ShaderRHI, ScreenSpaceReflectionsTexture, ScreenSpaceReflectionsSampler, TStaticSamplerState<SF_Point>::GetRHI(), SSRTexture);

		SetUniformBufferParameter(RHICmdList, ShaderRHI, GetUniformBufferParameter<FReflectionCaptureShaderData>(), View.ReflectionCaptureUniformBuffer);

		SetTextureParameter(RHICmdList, ShaderRHI, PreIntegratedGF, PreIntegratedGFSampler, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), GSystemTextures.PreintegratedGF->GetRenderTargetItem().ShaderResourceTexture);
	
		FReflectionUniformParameters ReflectionUniformParameters;
		SetupReflectionUniformParameters(View, ReflectionUniformParameters);
		SetUniformBufferParameterImmediate(RHICmdList, ShaderRHI, GetUniformBufferParameter<FReflectionUniformParameters>(), ReflectionUniformParameters);

		bool const bApplyBentNormalAO = DynamicBentNormalAO ? true : false;
		SkyLightParameters.SetParameters(RHICmdList, ShaderRHI, bApplyBentNormalAO, CVarSkySpecularOcclusionStrength.GetValueOnRenderThread(), Scene->SkyLight);

		SetUniformBufferParameter(RHICmdList, ShaderRHI, GetUniformBufferParameter<FForwardLightData>(), View.ForwardLightingResources->ForwardLightDataUniformBuffer);
		DFAOUpsampleParameters.Set(RHICmdList, ShaderRHI, View, DynamicBentNormalAO);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SceneTextureParameters;
		Ar << ReflectionCubemap;
		Ar << ReflectionCubemapSampler;
		Ar << ScreenSpaceReflectionsTexture;
		Ar << ScreenSpaceReflectionsSampler;
		Ar << PreIntegratedGF;
		Ar << PreIntegratedGFSampler;
		Ar << DFAOUpsampleParameters;
		Ar << SkyLightParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FSceneTextureShaderParameters SceneTextureParameters;
	FShaderResourceParameter ReflectionCubemap;
	FShaderResourceParameter ReflectionCubemapSampler;
	FShaderResourceParameter ScreenSpaceReflectionsTexture;
	FShaderResourceParameter ScreenSpaceReflectionsSampler;
	FShaderResourceParameter PreIntegratedGF;
	FShaderResourceParameter PreIntegratedGFSampler;
	FDFAOUpsampleParameters DFAOUpsampleParameters;
	FSkyLightParameters SkyLightParameters;
};

IMPLEMENT_GLOBAL_SHADER(FReflectionEnvironmentSkyLightingPS, "/Engine/Private/ReflectionEnvironmentPixelShader.usf", "ReflectionEnvironmentSkyLighting", SF_Pixel);

bool FDeferredShadingSceneRenderer::ShouldDoReflectionEnvironment() const
{
	const ERHIFeatureLevel::Type SceneFeatureLevel = Scene->GetFeatureLevel();

	return IsReflectionEnvironmentAvailable(SceneFeatureLevel)
		&& Scene->ReflectionSceneData.RegisteredReflectionCaptures.Num()
		&& ViewFamily.EngineShowFlags.ReflectionEnvironment;
}

void FDeferredShadingSceneRenderer::RenderDeferredReflectionsAndSkyLighting(FRHICommandListImmediate& RHICmdList, TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO, TRefCountPtr<IPooledRenderTarget>& VelocityRT)
{
	check(RHICmdList.IsOutsideRenderPass());

	if (ViewFamily.EngineShowFlags.VisualizeLightCulling || !ViewFamily.EngineShowFlags.Lighting)
	{
		return;
	}

	// If we're currently capturing a reflection capture, output SpecularColor * IndirectIrradiance for metals so they are not black in reflections,
	// Since we don't have multiple bounce specular reflections
	bool bReflectionCapture = false;
	bool bAnyViewWithRaytracingReflections = false;
	for (int32 ViewIndex = 0, Num = Views.Num(); ViewIndex < Num; ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];
		bReflectionCapture = bReflectionCapture || View.bIsReflectionCapture;
		//#dxr_todo: multiview case
		bAnyViewWithRaytracingReflections = bAnyViewWithRaytracingReflections || (View.FinalPostProcessSettings.ReflectionsType == EReflectionsType::RayTracing);
	}

	const bool bRayTracedReflections = IsRayTracingEnabled() && (GRayTracingReflections < 0 ? bAnyViewWithRaytracingReflections : GRayTracingReflections);

	// The specular sky light contribution is also needed by RT Reflections as a fallback.
	const bool bSkyLight = Scene->SkyLight
		&& Scene->SkyLight->ProcessedTexture
		&& !Scene->SkyLight->bHasStaticLighting;

	bool bDynamicSkyLight = ShouldRenderDeferredDynamicSkyLight(Scene, ViewFamily);
	bool bApplySkyShadowing = false;
	if (bDynamicSkyLight)
	{
		SCOPED_DRAW_EVENT(RHICmdList, SkyLightDiffuse);
		SCOPED_GPU_STAT(RHICmdList, SkyLightDiffuse);

		FDistanceFieldAOParameters Parameters(Scene->SkyLight->OcclusionMaxDistance, Scene->SkyLight->Contrast);

		extern int32 GDistanceFieldAOApplyToStaticIndirect;
		if (Scene->SkyLight->bCastShadows
			&& !GDistanceFieldAOApplyToStaticIndirect
			&& ShouldRenderDistanceFieldAO()
			&& ViewFamily.EngineShowFlags.AmbientOcclusion)
		{
			bApplySkyShadowing = RenderDistanceFieldLighting(RHICmdList, Parameters, VelocityRT, DynamicBentNormalAO, false, false);
		}
	}

	check(RHICmdList.IsOutsideRenderPass());

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	const bool bReflectionEnv = ShouldDoReflectionEnvironment();

	for (int32 ViewIndex = 0, Num = Views.Num(); ViewIndex < Num; ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		const bool bScreenSpaceReflections = !bRayTracedReflections && ShouldRenderScreenSpaceReflections(Views[ViewIndex]);

		TRefCountPtr<IPooledRenderTarget> ReflectionsColor = GSystemTextures.BlackDummy;
		if (bRayTracedReflections)
		{
#if RHI_RAYTRACING
			SCOPED_DRAW_EVENT(RHICmdList, RayTracingReflections);
			SCOPED_GPU_STAT(RHICmdList, RayTracingReflections);

			FRDGBuilder GraphBuilder(RHICmdList); // TODO: convert the entire reflections to render graph.

			FSceneViewFamilyBlackboard SceneBlackboard;
			SetupSceneViewFamilyBlackboard(GraphBuilder, &SceneBlackboard);


			IScreenSpaceDenoiser::FReflectionsRayTracingConfig RayTracingConfig;
			RayTracingConfig.ResolutionFraction = FMath::Clamp(CVarReflectionScreenPercentage.GetValueOnRenderThread() / 100.0f, 0.25f, 1.0f);

			int32 RayTracingReflectionsSPP = GRayTracingReflectionsSamplesPerPixel > -1 ? GRayTracingReflectionsSamplesPerPixel : View.FinalPostProcessSettings.RayTracingReflectionsSamplesPerPixel;
			int32 DenoiserMode = CVarUseReflectionDenoiser.GetValueOnRenderThread();
			const bool bDenoise = DenoiserMode != 0 && RayTracingReflectionsSPP == 1;

			if (!bDenoise)
			{
				RayTracingConfig.ResolutionFraction = 1.0f;
			}

			// Ray trace the reflection.
			IScreenSpaceDenoiser::FReflectionsInputs DenoiserInputs;
			RenderRayTracingReflections(
				GraphBuilder,
				View, &DenoiserInputs.Color, &DenoiserInputs.RayHitDistance, &DenoiserInputs.RayImaginaryDepth,
				RayTracingReflectionsSPP, GRayTracingReflectionsHeightFog, RayTracingConfig.ResolutionFraction);


			// Denoise the reflections.
			if (bDenoise)
			{
				const IScreenSpaceDenoiser* DefaultDenoiser = IScreenSpaceDenoiser::GetDefaultDenoiser();
				const IScreenSpaceDenoiser* DenoiserToUse = DenoiserMode == 1 ? DefaultDenoiser : GScreenSpaceDenoiser;

				// Standard event scope for denoiser to have all profiling information not matter what, and with explicit detection of third party.
				RDG_EVENT_SCOPE(GraphBuilder, "%s%s(Reflections) %dx%d",
					DenoiserToUse != DefaultDenoiser ? TEXT("ThirdParty ") : TEXT(""),
					DenoiserToUse->GetDebugName(),
					View.ViewRect.Width(), View.ViewRect.Height());

				IScreenSpaceDenoiser::FReflectionsOutputs DenoiserOutputs = DenoiserToUse->DenoiseReflections(
					GraphBuilder,
					View,
					&View.PrevViewInfo,
					SceneBlackboard,
					DenoiserInputs,
					RayTracingConfig);

				GraphBuilder.QueueTextureExtraction(DenoiserOutputs.Color, &ReflectionsColor);
			}
			else
			{
				// The performance of ray tracing does not allow to run without a denoiser in real time.
				// Multiple rays per pixel is unsupported by the denoiser that will most likely more bound by to
				// many rays than exporting the hit distance buffer. Therefore no permutation of the ray generation
				// shader has been judged required to be supported.
				GraphBuilder.RemoveUnusedTextureWarning(DenoiserInputs.RayHitDistance);

				GraphBuilder.QueueTextureExtraction(DenoiserInputs.Color, &ReflectionsColor);
			}

			GraphBuilder.Execute();
#endif
		}
		else if (bScreenSpaceReflections)
		{
			RenderScreenSpaceReflections(RHICmdList, View, ReflectionsColor, VelocityRT);
		}

		bool bPlanarReflections = false;
		if (!bRayTracedReflections)
		{
			bPlanarReflections = RenderDeferredPlanarReflections(RHICmdList, View, false, ReflectionsColor);
		}

		bool bRequiresApply = bSkyLight || bDynamicSkyLight || bReflectionEnv || bScreenSpaceReflections || bPlanarReflections || bRayTracedReflections;

		if (bRequiresApply)
		{
			SCOPED_GPU_STAT(RHICmdList, ReflectionEnvironment);
			SCOPED_DRAW_EVENTF(RHICmdList, ReflectionEnvironment, TEXT("ReflectionEnvironmentAndSky"));

			// Render the reflection environment with tiled deferred culling
			bool bHasBoxCaptures = (View.NumBoxReflectionCaptures > 0);
			bool bHasSphereCaptures = (View.NumSphereReflectionCaptures > 0);

			TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);

			auto PermutationVector = FReflectionEnvironmentSkyLightingPS::BuildPermutationVector(View, bHasBoxCaptures, bHasSphereCaptures, DynamicBentNormalAO != NULL, bReflectionCapture, bSkyLight, bDynamicSkyLight, bApplySkyShadowing, bRayTracedReflections);

			TShaderMapRef<FReflectionEnvironmentSkyLightingPS> PixelShader(View.ShaderMap, PermutationVector);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;

			SceneContext.BeginRenderingSceneColor(
				RHICmdList,
				bReflectionCapture ? ESimpleRenderTargetMode::EUninitializedColorExistingDepth : ESimpleRenderTargetMode::EExistingColorAndDepth,
				FExclusiveDepthStencil::DepthRead_StencilWrite);
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			extern int32 GAOOverwriteSceneColor;
			if (bReflectionCapture)
			{
				GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI();
			}
			else if (GetReflectionEnvironmentCVar() == 2 || GAOOverwriteSceneColor)
			{
				// override scene color for debugging
				GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			}
			else
			{
				const bool bCheckerboardSubsurfaceRendering = FRCPassPostProcessSubsurface::RequiresCheckerboardSubsurfaceRendering(SceneContext.GetSceneColorFormat());
				if (bCheckerboardSubsurfaceRendering)
				{
					GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI();
				}
				else
				{
					GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();
				}
			}

			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

			PixelShader->SetParameters(RHICmdList, View, ReflectionsColor->GetRenderTargetItem().ShaderResourceTexture, DynamicBentNormalAO);

			if (bReflectionCapture)
			{
				DrawRectangle(
					RHICmdList,
					0, 0,
					View.ViewRect.Width(), View.ViewRect.Height(),
					0, 0,
					View.ViewRect.Width(), View.ViewRect.Height(),
					FIntPoint(View.ViewRect.Width(), View.ViewRect.Height()),
					SceneContext.GetBufferSizeXY(),
					*VertexShader,
					EDRF_UseTriangleOptimization);
			}
			else
			{
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

			SceneContext.FinishRenderingSceneColor(RHICmdList);

			ResolveSceneColor(RHICmdList);
		}
	}
}
