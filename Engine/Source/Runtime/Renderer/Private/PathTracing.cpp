// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RHI.h"

#if RHI_RAYTRACING

#include "RendererPrivate.h"
#include "GlobalShader.h"
#include "DeferredShadingRenderer.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PathTracingUniformBuffers.h"
#include "RHI/Public/PipelineStateCache.h"
#include "RayTracing/RayTracingSkyLight.h"
#include "RayTracing/RaytracingOptions.h"

static int32 GPathTracingMaxBounces = -1;
static FAutoConsoleVariableRef CVarPathTracingMaxBounces(
	TEXT("r.PathTracing.MaxBounces"),
	GPathTracingMaxBounces,
	TEXT("Sets the maximum number of path tracing bounces (default = -1 (driven by postprocesing volume))")
);

TAutoConsoleVariable<int32> CVarPathTracingSamplesPerPixel(
	TEXT("r.PathTracing.SamplesPerPixel"),
	-1,
	TEXT("Defines the samples per pixel before resetting the simulation (default = -1 (driven by postprocesing volume))"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingRandomSequence(
	TEXT("r.PathTracing.RandomSequence"),
	2,
	TEXT("Changes the underlying random sequence\n")
	TEXT("0: LCG (default\n")
	TEXT("1: Halton\n")
	TEXT("2: Scrambled Halton\n"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingAdaptiveSampling(
	TEXT("r.PathTracing.AdaptiveSampling"),
	1,
	TEXT("Toggles the use of adaptive sampling\n")
	TEXT("0: off\n")
	TEXT("1: on (default)\n"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingAdaptiveSamplingMinimumSamplesPerPixel(
	TEXT("r.PathTracing.AdaptiveSampling.MinimumSamplesPerPixel"),
	16,
	TEXT("Changes the minimum samples-per-pixel before applying adaptive sampling (default=16)\n"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingVarianceMapRebuildFrequency(
	TEXT("r.PathTracing.VarianceMapRebuildFrequency"),
	16,
	TEXT("Sets the variance map rebuild frequency (default = every 16 iterations)"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingRayCountFrequency(
	TEXT("r.PathTracing.RayCountFrequency"),
	128,
	TEXT("Sets the ray count computation frequency (default = every 128 iterations)"),
	ECVF_RenderThreadSafe
);

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FPathTracingData, "PathTracingData");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FPathTracingLightData, "SceneLightsData");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FPathTracingAdaptiveSamplingData, "AdaptiveSamplingData");

class FPathTracingRG : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPathTracingRG, Global);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	FPathTracingRG() {}
	virtual ~FPathTracingRG() {}

	FPathTracingRG(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		TLASParameter.Bind(Initializer.ParameterMap, TEXT("TLAS"));
		ViewParameter.Bind(Initializer.ParameterMap, TEXT("View"));
		SceneLightsParameters.Bind(Initializer.ParameterMap, TEXT("SceneLightsData"));
		PathTracingParameters.Bind(Initializer.ParameterMap, TEXT("PathTracingData"));
		SkyLightParameters.Bind(Initializer.ParameterMap, TEXT("SkyLight"));
		check(SkyLightParameters.IsBound());
		AdaptiveSamplingParameters.Bind(Initializer.ParameterMap, TEXT("AdaptiveSamplingData"));

		// Output
		RadianceRT.Bind(Initializer.ParameterMap, TEXT("RadianceRT"));
		SampleCountRT.Bind(Initializer.ParameterMap, TEXT("SampleCountRT"));
		PixelPositionRT.Bind(Initializer.ParameterMap, TEXT("PixelPositionRT"));
		RayCountPerPixelRT.Bind(Initializer.ParameterMap, TEXT("RayCountPerPixelRT"));
	}

	void SetParameters(
		FScene* Scene,
		const FViewInfo& View,
		FRayTracingShaderBindingsWriter& GlobalResources,
		const FRayTracingScene& RayTracingScene,
		FUniformBufferRHIParamRef ViewUniformBuffer,
		FUniformBufferRHIParamRef SceneTexturesUniformBuffer,
		// Light buffer
		const TSparseArray<FLightSceneInfoCompact>& Lights,
		// Adaptive sampling
		uint32 Iteration,
		FIntVector VarianceDimensions,
		const FRWBuffer& VarianceMipTree,
		// Output
		FUnorderedAccessViewRHIParamRef RadianceUAV,
		FUnorderedAccessViewRHIParamRef SampleCountUAV,
		FUnorderedAccessViewRHIParamRef PixelPositionUAV,
		FUnorderedAccessViewRHIParamRef RayCountPerPixelUAV)
	{

		GlobalResources.Set(TLASParameter, RayTracingScene.RayTracingSceneRHI->GetShaderResourceView());
		GlobalResources.Set(ViewParameter, ViewUniformBuffer);

		// Path tracing data
		{
			FPathTracingData PathTracingData;

			int32 PathTracingMaxBounces = GPathTracingMaxBounces > -1 ? GPathTracingMaxBounces : View.FinalPostProcessSettings.PathTracingMaxBounces;
			PathTracingData.MaxBounces = PathTracingMaxBounces;
			static uint32 PrevMaxBounces = PathTracingMaxBounces;
			if (PathTracingData.MaxBounces != PrevMaxBounces)
			{
				Scene->bPathTracingNeedsInvalidation = true;
				PrevMaxBounces = PathTracingData.MaxBounces;
			}

			FUniformBufferRHIRef PathTracingDataUniformBuffer = RHICreateUniformBuffer(&PathTracingData, FPathTracingData::StaticStructMetadata.GetLayout(), EUniformBufferUsage::UniformBuffer_SingleDraw);
			GlobalResources.Set(PathTracingParameters, PathTracingDataUniformBuffer);
		}

		// Sky light
		FSkyLightData SkyLightData;
		{
			SetupSkyLightParameters(*Scene, &SkyLightData);

			FUniformBufferRHIRef SkyLightUniformBuffer = RHICreateUniformBuffer(&SkyLightData, FSkyLightData::StaticStructMetadata.GetLayout(), EUniformBufferUsage::UniformBuffer_SingleDraw);
			GlobalResources.Set(SkyLightParameters, SkyLightUniformBuffer);
		}

		// Lights
		{
			FPathTracingLightData LightData;
			LightData.Count = 0;

			// Prepend SkyLight to light buffer
			// WARNING: Until ray payload encodes Light data buffer, the execution depends on this ordering!
			uint32 SkyLightIndex = 0;
			LightData.Type[SkyLightIndex] = 0;
			LightData.Color[SkyLightIndex] = FVector(SkyLightData.Color);
			LightData.Count++;

			for (auto Light : Lights)
			{
				if (LightData.Count >= GLightCountMaximum) break;

				if (Light.LightSceneInfo->Proxy->HasStaticLighting() && Light.LightSceneInfo->IsPrecomputedLightingValid()) continue;

				FLightShaderParameters LightParameters;
				Light.LightSceneInfo->Proxy->GetLightShaderParameters(LightParameters);

				ELightComponentType LightComponentType = (ELightComponentType)Light.LightSceneInfo->Proxy->GetLightType();
				switch (LightComponentType)
				{
					// TODO: LightType_Spot
					case LightType_Directional:
					{
						LightData.Type[LightData.Count] = 2;
						LightData.Normal[LightData.Count] = LightParameters.Direction;
						LightData.Color[LightData.Count] = LightParameters.Color;
						LightData.Attenuation[LightData.Count] = 1.0 / LightParameters.InvRadius;
						break;
					}
					case LightType_Rect:
					{
						LightData.Type[LightData.Count] = 3;
						LightData.Position[LightData.Count] = LightParameters.Position;
						LightData.Normal[LightData.Count] = -LightParameters.Direction;
						LightData.dPdu[LightData.Count] = FVector::CrossProduct(LightParameters.Tangent, LightParameters.Direction);
						LightData.dPdv[LightData.Count] = LightParameters.Tangent;
						// #dxr_todo: define these differences from Lit..
						LightData.Color[LightData.Count] = LightParameters.Color / 4.0;
						LightData.Dimensions[LightData.Count] = FVector(2.0f * LightParameters.SourceRadius, 2.0f * LightParameters.SourceLength, 0.0f);
						LightData.Attenuation[LightData.Count] = 1.0 / LightParameters.InvRadius;
						LightData.RectLightBarnCosAngle[LightData.Count] = LightParameters.RectLightBarnCosAngle;
						LightData.RectLightBarnLength[LightData.Count] = LightParameters.RectLightBarnLength;
						break;
					}
					case LightType_Spot:
					{
						LightData.Type[LightData.Count] = 4;
						LightData.Position[LightData.Count] = LightParameters.Position;
						LightData.Normal[LightData.Count] = -LightParameters.Direction;
						// #dxr_todo: define these differences from Lit..
						LightData.Color[LightData.Count] = 4.0 * PI * LightParameters.Color;
						LightData.Dimensions[LightData.Count] = FVector(LightParameters.SpotAngles, LightParameters.SourceRadius);
						LightData.Attenuation[LightData.Count] = 1.0 / LightParameters.InvRadius;
						break;
					}
					case LightType_Point:
					default:
					{
						LightData.Type[LightData.Count] = 1;
						LightData.Position[LightData.Count] = LightParameters.Position;
						// #dxr_todo: define these differences from Lit..
						LightData.Color[LightData.Count] = LightParameters.Color / (4.0 * PI);
						LightData.Dimensions[LightData.Count] = FVector(0.0, 0.0, LightParameters.SourceRadius);
						LightData.Attenuation[LightData.Count] = 1.0 / LightParameters.InvRadius;
						break;
					}
				};

				LightData.Count++;
			}

			FUniformBufferRHIRef SceneLightsUniformBuffer = RHICreateUniformBuffer(&LightData, FPathTracingLightData::StaticStructMetadata.GetLayout(), EUniformBufferUsage::UniformBuffer_SingleDraw);
			GlobalResources.Set(SceneLightsParameters, SceneLightsUniformBuffer);
		}

		// Adaptive sampling
		{
			FPathTracingAdaptiveSamplingData AdaptiveSamplingData;
			AdaptiveSamplingData.MaxNormalBias = GetRaytracingMaxNormalBias();
			AdaptiveSamplingData.UseAdaptiveSampling = CVarPathTracingAdaptiveSampling.GetValueOnRenderThread();
			AdaptiveSamplingData.RandomSequence = CVarPathTracingRandomSequence.GetValueOnRenderThread();
			if (VarianceMipTree.NumBytes > 0)
			{
				AdaptiveSamplingData.Iteration = Iteration;
				AdaptiveSamplingData.VarianceDimensions = VarianceDimensions;
				AdaptiveSamplingData.VarianceMipTree = VarianceMipTree.SRV;
				AdaptiveSamplingData.MinimumSamplesPerPixel = CVarPathTracingAdaptiveSamplingMinimumSamplesPerPixel.GetValueOnRenderThread();
			}
			else
			{
				AdaptiveSamplingData.UseAdaptiveSampling = 0;
				AdaptiveSamplingData.Iteration = Iteration;
				AdaptiveSamplingData.VarianceDimensions = FIntVector(1, 1, 1);
				AdaptiveSamplingData.VarianceMipTree = RHICreateShaderResourceView(GBlackTexture->TextureRHI->GetTexture2D(), 0);
				AdaptiveSamplingData.MinimumSamplesPerPixel = CVarPathTracingAdaptiveSamplingMinimumSamplesPerPixel.GetValueOnRenderThread();
			}

			FUniformBufferRHIRef AdaptiveSamplingDataUniformBuffer = RHICreateUniformBuffer(&AdaptiveSamplingData, FPathTracingAdaptiveSamplingData::StaticStructMetadata.GetLayout(), EUniformBufferUsage::UniformBuffer_SingleDraw);
			GlobalResources.Set(AdaptiveSamplingParameters, AdaptiveSamplingDataUniformBuffer);
		}

		// Output
		{
			GlobalResources.Set(RadianceRT, RadianceUAV);
			GlobalResources.Set(SampleCountRT, SampleCountUAV);
			GlobalResources.Set(PixelPositionRT, PixelPositionUAV);
			GlobalResources.Set(RayCountPerPixelRT, RayCountPerPixelUAV);
		}
	}

	bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << TLASParameter;
		Ar << ViewParameter;
		Ar << PathTracingParameters;
		Ar << SceneLightsParameters;
		Ar << SkyLightParameters;
		Ar << AdaptiveSamplingParameters;
		// Output
		Ar << RadianceRT;
		Ar << SampleCountRT;
		Ar << PixelPositionRT;
		Ar << RayCountPerPixelRT;

		return bShaderHasOutdatedParameters;
	}

	FShaderResourceParameter		TLASParameter;   // RaytracingAccelerationStructure
	FShaderUniformBufferParameter	ViewParameter;
	FShaderUniformBufferParameter	PathTracingParameters;
	FShaderUniformBufferParameter	SceneLightsParameters;
	FShaderUniformBufferParameter	SkyLightParameters;
	FShaderUniformBufferParameter	AdaptiveSamplingParameters;

	// Output parameters
	FShaderResourceParameter		RadianceRT;
	FShaderResourceParameter		SampleCountRT;
	FShaderResourceParameter        PixelPositionRT;
	FShaderResourceParameter		RayCountPerPixelRT;
};
IMPLEMENT_SHADER_TYPE(, FPathTracingRG, TEXT("/Engine/Private/PathTracing/PathTracing.usf"), TEXT("PathTracingMainRG"), SF_RayGen);

class FPathTracingCHS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FPathTracingCHS, Global);

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	FPathTracingCHS() {}
	virtual ~FPathTracingCHS() {}

	FPathTracingCHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
};

IMPLEMENT_SHADER_TYPE(, FPathTracingCHS, TEXT("/Engine/Private/PathTracing/PathTracingCHS.usf"), TEXT("PathTracingMainCHS"), SF_RayHitGroup);

class FPathTracingMS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FPathTracingMS, Global);

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	FPathTracingMS() {}
	virtual ~FPathTracingMS() {}

	FPathTracingMS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
};

IMPLEMENT_SHADER_TYPE(, FPathTracingMS, TEXT("/Engine/Private/PathTracing/PathTracingMS.usf"), TEXT("PathTracingMainMS"), SF_RayMiss);

DECLARE_GPU_STAT_NAMED(Stat_GPU_PathTracing, TEXT("Reference Path Tracing"));
DECLARE_GPU_STAT_NAMED(Stat_GPU_PathTracingBuildSkyLightCDF, TEXT("Path Tracing: Build Sky Light CDF"));
DECLARE_GPU_STAT_NAMED(Stat_GPU_PathTracingBuildVarianceMipTree, TEXT("Path Tracing: Build Variance Map Tree"));

class FPathTracingCompositorPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPathTracingCompositorPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		// #dxr_todo: this should also check if ray tracing is enabled for the target platform & project
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	FPathTracingCompositorPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		RadianceRedTexture.Bind(Initializer.ParameterMap, TEXT("RadianceRedTexture"));
		RadianceGreenTexture.Bind(Initializer.ParameterMap, TEXT("RadianceGreenTexture"));
		RadianceBlueTexture.Bind(Initializer.ParameterMap, TEXT("RadianceBlueTexture"));
		RadianceAlphaTexture.Bind(Initializer.ParameterMap, TEXT("RadianceAlphaTexture"));
		SampleCountTexture.Bind(Initializer.ParameterMap, TEXT("SampleCountTexture"));

		CumulativeIrradianceTexture.Bind(Initializer.ParameterMap, TEXT("CumulativeIrradianceTexture"));
		CumulativeSampleCountTexture.Bind(Initializer.ParameterMap, TEXT("CumulativeSampleCountTexture"));
	}

	FPathTracingCompositorPS()
	{
	}

	template<typename TRHICommandList>
	void SetParameters(
		TRHICommandList& RHICmdList,
		const FViewInfo& View,
		FTextureRHIParamRef RadianceRedRT,
		FTextureRHIParamRef RadianceGreenRT,
		FTextureRHIParamRef RadianceBlueRT,
		FTextureRHIParamRef RadianceAlphaRT,
		FTextureRHIParamRef SampleCountRT,
		FTextureRHIParamRef CumulativeIrradianceRT,
		FTextureRHIParamRef CumulativeSampleCountRT)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		SetTextureParameter(RHICmdList, ShaderRHI, RadianceRedTexture, RadianceRedRT);
		SetTextureParameter(RHICmdList, ShaderRHI, RadianceGreenTexture, RadianceGreenRT);
		SetTextureParameter(RHICmdList, ShaderRHI, RadianceBlueTexture, RadianceBlueRT);
		SetTextureParameter(RHICmdList, ShaderRHI, RadianceAlphaTexture, RadianceAlphaRT);
		SetTextureParameter(RHICmdList, ShaderRHI, SampleCountTexture, SampleCountRT);
		SetTextureParameter(RHICmdList, ShaderRHI, CumulativeIrradianceTexture, CumulativeIrradianceRT);
		SetTextureParameter(RHICmdList, ShaderRHI, CumulativeSampleCountTexture, CumulativeSampleCountRT);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOudatedParameters = FGlobalShader::Serialize(Ar);
		Ar << RadianceRedTexture;
		Ar << RadianceGreenTexture;
		Ar << RadianceBlueTexture;
		Ar << RadianceAlphaTexture;
		Ar << SampleCountTexture;
		Ar << CumulativeIrradianceTexture;
		Ar << CumulativeSampleCountTexture;
		return bShaderHasOudatedParameters;
	}

public:
	FShaderResourceParameter RadianceRedTexture;
	FShaderResourceParameter RadianceGreenTexture;
	FShaderResourceParameter RadianceBlueTexture;
	FShaderResourceParameter RadianceAlphaTexture;
	FShaderResourceParameter SampleCountTexture;

	FShaderResourceParameter CumulativeIrradianceTexture;
	FShaderResourceParameter CumulativeSampleCountTexture;
};

IMPLEMENT_SHADER_TYPE(, FPathTracingCompositorPS, TEXT("/Engine/Private/PathTracing/PathTracingCompositingPixelShader.usf"), TEXT("CompositeMain"), SF_Pixel);

void FDeferredShadingSceneRenderer::PreparePathTracing(const FViewInfo& View, TArray<FRayTracingShaderRHIParamRef>& OutRayGenShaders)
{
	// Declare all RayGen shaders that require material closest hit shaders to be bound
	auto RayGenShader = View.ShaderMap->GetShader<FPathTracingRG>();
	OutRayGenShaders.Add(RayGenShader->GetRayTracingShader());
}

void FDeferredShadingSceneRenderer::RenderPathTracing(FRHICommandListImmediate& RHICmdList, const FViewInfo& View)
{
	SCOPED_DRAW_EVENT(RHICmdList, PathTracing);

	SCOPED_GPU_STAT(RHICmdList, Stat_GPU_PathTracing);

	// The local iteration counter.
	static int32 SPPCount = 0;

	// Conditionally rebuild sky light CDFs
	if (Scene->SkyLight && Scene->SkyLight->ShouldRebuildCdf())
	{
		BuildSkyLightCdfs(RHICmdList, Scene->SkyLight);
	}

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	auto ViewSize = View.ViewRect.Size();
	FSceneViewState* ViewState = (FSceneViewState*)View.State;

	// Construct render targets for compositing
	TRefCountPtr<IPooledRenderTarget> RadianceRT;
	TRefCountPtr<IPooledRenderTarget> SampleCountRT;
	TRefCountPtr<IPooledRenderTarget> PixelPositionRT;
	TRefCountPtr<IPooledRenderTarget> RayCountPerPixelRT;

	FPooledRenderTargetDesc Desc = SceneContext.GetSceneColor()->GetDesc();
	Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
	Desc.Format = PF_FloatRGBA;
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, RadianceRT, TEXT("RadianceRT"));
	// TODO: InterlockedCompareExchange() doesn't appear to work with 16-bit uint render target
	//Desc.Format = PF_R16_UINT;
	Desc.Format = PF_R32_UINT;
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, SampleCountRT, TEXT("SampleCountRT"));
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, PixelPositionRT, TEXT("PixelPositionRT"));
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, RayCountPerPixelRT, TEXT("RayCountPerPixelRT"));

	// Clear render targets
	ClearUAV(RHICmdList, RadianceRT->GetRenderTargetItem(), FLinearColor::Black);
	ClearUAV(RHICmdList, SampleCountRT->GetRenderTargetItem(), FLinearColor::Black);
	ClearUAV(RHICmdList, PixelPositionRT->GetRenderTargetItem(), FLinearColor::Black);
	ClearUAV(RHICmdList, RayCountPerPixelRT->GetRenderTargetItem(), FLinearColor::Black);

	auto RayGenShader = GetGlobalShaderMap(FeatureLevel)->GetShader<FPathTracingRG>();
	auto MissShader = GetGlobalShaderMap(FeatureLevel)->GetShader<FPathTracingMS>();
	auto ClosestHitShader = GetGlobalShaderMap(FeatureLevel)->GetShader<FPathTracingCHS>();

	FRayTracingShaderBindingsWriter GlobalResources;

	FSceneTexturesUniformParameters SceneTextures;
	SetupSceneTextureUniformParameters(SceneContext, FeatureLevel, ESceneTextureSetupMode::All, SceneTextures);
	FUniformBufferRHIParamRef SceneTexturesUniformBuffer = RHICreateUniformBuffer(&SceneTextures, FSceneTexturesUniformParameters::StaticStructMetadata.GetLayout(), EUniformBufferUsage::UniformBuffer_SingleDraw);

	RayGenShader->SetParameters(
		Scene,
		View,
		GlobalResources,
		View.RayTracingScene,
		View.ViewUniformBuffer,
		SceneTexturesUniformBuffer,
		Scene->Lights,
		SPPCount, ViewState->VarianceMipTreeDimensions, *ViewState->VarianceMipTree,
		RadianceRT->GetRenderTargetItem().UAV,
		SampleCountRT->GetRenderTargetItem().UAV,
		PixelPositionRT->GetRenderTargetItem().UAV,
		RayCountPerPixelRT->GetRenderTargetItem().UAV
	);

	FRayTracingSceneRHIParamRef RayTracingSceneRHI = View.RayTracingScene.RayTracingSceneRHI;
	RHICmdList.RayTraceDispatch(View.RayTracingMaterialPipeline, RayGenShader->GetRayTracingShader(), RayTracingSceneRHI, GlobalResources, View.ViewRect.Size().X, View.ViewRect.Size().Y);

	// Save RayTracingIndirect for compositing
	RHICmdList.CopyToResolveTarget(RadianceRT->GetRenderTargetItem().TargetableTexture, RadianceRT->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());
	RHICmdList.CopyToResolveTarget(SampleCountRT->GetRenderTargetItem().TargetableTexture, SampleCountRT->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());
	RHICmdList.CopyToResolveTarget(PixelPositionRT->GetRenderTargetItem().TargetableTexture, PixelPositionRT->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());
	RHICmdList.CopyToResolveTarget(RayCountPerPixelRT->GetRenderTargetItem().TargetableTexture, RayCountPerPixelRT->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());

	// Run ray counter shader
	if (SPPCount % CVarPathTracingRayCountFrequency.GetValueOnRenderThread() == 0)
	{
		ComputeRayCount(RHICmdList, View, RayCountPerPixelRT->GetRenderTargetItem().ShaderResourceTexture);
	}

	// Run ray continuation compute shader
	TRefCountPtr<IPooledRenderTarget> RadianceSortedRedRT;
	TRefCountPtr<IPooledRenderTarget> RadianceSortedGreenRT;
	TRefCountPtr<IPooledRenderTarget> RadianceSortedBlueRT;
	TRefCountPtr<IPooledRenderTarget> RadianceSortedAlphaRT;
	TRefCountPtr<IPooledRenderTarget> SampleCountSortedRT;
	//	FPooledRenderTargetDesc Desc = SceneContext.GetSceneColor()->GetDesc();
	//	Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
	Desc.Format = PF_R32_UINT;
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, RadianceSortedRedRT, TEXT("RadianceSortedRedRT"));
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, RadianceSortedGreenRT, TEXT("RadianceSortedGreenRT"));
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, RadianceSortedBlueRT, TEXT("RadianceSortedBlueRT"));
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, RadianceSortedAlphaRT, TEXT("RadianceSortedAlphaRT"));
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, SampleCountSortedRT, TEXT("SampleCountSortedRT"));

	ClearUAV(RHICmdList, RadianceSortedRedRT->GetRenderTargetItem(), FLinearColor::Black);
	ClearUAV(RHICmdList, RadianceSortedGreenRT->GetRenderTargetItem(), FLinearColor::Black);
	ClearUAV(RHICmdList, RadianceSortedBlueRT->GetRenderTargetItem(), FLinearColor::Black);
	ClearUAV(RHICmdList, RadianceSortedAlphaRT->GetRenderTargetItem(), FLinearColor::Black);
	ClearUAV(RHICmdList, SampleCountSortedRT->GetRenderTargetItem(), FLinearColor::Black);

	ComputePathCompaction(
		RHICmdList,
		View,
		RadianceRT->GetRenderTargetItem().ShaderResourceTexture,
		SampleCountRT->GetRenderTargetItem().ShaderResourceTexture,
		PixelPositionRT->GetRenderTargetItem().ShaderResourceTexture,
		RadianceSortedRedRT->GetRenderTargetItem().UAV,
		RadianceSortedGreenRT->GetRenderTargetItem().UAV,
		RadianceSortedBlueRT->GetRenderTargetItem().UAV,
		RadianceSortedAlphaRT->GetRenderTargetItem().UAV,
		SampleCountSortedRT->GetRenderTargetItem().UAV
	);

	RHICmdList.CopyToResolveTarget(RadianceSortedRedRT->GetRenderTargetItem().TargetableTexture, RadianceSortedRedRT->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());
	RHICmdList.CopyToResolveTarget(RadianceSortedGreenRT->GetRenderTargetItem().TargetableTexture, RadianceSortedGreenRT->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());
	RHICmdList.CopyToResolveTarget(RadianceSortedBlueRT->GetRenderTargetItem().TargetableTexture, RadianceSortedBlueRT->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());
	RHICmdList.CopyToResolveTarget(RadianceSortedAlphaRT->GetRenderTargetItem().TargetableTexture, RadianceSortedAlphaRT->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());
	RHICmdList.CopyToResolveTarget(SampleCountSortedRT->GetRenderTargetItem().TargetableTexture, SampleCountSortedRT->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());

	// Construct render targets for compositing
	TRefCountPtr<IPooledRenderTarget> OutputRadianceRT;
	TRefCountPtr<IPooledRenderTarget> OutputSampleCountRT;
	Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
	Desc.Format = PF_A32B32G32R32F;
	//Desc.Format = PF_A16B16G16R16;
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, OutputRadianceRT, TEXT("OutputRadianceRT"));
	Desc.Format = PF_R16_UINT;
	//Desc.Format = PF_R32_UINT;
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, OutputSampleCountRT, TEXT("OutputSampleCountRT"));
	ClearUAV(RHICmdList, OutputRadianceRT->GetRenderTargetItem(), FLinearColor::Black);
	ClearUAV(RHICmdList, OutputSampleCountRT->GetRenderTargetItem(), FLinearColor::Black);

	// Run compositing engine
	const auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

	TShaderMapRef<FPostProcessVS> VertexShader(ShaderMap);
	TShaderMapRef<FPathTracingCompositorPS> PixelShader(ShaderMap);
	FTextureRHIParamRef RenderTargets[3] =
	{
		SceneContext.GetSceneColor()->GetRenderTargetItem().TargetableTexture,
		OutputRadianceRT->GetRenderTargetItem().TargetableTexture,
		OutputSampleCountRT->GetRenderTargetItem().TargetableTexture
	};
	FRHIRenderPassInfo RenderPassInfo(3, RenderTargets, ERenderTargetActions::Load_Store);
	RHICmdList.BeginRenderPass(RenderPassInfo, TEXT("PathTracing"));

	// DEBUG: Inspect render target in isolation
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	//for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
		FTextureRHIRef RadianceRedTexture = RadianceSortedRedRT->GetRenderTargetItem().ShaderResourceTexture;
		FTextureRHIRef RadianceGreenTexture = RadianceSortedGreenRT->GetRenderTargetItem().ShaderResourceTexture;
		FTextureRHIRef RadianceBlueTexture = RadianceSortedBlueRT->GetRenderTargetItem().ShaderResourceTexture;
		FTextureRHIRef RadianceAlphaTexture = RadianceSortedAlphaRT->GetRenderTargetItem().ShaderResourceTexture;
		FTextureRHIRef SampleCountTexture = SampleCountSortedRT->GetRenderTargetItem().ShaderResourceTexture;

		FTextureRHIRef CumulativeRadianceTexture = GBlackTexture->TextureRHI;
		FTextureRHIRef CumulativeSampleCount = GBlackTexture->TextureRHI;

		int32 SamplesPerPixelCVar = CVarPathTracingSamplesPerPixel.GetValueOnRenderThread();
		int32 PathTracingSamplesPerPixel = SamplesPerPixelCVar > -1 ? SamplesPerPixelCVar : View.FinalPostProcessSettings.PathTracingSamplesPerPixel;
		if (ViewState->PathTracingIrradianceRT && SPPCount < PathTracingSamplesPerPixel)
		{
			CumulativeRadianceTexture = ViewState->PathTracingIrradianceRT->GetRenderTargetItem().ShaderResourceTexture;
			CumulativeSampleCount = ViewState->PathTracingSampleCountRT->GetRenderTargetItem().ShaderResourceTexture;
			SPPCount++;
		}
		else
		{
			SPPCount = 0;
		}

		PixelShader->SetParameters(RHICmdList, View, RadianceRedTexture, RadianceGreenTexture, RadianceBlueTexture, RadianceAlphaTexture, SampleCountTexture, CumulativeRadianceTexture, CumulativeSampleCount);
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
	RHICmdList.EndRenderPass();

	RHICmdList.CopyToResolveTarget(OutputRadianceRT->GetRenderTargetItem().TargetableTexture, OutputRadianceRT->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());
	RHICmdList.CopyToResolveTarget(OutputSampleCountRT->GetRenderTargetItem().TargetableTexture, OutputSampleCountRT->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());
	GVisualizeTexture.SetCheckPoint(RHICmdList, OutputRadianceRT);
	GVisualizeTexture.SetCheckPoint(RHICmdList, OutputSampleCountRT);

	// Cache values for reuse
	ViewState->PathTracingIrradianceRT = OutputRadianceRT;
	ViewState->PathTracingSampleCountRT = OutputSampleCountRT;

	// Process variance mip for adaptive sampling
	if (SPPCount % CVarPathTracingVarianceMapRebuildFrequency.GetValueOnRenderThread() == 0)
	{
		SCOPED_GPU_STAT(RHICmdList, Stat_GPU_PathTracingBuildVarianceMipTree);

		BuildVarianceMipTree(RHICmdList, View, OutputRadianceRT->GetRenderTargetItem().ShaderResourceTexture, *ViewState->VarianceMipTree, ViewState->VarianceMipTreeDimensions);
	}

	VisualizeVarianceMipTree(RHICmdList, View, *ViewState->VarianceMipTree, ViewState->VarianceMipTreeDimensions);

	ResolveSceneColor(RHICmdList);

}
#endif