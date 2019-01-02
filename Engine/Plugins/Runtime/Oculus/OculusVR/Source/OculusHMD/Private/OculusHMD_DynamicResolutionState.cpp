// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_DynamicResolutionState.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "SceneView.h"

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FScreenPercentage
//-------------------------------------------------------------------------------------------------

class FScreenPercentage : public ISceneViewFamilyScreenPercentage
{
public:
	FScreenPercentage(const FSceneViewFamily& InViewFamily, float InResolutionFraction, float InResolutionFractionUpperBound);

protected:
	// ISceneViewFamilyScreenPercentage
	virtual float GetPrimaryResolutionFractionUpperBound() const override;
	virtual ISceneViewFamilyScreenPercentage* Fork_GameThread(const class FSceneViewFamily& ForkedViewFamily) const override;
	virtual void ComputePrimaryResolutionFractions_RenderThread(TArray<FSceneViewScreenPercentageConfig>& OutViewScreenPercentageConfigs) const override;

private:
	const FSceneViewFamily& ViewFamily;
	const float ResolutionFraction;
	const float ResolutionFractionUpperBound;
};


//-------------------------------------------------------------------------------------------------
// FScreenPercentage implementation
//-------------------------------------------------------------------------------------------------

FScreenPercentage::FScreenPercentage(const FSceneViewFamily& InViewFamily, float InResolutionFraction, float InResolutionFractionUpperBound)
	: ViewFamily(InViewFamily)
	, ResolutionFraction(InResolutionFraction)
	, ResolutionFractionUpperBound(InResolutionFractionUpperBound)
{
	check(ViewFamily.EngineShowFlags.ScreenPercentage == true);
}

float FScreenPercentage::GetPrimaryResolutionFractionUpperBound() const
{
	return ResolutionFractionUpperBound;
}

ISceneViewFamilyScreenPercentage* FScreenPercentage::Fork_GameThread(const class FSceneViewFamily& ForkedViewFamily) const
{
	return new FScreenPercentage(ForkedViewFamily, ResolutionFraction, ResolutionFractionUpperBound);
}

void FScreenPercentage::ComputePrimaryResolutionFractions_RenderThread(TArray<FSceneViewScreenPercentageConfig>& OutViewScreenPercentageConfigs) const
{
	check(IsInRenderingThread());
	check(ViewFamily.EngineShowFlags.ScreenPercentage == true);

	for (int32 ConfigIter = 0; ConfigIter < OutViewScreenPercentageConfigs.Num(); ++ConfigIter)
	{
		OutViewScreenPercentageConfigs[ConfigIter].PrimaryResolutionFraction = ResolutionFraction;
	}
}


//-------------------------------------------------------------------------------------------------
// FDynamicResolutionState implementation
//-------------------------------------------------------------------------------------------------

FDynamicResolutionState::FDynamicResolutionState(const OculusHMD::FSettingsPtr InSettings)
	: Settings(InSettings)
	, ResolutionFraction(-1.0f)
	, ResolutionFractionUpperBound(-1.0f)
{
	check(Settings.IsValid());
}

void FDynamicResolutionState::ResetHistory()
{
	// Empty - Oculus drives resolution fraction externally
};

bool FDynamicResolutionState::IsSupported() const
{
	return true;
}

void FDynamicResolutionState::SetupMainViewFamily(class FSceneViewFamily& ViewFamily)
{
	check(IsInGameThread());
	check(ViewFamily.EngineShowFlags.ScreenPercentage == true);

	if (ViewFamily.Views.Num() > 0 && IsEnabled())
	{
		// We can assume both eyes have the same fraction
		const FSceneView& View = *ViewFamily.Views[0];
		check(View.UnconstrainedViewRect == View.UnscaledViewRect);

		// Compute desired resolution fraction range
		float MinResolutionFraction = Settings->PixelDensityMin;
		float MaxResolutionFraction = Settings->PixelDensityMax;

		// Clamp resolution fraction to what the renderer can do.
		MinResolutionFraction = FMath::Max(MinResolutionFraction, FSceneViewScreenPercentageConfig::kMinResolutionFraction);
		MaxResolutionFraction = FMath::Min(MaxResolutionFraction, FSceneViewScreenPercentageConfig::kMaxResolutionFraction);

		// Temporal upsample has a smaller resolution fraction range.
		if (View.AntiAliasingMethod == AAM_TemporalAA)
		{
			MinResolutionFraction = FMath::Max(MinResolutionFraction, FSceneViewScreenPercentageConfig::kMinTAAUpsampleResolutionFraction);
			MaxResolutionFraction = FMath::Min(MaxResolutionFraction, FSceneViewScreenPercentageConfig::kMaxTAAUpsampleResolutionFraction);
		}

		ResolutionFraction = FMath::Clamp(Settings->PixelDensity, MinResolutionFraction, MaxResolutionFraction);
		ResolutionFractionUpperBound = MaxResolutionFraction;

		ViewFamily.SetScreenPercentageInterface(new FScreenPercentage(ViewFamily, ResolutionFraction, ResolutionFractionUpperBound));
	}
}

float FDynamicResolutionState::GetResolutionFractionApproximation() const
{
	return ResolutionFraction;
}

float FDynamicResolutionState::GetResolutionFractionUpperBound() const
{
	return ResolutionFractionUpperBound;
}

void FDynamicResolutionState::SetEnabled(bool bEnable)
{
	check(IsInGameThread());
	Settings->Flags.bPixelDensityAdaptive = bEnable;
}

bool FDynamicResolutionState::IsEnabled() const
{
	check(IsInGameThread());
	return Settings->Flags.bPixelDensityAdaptive;
}

void FDynamicResolutionState::ProcessEvent(EDynamicResolutionStateEvent Event)
{
	// Empty - Oculus drives resolution fraction externally
};

} // namespace OculusHMD

#endif //OCULUS_HMD_SUPPORTED_PLATFORMS
