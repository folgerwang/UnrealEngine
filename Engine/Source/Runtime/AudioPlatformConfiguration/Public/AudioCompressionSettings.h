// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "AudioCompressionSettings.generated.h"

UENUM()
enum class ESoundwaveSampleRateSettings : uint8
{
	Max,
	High,
	Medium,
	Low,
	Min,
	// Use this setting to resample soundwaves to the device's sample rate to avoid having to perform sample rate conversion at runtime.
	MatchDevice
};


/************************************************************************/
/* FPlatformAudioCookOverrides                                          */
/* This struct is used for settings used during the cook to a target    */
/* platform (platform-specific compression quality and resampling, etc.)*/
/************************************************************************/
struct FPlatformAudioCookOverrides
{
	bool bResampleForDevice;

	// Mapping of which sample rates are used for each sample rate quality for a specific platform.
	TMap<ESoundwaveSampleRateSettings, float> PlatformSampleRates;

	// Scales all compression qualities when cooking to this platform. For example, 0.5 will halve all compression qualities, and 1.0 will leave them unchanged.
	float CompressionQualityModifier;

	FPlatformAudioCookOverrides()
		: bResampleForDevice(false)
		, CompressionQualityModifier(1.0f)
	{
		PlatformSampleRates.Add(ESoundwaveSampleRateSettings::Max, 48000);
		PlatformSampleRates.Add(ESoundwaveSampleRateSettings::High, 32000);
		PlatformSampleRates.Add(ESoundwaveSampleRateSettings::Medium, 24000);
		PlatformSampleRates.Add(ESoundwaveSampleRateSettings::Low, 12000);
		PlatformSampleRates.Add(ESoundwaveSampleRateSettings::Min, 8000);
	}

	// This is used to invalidate compressed audio for a specific platform

	static void GetHashSuffix(const FPlatformAudioCookOverrides* InOverrides, FString& OutSuffix)
	{
		if (InOverrides == nullptr)
		{
			return;
		}

		int32 CompressionQualityHash = FMath::FloorToInt(InOverrides->CompressionQualityModifier * 100.0f);
		OutSuffix.AppendInt(CompressionQualityHash);

		int32 ResampleBoolHash = (int32)InOverrides->bResampleForDevice;
		OutSuffix.AppendInt(ResampleBoolHash);

		for (auto& SampleRateQuality : InOverrides->PlatformSampleRates)
		{
			int32 SampleRateHash = FMath::FloorToInt(SampleRateQuality.Value / 1000.0f);
			OutSuffix.AppendInt(SampleRateHash);
		}
	}

};

USTRUCT()
struct AUDIOPLATFORMCONFIGURATION_API FPlatformRuntimeAudioCompressionOverrides
{
	GENERATED_USTRUCT_BODY()

	// Set this to true to override Sound Groups and use the Duration Threshold value to determine whether a sound should be fully decompressed during initial loading.
	UPROPERTY(EditAnywhere, Category = "DecompressOnLoad")
	bool bOverrideCompressionTimes;
	
	// When Override Compression Times is set to true, any sound under this threshold (in seconds) will be fully decompressed on load.
	// Otherwise the first chunk of this sound is cached at load and the rest is decompressed in real time.
	UPROPERTY(EditAnywhere, Category = "DecompressOnLoad")
	float DurationThreshold;

	// On this platform, any random nodes on Sound Cues will automatically only preload this number of branches and dispose of any others
	// on load. This can drastically cut down on memory usage.
	UPROPERTY(EditAnywhere, Category = "SoundCueLoading", meta = (DisplayName = "Maximum Branches on Random SoundCue nodes", ClampMin = "1"))
	int32 MaxNumRandomBranches;

	// On this platform, use the specified quality at this index to override the quality used for SoundCues on this platform
	UPROPERTY(EditAnywhere, Category = "SoundCueLoading", meta = (DisplayName = "Quality Index for Sound Cues", ClampMin = "-1", ClampMax = "50"))
	int32 SoundCueQualityIndex;

	FPlatformRuntimeAudioCompressionOverrides();

	// Get singleton containing default settings for compression.
	static FPlatformRuntimeAudioCompressionOverrides* GetDefaultCompressionOverrides()
	{
		if (DefaultCompressionOverrides == nullptr)
		{
			DefaultCompressionOverrides = new FPlatformRuntimeAudioCompressionOverrides();
		}

		return DefaultCompressionOverrides;
	}

private:
	static FPlatformRuntimeAudioCompressionOverrides* DefaultCompressionOverrides;
};