// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessTonemap.cpp: Post processing tone mapping implementation.
=============================================================================*/

#include "PostProcess/PostProcessTonemap.h"
#include "EngineGlobals.h"
#include "ScenePrivate.h"
#include "RendererModule.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessCombineLUTs.h"
#include "PostProcess/PostProcessMobile.h"
#include "ClearQuad.h"
#include "PipelineStateCache.h"


// ---------------------------------------------------- CVars

static TAutoConsoleVariable<float> CVarTonemapperSharpen(
	TEXT("r.Tonemapper.Sharpen"),
	0,
	TEXT("Sharpening in the tonemapper (not for ES2), actual implementation is work in progress, clamped at 10\n")
	TEXT("   0: off(default)\n")
	TEXT(" 0.5: half strength\n")
	TEXT("   1: full strength"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

// Note: Enables or disables HDR support for a project. Typically this would be set on a per-project/per-platform basis in defaultengine.ini
static TAutoConsoleVariable<int32> CVarAllowHDR(
	TEXT("r.AllowHDR"),
	0,
	TEXT("Creates an HDR compatible swap-chain and enables HDR display output.")
	TEXT("0: Disabled (default)\n")
	TEXT("1: Allow HDR, if supported by the platform and display \n"),
	ECVF_ReadOnly
);

// Note: These values are directly referenced in code. They are set in code at runtime and therefore cannot be set via ini files
// Please update all paths if changing
static TAutoConsoleVariable<int32> CVarDisplayColorGamut(
	TEXT("r.HDR.Display.ColorGamut"),
	0,
	TEXT("Color gamut of the output display:\n")
	TEXT("0: Rec709 / sRGB, D65 (default)\n")
	TEXT("1: DCI-P3, D65\n")
	TEXT("2: Rec2020 / BT2020, D65\n")
	TEXT("3: ACES, D60\n")
	TEXT("4: ACEScg, D60\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

// Note: These values are directly referenced in code, please update all paths if changing
enum class FTonemapperOutputDevice
{
	sRGB,
	Rec709,
	ExplicitGammaMapping,
	ACES1000nitST2084,
	ACES2000nitST2084,
	ACES1000nitScRGB,
	ACES2000nitScRGB,
	LinearEXR,

	MAX
};

static TAutoConsoleVariable<int32> CVarDisplayOutputDevice(
	TEXT("r.HDR.Display.OutputDevice"),
	0,
	TEXT("Device format of the output display:\n")
	TEXT("0: sRGB (LDR)\n")
	TEXT("1: Rec709 (LDR)\n")
	TEXT("2: Explicit gamma mapping (LDR)\n")
	TEXT("3: ACES 1000 nit ST-2084 (Dolby PQ) (HDR)\n")
	TEXT("4: ACES 2000 nit ST-2084 (Dolby PQ) (HDR)\n")
	TEXT("5: ACES 1000 nit ScRGB (HDR)\n")
	TEXT("6: ACES 2000 nit ScRGB (HDR)\n")
	TEXT("7: Linear EXR (HDR)\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe);
	
static TAutoConsoleVariable<int32> CVarHDROutputEnabled(
	TEXT("r.HDR.EnableHDROutput"),
	0,
	TEXT("Creates an HDR compatible swap-chain and enables HDR display output.")
	TEXT("0: Disabled (default)\n")
	TEXT("1: Enable hardware-specific implementation\n"),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<float> CVarTonemapperGamma(
	TEXT("r.TonemapperGamma"),
	0.0f,
	TEXT("0: Default behavior\n")
	TEXT("#: Use fixed gamma # instead of sRGB or Rec709 transform"),
	ECVF_Scalability | ECVF_RenderThreadSafe);	

static TAutoConsoleVariable<float> CVarGamma(
	TEXT("r.Gamma"),
	1.0f,
	TEXT("Gamma on output"),
	ECVF_RenderThreadSafe);


// ---------------------------------------------------- Constants

const int32 GTonemapComputeTileSizeX = 8;
const int32 GTonemapComputeTileSizeY = 8;


// ---------------------------------------------------- Shader permutation handling

namespace
{

namespace TonemapperPermutation
{

// Shared permutation dimensions between deferred and mobile renderer.
class FTonemapperBloomDim          : SHADER_PERMUTATION_BOOL("USE_BLOOM");
class FTonemapperGammaOnlyDim      : SHADER_PERMUTATION_BOOL("USE_GAMMA_ONLY");
class FTonemapperGrainIntensityDim : SHADER_PERMUTATION_BOOL("USE_GRAIN_INTENSITY");
class FTonemapperVignetteDim       : SHADER_PERMUTATION_BOOL("USE_VIGNETTE");
class FTonemapperSharpenDim        : SHADER_PERMUTATION_BOOL("USE_SHARPEN");
class FTonemapperGrainJitterDim    : SHADER_PERMUTATION_BOOL("USE_GRAIN_JITTER");

using FCommonDomain = TShaderPermutationDomain<
	FTonemapperBloomDim,
	FTonemapperGammaOnlyDim,
	FTonemapperGrainIntensityDim,
	FTonemapperVignetteDim,
	FTonemapperSharpenDim,
	FTonemapperGrainJitterDim>;

FORCEINLINE bool ShouldCompileCommonPermutation(const FCommonDomain& PermutationVector)
{
	// If GammaOnly, don't compile any other dimmension == true.
	if (PermutationVector.Get<FTonemapperGammaOnlyDim>())
	{
		return !PermutationVector.Get<FTonemapperBloomDim>() &&
			!PermutationVector.Get<FTonemapperGrainIntensityDim>() &&
			!PermutationVector.Get<FTonemapperVignetteDim>() &&
			!PermutationVector.Get<FTonemapperSharpenDim>() &&
			!PermutationVector.Get<FTonemapperGrainJitterDim>();
	}
	return true;
}

// Common conversion of engine settings into.
FCommonDomain BuildCommonPermutationDomain(const FViewInfo& View, bool bGammaOnly)
{
	const FSceneViewFamily* Family = View.Family;

	FCommonDomain PermutationVector;

	// Gamma
	if (bGammaOnly ||
		(Family->EngineShowFlags.Tonemapper == 0) ||
		(Family->EngineShowFlags.PostProcessing == 0))
	{
		PermutationVector.Set<FTonemapperGammaOnlyDim>(true);
		return PermutationVector;
	}

	const FPostProcessSettings& Settings = View.FinalPostProcessSettings;
	PermutationVector.Set<FTonemapperGrainIntensityDim>(Settings.GrainIntensity > 0.0f);
	PermutationVector.Set<FTonemapperVignetteDim>(Settings.VignetteIntensity > 0.0f);
	PermutationVector.Set<FTonemapperBloomDim>(Settings.BloomIntensity > 0.0);
	PermutationVector.Set<FTonemapperGrainJitterDim>(Settings.GrainJitter > 0.0f);
	PermutationVector.Set<FTonemapperSharpenDim>(CVarTonemapperSharpen.GetValueOnRenderThread() > 0.0f);

	return PermutationVector;
}


// Desktop renderer permutation dimensions.
class FTonemapperColorFringeDim       : SHADER_PERMUTATION_BOOL("USE_COLOR_FRINGE");
class FTonemapperGrainQuantizationDim : SHADER_PERMUTATION_BOOL("USE_GRAIN_QUANTIZATION");
class FTonemapperOutputDeviceDim      : SHADER_PERMUTATION_ENUM_CLASS("DIM_OUTPUT_DEVICE", FTonemapperOutputDevice);

using FDesktopDomain = TShaderPermutationDomain<
	FCommonDomain,
	FTonemapperColorFringeDim,
	FTonemapperGrainQuantizationDim,
	FTonemapperOutputDeviceDim>;

FDesktopDomain RemapPermutation(FDesktopDomain PermutationVector)
{
	FCommonDomain CommonPermutationVector = PermutationVector.Get<FCommonDomain>();

	// No remapping if gamma only.
	if (CommonPermutationVector.Get<FTonemapperGammaOnlyDim>())
	{
		return PermutationVector;
	}

	// Grain jitter or intensity looks bad anyway.
	bool bFallbackToSlowest = false;
	bFallbackToSlowest = bFallbackToSlowest || CommonPermutationVector.Get<FTonemapperGrainIntensityDim>();
	bFallbackToSlowest = bFallbackToSlowest || CommonPermutationVector.Get<FTonemapperGrainJitterDim>();

	if (bFallbackToSlowest)
	{
		CommonPermutationVector.Set<FTonemapperGrainIntensityDim>(true);
		CommonPermutationVector.Set<FTonemapperGrainJitterDim>(true);
		CommonPermutationVector.Set<FTonemapperSharpenDim>(true);

		PermutationVector.Set<FTonemapperColorFringeDim>(true);
	}

	// You most likely need Bloom anyway.
	CommonPermutationVector.Set<FTonemapperBloomDim>(true);

	// Grain quantization is pretty important anyway.
	PermutationVector.Set<FTonemapperGrainQuantizationDim>(true);

	PermutationVector.Set<FCommonDomain>(CommonPermutationVector);
	return PermutationVector;
}

bool ShouldCompileDesktopPermutation(FDesktopDomain PermutationVector)
{
	auto CommonPermutationVector = PermutationVector.Get<FCommonDomain>();

	if (RemapPermutation(PermutationVector) != PermutationVector)
	{
		return false;
	}

	if (!ShouldCompileCommonPermutation(CommonPermutationVector))
	{
		return false;
	}

	if (CommonPermutationVector.Get<FTonemapperGammaOnlyDim>())
	{
		return !PermutationVector.Get<FTonemapperColorFringeDim>() &&
			!PermutationVector.Get<FTonemapperGrainQuantizationDim>();
	}

	return true;
}


} // namespace TonemapperPermutation


} // namespace


// ---------------------------------------------------- Functions

static FTonemapperOutputDevice GetOutputDeviceValue()
{
	int32 OutputDeviceValue = CVarDisplayOutputDevice.GetValueOnRenderThread();
	float Gamma = CVarTonemapperGamma.GetValueOnRenderThread();

	if (PLATFORM_APPLE && Gamma == 0.0f)
	{
		Gamma = 2.2f;
	}

	if (Gamma > 0.0f)
	{
		// Enforce user-controlled ramp over sRGB or Rec709
		OutputDeviceValue = FMath::Max(OutputDeviceValue, 2);
	}
	return FTonemapperOutputDevice(FMath::Clamp(OutputDeviceValue, 0, int32(FTonemapperOutputDevice::MAX) - 1));
}


void GrainPostSettings(FVector* RESTRICT const Constant, const FPostProcessSettings* RESTRICT const Settings)
{
	float GrainJitter = Settings->GrainJitter;
	float GrainIntensity = Settings->GrainIntensity;
	Constant->X = GrainIntensity;
	Constant->Y = 1.0f + (-0.5f * GrainIntensity);
	Constant->Z = GrainJitter;
}

// This code is shared by PostProcessTonemap and VisualizeHDR.
void FilmPostSetConstants(FVector4* RESTRICT const Constants, const FPostProcessSettings* RESTRICT const FinalPostProcessSettings, bool bMobile, bool UseColorMatrix, bool UseShadowTint, bool UseContrast)
{
	// Must insure inputs are in correct range (else possible generation of NaNs).
	float InExposure = 1.0f;
	FVector InWhitePoint(FinalPostProcessSettings->FilmWhitePoint);
	float InSaturation = FMath::Clamp(FinalPostProcessSettings->FilmSaturation, 0.0f, 2.0f);
	FVector InLuma = FVector(1.0f/3.0f, 1.0f/3.0f, 1.0f/3.0f);
	FVector InMatrixR(FinalPostProcessSettings->FilmChannelMixerRed);
	FVector InMatrixG(FinalPostProcessSettings->FilmChannelMixerGreen);
	FVector InMatrixB(FinalPostProcessSettings->FilmChannelMixerBlue);
	float InContrast = FMath::Clamp(FinalPostProcessSettings->FilmContrast, 0.0f, 1.0f) + 1.0f;
	float InDynamicRange = powf(2.0f, FMath::Clamp(FinalPostProcessSettings->FilmDynamicRange, 1.0f, 4.0f));
	float InToe = (1.0f - FMath::Clamp(FinalPostProcessSettings->FilmToeAmount, 0.0f, 1.0f)) * 0.18f;
	InToe = FMath::Clamp(InToe, 0.18f/8.0f, 0.18f * (15.0f/16.0f));
	float InHeal = 1.0f - (FMath::Max(1.0f/32.0f, 1.0f - FMath::Clamp(FinalPostProcessSettings->FilmHealAmount, 0.0f, 1.0f)) * (1.0f - 0.18f)); 
	FVector InShadowTint(FinalPostProcessSettings->FilmShadowTint);
	float InShadowTintBlend = FMath::Clamp(FinalPostProcessSettings->FilmShadowTintBlend, 0.0f, 1.0f) * 64.0f;

	// Shadow tint amount enables turning off shadow tinting.
	float InShadowTintAmount = FMath::Clamp(FinalPostProcessSettings->FilmShadowTintAmount, 0.0f, 1.0f);
	InShadowTint = InWhitePoint + (InShadowTint - InWhitePoint) * InShadowTintAmount;

	// Make sure channel mixer inputs sum to 1 (+ smart dealing with all zeros).
	InMatrixR.X += 1.0f / (256.0f*256.0f*32.0f);
	InMatrixG.Y += 1.0f / (256.0f*256.0f*32.0f);
	InMatrixB.Z += 1.0f / (256.0f*256.0f*32.0f);
	InMatrixR *= 1.0f / FVector::DotProduct(InMatrixR, FVector(1.0f));
	InMatrixG *= 1.0f / FVector::DotProduct(InMatrixG, FVector(1.0f));
	InMatrixB *= 1.0f / FVector::DotProduct(InMatrixB, FVector(1.0f));

	// Conversion from linear rgb to luma (using HDTV coef).
	FVector LumaWeights = FVector(0.2126f, 0.7152f, 0.0722f);

	// Make sure white point has 1.0 as luma (so adjusting white point doesn't change exposure).
	// Make sure {0.0,0.0,0.0} inputs do something sane (default to white).
	InWhitePoint += FVector(1.0f / (256.0f*256.0f*32.0f));
	InWhitePoint *= 1.0f / FVector::DotProduct(InWhitePoint, LumaWeights);
	InShadowTint += FVector(1.0f / (256.0f*256.0f*32.0f));
	InShadowTint *= 1.0f / FVector::DotProduct(InShadowTint, LumaWeights);

	// Grey after color matrix is applied.
	FVector ColorMatrixLuma = FVector(
	FVector::DotProduct(InLuma.X * FVector(InMatrixR.X, InMatrixG.X, InMatrixB.X), FVector(1.0f)),
	FVector::DotProduct(InLuma.Y * FVector(InMatrixR.Y, InMatrixG.Y, InMatrixB.Y), FVector(1.0f)),
	FVector::DotProduct(InLuma.Z * FVector(InMatrixR.Z, InMatrixG.Z, InMatrixB.Z), FVector(1.0f)));

	FVector OutMatrixR = FVector(0.0f);
	FVector OutMatrixG = FVector(0.0f);
	FVector OutMatrixB = FVector(0.0f);
	FVector OutColorShadow_Luma = LumaWeights * InShadowTintBlend;
	FVector OutColorShadow_Tint1 = InWhitePoint;
	FVector OutColorShadow_Tint2 = InShadowTint - InWhitePoint;

	if(UseColorMatrix)
	{
		// Final color matrix effected by saturation and exposure.
		OutMatrixR = (ColorMatrixLuma + ((InMatrixR - ColorMatrixLuma) * InSaturation)) * InExposure;
		OutMatrixG = (ColorMatrixLuma + ((InMatrixG - ColorMatrixLuma) * InSaturation)) * InExposure;
		OutMatrixB = (ColorMatrixLuma + ((InMatrixB - ColorMatrixLuma) * InSaturation)) * InExposure;
		if(!UseShadowTint)
		{
			OutMatrixR = OutMatrixR * InWhitePoint.X;
			OutMatrixG = OutMatrixG * InWhitePoint.Y;
			OutMatrixB = OutMatrixB * InWhitePoint.Z;
		}
	}
	else
	{
		// No color matrix fast path.
		if(!UseShadowTint)
		{
			OutMatrixB = InExposure * InWhitePoint;
		}
		else
		{
			// Need to drop exposure in.
			OutColorShadow_Luma *= InExposure;
			OutColorShadow_Tint1 *= InExposure;
			OutColorShadow_Tint2 *= InExposure;
		}
	}

	// Curve constants.
	float OutColorCurveCh3;
	float OutColorCurveCh0Cm1;
	float OutColorCurveCd2;
	float OutColorCurveCm0Cd0;
	float OutColorCurveCh1;
	float OutColorCurveCh2;
	float OutColorCurveCd1;
	float OutColorCurveCd3Cm3;
	float OutColorCurveCm2;

	// Line for linear section.
	float FilmLineOffset = 0.18f - 0.18f*InContrast;
	float FilmXAtY0 = -FilmLineOffset/InContrast;
	float FilmXAtY1 = (1.0f - FilmLineOffset) / InContrast;
	float FilmXS = FilmXAtY1 - FilmXAtY0;

	// Coordinates of linear section.
	float FilmHiX = FilmXAtY0 + InHeal*FilmXS;
	float FilmHiY = FilmHiX*InContrast + FilmLineOffset;
	float FilmLoX = FilmXAtY0 + InToe*FilmXS;
	float FilmLoY = FilmLoX*InContrast + FilmLineOffset;
	// Supported exposure range before clipping.
	float FilmHeal = InDynamicRange - FilmHiX;
	// Intermediates.
	float FilmMidXS = FilmHiX - FilmLoX;
	float FilmMidYS = FilmHiY - FilmLoY;
	float FilmSlope = FilmMidYS / (FilmMidXS);
	float FilmHiYS = 1.0f - FilmHiY;
	float FilmLoYS = FilmLoY;
	float FilmToe = FilmLoX;
	float FilmHiG = (-FilmHiYS + (FilmSlope*FilmHeal)) / (FilmSlope*FilmHeal);
	float FilmLoG = (-FilmLoYS + (FilmSlope*FilmToe)) / (FilmSlope*FilmToe);

	if(UseContrast)
	{
		// Constants.
		OutColorCurveCh1 = FilmHiYS/FilmHiG;
		OutColorCurveCh2 = -FilmHiX*(FilmHiYS/FilmHiG);
		OutColorCurveCh3 = FilmHiYS/(FilmSlope*FilmHiG) - FilmHiX;
		OutColorCurveCh0Cm1 = FilmHiX;
		OutColorCurveCm2 = FilmSlope;
		OutColorCurveCm0Cd0 = FilmLoX;
		OutColorCurveCd3Cm3 = FilmLoY - FilmLoX*FilmSlope;
		// Handle these separate in case of FilmLoG being 0.
		if(FilmLoG != 0.0f)
		{
			OutColorCurveCd1 = -FilmLoYS/FilmLoG;
			OutColorCurveCd2 = FilmLoYS/(FilmSlope*FilmLoG);
		}
		else
		{
			// FilmLoG being zero means dark region is a linear segment (so just continue the middle section).
			OutColorCurveCd1 = 0.0f;
			OutColorCurveCd2 = 1.0f;
			OutColorCurveCm0Cd0 = 0.0f;
			OutColorCurveCd3Cm3 = 0.0f;
		}
	}
	else
	{
		// Simplified for no dark segment.
		OutColorCurveCh1 = FilmHiYS/FilmHiG;
		OutColorCurveCh2 = -FilmHiX*(FilmHiYS/FilmHiG);
		OutColorCurveCh3 = FilmHiYS/(FilmSlope*FilmHiG) - FilmHiX;
		OutColorCurveCh0Cm1 = FilmHiX;
		// Not used.
		OutColorCurveCm2 = 0.0f;
		OutColorCurveCm0Cd0 = 0.0f;
		OutColorCurveCd3Cm3 = 0.0f;
		OutColorCurveCd1 = 0.0f;
		OutColorCurveCd2 = 0.0f;
	}

	Constants[0] = FVector4(OutMatrixR, OutColorCurveCd1);
	Constants[1] = FVector4(OutMatrixG, OutColorCurveCd3Cm3);
	Constants[2] = FVector4(OutMatrixB, OutColorCurveCm2); 
	Constants[3] = FVector4(OutColorCurveCm0Cd0, OutColorCurveCd2, OutColorCurveCh0Cm1, OutColorCurveCh3); 
	Constants[4] = FVector4(OutColorCurveCh1, OutColorCurveCh2, 0.0f, 0.0f);
	Constants[5] = FVector4(OutColorShadow_Luma, 0.0f);
	Constants[6] = FVector4(OutColorShadow_Tint1, 0.0f);
	Constants[7] = FVector4(OutColorShadow_Tint2, 0.0f);
}


BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FBloomDirtMaskParameters,)
	SHADER_PARAMETER(FVector4,Tint)
	SHADER_PARAMETER_TEXTURE(Texture2D,Mask)
	SHADER_PARAMETER_SAMPLER(SamplerState,MaskSampler)
END_GLOBAL_SHADER_PARAMETER_STRUCT()
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FBloomDirtMaskParameters,"BloomDirtMask");


// ---------------------------------------------------- Shared parameters for desktop's PS and CS

class FPostProcessTonemapShaderParameters
{
public:
	FPostProcessTonemapShaderParameters() {}

	FPostProcessTonemapShaderParameters(const FShaderParameterMap& ParameterMap)
	{
		ColorScale0.Bind(ParameterMap, TEXT("ColorScale0"));
		ColorScale1.Bind(ParameterMap, TEXT("ColorScale1"));
		NoiseTexture.Bind(ParameterMap,TEXT("NoiseTexture"));
		NoiseTextureSampler.Bind(ParameterMap,TEXT("NoiseTextureSampler"));
		TonemapperParams.Bind(ParameterMap, TEXT("TonemapperParams"));
		GrainScaleBiasJitter.Bind(ParameterMap, TEXT("GrainScaleBiasJitter"));
		ColorGradingLUT.Bind(ParameterMap, TEXT("ColorGradingLUT"));
		ColorGradingLUTSampler.Bind(ParameterMap, TEXT("ColorGradingLUTSampler"));
		InverseGamma.Bind(ParameterMap,TEXT("InverseGamma"));
		ChromaticAberrationParams.Bind(ParameterMap, TEXT("ChromaticAberrationParams"));
		ScreenPosToScenePixel.Bind(ParameterMap, TEXT("ScreenPosToScenePixel"));
		SceneUVMinMax.Bind(ParameterMap, TEXT("SceneUVMinMax"));
		SceneBloomUVMinMax.Bind(ParameterMap, TEXT("SceneBloomUVMinMax"));
	}
	
	template <typename TRHICmdList, typename TRHIShader>
	void Set(TRHICmdList& RHICmdList, const TRHIShader ShaderRHI, const FRenderingCompositePassContext& Context, const TShaderUniformBufferParameter<FBloomDirtMaskParameters>& BloomDirtMaskParam)
	{
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		const FSceneViewFamily& ViewFamily = *(Context.View.Family);

		{
			FLinearColor Col = Settings.SceneColorTint;
			FVector4 ColorScale(Col.R, Col.G, Col.B, 0);
			SetShaderValue(RHICmdList, ShaderRHI, ColorScale0, ColorScale);
		}
		
		{
			FLinearColor Col = FLinearColor::White * Settings.BloomIntensity;
			FVector4 ColorScale(Col.R, Col.G, Col.B, 0);
			SetShaderValue(RHICmdList, ShaderRHI, ColorScale1, ColorScale);
		}

		{
			UTexture2D* NoiseTextureValue = GEngine->HighFrequencyNoiseTexture;

			SetTextureParameter(RHICmdList, ShaderRHI, NoiseTexture, NoiseTextureSampler, TStaticSamplerState<SF_Point, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI(), NoiseTextureValue->Resource->TextureRHI);
		}

		{
			float Sharpen = FMath::Clamp(CVarTonemapperSharpen.GetValueOnRenderThread(), 0.0f, 10.0f);

			// /6.0 is to save one shader instruction
			FVector2D Value(Settings.VignetteIntensity, Sharpen / 6.0f);

			SetShaderValue(RHICmdList, ShaderRHI, TonemapperParams, Value);
		}

		FVector GrainValue;
		GrainPostSettings(&GrainValue, &Settings);
		SetShaderValue(RHICmdList, ShaderRHI, GrainScaleBiasJitter, GrainValue);

		if (BloomDirtMaskParam.IsBound())
		{
			FBloomDirtMaskParameters BloomDirtMaskParams;
			
			FLinearColor Col = Settings.BloomDirtMaskTint * Settings.BloomDirtMaskIntensity;
			BloomDirtMaskParams.Tint = FVector4(Col.R, Col.G, Col.B, 0.f /*unused*/);

			BloomDirtMaskParams.Mask = GSystemTextures.BlackDummy->GetRenderTargetItem().TargetableTexture;
			if(Settings.BloomDirtMask && Settings.BloomDirtMask->Resource)
			{
				BloomDirtMaskParams.Mask = Settings.BloomDirtMask->Resource->TextureRHI;
			}
			BloomDirtMaskParams.MaskSampler = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();

			FUniformBufferRHIRef BloomDirtMaskUB = TUniformBufferRef<FBloomDirtMaskParameters>::CreateUniformBufferImmediate(BloomDirtMaskParams, UniformBuffer_SingleDraw);
			SetUniformBufferParameter(RHICmdList, ShaderRHI, BloomDirtMaskParam, BloomDirtMaskUB);
		}

		{
			FRenderingCompositeOutputRef* OutputRef = Context.Pass->GetInput(ePId_Input3);

			const FTextureRHIRef* SrcTexture = Context.View.GetTonemappingLUTTexture();
			bool bShowErrorLog = false;
			// Use a provided tonemaping LUT (provided by a CombineLUTs pass). 
			if (!SrcTexture)
			{
				if (OutputRef && OutputRef->IsValid())
				{
					FRenderingCompositeOutput* Input = OutputRef->GetOutput();
					
					if(Input)
					{
						TRefCountPtr<IPooledRenderTarget> InputPooledElement = Input->RequestInput();
						
						if(InputPooledElement)
						{
							check(!InputPooledElement->IsFree());
							
							SrcTexture = &InputPooledElement->GetRenderTargetItem().ShaderResourceTexture;
						}
					}

					// Indicates the Tonemapper combined LUT node was nominally in the network, so error if it's not found
					bShowErrorLog = true;
				}
			}	

			if (SrcTexture && *SrcTexture)
			{
				SetTextureParameter(RHICmdList, ShaderRHI, ColorGradingLUT, ColorGradingLUTSampler, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), *SrcTexture);
			}
			else if (bShowErrorLog)
			{
				UE_LOG(LogRenderer, Error, TEXT("No Color LUT texture to sample: output will be invalid."));
			}
		}

		{
			FVector InvDisplayGammaValue;
			InvDisplayGammaValue.X = 1.0f / ViewFamily.RenderTarget->GetDisplayGamma();
			InvDisplayGammaValue.Y = 2.2f / ViewFamily.RenderTarget->GetDisplayGamma();
			{
				static TConsoleVariableData<float>* CVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.TonemapperGamma"));
				float Value = CVar->GetValueOnRenderThread();
				if(Value < 1.0f)
				{
					Value = 1.0f;
				}
				InvDisplayGammaValue.Z = 1.0f / Value;
			}
			SetShaderValue(RHICmdList, ShaderRHI, InverseGamma, InvDisplayGammaValue);
		}

		{
			// for scene color fringe
			// from percent to fraction
			float Offset = 0.f;
			float StartOffset = 0.f;
			float Multiplier = 1.f;

			if (Context.View.FinalPostProcessSettings.ChromaticAberrationStartOffset < 1.f - KINDA_SMALL_NUMBER)
			{
				Offset = Context.View.FinalPostProcessSettings.SceneFringeIntensity * 0.01f;
				StartOffset = Context.View.FinalPostProcessSettings.ChromaticAberrationStartOffset;
				Multiplier = 1.f / (1.f - StartOffset);
			}

			// Wavelength of primaries in nm
			const float PrimaryR = 611.3f;
			const float PrimaryG = 549.1f;
			const float PrimaryB = 464.3f;

			// Simple lens chromatic aberration is roughly linear in wavelength
			float ScaleR = 0.007f * (PrimaryR - PrimaryB);
			float ScaleG = 0.007f * (PrimaryG - PrimaryB);
			FVector4 Value(Offset * ScaleR * Multiplier, Offset * ScaleG * Multiplier, StartOffset, 0.f);

			// we only get bigger to not leak in content from outside
			SetShaderValue(Context.RHICmdList, ShaderRHI, ChromaticAberrationParams, Value);
		}

		{
			float InvBufferSizeX = 1.f / float(Context.ReferenceBufferSize.X);
			float InvBufferSizeY = 1.f / float(Context.ReferenceBufferSize.Y);
			FVector4 SceneUVMinMaxValue(
				(Context.SceneColorViewRect.Min.X + 0.5f) * InvBufferSizeX,
				(Context.SceneColorViewRect.Min.Y + 0.5f) * InvBufferSizeY,
				(Context.SceneColorViewRect.Max.X - 0.5f) * InvBufferSizeX,
				(Context.SceneColorViewRect.Max.Y - 0.5f) * InvBufferSizeY);
			SetShaderValue(RHICmdList, ShaderRHI, SceneUVMinMax, SceneUVMinMaxValue);
		}

		{
			float InvBufferSizeX = 1.f / float(Context.ReferenceBufferSize.X);
			float InvBufferSizeY = 1.f / float(Context.ReferenceBufferSize.Y);
			FVector4 SceneBloomUVMinMaxValue(
				(Context.SceneColorViewRect.Min.X + 1.5) * InvBufferSizeX,
				(Context.SceneColorViewRect.Min.Y + 1.5f) * InvBufferSizeY,
				(Context.SceneColorViewRect.Max.X - 1.5f) * InvBufferSizeX,
				(Context.SceneColorViewRect.Max.Y - 1.5f) * InvBufferSizeY);
			SetShaderValue(RHICmdList, ShaderRHI, SceneBloomUVMinMax, SceneBloomUVMinMaxValue);
		}

		{
			FIntPoint ViewportOffset = Context.SceneColorViewRect.Min;
			FIntPoint ViewportExtent = Context.SceneColorViewRect.Size();
			FVector4 ScreenPosToScenePixelValue(
				ViewportExtent.X * 0.5f,
				-ViewportExtent.Y * 0.5f,
				ViewportExtent.X * 0.5f - 0.5f + ViewportOffset.X,
				ViewportExtent.Y * 0.5f - 0.5f + ViewportOffset.Y);
			SetShaderValue(RHICmdList, ShaderRHI, ScreenPosToScenePixel, ScreenPosToScenePixelValue);
		}
	}

	friend FArchive& operator<<(FArchive& Ar,FPostProcessTonemapShaderParameters& P)
	{
		Ar << P.ColorScale0 << P.ColorScale1 << P.InverseGamma << P.NoiseTexture << P.NoiseTextureSampler;
		Ar << P.TonemapperParams << P.GrainScaleBiasJitter;
		Ar << P.ColorGradingLUT << P.ColorGradingLUTSampler;
		Ar << P.SceneUVMinMax << P.SceneBloomUVMinMax;
		Ar << P.ChromaticAberrationParams << P.ScreenPosToScenePixel;

		return Ar;
	}

	FShaderParameter ColorScale0;
	FShaderParameter ColorScale1;
	FShaderResourceParameter NoiseTexture;
	FShaderResourceParameter NoiseTextureSampler;
	FShaderParameter TonemapperParams;
	FShaderParameter GrainScaleBiasJitter;
	FShaderResourceParameter ColorGradingLUT;
	FShaderResourceParameter ColorGradingLUTSampler;
	FShaderParameter InverseGamma;
	FShaderParameter ChromaticAberrationParams;
	FShaderParameter ScreenPosToScenePixel;

	FShaderParameter SceneUVMinMax;
	FShaderParameter SceneBloomUVMinMax;
};


// Vertex Shader permutations based on bool AutoExposure.
IMPLEMENT_SHADER_TYPE(template<>, TPostProcessTonemapVS<true>, TEXT("/Engine/Private/PostProcessTonemap.usf"), TEXT("MainVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(template<>, TPostProcessTonemapVS<false>, TEXT("/Engine/Private/PostProcessTonemap.usf"), TEXT("MainVS"), SF_Vertex);


class FPostProcessTonemapPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FPostProcessTonemapPS);

	using FPermutationDomain = TonemapperPermutation::FDesktopDomain;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES2))
		{
			return false;
		}
		return TonemapperPermutation::ShouldCompileDesktopPermutation(FPermutationDomain(Parameters.PermutationId));
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("USE_VOLUME_LUT"), UseVolumeTextureLUT(Parameters.Platform));
	}

	/** Default constructor. */
	FPostProcessTonemapPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FPostProcessTonemapShaderParameters PostProcessTonemapShaderParameters;

	/** Initialization constructor. */
	FPostProcessTonemapPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
		, PostProcessTonemapShaderParameters(Initializer.ParameterMap)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		Ar << PostProcessTonemapShaderParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		{
			// filtering can cost performance so we use point where possible, we don't want anisotropic sampling
			FSamplerStateRHIParamRef Filters[] =
			{
				TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI(),		// todo: could be SF_Point if fringe is disabled
				TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI(),
				TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI(),
				TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI(),
			};

			PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, 0, eFC_0000, Filters);
		}

		PostProcessTonemapShaderParameters.Set(Context.RHICmdList, ShaderRHI, Context, GetUniformBufferParameter<FBloomDirtMaskParameters>());
	}
};

/** Encapsulates the post processing tonemap compute shader. */
class FPostProcessTonemapCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FPostProcessTonemapCS);

	class FEyeAdaptationDim : SHADER_PERMUTATION_BOOL("EYEADAPTATION_EXPOSURE_FIX");

	using FPermutationDomain = TShaderPermutationDomain<
		TonemapperPermutation::FDesktopDomain,
		FEyeAdaptationDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5))
		{
			return false;
		}

		FPermutationDomain PermutationVector(Parameters.PermutationId);

		return TonemapperPermutation::ShouldCompileDesktopPermutation(PermutationVector.Get<TonemapperPermutation::FDesktopDomain>());
	}	
	
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GTonemapComputeTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GTonemapComputeTileSizeY);
		OutEnvironment.SetDefine(TEXT("USE_VOLUME_LUT"), UseVolumeTextureLUT(Parameters.Platform));
	}

	/** Default constructor. */
	FPostProcessTonemapCS() {}

public:
	// CS params
	FPostProcessPassParameters PostprocessParameter;
	FRWShaderParameter OutComputeTex;
	FShaderParameter TonemapComputeParams;

	// VS params
	FShaderResourceParameter EyeAdaptation;
	FShaderParameter GrainRandomFull;
	FShaderParameter DefaultEyeExposure;

	// PS params
	FPostProcessTonemapShaderParameters PostProcessTonemapShaderParameters;

	/** Initialization constructor. */
	FPostProcessTonemapCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
		, PostProcessTonemapShaderParameters(Initializer.ParameterMap)
	{
		// CS params
		PostprocessParameter.Bind(Initializer.ParameterMap);
		OutComputeTex.Bind(Initializer.ParameterMap, TEXT("OutComputeTex"));
		TonemapComputeParams.Bind(Initializer.ParameterMap, TEXT("TonemapComputeParams"));

		// VS params
		EyeAdaptation.Bind(Initializer.ParameterMap, TEXT("EyeAdaptation"));
		GrainRandomFull.Bind(Initializer.ParameterMap, TEXT("GrainRandomFull"));
		DefaultEyeExposure.Bind(Initializer.ParameterMap, TEXT("DefaultEyeExposure"));
	}

	template <typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context, const FIntPoint& DestSize, FUnorderedAccessViewRHIParamRef DestUAV, FTextureRHIParamRef EyeAdaptationTex)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		const FSceneViewFamily& ViewFamily = *(Context.View.Family);

		// CS params
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		OutComputeTex.SetTexture(RHICmdList, ShaderRHI, nullptr, DestUAV);		
		
		FVector4 TonemapComputeValues(0, 0, 1.f / (float)DestSize.X, 1.f / (float)DestSize.Y);
		SetShaderValue(RHICmdList, ShaderRHI, TonemapComputeParams, TonemapComputeValues);

		// VS params
		FVector GrainRandomFullValue;
		{
			uint8 FrameIndexMod8 = 0;
			if (Context.View.State)
			{
				FrameIndexMod8 = Context.View.ViewState->GetFrameIndex(8);
			}
			GrainRandomFromFrame(&GrainRandomFullValue, FrameIndexMod8);
		}
		SetShaderValue(RHICmdList, ShaderRHI, GrainRandomFull, GrainRandomFullValue);
		
		SetTextureParameter(RHICmdList, ShaderRHI, EyeAdaptation, EyeAdaptationTex);

		{
			// Compute a CPU-based default.  NB: reverts to "1" if SM5 feature level is not supported
			float FixedExposure = FRCPassPostProcessEyeAdaptation::GetFixedExposure(Context.View);
			// Load a default value 
			SetShaderValue(RHICmdList, ShaderRHI, DefaultEyeExposure, FixedExposure);
		}

		// PS params
		{
			// filtering can cost performance so we use point where possible, we don't want anisotropic sampling
			FSamplerStateRHIParamRef Filters[] =
			{
				TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI(),		// todo: could be SF_Point if fringe is disabled
				TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI(),
				TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI(),
				TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI(),
			};

			PostprocessParameter.SetCS(ShaderRHI, Context, RHICmdList, 0, eFC_0000, Filters);
		}

		PostProcessTonemapShaderParameters.Set(RHICmdList, ShaderRHI, Context, GetUniformBufferParameter<FBloomDirtMaskParameters>());
	}

	template <typename TRHICmdList>
	void UnsetParameters(TRHICmdList& RHICmdList)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		OutComputeTex.UnsetUAV(RHICmdList, ShaderRHI);
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		// CS params
		Ar << PostprocessParameter << OutComputeTex << TonemapComputeParams;

		// VS params
		Ar << GrainRandomFull << EyeAdaptation << DefaultEyeExposure;

		// PS params
		Ar << PostProcessTonemapShaderParameters;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_GLOBAL_SHADER(FPostProcessTonemapPS, "/Engine/Private/PostProcessTonemap.usf", "MainPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FPostProcessTonemapCS, "/Engine/Private/PostProcessTonemap.usf", "MainCS", SF_Compute);


FRCPassPostProcessTonemap::FRCPassPostProcessTonemap(const FViewInfo& InView, bool bInDoGammaOnly, bool bInDoEyeAdaptation, bool bInHDROutput, bool bInIsComputePass)
	: bDoGammaOnly(bInDoGammaOnly)
	, bDoScreenPercentageInTonemapper(false)
	, bDoEyeAdaptation(bInDoEyeAdaptation)
	, bHDROutput(bInHDROutput)
	, View(InView)
{
	bIsComputePass = bInIsComputePass;
	bPreferAsyncCompute = false;
}

namespace
{

namespace PostProcessTonemapUtil
{

template <bool bVSDoEyeAdaptation>
static inline void ShaderTransitionResources(const FRenderingCompositePassContext& Context)
{
	typedef TPostProcessTonemapVS<bVSDoEyeAdaptation>				VertexShaderType;
	TShaderMapRef<VertexShaderType> VertexShader(Context.GetShaderMap());
	VertexShader->TransitionResources(Context);
}

} // PostProcessTonemapUtil

template <typename TRHICmdList>
inline void DispatchComputeShader(TRHICmdList& RHICmdList, FRenderingCompositePassContext& Context, const FIntRect& DestRect, FUnorderedAccessViewRHIParamRef DestUAV, const TonemapperPermutation::FDesktopDomain& DesktopPermutationVector, FTextureRHIParamRef EyeAdaptationTex, bool bDoEyeAdaptation)
{
	FPostProcessTonemapCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<TonemapperPermutation::FDesktopDomain>(DesktopPermutationVector);
	PermutationVector.Set<FPostProcessTonemapCS::FEyeAdaptationDim>(bDoEyeAdaptation);

	TShaderMapRef<FPostProcessTonemapCS> ComputeShader(Context.GetShaderMap(), PermutationVector);
	RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());		

	FIntPoint DestSize(DestRect.Width(), DestRect.Height());
	ComputeShader->SetParameters(RHICmdList, Context, DestSize, DestUAV, EyeAdaptationTex);

	uint32 GroupSizeX = FMath::DivideAndRoundUp(DestSize.X, GTonemapComputeTileSizeX);
	uint32 GroupSizeY = FMath::DivideAndRoundUp(DestSize.Y, GTonemapComputeTileSizeY);
	DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);

	ComputeShader->UnsetParameters(RHICmdList);
}

} // namespace

static TAutoConsoleVariable<int32> CVarTonemapperOverride(
	TEXT("r.Tonemapper.ConfigIndexOverride"),
	-1,
	TEXT("direct configindex override. Ignores all other tonemapper configuration cvars"),
	ECVF_RenderThreadSafe);

void FRCPassPostProcessTonemap::Process(FRenderingCompositePassContext& Context)
{
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);
	AsyncEndFence = FComputeFenceRHIRef();

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	const FSceneViewFamily& ViewFamily = *(View.Family);
	FIntRect SrcRect = Context.SceneColorViewRect;
	FIntRect DestRect = Context.GetSceneColorDestRect(DestRenderTarget);

	if (bDoScreenPercentageInTonemapper)
	{
		checkf(Context.IsViewFamilyRenderTarget(DestRenderTarget), TEXT("Doing screen percentage in tonemapper should only be when tonemapper is actually the last pass."));
		checkf(Context.View.PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::SpatialUpscale, TEXT("Tonemapper should only do screen percentage upscale if UpscalePass method should be used."));
	}

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	SCOPED_DRAW_EVENTF(Context.RHICmdList, PostProcessTonemap, TEXT("Tonemapper(%s GammaOnly=%d HandleScreenPercentage=%d) %dx%d"),
		bIsComputePass ? TEXT("CS") : TEXT("PS"), bDoGammaOnly, bDoScreenPercentageInTonemapper, DestRect.Width(), DestRect.Height());

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);

	// Generate permutation vector for the desktop tonemapper.
	TonemapperPermutation::FDesktopDomain DesktopPermutationVector;
	{
		TonemapperPermutation::FCommonDomain CommonDomain = TonemapperPermutation::BuildCommonPermutationDomain(View, bDoGammaOnly);
		DesktopPermutationVector.Set<TonemapperPermutation::FCommonDomain>(CommonDomain);

		if (!CommonDomain.Get<TonemapperPermutation::FTonemapperGammaOnlyDim>())
		{
			// Grain Quantization
			{
				static TConsoleVariableData<int32>* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Tonemapper.GrainQuantization"));
				int32 Value = CVar->GetValueOnRenderThread();
				DesktopPermutationVector.Set<TonemapperPermutation::FTonemapperGrainQuantizationDim>(Value > 0);
			}

			DesktopPermutationVector.Set<TonemapperPermutation::FTonemapperColorFringeDim>(View.FinalPostProcessSettings.SceneFringeIntensity > 0.01f);
		}

		DesktopPermutationVector.Set<TonemapperPermutation::FTonemapperOutputDeviceDim>(GetOutputDeviceValue());

		DesktopPermutationVector = TonemapperPermutation::RemapPermutation(DesktopPermutationVector);
	}

	if (bIsComputePass)
	{
		DestRect = { DestRect.Min, DestRect.Min + DestSize };

		// Common setup
		// #todo-renderpass remove once everything is renderpasses
		UnbindRenderTargets(Context.RHICmdList);
		Context.SetViewportAndCallRHI(DestRect, 0.0f, 1.0f);

		static FName AsyncEndFenceName(TEXT("AsyncTonemapEndFence"));
		AsyncEndFence = Context.RHICmdList.CreateComputeFence(AsyncEndFenceName);

		FTextureRHIRef EyeAdaptationTex = GWhiteTexture->TextureRHI;
		if (Context.View.HasValidEyeAdaptation())
		{
			EyeAdaptationTex = Context.View.GetEyeAdaptation(Context.RHICmdList)->GetRenderTargetItem().TargetableTexture;
		}

		if (IsAsyncComputePass())
		{
			// Async path
			FRHIAsyncComputeCommandListImmediate& RHICmdListComputeImmediate = FRHICommandListExecutor::GetImmediateAsyncComputeCommandList();
			{
				SCOPED_COMPUTE_EVENT(RHICmdListComputeImmediate, AsyncTonemap);
				WaitForInputPassComputeFences(RHICmdListComputeImmediate);

				RHICmdListComputeImmediate.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DestRenderTarget.UAV);
				DispatchComputeShader(RHICmdListComputeImmediate, Context, DestRect, DestRenderTarget.UAV, DesktopPermutationVector, EyeAdaptationTex, bDoEyeAdaptation);
				RHICmdListComputeImmediate.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, DestRenderTarget.UAV, AsyncEndFence);
			}
			FRHIAsyncComputeCommandListImmediate::ImmediateDispatch(RHICmdListComputeImmediate);
		}
		else
		{
			// Direct path
			WaitForInputPassComputeFences(Context.RHICmdList);

			Context.RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DestRenderTarget.UAV);
			DispatchComputeShader(Context.RHICmdList, Context, DestRect, DestRenderTarget.UAV, DesktopPermutationVector, EyeAdaptationTex, bDoEyeAdaptation);
			Context.RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, DestRenderTarget.UAV, AsyncEndFence);
		}
	}
	else
	{
		WaitForInputPassComputeFences(Context.RHICmdList);

		const EShaderPlatform ShaderPlatform = GShaderPlatformForFeatureLevel[Context.GetFeatureLevel()];
		if (bDoEyeAdaptation)
		{
			PostProcessTonemapUtil::ShaderTransitionResources<true>(Context);
		}
		else
		{
			PostProcessTonemapUtil::ShaderTransitionResources<false>(Context);
		}

		ERenderTargetLoadAction LoadAction = ERenderTargetLoadAction::ELoad;

		if (IsMobilePlatform(ShaderPlatform))
		{
			// clear target when processing first view in case of splitscreen
			const bool bFirstView = (&View == View.Family->Views[0]);

			// Full clear to avoid restore
			if ((View.StereoPass == eSSP_FULL && bFirstView) || View.StereoPass == eSSP_LEFT_EYE)
			{
				LoadAction = ERenderTargetLoadAction::EClear;
			}
		}
		else
		{
			LoadAction = Context.GetLoadActionForRenderTarget(DestRenderTarget);
			if (Context.View.AntiAliasingMethod == AAM_FXAA)
			{
				check(LoadAction != ERenderTargetLoadAction::ELoad);
				// needed to not have PostProcessAA leaking in content (e.g. Matinee black borders).
				LoadAction = ERenderTargetLoadAction::EClear;
			}
		}

		FRHIRenderPassInfo RPInfo(DestRenderTarget.TargetableTexture, MakeRenderTargetActions(LoadAction, ERenderTargetStoreAction::EStore));
		Context.RHICmdList.BeginRenderPass(RPInfo, TEXT("Tonemap"));
		{
			Context.SetViewportAndCallRHI(DestRect, 0.0f, 1.0f);

			FShader* VertexShader;
			{
				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
				GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

				if (bDoEyeAdaptation)
				{
					VertexShader = Context.GetShaderMap()->GetShader<TPostProcessTonemapVS<true>>();
				}
				else
				{
					VertexShader = Context.GetShaderMap()->GetShader<TPostProcessTonemapVS<false>>();
				}

				TShaderMapRef<FPostProcessTonemapPS> PixelShader(Context.GetShaderMap(), DesktopPermutationVector);

				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader->GetVertexShader();
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;

				SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

				if (bDoEyeAdaptation)
				{
					TShaderMapRef<TPostProcessTonemapVS<true>> VertexShaderMapRef(Context.GetShaderMap());
					VertexShaderMapRef->SetVS(Context);
				}
				else
				{
					TShaderMapRef<TPostProcessTonemapVS<false>> VertexShaderMapRef(Context.GetShaderMap());
					VertexShaderMapRef->SetVS(Context);
				}

				PixelShader->SetPS(Context);
			}

			DrawPostProcessPass(
				Context.RHICmdList,
				0, 0,
				DestRect.Width(), DestRect.Height(),
				SrcRect.Min.X, SrcRect.Min.Y,
				SrcRect.Width(), SrcRect.Height(),
				DestRect.Size(),
				SrcSize,
				VertexShader,
				View.StereoPass,
				Context.HasHmdMesh(),
				EDRF_UseTriangleOptimization);
		}
		Context.RHICmdList.EndRenderPass();
		Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, FResolveParams());

		// We only release the SceneColor after the last view was processed (SplitScreen)
		if(Context.View.Family->Views[Context.View.Family->Views.Num() - 1] == &Context.View && !GIsEditor)
		{
			// The RT should be released as early as possible to allow sharing of that memory for other purposes.
			// This becomes even more important with some limited VRam (XBoxOne).
			SceneContext.SetSceneColor(0);
		}
	}
}

FPooledRenderTargetDesc FRCPassPostProcessTonemap::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();

	Ret.TargetableFlags &= ~(TexCreate_RenderTargetable | TexCreate_UAV);
	Ret.TargetableFlags |= bIsComputePass ? TexCreate_UAV : TexCreate_RenderTargetable;
	Ret.Format = bIsComputePass ? PF_R8G8B8A8 : PF_B8G8R8A8;

	// RGB is the color in LDR, A is the luminance for PostprocessAA
	Ret.Format = bHDROutput ? GRHIHDRDisplayOutputFormat : Ret.Format;
	Ret.DebugName = TEXT("Tonemap");
	Ret.ClearValue = FClearValueBinding(FLinearColor(0, 0, 0, 0));
	Ret.Flags |= GFastVRamConfig.Tonemap;

	if (CVarDisplayOutputDevice.GetValueOnRenderThread() == 7)
	{
		Ret.Format = PF_A32B32G32R32F;
	}


	// Mobile needs to override the extent
	if (bDoScreenPercentageInTonemapper && View.GetFeatureLevel() <= ERHIFeatureLevel::ES3_1)
	{
		Ret.Extent = View.UnscaledViewRect.Max;
	}
	return Ret;
}





// ES2 version

class FPostProcessTonemapPS_ES2 : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FPostProcessTonemapPS_ES2);

	// Mobile renderer specific permutation dimensions.
	class FTonemapperMsaaDim        : SHADER_PERMUTATION_BOOL("USE_MSAA");
	class FTonemapperDOFDim         : SHADER_PERMUTATION_BOOL("USE_DOF");
	class FTonemapperLightShaftsDim : SHADER_PERMUTATION_BOOL("USE_LIGHT_SHAFTS");
	class FTonemapper32BPPHDRDim    : SHADER_PERMUTATION_BOOL("USE_32BPP_HDR");
	class FTonemapperColorMatrixDim : SHADER_PERMUTATION_BOOL("USE_COLOR_MATRIX");
	class FTonemapperShadowTintDim  : SHADER_PERMUTATION_BOOL("USE_SHADOW_TINT");
	class FTonemapperContrastDim    : SHADER_PERMUTATION_BOOL("USE_CONTRAST");
	
	using FPermutationDomain = TShaderPermutationDomain<
		TonemapperPermutation::FCommonDomain,
		FTonemapperMsaaDim,
		FTonemapperDOFDim,
		FTonemapperLightShaftsDim,
		FTonemapper32BPPHDRDim,
		FTonemapperColorMatrixDim,
		FTonemapperShadowTintDim,
		FTonemapperContrastDim>;
	
	template<class TPermDim, class TPermVector>
	static void EnableIfSet(const TPermVector& SourceDomain, TPermVector& DestDomain)
	{
		if (SourceDomain.template Get<TPermDim>())
		{
			DestDomain.template Set<TPermDim>(true);
		}
	}

	// Reduce the number of permutations by combining common states
	static FPermutationDomain RemapPermutationVector(FPermutationDomain WantedPermutationVector)
	{
		TonemapperPermutation::FCommonDomain WantedCommonPermutationVector = WantedPermutationVector.Get<TonemapperPermutation::FCommonDomain>();
		FPermutationDomain RemappedPermutationVector;
		TonemapperPermutation::FCommonDomain RemappedCommonPermutationVector;

		// Note: FTonemapperSharpenDim, FTonemapperGrainJitterDim are not supported.

		// 32 bit hdr (encoding)
		EnableIfSet<FTonemapper32BPPHDRDim>(WantedPermutationVector, RemappedPermutationVector);

		// Gamma only
		if (WantedCommonPermutationVector.Get<TonemapperPermutation::FTonemapperGammaOnlyDim>())
		{
			RemappedCommonPermutationVector.Set<TonemapperPermutation::FTonemapperGammaOnlyDim>(true);

			// Mutually exclusive - clear the wanted vector. 
			WantedPermutationVector = FPermutationDomain();
			WantedCommonPermutationVector = WantedPermutationVector.Get<TonemapperPermutation::FCommonDomain>();
		}
		else
		{
			// Always enable contrast.
			RemappedPermutationVector.Set<FTonemapperContrastDim>(true);
		}

 		// Bloom permutation
		EnableIfSet<TonemapperPermutation::FTonemapperBloomDim>(WantedCommonPermutationVector, RemappedCommonPermutationVector);
 		// Vignette permutation
		EnableIfSet<TonemapperPermutation::FTonemapperVignetteDim>(WantedCommonPermutationVector, RemappedCommonPermutationVector);
 		// Grain intensity permutation
		EnableIfSet<TonemapperPermutation::FTonemapperGrainIntensityDim>(WantedCommonPermutationVector, RemappedCommonPermutationVector);
 		// Color matrix
		EnableIfSet<FTonemapperColorMatrixDim>(WantedPermutationVector, RemappedPermutationVector);
 		// msaa permutation.
		EnableIfSet<FTonemapperMsaaDim>(WantedPermutationVector, RemappedPermutationVector);

		// DoF
		if (WantedPermutationVector.Get<FTonemapperDOFDim>())
		{
			RemappedPermutationVector.Set<FTonemapperDOFDim>(true);
			RemappedPermutationVector.Set<FTonemapperLightShaftsDim>(true);
			RemappedCommonPermutationVector.Set<TonemapperPermutation::FTonemapperVignetteDim>(true);
			RemappedCommonPermutationVector.Set<TonemapperPermutation::FTonemapperBloomDim>(true);
		}

		// light shafts
		if (WantedPermutationVector.Get<FTonemapperLightShaftsDim>())
		{
			RemappedPermutationVector.Set<FTonemapperLightShaftsDim>(true);
			RemappedCommonPermutationVector.Set<TonemapperPermutation::FTonemapperVignetteDim>(true);
			RemappedCommonPermutationVector.Set<TonemapperPermutation::FTonemapperBloomDim>(true);
		}

		// Shadow tint
		if (WantedPermutationVector.Get<FTonemapperShadowTintDim>())
		{
			RemappedPermutationVector.Set<FTonemapperShadowTintDim>(true);
			RemappedPermutationVector.Set<FTonemapperColorMatrixDim>(true);
		}

		if (RemappedPermutationVector.Get<FTonemapper32BPPHDRDim>())
		{
			// 32 bpp hdr does not support:
			RemappedPermutationVector.Set<FTonemapperDOFDim>(false);
			RemappedPermutationVector.Set<FTonemapperMsaaDim>(false);
			RemappedPermutationVector.Set<FTonemapperLightShaftsDim>(false);
		}

		RemappedPermutationVector.Set<TonemapperPermutation::FCommonDomain>(RemappedCommonPermutationVector);
		return RemappedPermutationVector;
	}

	static FPermutationDomain BuildPermutationVector(const FViewInfo& View)
	{
		TonemapperPermutation::FCommonDomain CommonPermutationVector = TonemapperPermutation::BuildCommonPermutationDomain(View, /* bGammaOnly = */ false);

		FPostProcessTonemapPS_ES2::FPermutationDomain MobilePermutationVector;
		MobilePermutationVector.Set<TonemapperPermutation::FCommonDomain>(CommonPermutationVector);

		bool bUse32BPPHDR = IsMobileHDR32bpp();

		// Must early exit if gamma only.
		if (CommonPermutationVector.Get<TonemapperPermutation::FTonemapperGammaOnlyDim>())
		{
			MobilePermutationVector.Set<FTonemapper32BPPHDRDim>(bUse32BPPHDR);
			return RemapPermutationVector(MobilePermutationVector);
		}

		const FFinalPostProcessSettings& Settings = View.FinalPostProcessSettings;
		{
			FVector MixerR(Settings.FilmChannelMixerRed);
			FVector MixerG(Settings.FilmChannelMixerGreen);
			FVector MixerB(Settings.FilmChannelMixerBlue);
			if (
				(Settings.FilmSaturation != 1.0f) ||
				((MixerR - FVector(1.0f, 0.0f, 0.0f)).GetAbsMax() != 0.0f) ||
				((MixerG - FVector(0.0f, 1.0f, 0.0f)).GetAbsMax() != 0.0f) ||
				((MixerB - FVector(0.0f, 0.0f, 1.0f)).GetAbsMax() != 0.0f))
			{
				MobilePermutationVector.Set<FTonemapperColorMatrixDim>(true);
			}
		}
		MobilePermutationVector.Set<FTonemapperShadowTintDim>(Settings.FilmShadowTintAmount > 0.0f);
		MobilePermutationVector.Set<FTonemapperContrastDim>(Settings.FilmContrast > 0.0f);

		if (IsMobileHDRMosaic())
		{
			MobilePermutationVector.Set<FTonemapper32BPPHDRDim>(true);
			return MobilePermutationVector;
		}

		static const auto CVarMobileMSAA = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileMSAA"));
		const EShaderPlatform ShaderPlatform = GShaderPlatformForFeatureLevel[View.GetFeatureLevel()];
		if ((GSupportsShaderFramebufferFetch && (IsMetalMobilePlatform(ShaderPlatform) || IsVulkanMobilePlatform(ShaderPlatform))) && (CVarMobileMSAA ? CVarMobileMSAA->GetValueOnAnyThread() > 1 : false))
		{
			MobilePermutationVector.Set<FTonemapperMsaaDim>(true);
		}

		if (bUse32BPPHDR)
		{
			// add limited post for 32 bit encoded hdr.
			MobilePermutationVector.Set<FTonemapper32BPPHDRDim>(true);
		}
		else if (GSupportsRenderTargetFormat_PF_FloatRGBA)
		{
#if PLATFORM_HTML5 // EMSCRITPEN_TOOLCHAIN_UPGRADE_CHECK -- i.e. remove this when LLVM no longer errors -- appologies for the mess
			// UE-61742 : the following will coerce i160 bit (bMobileHQGaussian) to an i8 LLVM variable
			bool bUseDof = GetMobileDepthOfFieldScale(View) > 0.0f && ((1 - Settings.bMobileHQGaussian) + (View.GetFeatureLevel() < ERHIFeatureLevel::ES3_1));
#else
			bool bUseDof = GetMobileDepthOfFieldScale(View) > 0.0f && (!Settings.bMobileHQGaussian || (View.GetFeatureLevel() < ERHIFeatureLevel::ES3_1));
#endif

			MobilePermutationVector.Set<FTonemapperDOFDim>(bUseDof);
			MobilePermutationVector.Set<FTonemapperLightShaftsDim>(View.bLightShaftUse);
		}
		else
		{
			// Override Bloom because is not supported.
			CommonPermutationVector.Set<TonemapperPermutation::FTonemapperBloomDim>(false);
		}

		// Mobile is not currently supporting these.
		CommonPermutationVector.Set<TonemapperPermutation::FTonemapperGrainJitterDim>(false);
		CommonPermutationVector.Set<TonemapperPermutation::FTonemapperSharpenDim>(false);
		MobilePermutationVector.Set<TonemapperPermutation::FCommonDomain>(CommonPermutationVector);

		// We're not supporting every possible permutation, remap the permutation vector to combine common effects.
		return RemapPermutationVector(MobilePermutationVector);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		auto CommonPermutationVector = PermutationVector.Get<TonemapperPermutation::FCommonDomain>();
		if (!TonemapperPermutation::ShouldCompileCommonPermutation(CommonPermutationVector))
		{
			return false;
		}

		// If this permutation vector is remapped at runtime, we can avoid the compile.
		if (RemapPermutationVector(PermutationVector) != PermutationVector)
		{
			return false;
		}

		// Only cache for ES2/3.1 shader platforms, and only compile 32bpp shaders for Android or PC emulation
		return (IsMobilePlatform(Parameters.Platform) && (!PermutationVector.Get<FTonemapper32BPPHDRDim>()))
			|| Parameters.Platform == SP_OPENGL_ES2_ANDROID
			|| (IsMobilePlatform(Parameters.Platform) && IsPCPlatform(Parameters.Platform));
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		//Need to hack in exposure scale for < SM5
		OutEnvironment.SetDefine(TEXT("NO_EYEADAPTATION_EXPOSURE_FIX"), 1);
	}

	/** Default constructor. */
	FPostProcessTonemapPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter ColorScale0;
	FShaderParameter ColorScale1;
	FShaderParameter TexScale;
	FShaderParameter GrainScaleBiasJitter;
	FShaderParameter InverseGamma;
	FShaderParameter TonemapperParams;

	FShaderParameter ColorMatrixR_ColorCurveCd1;
	FShaderParameter ColorMatrixG_ColorCurveCd3Cm3;
	FShaderParameter ColorMatrixB_ColorCurveCm2;
	FShaderParameter ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3;
	FShaderParameter ColorCurve_Ch1_Ch2;
	FShaderParameter ColorShadow_Luma;
	FShaderParameter ColorShadow_Tint1;
	FShaderParameter ColorShadow_Tint2;

	FShaderParameter OverlayColor;
	FShaderParameter FringeIntensity;
	FShaderParameter SRGBAwareTargetParam;
	FShaderParameter DefaultEyeExposure;

	/** Initialization constructor. */
	FPostProcessTonemapPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		ColorScale0.Bind(Initializer.ParameterMap, TEXT("ColorScale0"));
		ColorScale1.Bind(Initializer.ParameterMap, TEXT("ColorScale1"));
		TexScale.Bind(Initializer.ParameterMap, TEXT("TexScale"));
		TonemapperParams.Bind(Initializer.ParameterMap, TEXT("TonemapperParams"));
		GrainScaleBiasJitter.Bind(Initializer.ParameterMap, TEXT("GrainScaleBiasJitter"));
		InverseGamma.Bind(Initializer.ParameterMap,TEXT("InverseGamma"));

		ColorMatrixR_ColorCurveCd1.Bind(Initializer.ParameterMap, TEXT("ColorMatrixR_ColorCurveCd1"));
		ColorMatrixG_ColorCurveCd3Cm3.Bind(Initializer.ParameterMap, TEXT("ColorMatrixG_ColorCurveCd3Cm3"));
		ColorMatrixB_ColorCurveCm2.Bind(Initializer.ParameterMap, TEXT("ColorMatrixB_ColorCurveCm2"));
		ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3.Bind(Initializer.ParameterMap, TEXT("ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3"));
		ColorCurve_Ch1_Ch2.Bind(Initializer.ParameterMap, TEXT("ColorCurve_Ch1_Ch2"));
		ColorShadow_Luma.Bind(Initializer.ParameterMap, TEXT("ColorShadow_Luma"));
		ColorShadow_Tint1.Bind(Initializer.ParameterMap, TEXT("ColorShadow_Tint1"));
		ColorShadow_Tint2.Bind(Initializer.ParameterMap, TEXT("ColorShadow_Tint2"));

		OverlayColor.Bind(Initializer.ParameterMap, TEXT("OverlayColor"));
		FringeIntensity.Bind(Initializer.ParameterMap, TEXT("FringeIntensity"));

		SRGBAwareTargetParam.Bind(Initializer.ParameterMap, TEXT("SRGBAwareTarget"));

		DefaultEyeExposure.Bind(Initializer.ParameterMap, TEXT("DefaultEyeExposure"));
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar  << PostprocessParameter << ColorScale0 << ColorScale1 << InverseGamma
			<< TexScale << GrainScaleBiasJitter << TonemapperParams
			<< ColorMatrixR_ColorCurveCd1 << ColorMatrixG_ColorCurveCd3Cm3 << ColorMatrixB_ColorCurveCm2 << ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3 << ColorCurve_Ch1_Ch2 << ColorShadow_Luma << ColorShadow_Tint1 << ColorShadow_Tint2
			<< OverlayColor
			<< FringeIntensity
			<< SRGBAwareTargetParam
			<< DefaultEyeExposure;

		return bShaderHasOutdatedParameters;
	}

	template <typename TRHICmdList>
	void SetPS(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context, const FPermutationDomain& PermutationVector, bool bSRGBAwareTarget)
	{
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		const FSceneViewFamily& ViewFamily = *(Context.View.Family);

		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		if (PermutationVector.Get<FTonemapper32BPPHDRDim>() && IsMobileHDRMosaic())
		{
			PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		}
		else
		{
			PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		}
			
		SetShaderValue(RHICmdList, ShaderRHI, OverlayColor, Context.View.OverlayColor);
		SetShaderValue(RHICmdList, ShaderRHI, FringeIntensity, fabsf(Settings.SceneFringeIntensity) * 0.01f); // Interpreted as [0-1] percentage

		{
			FLinearColor Col = Settings.SceneColorTint;
			FVector4 ColorScale(Col.R, Col.G, Col.B, 0);
			SetShaderValue(RHICmdList, ShaderRHI, ColorScale0, ColorScale);
		}
		
		{
			FLinearColor Col = FLinearColor::White * Settings.BloomIntensity;
			FVector4 ColorScale(Col.R, Col.G, Col.B, 0);
			SetShaderValue(RHICmdList, ShaderRHI, ColorScale1, ColorScale);
		}

		{
			const FPooledRenderTargetDesc* InputDesc = Context.Pass->GetInputDesc(ePId_Input0);

			// we assume the this pass runs in 1:1 pixel
			FVector2D TexScaleValue = FVector2D(InputDesc->Extent) / FVector2D(Context.View.ViewRect.Size());

			SetShaderValue(RHICmdList, ShaderRHI, TexScale, TexScaleValue);
		}

		{
			float Sharpen = FMath::Clamp(CVarTonemapperSharpen.GetValueOnRenderThread(), 0.0f, 10.0f);

			FVector2D Value(Settings.VignetteIntensity, Sharpen);

			SetShaderValue(RHICmdList, ShaderRHI, TonemapperParams, Value);
		}

		FVector GrainValue;
		GrainPostSettings(&GrainValue, &Settings);
		SetShaderValue(RHICmdList, ShaderRHI, GrainScaleBiasJitter, GrainValue);

		{
			FVector InvDisplayGammaValue;
			InvDisplayGammaValue.X = 1.0f / ViewFamily.RenderTarget->GetDisplayGamma();
			InvDisplayGammaValue.Y = 2.2f / ViewFamily.RenderTarget->GetDisplayGamma();
			InvDisplayGammaValue.Z = 1.0; // Unused on mobile.
			SetShaderValue(RHICmdList, ShaderRHI, InverseGamma, InvDisplayGammaValue);
		}

		{
			FVector4 Constants[8];

			FilmPostSetConstants(Constants, &Context.View.FinalPostProcessSettings,
				/* bMobile = */ true,
				/* UseColorMatrix = */ PermutationVector.Get<FTonemapperColorMatrixDim>(),
				/* UseShadowTint = */ PermutationVector.Get<FTonemapperShadowTintDim>(),
				/* UseContrast = */ PermutationVector.Get<FTonemapperContrastDim>());

			SetShaderValue(RHICmdList, ShaderRHI, ColorMatrixR_ColorCurveCd1, Constants[0]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorMatrixG_ColorCurveCd3Cm3, Constants[1]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorMatrixB_ColorCurveCm2, Constants[2]); 
			SetShaderValue(RHICmdList, ShaderRHI, ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3, Constants[3]); 
			SetShaderValue(RHICmdList, ShaderRHI, ColorCurve_Ch1_Ch2, Constants[4]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorShadow_Luma, Constants[5]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorShadow_Tint1, Constants[6]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorShadow_Tint2, Constants[7]);
		}

		SetShaderValue(RHICmdList, ShaderRHI, SRGBAwareTargetParam, bSRGBAwareTarget ? 1.0f : 0.0f );

		float FixedExposure = FRCPassPostProcessEyeAdaptation::GetFixedExposure(Context.View);
		SetShaderValue(RHICmdList, ShaderRHI, DefaultEyeExposure, FixedExposure);
	}
};

class FPostProcessTonemapVS_ES2 : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FPostProcessTonemapVS_ES2);

	using FPermutationDomain = TShaderPermutationDomain<>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return !IsConsolePlatform(Parameters.Platform);
	}

	FPostProcessTonemapVS_ES2() { }

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderResourceParameter EyeAdaptation;
	FShaderParameter GrainRandomFull;
	FShaderParameter FringeIntensity;
	FShaderParameter ScreenPosToScenePixel;
	bool bUsedFramebufferFetch;

	FPostProcessTonemapVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		GrainRandomFull.Bind(Initializer.ParameterMap, TEXT("GrainRandomFull"));
		FringeIntensity.Bind(Initializer.ParameterMap, TEXT("FringeIntensity"));
		ScreenPosToScenePixel.Bind(Initializer.ParameterMap, TEXT("ScreenPosToScenePixel"));
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		FVector GrainRandomFullValue;
		{
			uint8 FrameIndexMod8 = 0;
			if (Context.View.State)
			{
				FrameIndexMod8 = Context.View.ViewState->GetFrameIndex(8);
			}
			GrainRandomFromFrame(&GrainRandomFullValue, FrameIndexMod8);
		}

		// TODO: Don't use full on mobile with framebuffer fetch.
		GrainRandomFullValue.Z = bUsedFramebufferFetch ? 0.0f : 1.0f;
		SetShaderValue(Context.RHICmdList, ShaderRHI, GrainRandomFull, GrainRandomFullValue);

		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		SetShaderValue(Context.RHICmdList, ShaderRHI, FringeIntensity, fabsf(Settings.SceneFringeIntensity) * 0.01f); // Interpreted as [0-1] percentage


		{
			FIntPoint ViewportOffset = Context.SceneColorViewRect.Min;
			FIntPoint ViewportExtent = Context.SceneColorViewRect.Size();
			FVector4 ScreenPosToScenePixelValue(
				ViewportExtent.X * 0.5f,
				-ViewportExtent.Y * 0.5f,
				ViewportExtent.X * 0.5f - 0.5f + ViewportOffset.X,
				ViewportExtent.Y * 0.5f - 0.5f + ViewportOffset.Y);
			SetShaderValue(Context.RHICmdList, ShaderRHI, ScreenPosToScenePixel, ScreenPosToScenePixelValue);
		}
	}
	
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << GrainRandomFull << FringeIntensity << ScreenPosToScenePixel;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_GLOBAL_SHADER(FPostProcessTonemapVS_ES2, "/Engine/Private/PostProcessTonemap.usf", "MainVS_ES2", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FPostProcessTonemapPS_ES2, "/Engine/Private/PostProcessTonemap.usf", "MainPS_ES2", SF_Pixel);


FRCPassPostProcessTonemapES2::FRCPassPostProcessTonemapES2(const FViewInfo& InView, bool bInUsedFramebufferFetch, bool bInSRGBAwareTarget)
	: bDoScreenPercentageInTonemapper(false)
	, View(InView)
	, bUsedFramebufferFetch(bInUsedFramebufferFetch)
	, bSRGBAwareTarget(bInSRGBAwareTarget)
{ }

void FRCPassPostProcessTonemapES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENTF(Context.RHICmdList, PostProcessTonemapES2, TEXT("Tonemapper(ES2 FramebufferFetch=%s)"), bUsedFramebufferFetch ? TEXT("0") : TEXT("1"));

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	const FSceneViewFamily& ViewFamily = *(View.Family);
	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	const FPooledRenderTargetDesc& OutputDesc = PassOutputs[0].RenderTargetDesc;

	// no upscale if separate ren target is used.
	FIntRect SrcRect = View.ViewRect;
	FIntRect DestRect = bDoScreenPercentageInTonemapper ? View.UnscaledViewRect : View.ViewRect;
	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DstSize = OutputDesc.Extent;

	ERenderTargetLoadAction LoadAction = ERenderTargetLoadAction::ELoad;

	// Set the view family's render target/viewport.
	{
		// clear target when processing first view in case of splitscreen
		const bool bFirstView = (&View == View.Family->Views[0]);
		
		// Full clear to avoid restore
		if ((View.StereoPass == eSSP_FULL && bFirstView) || View.StereoPass == eSSP_LEFT_EYE)
		{
			LoadAction = ERenderTargetLoadAction::EClear;
		}
	}

	FRHIRenderPassInfo RPInfo(DestRenderTarget.TargetableTexture, MakeRenderTargetActions(LoadAction, ERenderTargetStoreAction::EStore));
	Context.RHICmdList.BeginRenderPass(RPInfo, TEXT("TonemapES2"));
	{
		Context.SetViewportAndCallRHI(DestRect);

		auto PermutationVector = FPostProcessTonemapPS_ES2::BuildPermutationVector(View);

		TShaderMapRef<FPostProcessTonemapVS_ES2> VertexShader(Context.GetShaderMap());
		TShaderMapRef<FPostProcessTonemapPS_ES2> PixelShader(Context.GetShaderMap(), PermutationVector);

		{
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

			VertexShader->bUsedFramebufferFetch = bUsedFramebufferFetch;

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

			VertexShader->SetVS(Context);
			PixelShader->SetPS(Context.RHICmdList, Context, PermutationVector, bSRGBAwareTarget);
		}

		DrawRectangle(
			Context.RHICmdList,
			0, 0,
			DstSize.X, DstSize.Y,
			SrcRect.Min.X, SrcRect.Min.Y,
			SrcRect.Width(), SrcRect.Height(),
			DstSize,
			SrcSize,
			*VertexShader,
			EDRF_UseTriangleOptimization);
	}
	Context.RHICmdList.EndRenderPass();
	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessTonemapES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.Format = PF_B8G8R8A8;
	Ret.DebugName = TEXT("Tonemap");
	Ret.ClearValue = FClearValueBinding(FLinearColor::Black);
	if (bDoScreenPercentageInTonemapper)
	{
		Ret.Extent = View.UnscaledViewRect.Max;
	}
	return Ret;
}
