// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Engine/Scene.h"
#include "HAL/IConsoleManager.h"

void FColorGradingSettings::ExportToPostProcessSettings(FPostProcessSettings* OutPostProcessSettings) const
{
	OutPostProcessSettings->bOverride_ColorSaturation = true;
	OutPostProcessSettings->bOverride_ColorContrast = true;
	OutPostProcessSettings->bOverride_ColorGamma = true;
	OutPostProcessSettings->bOverride_ColorGain = true;
	OutPostProcessSettings->bOverride_ColorOffset = true;

	OutPostProcessSettings->bOverride_ColorSaturationShadows = true;
	OutPostProcessSettings->bOverride_ColorContrastShadows = true;
	OutPostProcessSettings->bOverride_ColorGammaShadows = true;
	OutPostProcessSettings->bOverride_ColorGainShadows = true;
	OutPostProcessSettings->bOverride_ColorOffsetShadows = true;

	OutPostProcessSettings->bOverride_ColorSaturationMidtones = true;
	OutPostProcessSettings->bOverride_ColorContrastMidtones = true;
	OutPostProcessSettings->bOverride_ColorGammaMidtones = true;
	OutPostProcessSettings->bOverride_ColorGainMidtones = true;
	OutPostProcessSettings->bOverride_ColorOffsetMidtones = true;

	OutPostProcessSettings->bOverride_ColorSaturationHighlights = true;
	OutPostProcessSettings->bOverride_ColorContrastHighlights = true;
	OutPostProcessSettings->bOverride_ColorGammaHighlights = true;
	OutPostProcessSettings->bOverride_ColorGainHighlights = true;
	OutPostProcessSettings->bOverride_ColorOffsetHighlights = true;

	OutPostProcessSettings->bOverride_ColorCorrectionShadowsMax = true;
	OutPostProcessSettings->bOverride_ColorCorrectionHighlightsMin = true;

	OutPostProcessSettings->ColorSaturation = Global.Saturation;
	OutPostProcessSettings->ColorContrast = Global.Contrast;
	OutPostProcessSettings->ColorGamma = Global.Gamma;
	OutPostProcessSettings->ColorGain = Global.Gain;
	OutPostProcessSettings->ColorOffset = Global.Offset;

	OutPostProcessSettings->ColorSaturationShadows = Shadows.Saturation;
	OutPostProcessSettings->ColorContrastShadows = Shadows.Contrast;
	OutPostProcessSettings->ColorGammaShadows = Shadows.Gamma;
	OutPostProcessSettings->ColorGainShadows = Shadows.Gain;
	OutPostProcessSettings->ColorOffsetShadows = Shadows.Offset;

	OutPostProcessSettings->ColorSaturationMidtones = Midtones.Saturation;
	OutPostProcessSettings->ColorContrastMidtones = Midtones.Contrast;
	OutPostProcessSettings->ColorGammaMidtones = Midtones.Gamma;
	OutPostProcessSettings->ColorGainMidtones = Midtones.Gain;
	OutPostProcessSettings->ColorOffsetMidtones = Midtones.Offset;

	OutPostProcessSettings->ColorSaturationHighlights = Highlights.Saturation;
	OutPostProcessSettings->ColorContrastHighlights = Highlights.Contrast;
	OutPostProcessSettings->ColorGammaHighlights = Highlights.Gamma;
	OutPostProcessSettings->ColorGainHighlights = Highlights.Gain;
	OutPostProcessSettings->ColorOffsetHighlights = Highlights.Offset;

	OutPostProcessSettings->ColorCorrectionShadowsMax = ShadowsMax;
	OutPostProcessSettings->ColorCorrectionHighlightsMin = HighlightsMin;
}

void FFilmStockSettings::ExportToPostProcessSettings(FPostProcessSettings* OutPostProcessSettings) const
{
	OutPostProcessSettings->bOverride_FilmSlope = true;
	OutPostProcessSettings->bOverride_FilmToe = true;
	OutPostProcessSettings->bOverride_FilmShoulder = true;
	OutPostProcessSettings->bOverride_FilmBlackClip = true;
	OutPostProcessSettings->bOverride_FilmWhiteClip = true;

	OutPostProcessSettings->FilmSlope = Slope;
	OutPostProcessSettings->FilmToe = Toe;
	OutPostProcessSettings->FilmShoulder = Shoulder;
	OutPostProcessSettings->FilmBlackClip = BlackClip;
	OutPostProcessSettings->FilmWhiteClip = WhiteClip;
}

void FGaussianSumBloomSettings::ExportToPostProcessSettings(FPostProcessSettings* OutPostProcessSettings) const
{
	OutPostProcessSettings->bOverride_BloomIntensity = true;
	OutPostProcessSettings->bOverride_BloomThreshold = true;
	OutPostProcessSettings->bOverride_BloomSizeScale = true;
	OutPostProcessSettings->bOverride_Bloom1Tint = true;
	OutPostProcessSettings->bOverride_Bloom1Size = true;
	OutPostProcessSettings->bOverride_Bloom2Tint = true;
	OutPostProcessSettings->bOverride_Bloom2Size = true;
	OutPostProcessSettings->bOverride_Bloom3Tint = true;
	OutPostProcessSettings->bOverride_Bloom3Size = true;
	OutPostProcessSettings->bOverride_Bloom4Tint = true;
	OutPostProcessSettings->bOverride_Bloom4Size = true;
	OutPostProcessSettings->bOverride_Bloom5Tint = true;
	OutPostProcessSettings->bOverride_Bloom5Size = true;
	OutPostProcessSettings->bOverride_Bloom6Tint = true;
	OutPostProcessSettings->bOverride_Bloom6Size = true;

	OutPostProcessSettings->BloomIntensity = Intensity;
	OutPostProcessSettings->BloomThreshold = Threshold;
	OutPostProcessSettings->BloomSizeScale = SizeScale;
	OutPostProcessSettings->Bloom1Tint = Filter1Tint;
	OutPostProcessSettings->Bloom1Size = Filter1Size;
	OutPostProcessSettings->Bloom2Tint = Filter2Tint;
	OutPostProcessSettings->Bloom2Size = Filter2Size;
	OutPostProcessSettings->Bloom3Tint = Filter3Tint;
	OutPostProcessSettings->Bloom3Size = Filter3Size;
	OutPostProcessSettings->Bloom4Tint = Filter4Tint;
	OutPostProcessSettings->Bloom4Size = Filter4Size;
	OutPostProcessSettings->Bloom5Tint = Filter5Tint;
	OutPostProcessSettings->Bloom5Size = Filter5Size;
	OutPostProcessSettings->Bloom6Tint = Filter6Tint;
	OutPostProcessSettings->Bloom6Size = Filter6Size;
}

void FConvolutionBloomSettings::ExportToPostProcessSettings(FPostProcessSettings* OutPostProcessSettings) const
{
	OutPostProcessSettings->bOverride_BloomConvolutionTexture = true;
	OutPostProcessSettings->bOverride_BloomConvolutionSize = true;
	OutPostProcessSettings->bOverride_BloomConvolutionCenterUV = true;
	OutPostProcessSettings->bOverride_BloomConvolutionPreFilterMin = true;
	OutPostProcessSettings->bOverride_BloomConvolutionPreFilterMax = true;
	OutPostProcessSettings->bOverride_BloomConvolutionPreFilterMult = true;
	OutPostProcessSettings->bOverride_BloomConvolutionBufferScale = true;

	OutPostProcessSettings->BloomConvolutionTexture = Texture;
	OutPostProcessSettings->BloomConvolutionSize = Size;
	OutPostProcessSettings->BloomConvolutionCenterUV = CenterUV;
	OutPostProcessSettings->BloomConvolutionPreFilterMin = PreFilterMin;
	OutPostProcessSettings->BloomConvolutionPreFilterMax = PreFilterMax;
	OutPostProcessSettings->BloomConvolutionPreFilterMult = PreFilterMult;
	OutPostProcessSettings->BloomConvolutionBufferScale = BufferScale;
}

void FLensBloomSettings::ExportToPostProcessSettings(FPostProcessSettings* OutPostProcessSettings) const
{
	GaussianSum.ExportToPostProcessSettings(OutPostProcessSettings);
	Convolution.ExportToPostProcessSettings(OutPostProcessSettings);

	OutPostProcessSettings->bOverride_BloomMethod = true;
	OutPostProcessSettings->BloomMethod = Method;
}

void FLensImperfectionSettings::ExportToPostProcessSettings(FPostProcessSettings* OutPostProcessSettings) const
{
	OutPostProcessSettings->bOverride_BloomDirtMask = true;
	OutPostProcessSettings->bOverride_BloomDirtMaskIntensity = true;
	OutPostProcessSettings->bOverride_BloomDirtMaskTint = true;

	OutPostProcessSettings->BloomDirtMask = DirtMask;
	OutPostProcessSettings->BloomDirtMaskIntensity = DirtMaskIntensity;
	OutPostProcessSettings->BloomDirtMaskTint = DirtMaskTint;
}

void FLensSettings::ExportToPostProcessSettings(FPostProcessSettings* OutPostProcessSettings) const
{
	Bloom.ExportToPostProcessSettings(OutPostProcessSettings);
	Imperfections.ExportToPostProcessSettings(OutPostProcessSettings);

	OutPostProcessSettings->bOverride_SceneFringeIntensity = true;
	OutPostProcessSettings->SceneFringeIntensity = ChromaticAberration;
}

FCameraExposureSettings::FCameraExposureSettings()
{
	static const auto VarDefaultAutoExposureExtendDefaultLuminanceRange = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange"));
	const bool bExtendedLuminanceRange = VarDefaultAutoExposureExtendDefaultLuminanceRange->GetValueOnAnyThread() == 1;

	// next value might get overwritten by r.DefaultFeature.AutoExposure.Method
	Method = AEM_Histogram;
	LowPercent = 80.0f;
	HighPercent = 98.3f;

	if (bExtendedLuminanceRange)
	{
		// When this project setting is set, the following values are in EV100.
		MinBrightness = -10.0f;
		MaxBrightness = 20.0f;
		HistogramLogMin = -10.0f;
		HistogramLogMax = 20.0f;
	}
	else
	{
		MinBrightness = 0.03f;
		MaxBrightness = 2.0f;
		HistogramLogMin = -8.0f;
		HistogramLogMax = 4.0f;
	}

	SpeedUp = 3.0f;
	SpeedDown = 1.0f;
	Bias = 0.0f;
	CalibrationConstant	= 16.0;
}

void FCameraExposureSettings::ExportToPostProcessSettings(FPostProcessSettings* OutPostProcessSettings) const
{
	OutPostProcessSettings->bOverride_AutoExposureMethod = true;
	OutPostProcessSettings->bOverride_AutoExposureLowPercent = true;
	OutPostProcessSettings->bOverride_AutoExposureHighPercent = true;
	OutPostProcessSettings->bOverride_AutoExposureMinBrightness = true;
	OutPostProcessSettings->bOverride_AutoExposureMaxBrightness = true;
	OutPostProcessSettings->bOverride_AutoExposureSpeedUp = true;
	OutPostProcessSettings->bOverride_AutoExposureSpeedDown = true;
	OutPostProcessSettings->bOverride_AutoExposureBias = true;
	OutPostProcessSettings->bOverride_HistogramLogMin = true;
	OutPostProcessSettings->bOverride_HistogramLogMax = true;

	OutPostProcessSettings->AutoExposureLowPercent = LowPercent;
	OutPostProcessSettings->AutoExposureHighPercent = HighPercent;
	OutPostProcessSettings->AutoExposureMinBrightness = MinBrightness;
	OutPostProcessSettings->AutoExposureMaxBrightness = MaxBrightness;
	OutPostProcessSettings->AutoExposureSpeedUp = SpeedUp;
	OutPostProcessSettings->AutoExposureSpeedDown = SpeedDown;
	OutPostProcessSettings->AutoExposureBias = Bias;
	OutPostProcessSettings->HistogramLogMin = HistogramLogMin;
	OutPostProcessSettings->HistogramLogMax = HistogramLogMax;
}


// Check there is no divergence between FPostProcessSettings and the smaller settings structures.
#if DO_CHECK && WITH_EDITOR

static void VerifyPostProcessingProperties(
	const FString& PropertyPrefix,
	const TArray<const UStruct*>& NewStructs,
	const TMap<FString, FString>& RenameMap)
{
	const UStruct* LegacyStruct = FPostProcessSettings::StaticStruct();

	TMap<FString, const UProperty*> NewPropertySet;

	// Walk new struct and build list of property name.
	for (const UStruct* NewStruct : NewStructs)
	{
		for (UProperty* Property = NewStruct->PropertyLink; Property; Property = Property->PropertyLinkNext)
		{
			// Make sure there is no duplicate.
			check(!NewPropertySet.Contains(Property->GetNameCPP()));
			NewPropertySet.Add(Property->GetNameCPP(), Property);
		}
	}

	// Walk FPostProcessSettings.
	for (UProperty* Property = LegacyStruct->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		if (!Property->GetNameCPP().StartsWith(PropertyPrefix))
		{
			continue;
		}

		FString OldPropertyName = Property->GetName();
		FString NewPropertyName = OldPropertyName.Mid(PropertyPrefix.Len());

		if (RenameMap.Contains(Property->GetNameCPP()))
		{
			if (RenameMap.FindChecked(Property->GetNameCPP()) == TEXT(""))
			{
				// This property is part of deprecated feature (such as legacy tonemapper).
				check(!NewPropertySet.Contains(NewPropertyName));
				continue;
			}

			NewPropertyName = RenameMap.FindChecked(Property->GetNameCPP());
		}

		if (Property->GetNameCPP().EndsWith(TEXT("_DEPRECATED")))
		{
			check(!NewPropertySet.Contains(NewPropertyName));
		}
		else
		{
			check(Property->SameType(NewPropertySet.FindChecked(NewPropertyName)));
		}
	}
}

static void DoPostProcessSettingsSanityCheck()
{
	{
		TMap<FString, FString> RenameMap;
		RenameMap.Add(TEXT("Bloom1Size"), TEXT("Filter1Size"));
		RenameMap.Add(TEXT("Bloom2Size"), TEXT("Filter2Size"));
		RenameMap.Add(TEXT("Bloom3Size"), TEXT("Filter3Size"));
		RenameMap.Add(TEXT("Bloom4Size"), TEXT("Filter4Size"));
		RenameMap.Add(TEXT("Bloom5Size"), TEXT("Filter5Size"));
		RenameMap.Add(TEXT("Bloom6Size"), TEXT("Filter6Size"));
		RenameMap.Add(TEXT("Bloom1Tint"), TEXT("Filter1Tint"));
		RenameMap.Add(TEXT("Bloom2Tint"), TEXT("Filter2Tint"));
		RenameMap.Add(TEXT("Bloom3Tint"), TEXT("Filter3Tint"));
		RenameMap.Add(TEXT("Bloom4Tint"), TEXT("Filter4Tint"));
		RenameMap.Add(TEXT("Bloom5Tint"), TEXT("Filter5Tint"));
		RenameMap.Add(TEXT("Bloom6Tint"), TEXT("Filter6Tint"));

		RenameMap.Add(TEXT("BloomConvolutionTexture"), TEXT(""));
		RenameMap.Add(TEXT("BloomConvolutionSize"), TEXT(""));
		RenameMap.Add(TEXT("BloomConvolutionCenterUV"), TEXT(""));
		RenameMap.Add(TEXT("BloomConvolutionPreFilterMin"), TEXT(""));
		RenameMap.Add(TEXT("BloomConvolutionPreFilterMax"), TEXT(""));
		RenameMap.Add(TEXT("BloomConvolutionPreFilterMult"), TEXT(""));
		RenameMap.Add(TEXT("BloomConvolutionBufferScale"), TEXT(""));

		VerifyPostProcessingProperties(TEXT("Bloom"),
			TArray<const UStruct*>({
				FGaussianSumBloomSettings::StaticStruct(),
				FLensBloomSettings::StaticStruct(),
				FLensImperfectionSettings::StaticStruct()}),
			RenameMap);
	}
	
	{
		TMap<FString, FString> RenameMap;
		VerifyPostProcessingProperties(TEXT("BloomConvolution"),
			TArray<const UStruct*>({FConvolutionBloomSettings::StaticStruct()}),
			RenameMap);
	}

	{
		TMap<FString, FString> RenameMap;
		VerifyPostProcessingProperties(TEXT("Exposure"),
			TArray<const UStruct*>({
				FCameraExposureSettings::StaticStruct()}),
			RenameMap);
	}

	{
		TMap<FString, FString> RenameMap;
		// Old tonemapper parameters are ignored.
		RenameMap.Add(TEXT("FilmWhitePoint"), TEXT(""));
		RenameMap.Add(TEXT("FilmSaturation"), TEXT(""));
		RenameMap.Add(TEXT("FilmChannelMixerRed"), TEXT(""));
		RenameMap.Add(TEXT("FilmChannelMixerGreen"), TEXT(""));
		RenameMap.Add(TEXT("FilmChannelMixerBlue"), TEXT(""));
		RenameMap.Add(TEXT("FilmContrast"), TEXT(""));
		RenameMap.Add(TEXT("FilmDynamicRange"), TEXT(""));
		RenameMap.Add(TEXT("FilmHealAmount"), TEXT(""));
		RenameMap.Add(TEXT("FilmToeAmount"), TEXT(""));
		RenameMap.Add(TEXT("FilmShadowTint"), TEXT(""));
		RenameMap.Add(TEXT("FilmShadowTintBlend"), TEXT(""));
		RenameMap.Add(TEXT("FilmShadowTintAmount"), TEXT(""));
		VerifyPostProcessingProperties(TEXT("Film"),
			TArray<const UStruct*>({FFilmStockSettings::StaticStruct()}),
			RenameMap);
	}
}

#endif // DO_CHECK

FPostProcessSettings::FPostProcessSettings()
{
	// to set all bOverride_.. by default to false
	FMemory::Memzero(this, sizeof(FPostProcessSettings));

	WhiteTemp = 6500.0f;
	WhiteTint = 0.0f;

	// Color Correction controls
	ColorSaturation = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorContrast = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorGamma = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorGain = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorOffset = FVector4(0.0f, 0.0f, 0.0f, 0.0f);

	ColorSaturationShadows = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorContrastShadows = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorGammaShadows = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorGainShadows = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorOffsetShadows = FVector4(0.0f, 0.0f, 0.0f, 0.0f);

	ColorSaturationMidtones = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorContrastMidtones = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorGammaMidtones = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorGainMidtones = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorOffsetMidtones = FVector4(0.f, 0.0f, 0.0f, 0.0f);

	ColorSaturationHighlights = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorContrastHighlights = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorGammaHighlights = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorGainHighlights = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorOffsetHighlights = FVector4(0.0f, 0.0f, 0.0f, 0.0f);

	ColorCorrectionShadowsMax = 0.09f;
	ColorCorrectionHighlightsMin = 0.5f;

	BlueCorrection = 0.6f;
	ExpandGamut = 1.0f;

	// default values:
	FilmWhitePoint = FLinearColor(1.0f, 1.0f, 1.0f);
	FilmSaturation = 1.0f;
	FilmChannelMixerRed = FLinearColor(1.0f, 0.0f, 0.0f);
	FilmChannelMixerGreen = FLinearColor(0.0f, 1.0f, 0.0f);
	FilmChannelMixerBlue = FLinearColor(0.0f, 0.0f, 1.0f);
	FilmContrast = 0.03f;
	FilmDynamicRange = 4.0f;
	FilmHealAmount = 1.0f;
	FilmToeAmount = 1.0f;
	FilmShadowTint = FLinearColor(1.0f, 1.0f, 1.0f);
	FilmShadowTintBlend = 0.5;
	FilmShadowTintAmount = 0.0;

	// ACES settings
	FilmSlope = 0.88f;
	FilmToe = 0.55f;
	FilmShoulder = 0.26f;
	FilmBlackClip = 0.0f;
	FilmWhiteClip = 0.04f;

	SceneColorTint = FLinearColor(1, 1, 1);
	SceneFringeIntensity = 0.0f;
	BloomMethod = BM_SOG;
	// next value might get overwritten by r.DefaultFeature.Bloom
	BloomIntensity = 0.675f;
	BloomThreshold = -1.0f;
	// default is 4 to maintain old settings after fixing something that caused a factor of 4
	BloomSizeScale = 4.0;
	Bloom1Tint = FLinearColor(0.3465f, 0.3465f, 0.3465f);
	Bloom1Size = 0.3f;
	Bloom2Tint = FLinearColor(0.138f, 0.138f, 0.138f);
	Bloom2Size = 1.0f;
	Bloom3Tint = FLinearColor(0.1176f, 0.1176f, 0.1176f);
	Bloom3Size = 2.0f;
	Bloom4Tint = FLinearColor(0.066f, 0.066f, 0.066f);
	Bloom4Size = 10.0f;
	Bloom5Tint = FLinearColor(0.066f, 0.066f, 0.066f);
	Bloom5Size = 30.0f;
	Bloom6Tint = FLinearColor(0.061f, 0.061f, 0.061f);
	Bloom6Size = 64.0f;
	BloomConvolutionSize = 1.f;
	BloomConvolutionCenterUV = FVector2D(0.5f, 0.5f);
#if WITH_EDITORONLY_DATA
	BloomConvolutionPreFilter_DEPRECATED = FVector(-1.f, -1.f, -1.f);
#endif
	BloomConvolutionPreFilterMin = 7.f;
	BloomConvolutionPreFilterMax = 15000.f;
	BloomConvolutionPreFilterMult = 15.f;
	BloomConvolutionBufferScale = 0.133f;
	BloomDirtMaskIntensity = 0.0f;
	BloomDirtMaskTint = FLinearColor(0.5f, 0.5f, 0.5f);
	AmbientCubemapIntensity = 1.0f;
	AmbientCubemapTint = FLinearColor(1, 1, 1);
	LPVIntensity = 1.0f;
	LPVSize = 5312.0f;
	LPVSecondaryOcclusionIntensity = 0.0f;
	LPVSecondaryBounceIntensity = 0.0f;
	LPVVplInjectionBias = 0.64f;
	LPVGeometryVolumeBias = 0.384f;
	LPVEmissiveInjectionIntensity = 1.0f;
	// next value might get overwritten by r.DefaultFeature.AutoExposure.Method
	CameraShutterSpeed = 60.f;
	CameraISO = 100.f;
	AutoExposureCalibrationConstant = 16.f;
	AutoExposureMethod = AEM_Histogram;
	AutoExposureLowPercent = 80.0f;
	AutoExposureHighPercent = 98.3f;

	// next value might get overwritten by r.DefaultFeature.AutoExposure
	static const auto VarDefaultAutoExposureExtendDefaultLuminanceRange = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange"));
	if (VarDefaultAutoExposureExtendDefaultLuminanceRange->GetValueOnAnyThread() != 0)
	{
		// When this project setting is set, the following values are in EV100.
		AutoExposureMinBrightness = -10.0f;
		AutoExposureMaxBrightness = 20.0f;
		HistogramLogMin = -10.0f;
		HistogramLogMax = 20.0f;
	}
	else
	{
		AutoExposureMinBrightness = 0.03f;
		AutoExposureMaxBrightness = 2.0f;
		HistogramLogMin = -8.0f;
		HistogramLogMax = 4.0f;
	}

	AutoExposureBias = 0.0f;
	AutoExposureSpeedUp = 3.0f;
	AutoExposureSpeedDown = 1.0f;
	LPVDirectionalOcclusionIntensity = 0.0f;
	LPVDirectionalOcclusionRadius = 8.0f;
	LPVDiffuseOcclusionExponent = 1.0f;
	LPVSpecularOcclusionExponent = 7.0f;
	LPVDiffuseOcclusionIntensity = 1.0f;
	LPVSpecularOcclusionIntensity = 1.0f;
	LPVFadeRange = 0.0f;
	LPVDirectionalOcclusionFadeRange = 0.0f;

	// next value might get overwritten by r.DefaultFeature.LensFlare
	LensFlareIntensity = 1.0f;
	LensFlareTint = FLinearColor(1.0f, 1.0f, 1.0f);
	LensFlareBokehSize = 3.0f;
	LensFlareThreshold = 8.0f;
	VignetteIntensity = 0.4f;
	GrainIntensity = 0.0f;
	GrainJitter = 0.0f;
	// next value might get overwritten by r.DefaultFeature.AmbientOcclusion
	AmbientOcclusionIntensity = .5f;
	// next value might get overwritten by r.DefaultFeature.AmbientOcclusionStaticFraction
	AmbientOcclusionStaticFraction = 1.0f;
	AmbientOcclusionRadius = 200.0f;
	AmbientOcclusionDistance_DEPRECATED = 80.0f;
	AmbientOcclusionFadeDistance = 8000.0f;
	AmbientOcclusionFadeRadius = 5000.0f;
	AmbientOcclusionPower = 2.0f;
	AmbientOcclusionBias = 3.0f;
	AmbientOcclusionQuality = 50.0f;
	AmbientOcclusionMipBlend = 0.6f;
	AmbientOcclusionMipScale = 1.7f;
	AmbientOcclusionMipThreshold = 0.01f;
	AmbientOcclusionRadiusInWS = false;
	IndirectLightingColor = FLinearColor(1.0f, 1.0f, 1.0f);
	IndirectLightingIntensity = 1.0f;
	ColorGradingIntensity = 1.0f;
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	DepthOfFieldFocalDistance = 1000.0f;
	DepthOfFieldFstop = 4.0f;
	DepthOfFieldMinFstop = 1.2f;
	DepthOfFieldBladeCount = FPostProcessSettings::kDefaultDepthOfFieldBladeCount;
	DepthOfFieldSensorWidth = 24.576f;			// APS-C
	DepthOfFieldDepthBlurAmount = 1.0f;
	DepthOfFieldDepthBlurRadius = 0.0f;
	DepthOfFieldFocalRegion = 0.0f;
	DepthOfFieldNearTransitionRegion = 300.0f;
	DepthOfFieldFarTransitionRegion = 500.0f;
	DepthOfFieldScale = 0.0f;
	DepthOfFieldMaxBokehSize = 15.0f;
	DepthOfFieldNearBlurSize = 15.0f;
	DepthOfFieldFarBlurSize = 15.0f;
	DepthOfFieldOcclusion = 0.4f;
	DepthOfFieldColorThreshold = 1.0f;
	DepthOfFieldSizeThreshold = 0.08f;
	DepthOfFieldSkyFocusDistance = 0.0f;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	// 200 should be enough even for extreme aspect ratios to give the default no effect
	DepthOfFieldVignetteSize = 200.0f;
	LensFlareTints[0] = FLinearColor(1.0f, 0.8f, 0.4f, 0.6f);
	LensFlareTints[1] = FLinearColor(1.0f, 1.0f, 0.6f, 0.53f);
	LensFlareTints[2] = FLinearColor(0.8f, 0.8f, 1.0f, 0.46f);
	LensFlareTints[3] = FLinearColor(0.5f, 1.0f, 0.4f, 0.39f);
	LensFlareTints[4] = FLinearColor(0.5f, 0.8f, 1.0f, 0.31f);
	LensFlareTints[5] = FLinearColor(0.9f, 1.0f, 0.8f, 0.27f);
	LensFlareTints[6] = FLinearColor(1.0f, 0.8f, 0.4f, 0.22f);
	LensFlareTints[7] = FLinearColor(0.9f, 0.7f, 0.7f, 0.15f);
	// next value might get overwritten by r.DefaultFeature.MotionBlur
	MotionBlurAmount = 0.5f;
	MotionBlurMax = 5.0f;
	MotionBlurPerObjectSize = 0.5f;
	ScreenPercentage = 100.0f;
	ScreenSpaceReflectionIntensity = 100.0f;
	ScreenSpaceReflectionQuality = 50.0f;
	ScreenSpaceReflectionMaxRoughness = 0.6f;
	bMobileHQGaussian = false;

#if DO_CHECK && WITH_EDITOR
	static bool bCheckedMembers = false;
	if (!bCheckedMembers)
	{
		bCheckedMembers = true;
		DoPostProcessSettingsSanityCheck();
	}
#endif // DO_CHECK
}

FPostProcessSettings::FPostProcessSettings(const FPostProcessSettings& Settings)
	: bOverride_WhiteTemp(Settings.bOverride_WhiteTemp)
	, bOverride_WhiteTint(Settings.bOverride_WhiteTint)
	, bOverride_ColorSaturation(Settings.bOverride_ColorSaturation)
	, bOverride_ColorContrast(Settings.bOverride_ColorContrast)
	, bOverride_ColorGamma(Settings.bOverride_ColorGamma)
	, bOverride_ColorGain(Settings.bOverride_ColorGain)
	, bOverride_ColorOffset(Settings.bOverride_ColorOffset)
	, bOverride_ColorSaturationShadows(Settings.bOverride_ColorSaturationShadows)
	, bOverride_ColorContrastShadows(Settings.bOverride_ColorContrastShadows)
	, bOverride_ColorGammaShadows(Settings.bOverride_ColorGammaShadows)
	, bOverride_ColorGainShadows(Settings.bOverride_ColorGainShadows)
	, bOverride_ColorOffsetShadows(Settings.bOverride_ColorOffsetShadows)
	, bOverride_ColorSaturationMidtones(Settings.bOverride_ColorSaturationMidtones)
	, bOverride_ColorContrastMidtones(Settings.bOverride_ColorContrastMidtones)
	, bOverride_ColorGammaMidtones(Settings.bOverride_ColorGammaMidtones)
	, bOverride_ColorGainMidtones(Settings.bOverride_ColorGainMidtones)
	, bOverride_ColorOffsetMidtones(Settings.bOverride_ColorOffsetMidtones)
	, bOverride_ColorSaturationHighlights(Settings.bOverride_ColorSaturationHighlights)
	, bOverride_ColorContrastHighlights(Settings.bOverride_ColorContrastHighlights)
	, bOverride_ColorGammaHighlights(Settings.bOverride_ColorGammaHighlights)
	, bOverride_ColorGainHighlights(Settings.bOverride_ColorGainHighlights)
	, bOverride_ColorOffsetHighlights(Settings.bOverride_ColorOffsetHighlights)
	, bOverride_ColorCorrectionShadowsMax(Settings.bOverride_ColorCorrectionShadowsMax)
	, bOverride_ColorCorrectionHighlightsMin(Settings.bOverride_ColorCorrectionHighlightsMin)
	, bOverride_BlueCorrection(Settings.bOverride_BlueCorrection)
	, bOverride_ExpandGamut(Settings.bOverride_ExpandGamut)
	, bOverride_FilmWhitePoint(Settings.bOverride_FilmWhitePoint)
	, bOverride_FilmSaturation(Settings.bOverride_FilmSaturation)
	, bOverride_FilmChannelMixerRed(Settings.bOverride_FilmChannelMixerRed)
	, bOverride_FilmChannelMixerGreen(Settings.bOverride_FilmChannelMixerGreen)
	, bOverride_FilmChannelMixerBlue(Settings.bOverride_FilmChannelMixerBlue)
	, bOverride_FilmContrast(Settings.bOverride_FilmContrast)
	, bOverride_FilmDynamicRange(Settings.bOverride_FilmDynamicRange)
	, bOverride_FilmHealAmount(Settings.bOverride_FilmHealAmount)
	, bOverride_FilmToeAmount(Settings.bOverride_FilmToeAmount)
	, bOverride_FilmShadowTint(Settings.bOverride_FilmShadowTint)
	, bOverride_FilmShadowTintBlend(Settings.bOverride_FilmShadowTintBlend)
	, bOverride_FilmShadowTintAmount(Settings.bOverride_FilmShadowTintAmount)
	, bOverride_FilmSlope(Settings.bOverride_FilmSlope)
	, bOverride_FilmToe(Settings.bOverride_FilmToe)
	, bOverride_FilmShoulder(Settings.bOverride_FilmShoulder)
	, bOverride_FilmBlackClip(Settings.bOverride_FilmBlackClip)
	, bOverride_FilmWhiteClip(Settings.bOverride_FilmWhiteClip)
	, bOverride_SceneColorTint(Settings.bOverride_SceneColorTint)
	, bOverride_SceneFringeIntensity(Settings.bOverride_SceneFringeIntensity)
	, bOverride_ChromaticAberrationStartOffset(Settings.bOverride_ChromaticAberrationStartOffset)
	, bOverride_AmbientCubemapTint(Settings.bOverride_AmbientCubemapTint)
	, bOverride_AmbientCubemapIntensity(Settings.bOverride_AmbientCubemapIntensity)
	, bOverride_BloomMethod(Settings.bOverride_BloomMethod)
	, bOverride_BloomIntensity(Settings.bOverride_BloomIntensity)
	, bOverride_BloomThreshold(Settings.bOverride_BloomThreshold)
	, bOverride_Bloom1Tint(Settings.bOverride_Bloom1Tint)
	, bOverride_Bloom1Size(Settings.bOverride_Bloom1Size)
	, bOverride_Bloom2Size(Settings.bOverride_Bloom2Size)
	, bOverride_Bloom2Tint(Settings.bOverride_Bloom2Tint)
	, bOverride_Bloom3Tint(Settings.bOverride_Bloom3Tint)
	, bOverride_Bloom3Size(Settings.bOverride_Bloom3Size)
	, bOverride_Bloom4Tint(Settings.bOverride_Bloom4Tint)
	, bOverride_Bloom4Size(Settings.bOverride_Bloom4Size)
	, bOverride_Bloom5Tint(Settings.bOverride_Bloom5Tint)
	, bOverride_Bloom5Size(Settings.bOverride_Bloom5Size)
	, bOverride_Bloom6Tint(Settings.bOverride_Bloom6Tint)
	, bOverride_Bloom6Size(Settings.bOverride_Bloom6Size)
	, bOverride_BloomSizeScale(Settings.bOverride_BloomSizeScale)
	, bOverride_BloomConvolutionTexture(Settings.bOverride_BloomConvolutionTexture)
	, bOverride_BloomConvolutionSize(Settings.bOverride_BloomConvolutionSize)
	, bOverride_BloomConvolutionCenterUV(Settings.bOverride_BloomConvolutionCenterUV)
	//, bOverride_BloomConvolutionPreFilter_DEPRECATED(Settings.bOverride_BloomConvolutionPreFilter_DEPRECATED)
	, bOverride_BloomConvolutionPreFilterMin(Settings.bOverride_BloomConvolutionPreFilterMin)
	, bOverride_BloomConvolutionPreFilterMax(Settings.bOverride_BloomConvolutionPreFilterMax)
	, bOverride_BloomConvolutionPreFilterMult(Settings.bOverride_BloomConvolutionPreFilterMult)
	, bOverride_BloomConvolutionBufferScale(Settings.bOverride_BloomConvolutionBufferScale)
	, bOverride_BloomDirtMaskIntensity(Settings.bOverride_BloomDirtMaskIntensity)
	, bOverride_BloomDirtMaskTint(Settings.bOverride_BloomDirtMaskTint)
	, bOverride_BloomDirtMask(Settings.bOverride_BloomDirtMask)
	, bOverride_CameraShutterSpeed(Settings.bOverride_CameraShutterSpeed)
	, bOverride_CameraISO(Settings.bOverride_CameraISO)
	, bOverride_AutoExposureMethod(Settings.bOverride_AutoExposureMethod)
	, bOverride_AutoExposureLowPercent(Settings.bOverride_AutoExposureLowPercent)
	, bOverride_AutoExposureHighPercent(Settings.bOverride_AutoExposureHighPercent)
	, bOverride_AutoExposureMinBrightness(Settings.bOverride_AutoExposureMinBrightness)
	, bOverride_AutoExposureMaxBrightness(Settings.bOverride_AutoExposureMaxBrightness)
	, bOverride_AutoExposureCalibrationConstant(Settings.bOverride_AutoExposureCalibrationConstant)
	, bOverride_AutoExposureSpeedUp(Settings.bOverride_AutoExposureSpeedUp)
	, bOverride_AutoExposureSpeedDown(Settings.bOverride_AutoExposureSpeedDown)
	, bOverride_AutoExposureBias(Settings.bOverride_AutoExposureBias)
	, bOverride_HistogramLogMin(Settings.bOverride_HistogramLogMin)
	, bOverride_HistogramLogMax(Settings.bOverride_HistogramLogMax)
	, bOverride_LensFlareIntensity(Settings.bOverride_LensFlareIntensity)
	, bOverride_LensFlareTint(Settings.bOverride_LensFlareTint)
	, bOverride_LensFlareTints(Settings.bOverride_LensFlareTints)
	, bOverride_LensFlareBokehSize(Settings.bOverride_LensFlareBokehSize)
	, bOverride_LensFlareBokehShape(Settings.bOverride_LensFlareBokehShape)
	, bOverride_LensFlareThreshold(Settings.bOverride_LensFlareThreshold)
	, bOverride_VignetteIntensity(Settings.bOverride_VignetteIntensity)
	, bOverride_GrainIntensity(Settings.bOverride_GrainIntensity)
	, bOverride_GrainJitter(Settings.bOverride_GrainJitter)
	, bOverride_AmbientOcclusionIntensity(Settings.bOverride_AmbientOcclusionIntensity)
	, bOverride_AmbientOcclusionStaticFraction(Settings.bOverride_AmbientOcclusionStaticFraction)
	, bOverride_AmbientOcclusionRadius(Settings.bOverride_AmbientOcclusionRadius)
	, bOverride_AmbientOcclusionFadeDistance(Settings.bOverride_AmbientOcclusionFadeDistance)
	, bOverride_AmbientOcclusionFadeRadius(Settings.bOverride_AmbientOcclusionFadeRadius)
	//, bOverride_AmbientOcclusionDistance_DEPRECATED(Settings.bOverride_AmbientOcclusionDistance_DEPRECATED)
	, bOverride_AmbientOcclusionRadiusInWS(Settings.bOverride_AmbientOcclusionRadiusInWS)
	, bOverride_AmbientOcclusionPower(Settings.bOverride_AmbientOcclusionPower)
	, bOverride_AmbientOcclusionBias(Settings.bOverride_AmbientOcclusionBias)
	, bOverride_AmbientOcclusionQuality(Settings.bOverride_AmbientOcclusionQuality)
	, bOverride_AmbientOcclusionMipBlend(Settings.bOverride_AmbientOcclusionMipBlend)
	, bOverride_AmbientOcclusionMipScale(Settings.bOverride_AmbientOcclusionMipScale)
	, bOverride_AmbientOcclusionMipThreshold(Settings.bOverride_AmbientOcclusionMipThreshold)
	, bOverride_LPVIntensity(Settings.bOverride_LPVIntensity)
	, bOverride_LPVDirectionalOcclusionIntensity(Settings.bOverride_LPVDirectionalOcclusionIntensity)
	, bOverride_LPVDirectionalOcclusionRadius(Settings.bOverride_LPVDirectionalOcclusionRadius)
	, bOverride_LPVDiffuseOcclusionExponent(Settings.bOverride_WhiteTemp)
	, bOverride_LPVSpecularOcclusionExponent(Settings.bOverride_LPVSpecularOcclusionExponent)
	, bOverride_LPVDiffuseOcclusionIntensity(Settings.bOverride_LPVDiffuseOcclusionIntensity)
	, bOverride_LPVSpecularOcclusionIntensity(Settings.bOverride_LPVSpecularOcclusionIntensity)
	, bOverride_LPVSize(Settings.bOverride_LPVSize)
	, bOverride_LPVSecondaryOcclusionIntensity(Settings.bOverride_LPVSecondaryOcclusionIntensity)
	, bOverride_LPVSecondaryBounceIntensity(Settings.bOverride_LPVSecondaryBounceIntensity)
	, bOverride_LPVGeometryVolumeBias(Settings.bOverride_LPVGeometryVolumeBias)
	, bOverride_LPVVplInjectionBias(Settings.bOverride_LPVVplInjectionBias)
	, bOverride_LPVEmissiveInjectionIntensity(Settings.bOverride_LPVEmissiveInjectionIntensity)
	, bOverride_LPVFadeRange(Settings.bOverride_LPVFadeRange)
	, bOverride_LPVDirectionalOcclusionFadeRange(Settings.bOverride_LPVDirectionalOcclusionFadeRange)
	, bOverride_IndirectLightingColor(Settings.bOverride_IndirectLightingColor)
	, bOverride_IndirectLightingIntensity(Settings.bOverride_IndirectLightingIntensity)
	, bOverride_ColorGradingIntensity(Settings.bOverride_ColorGradingIntensity)
	, bOverride_ColorGradingLUT(Settings.bOverride_ColorGradingLUT)
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	, bOverride_DepthOfFieldFocalDistance(Settings.bOverride_DepthOfFieldFocalDistance)
	, bOverride_DepthOfFieldFstop(Settings.bOverride_DepthOfFieldFstop)
	, bOverride_DepthOfFieldMinFstop(Settings.bOverride_DepthOfFieldMinFstop)
	, bOverride_DepthOfFieldBladeCount(Settings.bOverride_DepthOfFieldBladeCount)
	, bOverride_DepthOfFieldSensorWidth(Settings.bOverride_DepthOfFieldSensorWidth)
	, bOverride_DepthOfFieldDepthBlurRadius(Settings.bOverride_DepthOfFieldDepthBlurRadius)
	, bOverride_DepthOfFieldDepthBlurAmount(Settings.bOverride_DepthOfFieldDepthBlurAmount)
	, bOverride_DepthOfFieldFocalRegion(Settings.bOverride_DepthOfFieldFocalRegion)
	, bOverride_DepthOfFieldNearTransitionRegion(Settings.bOverride_DepthOfFieldNearTransitionRegion)
	, bOverride_DepthOfFieldFarTransitionRegion(Settings.bOverride_DepthOfFieldFarTransitionRegion)
	, bOverride_DepthOfFieldScale(Settings.bOverride_DepthOfFieldScale)
	, bOverride_DepthOfFieldMaxBokehSize(Settings.bOverride_DepthOfFieldMaxBokehSize)
	, bOverride_DepthOfFieldNearBlurSize(Settings.bOverride_DepthOfFieldNearBlurSize)
	, bOverride_DepthOfFieldFarBlurSize(Settings.bOverride_DepthOfFieldFarBlurSize)
	, bOverride_DepthOfFieldMethod(Settings.bOverride_DepthOfFieldMethod)
	, bOverride_MobileHQGaussian(Settings.bOverride_MobileHQGaussian)
	, bOverride_DepthOfFieldBokehShape(Settings.bOverride_DepthOfFieldBokehShape)
	, bOverride_DepthOfFieldOcclusion(Settings.bOverride_DepthOfFieldOcclusion)
	, bOverride_DepthOfFieldColorThreshold(Settings.bOverride_DepthOfFieldColorThreshold)
	, bOverride_DepthOfFieldSizeThreshold(Settings.bOverride_DepthOfFieldSizeThreshold)
	, bOverride_DepthOfFieldSkyFocusDistance(Settings.bOverride_DepthOfFieldSkyFocusDistance)
	, bOverride_DepthOfFieldVignetteSize(Settings.bOverride_DepthOfFieldVignetteSize)
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	, bOverride_MotionBlurAmount(Settings.bOverride_MotionBlurAmount)
	, bOverride_MotionBlurMax(Settings.bOverride_MotionBlurMax)
	, bOverride_MotionBlurPerObjectSize(Settings.bOverride_MotionBlurPerObjectSize)
	, bOverride_ScreenPercentage(Settings.bOverride_ScreenPercentage)
	, bOverride_ScreenSpaceReflectionIntensity(Settings.bOverride_ScreenSpaceReflectionIntensity)
	, bOverride_ScreenSpaceReflectionQuality(Settings.bOverride_ScreenSpaceReflectionQuality)
	, bOverride_ScreenSpaceReflectionMaxRoughness(Settings.bOverride_ScreenSpaceReflectionMaxRoughness)
	, bOverride_ScreenSpaceReflectionRoughnessScale(Settings.bOverride_ScreenSpaceReflectionRoughnessScale)

	, bMobileHQGaussian(Settings.bMobileHQGaussian)
	, BloomMethod(Settings.BloomMethod)
	, AutoExposureMethod(Settings.AutoExposureMethod)
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	, DepthOfFieldMethod(Settings.DepthOfFieldMethod)
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	, WhiteTemp(Settings.WhiteTemp)
	, WhiteTint(Settings.WhiteTint)
	, ColorSaturation(Settings.ColorSaturation)
	, ColorContrast(Settings.ColorContrast)
	, ColorGamma(Settings.ColorGamma)
	, ColorGain(Settings.ColorGain)
	, ColorOffset(Settings.ColorOffset)
	, ColorSaturationShadows(Settings.ColorSaturationShadows)
	, ColorContrastShadows(Settings.ColorContrastShadows)
	, ColorGammaShadows(Settings.ColorGammaShadows)
	, ColorGainShadows(Settings.ColorGainShadows)
	, ColorOffsetShadows(Settings.ColorOffsetShadows)
	, ColorSaturationMidtones(Settings.ColorSaturationMidtones)
	, ColorContrastMidtones(Settings.ColorContrastMidtones)
	, ColorGammaMidtones(Settings.ColorGammaMidtones)
	, ColorGainMidtones(Settings.ColorGainMidtones)
	, ColorOffsetMidtones(Settings.ColorOffsetMidtones)
	, ColorSaturationHighlights(Settings.ColorSaturationHighlights)
	, ColorContrastHighlights(Settings.ColorContrastHighlights)
	, ColorGammaHighlights(Settings.ColorGammaHighlights)
	, ColorGainHighlights(Settings.ColorGainHighlights)
	, ColorOffsetHighlights(Settings.ColorOffsetHighlights)
	, ColorCorrectionHighlightsMin(Settings.ColorCorrectionHighlightsMin)
	, ColorCorrectionShadowsMax(Settings.ColorCorrectionShadowsMax)
	, BlueCorrection(Settings.BlueCorrection)
	, ExpandGamut(Settings.ExpandGamut)
	, FilmSlope(Settings.FilmSlope)
	, FilmToe(Settings.FilmToe)
	, FilmShoulder(Settings.FilmShoulder)
	, FilmBlackClip(Settings.FilmBlackClip)
	, FilmWhiteClip(Settings.FilmWhiteClip)
	, FilmWhitePoint(Settings.FilmWhitePoint)
	, FilmShadowTint(Settings.FilmShadowTint)
	, FilmShadowTintBlend(Settings.FilmShadowTintBlend)
	, FilmShadowTintAmount(Settings.FilmShadowTintAmount)
	, FilmSaturation(Settings.FilmSaturation)
	, FilmChannelMixerRed(Settings.FilmChannelMixerRed)
	, FilmChannelMixerGreen(Settings.FilmChannelMixerGreen)
	, FilmChannelMixerBlue(Settings.FilmChannelMixerBlue)
	, FilmContrast(Settings.FilmContrast)
	, FilmToeAmount(Settings.FilmToeAmount)
	, FilmHealAmount(Settings.FilmHealAmount)
	, FilmDynamicRange(Settings.FilmDynamicRange)
	, SceneColorTint(Settings.SceneColorTint)
	, SceneFringeIntensity(Settings.SceneFringeIntensity)
	, ChromaticAberrationStartOffset(Settings.ChromaticAberrationStartOffset)
	, BloomIntensity(Settings.BloomIntensity)
	, BloomThreshold(Settings.BloomThreshold)
	, BloomSizeScale(Settings.BloomSizeScale)
	, Bloom1Size(Settings.Bloom1Size)
	, Bloom2Size(Settings.Bloom2Size)
	, Bloom3Size(Settings.Bloom3Size)
	, Bloom4Size(Settings.Bloom4Size)
	, Bloom5Size(Settings.Bloom5Size)
	, Bloom6Size(Settings.Bloom6Size)
	, Bloom1Tint(Settings.Bloom1Tint)
	, Bloom2Tint(Settings.Bloom2Tint)
	, Bloom3Tint(Settings.Bloom3Tint)
	, Bloom4Tint(Settings.Bloom4Tint)
	, Bloom5Tint(Settings.Bloom5Tint)
	, Bloom6Tint(Settings.Bloom6Tint)
	, BloomConvolutionSize(Settings.BloomConvolutionSize)
	, BloomConvolutionTexture(Settings.BloomConvolutionTexture)
	, BloomConvolutionCenterUV(Settings.BloomConvolutionCenterUV)
	//, BloomConvolutionPreFilter_DEPRECATED(Settings.BloomConvolutionPreFilter_DEPRECATED)
	, BloomConvolutionPreFilterMin(Settings.BloomConvolutionPreFilterMin)
	, BloomConvolutionPreFilterMax(Settings.BloomConvolutionPreFilterMax)
	, BloomConvolutionPreFilterMult(Settings.BloomConvolutionPreFilterMult)
	, BloomConvolutionBufferScale(Settings.BloomConvolutionBufferScale)
	, BloomDirtMask(Settings.BloomDirtMask)
	, BloomDirtMaskIntensity(Settings.BloomDirtMaskIntensity)
	, BloomDirtMaskTint(Settings.BloomDirtMaskTint)
	, AmbientCubemapTint(Settings.AmbientCubemapTint)
	, AmbientCubemapIntensity(Settings.AmbientCubemapIntensity)
	, AmbientCubemap(Settings.AmbientCubemap)
	, CameraShutterSpeed(Settings.CameraShutterSpeed)
	, CameraISO(Settings.CameraISO)
	, DepthOfFieldFstop(Settings.DepthOfFieldFstop)
	, DepthOfFieldMinFstop(Settings.DepthOfFieldMinFstop)
	, DepthOfFieldBladeCount(Settings.DepthOfFieldBladeCount)
	, AutoExposureBias(Settings.AutoExposureBias)
	, AutoExposureLowPercent(Settings.AutoExposureLowPercent)
	, AutoExposureHighPercent(Settings.AutoExposureHighPercent)
	, AutoExposureMinBrightness(Settings.AutoExposureMinBrightness)
	, AutoExposureMaxBrightness(Settings.AutoExposureMaxBrightness)
	, AutoExposureSpeedUp(Settings.AutoExposureSpeedUp)
	, AutoExposureSpeedDown(Settings.AutoExposureSpeedDown)
	, HistogramLogMin(Settings.HistogramLogMin)
	, HistogramLogMax(Settings.HistogramLogMax)
	, AutoExposureCalibrationConstant(Settings.AutoExposureCalibrationConstant)
	, LensFlareIntensity(Settings.LensFlareIntensity)
	, LensFlareTint(Settings.LensFlareTint)
	, LensFlareBokehSize(Settings.LensFlareBokehSize)
	, LensFlareThreshold(Settings.LensFlareThreshold)
	, LensFlareBokehShape(Settings.LensFlareBokehShape)
	, VignetteIntensity(Settings.VignetteIntensity)
	, GrainJitter(Settings.GrainJitter)
	, GrainIntensity(Settings.GrainIntensity)
	, AmbientOcclusionIntensity(Settings.AmbientOcclusionIntensity)
	, AmbientOcclusionStaticFraction(Settings.AmbientOcclusionStaticFraction)
	, AmbientOcclusionRadius(Settings.AmbientOcclusionRadius)
	, AmbientOcclusionRadiusInWS(Settings.AmbientOcclusionRadiusInWS)
	, AmbientOcclusionFadeDistance(Settings.AmbientOcclusionFadeDistance)
	, AmbientOcclusionFadeRadius(Settings.AmbientOcclusionFadeRadius)
	//, AmbientOcclusionDistance_DEPRECATED(Settings.AmbientOcclusionDistance_DEPRECATED)
	, AmbientOcclusionPower(Settings.AmbientOcclusionPower)
	, AmbientOcclusionBias(Settings.AmbientOcclusionBias)
	, AmbientOcclusionQuality(Settings.AmbientOcclusionQuality)
	, AmbientOcclusionMipBlend(Settings.AmbientOcclusionMipBlend)
	, AmbientOcclusionMipScale(Settings.AmbientOcclusionMipScale)
	, AmbientOcclusionMipThreshold(Settings.AmbientOcclusionMipThreshold)
	, IndirectLightingColor(Settings.IndirectLightingColor)
	, IndirectLightingIntensity(Settings.IndirectLightingIntensity)
	, ColorGradingIntensity(Settings.ColorGradingIntensity)
	, ColorGradingLUT(Settings.ColorGradingLUT)
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	, DepthOfFieldSensorWidth(Settings.DepthOfFieldSensorWidth)
	, DepthOfFieldFocalDistance(Settings.DepthOfFieldFocalDistance)
	, DepthOfFieldDepthBlurAmount(Settings.DepthOfFieldDepthBlurAmount)
	, DepthOfFieldDepthBlurRadius(Settings.DepthOfFieldDepthBlurRadius)
	, DepthOfFieldFocalRegion(Settings.DepthOfFieldFocalRegion)
	, DepthOfFieldNearTransitionRegion(Settings.DepthOfFieldNearTransitionRegion)
	, DepthOfFieldFarTransitionRegion(Settings.DepthOfFieldFarTransitionRegion)
	, DepthOfFieldScale(Settings.DepthOfFieldScale)
	, DepthOfFieldMaxBokehSize(Settings.DepthOfFieldMaxBokehSize)
	, DepthOfFieldNearBlurSize(Settings.DepthOfFieldNearBlurSize)
	, DepthOfFieldFarBlurSize(Settings.DepthOfFieldFarBlurSize)
	, DepthOfFieldOcclusion(Settings.DepthOfFieldOcclusion)
	, DepthOfFieldBokehShape(Settings.DepthOfFieldBokehShape)
	, DepthOfFieldColorThreshold(Settings.DepthOfFieldColorThreshold)
	, DepthOfFieldSizeThreshold(Settings.DepthOfFieldSizeThreshold)
	, DepthOfFieldSkyFocusDistance(Settings.DepthOfFieldSkyFocusDistance)
	, DepthOfFieldVignetteSize(Settings.DepthOfFieldVignetteSize)
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	, MotionBlurAmount(Settings.MotionBlurAmount)
	, MotionBlurMax(Settings.MotionBlurMax)
	, MotionBlurPerObjectSize(Settings.MotionBlurPerObjectSize)
	, LPVIntensity(Settings.LPVIntensity)
	, LPVVplInjectionBias(Settings.LPVVplInjectionBias)
	, LPVSize(Settings.LPVSize)
	, LPVSecondaryOcclusionIntensity(Settings.LPVSecondaryOcclusionIntensity)
	, LPVSecondaryBounceIntensity(Settings.LPVSecondaryBounceIntensity)
	, LPVGeometryVolumeBias(Settings.LPVGeometryVolumeBias)
	, LPVEmissiveInjectionIntensity(Settings.LPVEmissiveInjectionIntensity)
	, LPVDirectionalOcclusionIntensity(Settings.LPVDirectionalOcclusionIntensity)
	, LPVDirectionalOcclusionRadius(Settings.LPVDirectionalOcclusionRadius)
	, LPVDiffuseOcclusionExponent(Settings.WhiteTemp)
	, LPVSpecularOcclusionExponent(Settings.LPVSpecularOcclusionExponent)
	, LPVDiffuseOcclusionIntensity(Settings.LPVDiffuseOcclusionIntensity)
	, LPVSpecularOcclusionIntensity(Settings.LPVSpecularOcclusionIntensity)
	, ScreenSpaceReflectionIntensity(Settings.ScreenSpaceReflectionIntensity)
	, ScreenSpaceReflectionQuality(Settings.ScreenSpaceReflectionQuality)
	, ScreenSpaceReflectionMaxRoughness(Settings.ScreenSpaceReflectionMaxRoughness)
	, LPVFadeRange(Settings.LPVFadeRange)
	, LPVDirectionalOcclusionFadeRange(Settings.LPVDirectionalOcclusionFadeRange)
	, ScreenPercentage(Settings.ScreenPercentage)

	, WeightedBlendables(Settings.WeightedBlendables)
	//, Blendables_DEPRECATED(Settings.Blendables_DEPRECATED)
{
	for (int32 i = 0; i < ARRAY_COUNT(LensFlareTints); i++)
		LensFlareTints[i] = Settings.LensFlareTints[i];
}

UScene::UScene(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}
