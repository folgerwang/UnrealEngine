// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessTemporalAA.cpp: Post process MotionBlur implementation.
=============================================================================*/

#include "PostProcess/PostProcessTemporalAA.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "SceneRenderTargetParameters.h"
#include "SceneRendering.h"
#include "ScenePrivate.h"
#include "PostProcess/SceneFilterRendering.h"
#include "CompositionLighting/PostProcessAmbientOcclusion.h"
#include "PostProcess/PostProcessTonemap.h"
#include "ClearQuad.h"
#include "PipelineStateCache.h"
#include "PostProcessing.h"

const int32 GTemporalAATileSizeX = 8;
const int32 GTemporalAATileSizeY = 8;

static TAutoConsoleVariable<float> CVarTemporalAAFilterSize(
	TEXT("r.TemporalAAFilterSize"),
	1.0f,
	TEXT("Size of the filter kernel. (1.0 = smoother, 0.0 = sharper but aliased)."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarTemporalAACatmullRom(
	TEXT("r.TemporalAACatmullRom"),
	0,
	TEXT("Whether to use a Catmull-Rom filter kernel. Should be a bit sharper than Gaussian."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarTemporalAAPauseCorrect(
	TEXT("r.TemporalAAPauseCorrect"),
	1,
	TEXT("Correct temporal AA in pause. This holds onto render targets longer preventing reuse and consumes more memory."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarTemporalAACurrentFrameWeight(
	TEXT("r.TemporalAACurrentFrameWeight"),
	.04f,
	TEXT("Weight of current frame's contribution to the history.  Low values cause blurriness and ghosting, high values fail to hide jittering."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarTemporalAAUpsampleFiltered(
	TEXT("r.TemporalAAUpsampleFiltered"),
	1,
	TEXT("Use filtering to fetch color history during TamporalAA upsampling (see AA_FILTERED define in TAA shader). Disabling this makes TAAU faster, but lower quality. "),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static float CatmullRom( float x )
{
	float ax = FMath::Abs(x);
	if( ax > 1.0f )
		return ( ( -0.5f * ax + 2.5f ) * ax - 4.0f ) *ax + 2.0f;
	else
		return ( 1.5f * ax - 2.5f ) * ax*ax + 1.0f;
}


struct FTemporalAAParameters
{
public:
	FPostProcessPassParameters PostprocessParameter;
	FSceneTextureShaderParameters SceneTextureParameters;
	FShaderParameter SampleWeights;
	FShaderParameter PlusWeights;
	FShaderParameter DitherScale;
	FShaderParameter VelocityScaling;
	FShaderParameter CurrentFrameWeight;
	FShaderParameter ScreenPosAbsMax;
	FShaderParameter ScreenPosToHistoryBufferUV;
	FShaderResourceParameter HistoryBuffer[FTemporalAAHistory::kRenderTargetCount];
	FShaderResourceParameter HistoryBufferSampler[FTemporalAAHistory::kRenderTargetCount];
	FShaderParameter HistoryBufferSize;
	FShaderParameter HistoryBufferUVMinMax;
	FShaderParameter MaxViewportUVAndSvPositionToViewportUV;
	FShaderParameter PreExposureSettings;
	FShaderParameter ViewportUVToInputBufferUV;

	void Bind(const FShader::CompiledShaderInitializerType& Initializer)
	{
		const FShaderParameterMap& ParameterMap = Initializer.ParameterMap;
		PostprocessParameter.Bind(ParameterMap);
		SceneTextureParameters.Bind(Initializer);
		SampleWeights.Bind(ParameterMap, TEXT("SampleWeights"));
		PlusWeights.Bind(ParameterMap, TEXT("PlusWeights"));
		DitherScale.Bind(ParameterMap, TEXT("DitherScale"));
		VelocityScaling.Bind(ParameterMap, TEXT("VelocityScaling"));
		CurrentFrameWeight.Bind(ParameterMap, TEXT("CurrentFrameWeight"));
		ScreenPosAbsMax.Bind(ParameterMap, TEXT("ScreenPosAbsMax"));
		ScreenPosToHistoryBufferUV.Bind(ParameterMap, TEXT("ScreenPosToHistoryBufferUV"));
		HistoryBuffer[0].Bind(ParameterMap, TEXT("HistoryBuffer0"));
		HistoryBuffer[1].Bind(ParameterMap, TEXT("HistoryBuffer1"));
		HistoryBufferSampler[0].Bind(ParameterMap, TEXT("HistoryBuffer0Sampler"));
		HistoryBufferSampler[1].Bind(ParameterMap, TEXT("HistoryBuffer1Sampler"));
		HistoryBufferSize.Bind(ParameterMap, TEXT("HistoryBufferSize"));
		HistoryBufferUVMinMax.Bind(ParameterMap, TEXT("HistoryBufferUVMinMax"));
		MaxViewportUVAndSvPositionToViewportUV.Bind(ParameterMap, TEXT("MaxViewportUVAndSvPositionToViewportUV"));
		PreExposureSettings.Bind(ParameterMap, TEXT("PreExposureSettings"));
		ViewportUVToInputBufferUV.Bind(ParameterMap, TEXT("ViewportUVToInputBufferUV"));
	}

	void Serialize(FArchive& Ar)
	{
		Ar << PostprocessParameter << SceneTextureParameters ;
		Ar << SampleWeights << PlusWeights << DitherScale << VelocityScaling << CurrentFrameWeight << ScreenPosAbsMax << ScreenPosToHistoryBufferUV << HistoryBuffer[0] << HistoryBuffer[1] << HistoryBufferSampler[0] << HistoryBufferSampler[1] << HistoryBufferSize << HistoryBufferUVMinMax;
		Ar << MaxViewportUVAndSvPositionToViewportUV << PreExposureSettings << ViewportUVToInputBufferUV;
	}

	template <typename TRHICmdList, typename TShaderRHIParamRef>
	void SetParameters(
		TRHICmdList& RHICmdList,
		const TShaderRHIParamRef ShaderRHI,
		const FRenderingCompositePassContext& Context,
		const FTemporalAAHistory& InputHistory,
		const FTAAPassParameters& PassParameters,
		bool bUseDither,
		const FIntPoint& SrcSize)
	{
		SceneTextureParameters.Set(RHICmdList, ShaderRHI, Context.View.FeatureLevel, ESceneTextureSetupMode::All);

		float ResDivisor = PassParameters.ResolutionDivisor;
		float ResDivisorInv = 1.0f / ResDivisor;

		// PS params
		{
			float JitterX = Context.View.TemporalJitterPixels.X;
			float JitterY = Context.View.TemporalJitterPixels.Y;

			static const float SampleOffsets[9][2] =
			{
				{ -1.0f, -1.0f },
				{  0.0f, -1.0f },
				{  1.0f, -1.0f },
				{ -1.0f,  0.0f },
				{  0.0f,  0.0f },
				{  1.0f,  0.0f },
				{ -1.0f,  1.0f },
				{  0.0f,  1.0f },
				{  1.0f,  1.0f },
			};

			float FilterSize = CVarTemporalAAFilterSize.GetValueOnRenderThread();
			int32 bCatmullRom = CVarTemporalAACatmullRom.GetValueOnRenderThread();

			float Weights[9];
			float WeightsPlus[5];
			float TotalWeight = 0.0f;
			float TotalWeightLow = 0.0f;
			float TotalWeightPlus = 0.0f;
			for (int32 i = 0; i < 9; i++)
			{
				float PixelOffsetX = SampleOffsets[i][0] - JitterX * ResDivisorInv;
				float PixelOffsetY = SampleOffsets[i][1] - JitterY * ResDivisorInv;

				PixelOffsetX /= FilterSize;
				PixelOffsetY /= FilterSize;

				if (bCatmullRom)
				{
					Weights[i] = CatmullRom(PixelOffsetX) * CatmullRom(PixelOffsetY);
					TotalWeight += Weights[i];
				}
				else
				{
					// Normal distribution, Sigma = 0.47
					Weights[i] = FMath::Exp(-2.29f * (PixelOffsetX * PixelOffsetX + PixelOffsetY * PixelOffsetY));
					TotalWeight += Weights[i];
				}
			}

			WeightsPlus[0] = Weights[1];
			WeightsPlus[1] = Weights[3];
			WeightsPlus[2] = Weights[4];
			WeightsPlus[3] = Weights[5];
			WeightsPlus[4] = Weights[7];
			TotalWeightPlus = Weights[1] + Weights[3] + Weights[4] + Weights[5] + Weights[7];

			for (int32 i = 0; i < 9; i++)
			{
				SetShaderValue(RHICmdList, ShaderRHI, SampleWeights, Weights[i] / TotalWeight, i);
			}

			for (int32 i = 0; i < 5; i++)
			{
				SetShaderValue(RHICmdList, ShaderRHI, PlusWeights, WeightsPlus[i] / TotalWeightPlus, i);
			}
		}

		SetShaderValue(RHICmdList, ShaderRHI, DitherScale, bUseDither ? 1.0f : 0.0f);

		const bool bIgnoreVelocity = (Context.View.ViewState && Context.View.ViewState->bSequencerIsPaused);
		SetShaderValue(RHICmdList, ShaderRHI, VelocityScaling, bIgnoreVelocity ? 0.0f : 1.0f);

		SetShaderValue(RHICmdList, ShaderRHI, CurrentFrameWeight, CVarTemporalAACurrentFrameWeight.GetValueOnRenderThread());

		// Set history shader parameters.
		if (InputHistory.IsValid())
		{
			FIntPoint ReferenceViewportOffset = InputHistory.ViewportRect.Min;
			FIntPoint ReferenceViewportExtent = InputHistory.ViewportRect.Size();
			FIntPoint ReferenceBufferSize = InputHistory.ReferenceBufferSize;

			float InvReferenceBufferSizeX = 1.f / float(InputHistory.ReferenceBufferSize.X);
			float InvReferenceBufferSizeY = 1.f / float(InputHistory.ReferenceBufferSize.Y);

			FVector4 ScreenPosToPixelValue(
				ReferenceViewportExtent.X * 0.5f * InvReferenceBufferSizeX,
				-ReferenceViewportExtent.Y * 0.5f * InvReferenceBufferSizeY,
				(ReferenceViewportExtent.X * 0.5f + ReferenceViewportOffset.X) * InvReferenceBufferSizeX,
				(ReferenceViewportExtent.Y * 0.5f + ReferenceViewportOffset.Y) * InvReferenceBufferSizeY);
			SetShaderValue(RHICmdList, ShaderRHI, ScreenPosToHistoryBufferUV, ScreenPosToPixelValue);

			FIntPoint ViewportOffset = ReferenceViewportOffset / PassParameters.ResolutionDivisor;
			FIntPoint ViewportExtent = FIntPoint::DivideAndRoundUp(ReferenceViewportExtent, PassParameters.ResolutionDivisor);
			FIntPoint BufferSize = ReferenceBufferSize / PassParameters.ResolutionDivisor;

			FVector2D ScreenPosAbsMaxValue(1.0f - 1.0f / float(ViewportExtent.X), 1.0f - 1.0f / float(ViewportExtent.Y));
			SetShaderValue(RHICmdList, ShaderRHI, ScreenPosAbsMax, ScreenPosAbsMaxValue);

			float InvBufferSizeX = 1.f / float(BufferSize.X);
			float InvBufferSizeY = 1.f / float(BufferSize.Y);

			FVector4 HistoryBufferUVMinMaxValue(
				(ViewportOffset.X + 0.5f) * InvBufferSizeX,
				(ViewportOffset.Y + 0.5f) * InvBufferSizeY,
				(ViewportOffset.X + ViewportExtent.X - 0.5f) * InvBufferSizeX,
				(ViewportOffset.Y + ViewportExtent.Y - 0.5f) * InvBufferSizeY);

			SetShaderValue(RHICmdList, ShaderRHI, HistoryBufferUVMinMax, HistoryBufferUVMinMaxValue);

			FVector4 HistoryBufferSizeValue(BufferSize.X, BufferSize.Y, InvBufferSizeX, InvBufferSizeY);
			SetShaderValue(RHICmdList, ShaderRHI, HistoryBufferSize, HistoryBufferSizeValue);

			for (uint32 i = 0; i < FTemporalAAHistory::kRenderTargetCount; i++)
			{
				if (InputHistory.RT[i].IsValid())
				{
					SetTextureParameter(
						RHICmdList, ShaderRHI,
						HistoryBuffer[i], HistoryBufferSampler[i],
						TStaticSamplerState<SF_Bilinear>::GetRHI(),
						InputHistory.RT[i]->GetRenderTargetItem().ShaderResourceTexture);
				}
			}
		}

		{
			FVector4 MaxViewportUVAndSvPositionToViewportUVValue(
				(PassParameters.OutputViewRect.Width() - 0.5f * ResDivisor) / float(PassParameters.OutputViewRect.Width()),
				(PassParameters.OutputViewRect.Height() - 0.5f * ResDivisor) / float(PassParameters.OutputViewRect.Height()),
				ResDivisor / float(PassParameters.OutputViewRect.Width()),
				ResDivisor / float(PassParameters.OutputViewRect.Height()));

			SetShaderValue(RHICmdList, ShaderRHI, MaxViewportUVAndSvPositionToViewportUV, MaxViewportUVAndSvPositionToViewportUVValue);
		}

		// Pre-exposure, One over Pre-exposure, History pre-exposure, History one over pre-exposure.
		// DOF settings must preserve scene color range.
		FVector4 PreExposureSettingsValue(1.f, 1.f, 1.f, 1.f);
		if (PassParameters.Pass == ETAAPassConfig::Main)
		{
			PreExposureSettingsValue.X = Context.View.PreExposure;
			PreExposureSettingsValue.Y = 1.f / FMath::Max<float>(SMALL_NUMBER, Context.View.PreExposure);
			PreExposureSettingsValue.Z = InputHistory.IsValid() ? InputHistory.SceneColorPreExposure : Context.View.PreExposure;
			PreExposureSettingsValue.W  = 1.f / FMath::Max<float>(SMALL_NUMBER, PreExposureSettingsValue.Z);
		}
		SetShaderValue(RHICmdList, ShaderRHI, PreExposureSettings, PreExposureSettingsValue);

		{
			float InvSizeX = 1.0f / float(SrcSize.X);
			float InvSizeY = 1.0f / float(SrcSize.Y);
			FVector4 ViewportUVToBufferUVValue(
				ResDivisorInv * PassParameters.InputViewRect.Width() * InvSizeX,
				ResDivisorInv * PassParameters.InputViewRect.Height() * InvSizeY,
				ResDivisorInv * PassParameters.InputViewRect.Min.X * InvSizeX,
				ResDivisorInv * PassParameters.InputViewRect.Min.Y * InvSizeY);

			SetShaderValue(RHICmdList, ShaderRHI, ViewportUVToInputBufferUV, ViewportUVToBufferUVValue);
		}
		
	}
};


// ---------------------------------------------------- Shader permutation dimensions

namespace
{

class FTAAPassConfigDim : SHADER_PERMUTATION_ENUM_CLASS("TAA_PASS_CONFIG", ETAAPassConfig);
class FTAAFastDim : SHADER_PERMUTATION_BOOL("TAA_FAST");
class FTAAResponsiveDim : SHADER_PERMUTATION_BOOL("TAA_RESPONSIVE");
class FTAACameraCutDim : SHADER_PERMUTATION_BOOL("TAA_CAMERA_CUT");
class FTAAScreenPercentageDim : SHADER_PERMUTATION_INT("TAA_SCREEN_PERCENTAGE_RANGE", 4);
class FTAAUpsampleFilteredDim : SHADER_PERMUTATION_BOOL("TAA_UPSAMPLE_FILTERED");
class FTAADownsampleDim : SHADER_PERMUTATION_BOOL("TAA_DOWNSAMPLE");

}


// ---------------------------------------------------- Shaders

class FPostProcessTemporalAAPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FPostProcessTemporalAAPS);

	using FPermutationDomain = TShaderPermutationDomain<
		FTAAPassConfigDim,
		FTAAFastDim,
		FTAAResponsiveDim,
		FTAACameraCutDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		// TAAU is compute shader only.
		if (IsTAAUpsamplingConfig(PermutationVector.Get<FTAAPassConfigDim>()))
		{
			return false;
		}

		// Fast dimensions is only for Main and Diaphragm DOF.
		if (PermutationVector.Get<FTAAFastDim>() &&
			!IsMainTAAConfig(PermutationVector.Get<FTAAPassConfigDim>()) &&
			!IsDOFTAAConfig(PermutationVector.Get<FTAAPassConfigDim>()))
		{
			return false;
		}

		// Responsive dimension is only for Main.
		if (PermutationVector.Get<FTAAResponsiveDim>() && !SupportsResponsiveDim(PermutationVector))
		{
			return false;
		}

		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM4);
	}

	static bool SupportsResponsiveDim(const FPermutationDomain& PermutationVector)
	{
		return PermutationVector.Get<FTAAPassConfigDim>() == ETAAPassConfig::Main;
	}

	/** Default constructor. */
	FPostProcessTemporalAAPS() {}

	/** Initialization constructor. */
	FPostProcessTemporalAAPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		Parameter.Bind(Initializer);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Parameter.Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	template <typename TRHICmdList>
	void SetParameters(
		TRHICmdList& RHICmdList,
		const FRenderingCompositePassContext& Context,
		const FTemporalAAHistory& InputHistory,
		const FTAAPassParameters& PassParameters,
		bool bUseDither,
		const FIntPoint& SrcSize)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		
		Parameter.PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context);

		Parameter.SetParameters(RHICmdList, ShaderRHI, Context, InputHistory, PassParameters, bUseDither, SrcSize);
	}

	FTemporalAAParameters Parameter;
};

IMPLEMENT_GLOBAL_SHADER(FPostProcessTemporalAAPS, "/Engine/Private/PostProcessTemporalAA.usf", "MainPS", SF_Pixel);


class FPostProcessTemporalAACS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FPostProcessTemporalAACS);

	using FPermutationDomain = TShaderPermutationDomain<
		FTAAPassConfigDim,
		FTAAFastDim,
		FTAACameraCutDim,
		FTAAScreenPercentageDim,
		FTAAUpsampleFilteredDim,
		FTAADownsampleDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		// Screen percentage dimension is only for upsampling permutation.
		if (!IsTAAUpsamplingConfig(PermutationVector.Get<FTAAPassConfigDim>()) &&
			PermutationVector.Get<FTAAScreenPercentageDim>() != 0)
		{
			return false;
		}

		if (PermutationVector.Get<FTAAPassConfigDim>() == ETAAPassConfig::MainSuperSampling)
		{
			// Super sampling is only high end PC SM5 functionality.
			if (!IsPCPlatform(Parameters.Platform))
			{
				return false;
			}

			// No point disabling filtering.
			if (!PermutationVector.Get<FTAAUpsampleFilteredDim>())
			{
				return false;
			}

			// No point doing a fast permutation since it is PC only.
			if (PermutationVector.Get<FTAAFastDim>())
			{
				return false;
			}
		}

		// No point disabling filtering if not using the fast permutation already.
		if (!PermutationVector.Get<FTAAUpsampleFilteredDim>() &&
			!PermutationVector.Get<FTAAFastDim>())
		{
			return false;
		}

		// No point downsampling if not using the fast permutation already.
		if (PermutationVector.Get<FTAADownsampleDim>() &&
			!PermutationVector.Get<FTAAFastDim>())
		{
			return false;
		}

		// Screen percentage range 3 is only for super sampling.
		if (PermutationVector.Get<FTAAPassConfigDim>() != ETAAPassConfig::MainSuperSampling &&
			PermutationVector.Get<FTAAScreenPercentageDim>() == 3)
		{
			return false;
		}

		// Fast dimensions is only for Main and Diaphragm DOF.
		if (PermutationVector.Get<FTAAFastDim>() &&
			!IsMainTAAConfig(PermutationVector.Get<FTAAPassConfigDim>()) &&
			!IsDOFTAAConfig(PermutationVector.Get<FTAAPassConfigDim>()))
		{
			return false;
		}
		
		// Non filtering option is only for upsampling.
		if (!PermutationVector.Get<FTAAUpsampleFilteredDim>() &&
			PermutationVector.Get<FTAAPassConfigDim>() != ETAAPassConfig::MainUpsampling)
		{
			return false;
		}

		// TAA_DOWNSAMPLE is only only for Main and MainUpsampling configs.
		if (PermutationVector.Get<FTAADownsampleDim>() &&
			!IsMainTAAConfig(PermutationVector.Get<FTAAPassConfigDim>()))
		{
			return false;
		}

		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GTemporalAATileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GTemporalAATileSizeY);
	}

	/** Default constructor. */
	FPostProcessTemporalAACS() {}

	/** Initialization constructor. */
	FPostProcessTemporalAACS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		Parameter.Bind(Initializer);

		EyeAdaptation.Bind(Initializer.ParameterMap, TEXT("EyeAdaptation"));
		OutComputeTex0.Bind(Initializer.ParameterMap, TEXT("OutComputeTex0"));
		OutComputeTex1.Bind(Initializer.ParameterMap, TEXT("OutComputeTex1"));
		OutComputeTexDownsampled.Bind(Initializer.ParameterMap, TEXT("OutComputeTexDownsampled"));
		InputViewMin.Bind(Initializer.ParameterMap, TEXT("InputViewMin"));
		InputViewSize.Bind(Initializer.ParameterMap, TEXT("InputViewSize"));
		TemporalJitterPixels.Bind(Initializer.ParameterMap, TEXT("TemporalJitterPixels"));
		ScreenPercentageAndUpscaleFactor.Bind(Initializer.ParameterMap, TEXT("ScreenPercentageAndUpscaleFactor"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Parameter.Serialize(Ar);
		Ar << EyeAdaptation << OutComputeTex0 << OutComputeTex1 << OutComputeTexDownsampled << InputViewMin << InputViewSize << TemporalJitterPixels << ScreenPercentageAndUpscaleFactor;
		return bShaderHasOutdatedParameters;
	}

	template <typename TRHICmdList>
	void SetParameters(
		TRHICmdList& RHICmdList,
		const FRenderingCompositePassContext& Context,
		const FTemporalAAHistory& InputHistory,
		const FTAAPassParameters& PassParameters,
		const FIntPoint& DestSize,
		const FSceneRenderTargetItem* DestRenderTarget[2],
		FUnorderedAccessViewRHIParamRef DestDownsampledUAV,
		const FIntPoint& SrcSize,
		bool bUseDither,
		FTextureRHIParamRef EyeAdaptationTex)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FSceneViewState* ViewState = (FSceneViewState*)Context.View.State;
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		// CS params
		Parameter.PostprocessParameter.SetCS(ShaderRHI, Context, RHICmdList);

		RHICmdList.SetUAVParameter(ShaderRHI, OutComputeTex0.GetBaseIndex(), DestRenderTarget[0]->UAV);
		if (DestRenderTarget[1])
			RHICmdList.SetUAVParameter(ShaderRHI, OutComputeTex1.GetBaseIndex(), DestRenderTarget[1]->UAV);

		if (DestDownsampledUAV)
		{
			RHICmdList.SetUAVParameter(ShaderRHI, OutComputeTexDownsampled.GetBaseIndex(), DestDownsampledUAV);
		}

		// VS params
		SetTextureParameter(RHICmdList, ShaderRHI, EyeAdaptation, EyeAdaptationTex);

		Parameter.SetParameters(RHICmdList, ShaderRHI, Context, InputHistory, PassParameters, bUseDither, SrcSize);

		// Temporal AA upscale specific params.
		{
			float InputViewSizeInvScale = PassParameters.ResolutionDivisor;
			float InputViewSizeScale = 1.0f / InputViewSizeInvScale;

			SetShaderValue(RHICmdList, ShaderRHI, TemporalJitterPixels, InputViewSizeScale * Context.View.TemporalJitterPixels);
			SetShaderValue(RHICmdList, ShaderRHI, ScreenPercentageAndUpscaleFactor, FVector2D(
				float(PassParameters.InputViewRect.Width()) / float(PassParameters.OutputViewRect.Width()),
				float(PassParameters.OutputViewRect.Width()) / float(PassParameters.InputViewRect.Width())));

			SetShaderValue(RHICmdList, ShaderRHI, InputViewMin, InputViewSizeScale * FVector2D(PassParameters.InputViewRect.Min.X, PassParameters.InputViewRect.Min.Y));
			SetShaderValue(RHICmdList, ShaderRHI, InputViewSize, FVector4(
				InputViewSizeScale * PassParameters.InputViewRect.Width(), InputViewSizeScale * PassParameters.InputViewRect.Height(),
				InputViewSizeInvScale / PassParameters.InputViewRect.Width(), InputViewSizeInvScale / PassParameters.InputViewRect.Height()));
		}
	}

	template <typename TRHICmdList>
	void UnsetParameters(TRHICmdList& RHICmdList)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		if (OutComputeTex0.IsBound())
			RHICmdList.SetUAVParameter(ShaderRHI, OutComputeTex0.GetBaseIndex(), NULL);
		if (OutComputeTex1.IsBound())
			RHICmdList.SetUAVParameter(ShaderRHI, OutComputeTex1.GetBaseIndex(), NULL);
		if (OutComputeTexDownsampled.IsBound())
			RHICmdList.SetUAVParameter(ShaderRHI, OutComputeTexDownsampled.GetBaseIndex(), NULL);
	}

	FTemporalAAParameters Parameter;
	FShaderResourceParameter EyeAdaptation;
	FShaderParameter TemporalAAComputeParams;
	FShaderParameter OutComputeTex0;
	FShaderParameter OutComputeTex1;
	FShaderParameter OutComputeTexDownsampled;
	FShaderParameter InputViewMin;
	FShaderParameter InputViewSize;
	FShaderParameter TemporalJitterPixels;
	FShaderParameter ScreenPercentageAndUpscaleFactor;
};

IMPLEMENT_GLOBAL_SHADER(FPostProcessTemporalAACS, "/Engine/Private/PostProcessTemporalAA.usf", "MainCS", SF_Compute);

static inline void TransitionPixelPassResources(FRenderingCompositePassContext& Context)
{
	TShaderMapRef< FPostProcessTonemapVS > VertexShader(Context.GetShaderMap());
	VertexShader->TransitionResources(Context);
}

void DrawPixelPassTemplate(
	FRenderingCompositePassContext& Context,
	const FPostProcessTemporalAAPS::FPermutationDomain& PermutationVector,
	const FIntPoint SrcSize,
	const FIntRect ViewRect,
	const FTemporalAAHistory& InputHistory,
	const FTAAPassParameters& PassParameters,
	const bool bUseDither,
	FDepthStencilStateRHIParamRef DepthStencilState)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = DepthStencilState;

	TShaderMapRef< FPostProcessTonemapVS > VertexShader(Context.GetShaderMap());
	TShaderMapRef< FPostProcessTemporalAAPS > PixelShader(Context.GetShaderMap(), PermutationVector);

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);
	VertexShader->SetVS(Context);
	PixelShader->SetParameters(Context.RHICmdList, Context, InputHistory, PassParameters, bUseDither, SrcSize);

	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		ViewRect.Width(), ViewRect.Height(),
		ViewRect.Min.X, ViewRect.Min.Y,
		ViewRect.Width(), ViewRect.Height(),
		ViewRect.Size(),
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);
}

template<class TRHICmdList>
void DispatchCSTemplate(
	TRHICmdList& RHICmdList,
	FRenderingCompositePassContext& Context,
	const FPostProcessTemporalAACS::FPermutationDomain& PermutationVector,
	const FTemporalAAHistory& InputHistory,
	const FTAAPassParameters& PassParameters,
	const FIntPoint& SrcSize,
	const FSceneRenderTargetItem* DestRenderTarget[2],
	FUnorderedAccessViewRHIParamRef DestDownsampledUAV,
	const bool bUseDither,
	FTextureRHIParamRef EyeAdaptationTex)
{
	auto ShaderMap = Context.GetShaderMap();
	TShaderMapRef<FPostProcessTemporalAACS> ComputeShader(ShaderMap, PermutationVector);

	RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

	FIntPoint DestSize = FIntPoint::DivideAndRoundUp(PassParameters.OutputViewRect.Size(), PassParameters.ResolutionDivisor);
	ComputeShader->SetParameters(RHICmdList, Context, InputHistory, PassParameters, DestSize, DestRenderTarget, DestDownsampledUAV, SrcSize, bUseDither, EyeAdaptationTex);

	uint32 GroupSizeX = FMath::DivideAndRoundUp(DestSize.X, GTemporalAATileSizeX);
	uint32 GroupSizeY = FMath::DivideAndRoundUp(DestSize.Y, GTemporalAATileSizeY);
	DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);

	ComputeShader->UnsetParameters(RHICmdList);
}


DECLARE_GPU_STAT(TAA)


const TCHAR* const kTAAOutputNames[] = {
	TEXT("DOFTemporalAA"),
	TEXT("TemporalAA"),
	TEXT("SSRTemporalAA"),
	TEXT("LightShaftTemporalAA"),
	TEXT("TemporalAA"),
	TEXT("DOFTemporalAA"),
	TEXT("DOFTemporalAA"),
	TEXT("TemporalAA"),
};

const TCHAR* const kTAAPassNames[] = {
	TEXT("LegacyDOF"),
	TEXT("Main"),
	TEXT("ScreenSpaceReflections"),
	TEXT("LightShaft"),
	TEXT("MainUpsampling"),
	TEXT("DiaphragmDOF"),
	TEXT("DiaphragmDOFUpsampling"),
	TEXT("MainSuperSampling"),
};


static_assert(ARRAY_COUNT(kTAAOutputNames) == int32(ETAAPassConfig::MAX), "Missing TAA output name.");
static_assert(ARRAY_COUNT(kTAAPassNames) == int32(ETAAPassConfig::MAX), "Missing TAA pass name.");


FRCPassPostProcessTemporalAA::FRCPassPostProcessTemporalAA(
	const FPostprocessContext& Context,
	const FTAAPassParameters& InParameters,
	const FTemporalAAHistory& InInputHistory,
	FTemporalAAHistory* OutOutputHistory)
	: Parameters(InParameters)
	, OutputExtent(0, 0)
	, InputHistory(InInputHistory)
	, OutputHistory(OutOutputHistory)
{
	bIsComputePass = Parameters.bIsComputePass;
	bPreferAsyncCompute = false;
	bPreferAsyncCompute &= (GNumAlternateFrameRenderingGroups == 1); // Can't handle multi-frame updates on async pipe

	bDownsamplePossible = Parameters.bDownsample &&
		bIsComputePass &&
		IsMainTAAConfig(Parameters.Pass);

	if (IsTAAUpsamplingConfig(Parameters.Pass))
	{
		bIsComputePass = true;

		check(Parameters.OutputViewRect.Min == FIntPoint::ZeroValue);
		FIntPoint PrimaryUpscaleViewSize = Parameters.OutputViewRect.Size();
		FIntPoint QuantizedPrimaryUpscaleViewSize;
		QuantizeSceneBufferSize(PrimaryUpscaleViewSize, QuantizedPrimaryUpscaleViewSize);

		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);
		OutputExtent.X = FMath::Max(SceneContext.GetBufferSizeXY().X, QuantizedPrimaryUpscaleViewSize.X);
		OutputExtent.Y = FMath::Max(SceneContext.GetBufferSizeXY().Y, QuantizedPrimaryUpscaleViewSize.Y);
	}
}

void FRCPassPostProcessTemporalAA::Process(FRenderingCompositePassContext& Context)
{
	AsyncEndFence = FComputeFenceRHIRef();

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);
	FIntPoint SrcSize = InputDesc->Extent;

	// Number of render target in TAA history.
	const int32 RenderTargetCount = IsDOFTAAConfig(Parameters.Pass) && FPostProcessing::HasAlphaChannelSupport() ? 2 : 1;

	const FSceneRenderTargetItem* DestRenderTarget[2] = {nullptr, nullptr};
	DestRenderTarget[0] = &PassOutputs[0].RequestSurface(Context);
	if (RenderTargetCount == 2)
		DestRenderTarget[1] = &PassOutputs[1].RequestSurface(Context);

	const FSceneRenderTargetItem& DestDownsampled = bDownsamplePossible ? PassOutputs[2].RequestSurface(Context) : FSceneRenderTargetItem();

	// Whether this is main TAA pass;
	bool bIsMainPass = IsMainTAAConfig(Parameters.Pass);

	// Whether to use camera cut shader permutation or not.
	const bool bCameraCut = !InputHistory.IsValid() || Context.View.bCameraCut;

	// Whether to use responsive stencil test.
	bool bUseResponsiveStencilTest = Parameters.Pass == ETAAPassConfig::Main && !bIsComputePass && !bCameraCut;

	// Only use dithering if we are outputting to a low precision format
	const bool bUseDither = PassOutputs[0].RenderTargetDesc.Format != PF_FloatRGBA && bIsMainPass;

	// Src rectangle.
	FIntRect SrcRect = Parameters.InputViewRect;

	// Dest rectangle is same as source rectangle, unless Upsampling.
	FIntRect DestRect = Parameters.OutputViewRect;
	check(IsTAAUpsamplingConfig(Parameters.Pass) || SrcRect == DestRect);

	// Name of the pass.
	const TCHAR* PassName = kTAAPassNames[static_cast<int32>(Parameters.Pass)];

	// Stats.
	SCOPED_GPU_STAT(Context.RHICmdList, TAA);

	if (bIsComputePass)
	{
		FPostProcessTemporalAACS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FTAAPassConfigDim>(Parameters.Pass);
		PermutationVector.Set<FTAAFastDim>(Parameters.bUseFast);
		PermutationVector.Set<FTAACameraCutDim>(!Context.View.PrevViewInfo.TemporalAAHistory.IsValid());
		PermutationVector.Set<FTAADownsampleDim>(DestDownsampled.IsValid());
		PermutationVector.Set<FTAAUpsampleFilteredDim>(true);

		if (IsTAAUpsamplingConfig(Parameters.Pass))
		{
			const bool bUpsampleFiltered = CVarTemporalAAUpsampleFiltered.GetValueOnRenderThread() != 0 || Parameters.Pass != ETAAPassConfig::MainUpsampling;
			PermutationVector.Set<FTAAUpsampleFilteredDim>(bUpsampleFiltered);

			// If screen percentage > 100% on X or Y axes, then use screen percentage range = 2 shader permutation to disable LDS caching.
			if (SrcRect.Width() > DestRect.Width() ||
				SrcRect.Height() > DestRect.Height())
			{
				PermutationVector.Set<FTAAScreenPercentageDim>(2);
			}
			// If screen percentage < 50% on X and Y axes, then use screen percentage range = 3 shader permutation.
			else if (SrcRect.Width() * 100 < 50 * DestRect.Width() &&
				SrcRect.Height() * 100 < 50 * DestRect.Height())
			{
				check(Parameters.Pass == ETAAPassConfig::MainSuperSampling);
				PermutationVector.Set<FTAAScreenPercentageDim>(3);
			}
			// If screen percentage < 71% on X and Y axes, then use screen percentage range = 1 shader permutation to have smaller LDS caching.
			else if (SrcRect.Width() * 100 < 71 * DestRect.Width() &&
				SrcRect.Height() * 100 < 71 * DestRect.Height())
			{
				PermutationVector.Set<FTAAScreenPercentageDim>(1);
			}
		}

		FIntRect PracticableSrcRect = FIntRect::DivideAndRoundUp(SrcRect, Parameters.ResolutionDivisor);
		FIntRect PracticableDestRect = FIntRect::DivideAndRoundUp(DestRect, Parameters.ResolutionDivisor);

		SCOPED_DRAW_EVENTF(Context.RHICmdList, TemporalAA, TEXT("TAA %s CS%s %dx%d -> %dx%d"),
			PassName, Parameters.bUseFast ? TEXT(" Fast") : TEXT(""), PracticableSrcRect.Width(), PracticableSrcRect.Height(), PracticableDestRect.Width(), PracticableDestRect.Height());

		// Common setup
		// #todo-renderpass remove once everything is renderpasses
		UnbindRenderTargets(Context.RHICmdList);
		Context.SetViewportAndCallRHI(PracticableDestRect, 0.0f, 1.0f);
		
		static FName AsyncEndFenceName(TEXT("AsyncTemporalAAEndFence"));
		AsyncEndFence = Context.RHICmdList.CreateComputeFence(AsyncEndFenceName);

		FTextureRHIRef EyeAdaptationTex = GWhiteTexture->TextureRHI;
		if (Context.View.HasValidEyeAdaptation())
		{
			EyeAdaptationTex = Context.View.GetEyeAdaptation(Context.RHICmdList)->GetRenderTargetItem().TargetableTexture;
		}

		FUnorderedAccessViewRHIParamRef UAVs[2];
		UAVs[0] = DestRenderTarget[0]->UAV;
		if (RenderTargetCount == 2)
			UAVs[1] = DestRenderTarget[1]->UAV;

		if (IsAsyncComputePass())
		{
			// Async path
			FRHIAsyncComputeCommandListImmediate& RHICmdListComputeImmediate = FRHICommandListExecutor::GetImmediateAsyncComputeCommandList();
			{
				SCOPED_COMPUTE_EVENT(RHICmdListComputeImmediate, AsyncTemporalAA);
				WaitForInputPassComputeFences(RHICmdListComputeImmediate);
					
				RHICmdListComputeImmediate.TransitionResources(
					EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute,
					UAVs, RenderTargetCount);

				if (DestDownsampled.IsValid())
				{
					RHICmdListComputeImmediate.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DestDownsampled.UAV);
				}

				DispatchCSTemplate(
					RHICmdListComputeImmediate, Context, PermutationVector,
					InputHistory, Parameters, SrcSize, DestRenderTarget, DestDownsampled.UAV, bUseDither, EyeAdaptationTex);

				RHICmdListComputeImmediate.TransitionResources(
					EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx,
					UAVs, RenderTargetCount, AsyncEndFence);
				if (DestDownsampled.IsValid())
				{
					RHICmdListComputeImmediate.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, DestDownsampled.UAV);
				}
			}
			FRHIAsyncComputeCommandListImmediate::ImmediateDispatch(RHICmdListComputeImmediate);
		}
		else
		{
			// Direct path
			WaitForInputPassComputeFences(Context.RHICmdList);
			Context.RHICmdList.BeginUpdateMultiFrameResource(DestRenderTarget[0]->ShaderResourceTexture);
			if (RenderTargetCount == 2)
				Context.RHICmdList.BeginUpdateMultiFrameResource(DestRenderTarget[1]->ShaderResourceTexture);

			Context.RHICmdList.TransitionResources(
				EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute,
				UAVs, RenderTargetCount);

			if (DestDownsampled.IsValid())
			{
				Context.RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DestDownsampled.UAV);
			}

			DispatchCSTemplate(
				Context.RHICmdList, Context, PermutationVector,
				InputHistory, Parameters, SrcSize, DestRenderTarget, DestDownsampled.UAV, bUseDither, EyeAdaptationTex);

			Context.RHICmdList.TransitionResources(
				EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx,
				UAVs, RenderTargetCount, AsyncEndFence);

			if (DestDownsampled.IsValid())
			{
				Context.RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, DestDownsampled.UAV);
			}

			Context.RHICmdList.EndUpdateMultiFrameResource(DestRenderTarget[0]->ShaderResourceTexture);
			if (RenderTargetCount == 2)
				Context.RHICmdList.EndUpdateMultiFrameResource(DestRenderTarget[1]->ShaderResourceTexture);
		}
	}
	else
	{
		check(!IsTAAUpsamplingConfig(Parameters.Pass));

		FIntRect ViewRect = FIntRect::DivideAndRoundUp(DestRect, Parameters.ResolutionDivisor);
		FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

		SCOPED_DRAW_EVENTF(Context.RHICmdList, TemporalAA, TEXT("TAA %s PS%s %dx%d"),
			PassName, Parameters.bUseFast ? TEXT(" Fast") : TEXT(""), ViewRect.Width(), ViewRect.Height());

		WaitForInputPassComputeFences(Context.RHICmdList);

		// Inform MultiGPU systems that we're starting to update this resource
		Context.RHICmdList.BeginUpdateMultiFrameResource(DestRenderTarget[0]->ShaderResourceTexture);

		// make sure we transition resources before we begin the render pass on Vulkan (which happens when we call SetRenderTargets)
		TransitionPixelPassResources(Context);

		// Setup render targets.
		
		// Inform MultiGPU systems that we're starting to update this resource
		Context.RHICmdList.BeginUpdateMultiFrameResource(DestRenderTarget[0]->ShaderResourceTexture);
		FRHIRenderPassInfo RPInfo(DestRenderTarget[0]->TargetableTexture, ERenderTargetActions::DontLoad_Store);

		if (RenderTargetCount == 2)
		{
			RPInfo.ColorRenderTargets[1].RenderTarget = DestRenderTarget[1]->TargetableTexture;
			RPInfo.ColorRenderTargets[1].Action = ERenderTargetActions::DontLoad_Store;
			RPInfo.ColorRenderTargets[1].ArraySlice = -1;
			RPInfo.ColorRenderTargets[1].MipIndex = 0;

			Context.RHICmdList.BeginUpdateMultiFrameResource(DestRenderTarget[1]->ShaderResourceTexture);
		}

		RPInfo.DepthStencilRenderTarget.DepthStencilTarget = SceneContext.GetSceneDepthTexture();
		RPInfo.DepthStencilRenderTarget.ResolveTarget = nullptr;
		RPInfo.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(ERenderTargetActions::DontLoad_DontStore, ERenderTargetActions::Load_Store);
		RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthRead_StencilWrite;

		Context.RHICmdList.BeginRenderPass(RPInfo, TEXT("TemporalAA"));
		{
			Context.SetViewportAndCallRHI(ViewRect);

			FPostProcessTemporalAAPS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FTAAPassConfigDim>(Parameters.Pass);
			PermutationVector.Set<FTAAFastDim>(Parameters.bUseFast);
			PermutationVector.Set<FTAACameraCutDim>(bCameraCut);

			if (bUseResponsiveStencilTest)
			{
				// Normal temporal feedback
				// Draw to pixels where stencil == 0
				FDepthStencilStateRHIParamRef DepthStencilState = TStaticDepthStencilState<
					false, CF_Always,
					true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
					false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
					STENCIL_TEMPORAL_RESPONSIVE_AA_MASK, STENCIL_TEMPORAL_RESPONSIVE_AA_MASK>::GetRHI();

				DrawPixelPassTemplate(
					Context, PermutationVector, SrcSize, ViewRect,
					InputHistory, Parameters, bUseDither,
					DepthStencilState);

				// Responsive feedback for tagged pixels
				// Draw to pixels where stencil != 0
				DepthStencilState = TStaticDepthStencilState<
					false, CF_Always,
					true, CF_NotEqual, SO_Keep, SO_Keep, SO_Keep,
					false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
					STENCIL_TEMPORAL_RESPONSIVE_AA_MASK, STENCIL_TEMPORAL_RESPONSIVE_AA_MASK>::GetRHI();

				PermutationVector.Set<FTAAResponsiveDim>(true);
				DrawPixelPassTemplate(
					Context, PermutationVector, SrcSize, ViewRect,
					InputHistory, Parameters, bUseDither,
					DepthStencilState);
			}
			else
			{
				DrawPixelPassTemplate(
					Context, PermutationVector, SrcSize, ViewRect,
					InputHistory, Parameters, bUseDither,
					TStaticDepthStencilState<false, CF_Always>::GetRHI());
			}

			if (RenderTargetCount == 2)
			{
				Context.RHICmdList.EndUpdateMultiFrameResource(DestRenderTarget[1]->ShaderResourceTexture);
			}
		}
		Context.RHICmdList.EndRenderPass();
		Context.RHICmdList.CopyToResolveTarget(DestRenderTarget[0]->TargetableTexture, DestRenderTarget[0]->ShaderResourceTexture, FResolveParams());

		if (RenderTargetCount == 2)
		{
			Context.RHICmdList.CopyToResolveTarget(DestRenderTarget[1]->TargetableTexture, DestRenderTarget[1]->ShaderResourceTexture, FResolveParams());
		}

		if (IsDOFTAAConfig(Parameters.Pass))
		{
			Context.RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToCompute, DestRenderTarget[0]->UAV);

			if (RenderTargetCount == 2)
			{
				Context.RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToCompute, DestRenderTarget[1]->UAV);
			}
		}

		// Inform MultiGPU systems that we've finished with this texture for this frame
		Context.RHICmdList.EndUpdateMultiFrameResource(DestRenderTarget[0]->ShaderResourceTexture);
	}

	if (!Context.View.bViewStateIsReadOnly)
	{
		OutputHistory->SafeRelease();
		OutputHistory->RT[0] = PassOutputs[0].PooledRenderTarget;
		OutputHistory->ViewportRect = DestRect;
		OutputHistory->ReferenceBufferSize = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY();
		OutputHistory->SceneColorPreExposure = Context.View.PreExposure;

		if (OutputExtent.X > 0)
		{
			OutputHistory->ReferenceBufferSize = OutputExtent;
		}
	}

	// Changes the view rectangle of the scene color and reference buffer size when doing temporal upsample for the
	// following passes to still work.
	if (Parameters.Pass == ETAAPassConfig::MainUpsampling || Parameters.Pass == ETAAPassConfig::MainSuperSampling)
	{
		Context.SceneColorViewRect = DestRect;
		Context.ReferenceBufferSize = OutputExtent;
	}
}

FPooledRenderTargetDesc FRCPassPostProcessTemporalAA::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;

	switch (InPassOutputId)
	{
	case ePId_Output0: // main color output
	case ePId_Output1:

		Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;
		Ret.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
		Ret.Reset();
		//regardless of input type, PF_FloatRGBA is required to properly accumulate between frames for a good result.
		Ret.Format = PF_FloatRGBA;
		Ret.DebugName = kTAAOutputNames[static_cast<int32>(Parameters.Pass)];
		Ret.AutoWritable = false;
		Ret.TargetableFlags &= ~(TexCreate_RenderTargetable | TexCreate_UAV);
		Ret.TargetableFlags |= bIsComputePass ? TexCreate_UAV : TexCreate_RenderTargetable;

		if (OutputExtent.X > 0)
		{
			check(OutputExtent.X % Parameters.ResolutionDivisor == 0);
			check(OutputExtent.Y % Parameters.ResolutionDivisor == 0);
			Ret.Extent = OutputExtent / Parameters.ResolutionDivisor;
		}

		// Need a UAV to resource transition from gfx to compute.
		if (IsDOFTAAConfig(Parameters.Pass))
		{
			Ret.TargetableFlags |= TexCreate_UAV;
		}

		break;

	case ePId_Output2: // downsampled color output

		if (!bDownsamplePossible)
		{
			break;
		}

		check(bIsComputePass);

		Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;
		Ret.Flags &= ~(TexCreate_FastVRAM);
		Ret.Reset();

		if (Parameters.DownsampleOverrideFormat != PF_Unknown)
		{
			Ret.Format = Parameters.DownsampleOverrideFormat;
		}

		Ret.DebugName = TEXT("SceneColorHalfRes");
		Ret.AutoWritable = false;
		Ret.TargetableFlags &= ~TexCreate_RenderTargetable;
		Ret.TargetableFlags |= TexCreate_UAV;

		if (OutputExtent.X > 0)
		{
			check(OutputExtent.X % Parameters.ResolutionDivisor == 0);
			check(OutputExtent.Y % Parameters.ResolutionDivisor == 0);
			Ret.Extent = OutputExtent / Parameters.ResolutionDivisor;
		}

		Ret.Extent = FIntPoint::DivideAndRoundUp(Ret.Extent, 2);
		Ret.Extent.X = FMath::Max(1, Ret.Extent.X);
		Ret.Extent.Y = FMath::Max(1, Ret.Extent.Y);

		break;

	default:
		check(false);
	}

	return Ret;
}
