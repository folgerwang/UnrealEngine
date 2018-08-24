// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AudioCompressionSettingsUtils.h"
#include "AudioCompressionSettings.h"

#define ENABLE_PLATFORM_COMPRESSION_OVERRIDES 1

#if PLATFORM_ANDROID && !PLATFORM_LUMIN && ENABLE_PLATFORM_COMPRESSION_OVERRIDES
#include "AndroidRuntimeSettings.h"
#endif

#if PLATFORM_IOS && ENABLE_PLATFORM_COMPRESSION_OVERRIDES
#include "IOSRuntimeSettings.h"
#endif

#if PLATFORM_SWITCH && ENABLE_PLATFORM_COMPRESSION_OVERRIDES
#include "SwitchRuntimeSettings.h"
#endif

#include "Misc/ConfigCacheIni.h"

const FPlatformRuntimeAudioCompressionOverrides* FPlatformCompressionUtilities::GetRuntimeCompressionOverridesForCurrentPlatform()
{
#if PLATFORM_ANDROID && !PLATFORM_LUMIN && ENABLE_PLATFORM_COMPRESSION_OVERRIDES
	static const UAndroidRuntimeSettings* Settings = GetDefault<UAndroidRuntimeSettings>();
	if (Settings)
	{
		return &(Settings->CompressionOverrides);
	}

#elif PLATFORM_IOS && ENABLE_PLATFORM_COMPRESSION_OVERRIDES
	static const UIOSRuntimeSettings* Settings = GetDefault<UIOSRuntimeSettings>();

	if (Settings)
	{
		return &(Settings->CompressionOverrides);
	}

#elif PLATFORM_SWITCH && ENABLE_PLATFORM_COMPRESSION_OVERRIDES
	static const USwitchRuntimeSettings* Settings = GetDefault<USwitchRuntimeSettings>();

	if (Settings)
	{
		return &(Settings->CompressionOverrides);
	}

#endif // PLATFORM_ANDROID && !PLATFORM_LUMIN
	return nullptr;
}

void CacheCurrentPlatformAudioCookOverrides(FPlatformAudioCookOverrides& OutOverrides)
{
#if PLATFORM_ANDROID && !PLATFORM_LUMIN
	const TCHAR* CategoryName = TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings");
#elif PLATFORM_IOS
	const TCHAR* CategoryName = TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings");
#elif PLATFORM_SWITCH
	const TCHAR* CategoryName = TEXT("/Script/SwitchRuntimeSettings.SwitchRuntimeSettings");
#else
	const TCHAR* CategoryName = TEXT("");
#endif

	GConfig->GetBool(CategoryName, TEXT("bResampleForDevice"), OutOverrides.bResampleForDevice, GEngineIni);

	GConfig->GetFloat(CategoryName, TEXT("CompressionQualityModifier"), OutOverrides.CompressionQualityModifier, GEngineIni);

	//Cache sample rate map.
	OutOverrides.PlatformSampleRates.Reset();

	float RetrievedSampleRate = -1.0f;

	GConfig->GetFloat(CategoryName, TEXT("MaxSampleRate"), RetrievedSampleRate, GEngineIni);
	OutOverrides.PlatformSampleRates.Add(ESoundwaveSampleRateSettings::Max, RetrievedSampleRate);

	RetrievedSampleRate = -1.0f;

	GConfig->GetFloat(CategoryName, TEXT("HighSampleRate"), RetrievedSampleRate, GEngineIni);
	OutOverrides.PlatformSampleRates.Add(ESoundwaveSampleRateSettings::High, RetrievedSampleRate);

	RetrievedSampleRate = -1.0f;

	GConfig->GetFloat(CategoryName, TEXT("MedSampleRate"), RetrievedSampleRate, GEngineIni);
	OutOverrides.PlatformSampleRates.Add(ESoundwaveSampleRateSettings::Medium, RetrievedSampleRate);

	RetrievedSampleRate = -1.0f;

	GConfig->GetFloat(CategoryName, TEXT("LowSampleRate"), RetrievedSampleRate, GEngineIni);
	OutOverrides.PlatformSampleRates.Add(ESoundwaveSampleRateSettings::Low, RetrievedSampleRate);

	RetrievedSampleRate = -1.0f;

	GConfig->GetFloat(CategoryName, TEXT("MinSampleRate"), RetrievedSampleRate, GEngineIni);
	OutOverrides.PlatformSampleRates.Add(ESoundwaveSampleRateSettings::Min, RetrievedSampleRate);
}

#if (PLATFORM_ANDROID && !PLATFORM_LUMIN) || PLATFORM_IOS || PLATFORM_SWITCH
static FPlatformAudioCookOverrides OutOverrides = FPlatformAudioCookOverrides();
#endif


const FPlatformAudioCookOverrides* FPlatformCompressionUtilities::GetCookOverridesForCurrentPlatform()
{
#if (PLATFORM_ANDROID && !PLATFORM_LUMIN) || PLATFORM_IOS || PLATFORM_SWITCH
	static bool bCachedCookOverrides = false;
	if (!bCachedCookOverrides)
	{
		CacheCurrentPlatformAudioCookOverrides(OutOverrides);
		bCachedCookOverrides = true;
	}
	return &OutOverrides;
#else 
	return nullptr;
#endif
}

float FPlatformCompressionUtilities::GetCompressionDurationForCurrentPlatform()
{
	float Threshold = -1.0f;

	const FPlatformRuntimeAudioCompressionOverrides* Settings = GetRuntimeCompressionOverridesForCurrentPlatform();
	if (Settings && Settings->bOverrideCompressionTimes)
	{
		Threshold = Settings->DurationThreshold;
	}

	return Threshold;
}

float FPlatformCompressionUtilities::GetTargetSampleRateForPlatform(ESoundwaveSampleRateSettings InSampleRateLevel /*= ESoundwaveSampleRateSettings::High*/, EAudioPlatform SpecificPlatform /*= AudioPluginUtilities::CurrentPlatform*/)
{
	float SampleRate = -1.0f;
	const FPlatformAudioCookOverrides* Settings = GetCookOverridesForCurrentPlatform();
	if (Settings && Settings->bResampleForDevice)
	{
		const float* FoundSampleRate = Settings->PlatformSampleRates.Find(InSampleRateLevel);

		if (FoundSampleRate)
		{
			SampleRate = *FoundSampleRate;
		}
		else
		{
			ensureMsgf(false, TEXT("Warning: Could not find a matching sample rate for this platform. Check your project settings."));
		}
	}

	return SampleRate;
}

int32 FPlatformCompressionUtilities::GetMaxPreloadedBranchesForCurrentPlatform()
{
	const FPlatformRuntimeAudioCompressionOverrides* Settings = GetRuntimeCompressionOverridesForCurrentPlatform();

	if (Settings)
	{
		return FMath::Max(Settings->MaxNumRandomBranches, 0);
	}
	else
	{
		return 0;
	}
}

int32 FPlatformCompressionUtilities::GetQualityIndexOverrideForCurrentPlatform()
{
	const FPlatformRuntimeAudioCompressionOverrides* Settings = GetRuntimeCompressionOverridesForCurrentPlatform();

	if (Settings)
	{
		return Settings->SoundCueQualityIndex;
	}
	else
	{
		return INDEX_NONE;
	}
}
