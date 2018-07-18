// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sound/SoundEffectSource.h"
#include "DSP/Delay.h"
#include "SourceEffectSimpleDelay.generated.h"

USTRUCT(BlueprintType)
struct SYNTHESIS_API FSourceEffectSimpleDelaySettings
{
	GENERATED_USTRUCT_BODY()

	// Speed of sound in meters per second when using distance-based delay
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.01", UIMin = "0.01", UIMax = "10000.0"))
	float SpeedOfSound;

	// Delay amount in seconds
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "2.0", UIMin = "0.0", UIMax = "2.0"))
	float DelayAmount;

	// Amount to feedback into the delay line (because why not)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Feedback;

	// Whether or not to delay the audio based on the distance to the listener or use manual delay
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	uint32 bDelayBasedOnDistance : 1;

	FSourceEffectSimpleDelaySettings()
		: SpeedOfSound(343.0f)
		, DelayAmount(0.0f)
		, Feedback(0.0f)
		, bDelayBasedOnDistance(true)
	{}
};

class SYNTHESIS_API FSourceEffectSimpleDelay : public FSoundEffectSource
{
public:
	// Called on an audio effect at initialization on main thread before audio processing begins.
	virtual void Init(const FSoundEffectSourceInitData& InSampleRate) override;
	
	// Called when an audio effect preset is changed
	virtual void OnPresetChanged() override;

	// Process the input block of audio. Called on audio thread.
	virtual void ProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData) override;

protected:
	// 2 Delay lines, one for each channel
	Audio::FDelay DelayLines[2];
	float FeedbackSamples[2];
	FSourceEffectSimpleDelaySettings SettingsCopy;
};

UCLASS(ClassGroup = AudioSourceEffect, meta = (BlueprintSpawnableComponent))
class SYNTHESIS_API USourceEffectSimpleDelayPreset : public USoundEffectSourcePreset
{
	GENERATED_BODY()

public:
	EFFECT_PRESET_METHODS(SourceEffectSimpleDelay)

	virtual FColor GetPresetColor() const override { return FColor(100.0f, 165.0f, 85.0f); }

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects")
	void SetSettings(const FSourceEffectSimpleDelaySettings& InSettings);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Effects")
	FSourceEffectSimpleDelaySettings Settings;
};
