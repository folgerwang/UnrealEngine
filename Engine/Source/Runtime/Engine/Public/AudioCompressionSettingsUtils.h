// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IAudioExtensionPlugin.h"
#include "AudioPluginUtilities.h"
#include "AudioCompressionSettings.h"

class ENGINE_API FPlatformCompressionUtilities
{
public:
	// Returns the Duration Threshold for the current platform if it is overridden, -1.0f otherwise.
	static float GetCompressionDurationForCurrentPlatform();

	// Returns the sample rate for a given platform,
	static float GetTargetSampleRateForPlatform(ESoundwaveSampleRateSettings InSampleRateLevel = ESoundwaveSampleRateSettings::High, EAudioPlatform SpecificPlatform = AudioPluginUtilities::CurrentPlatform);

	static int32 GetMaxPreloadedBranchesForCurrentPlatform();

	static int32 GetQualityIndexOverrideForCurrentPlatform();

	static const FPlatformAudioCookOverrides* GetCookOverridesForCurrentPlatform();

private:
	static const FPlatformRuntimeAudioCompressionOverrides* GetRuntimeCompressionOverridesForCurrentPlatform();
	
};
