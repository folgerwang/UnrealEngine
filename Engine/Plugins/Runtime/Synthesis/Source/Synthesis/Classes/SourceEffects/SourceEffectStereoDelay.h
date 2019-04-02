// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DSP/DelayStereo.h"
#include "Sound/SoundEffectSource.h"
#include "SourceEffectStereoDelay.generated.h"

UENUM(BlueprintType)
enum class EStereoDelaySourceEffect : uint8
{
	// Left input mixes with left delay line output and feeds to left output. 
	// Right input mixes with right delay line output and feeds to right output.
	Normal = 0,

	// Left input mixes right delay line output and feeds to right output.
	// Right input mixes with left delay line output and feeds to left output.
	Cross,

	// Left input mixes with left delay line output and feeds to right output.
	// Right input mixes with right delay line output and feeds to left output.
	PingPong,

	Count UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct SYNTHESIS_API FSourceEffectStereoDelaySettings
{
	GENERATED_USTRUCT_BODY()

	// What mode to set the stereo delay effect.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	EStereoDelaySourceEffect DelayMode;

	// The base amount of delay in the left and right channels of the delay line.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "2000.0", UIMin = "0.0", UIMax = "2000.0"))
	float DelayTimeMsec;

	// The amount of audio to feedback into the delay line once the delay has been tapped.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Feedback;

	// Delay spread for left and right channels. Allows left and right channels to have differential delay amounts. Useful for stereo channel decorrelation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "-1.0", ClampMax = "1.0", UIMin = "-1.0", UIMax = "1.0"))
	float DelayRatio;

	// The amount of delay effect to mix with the dry input signal into the effect.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float WetLevel;

	FSourceEffectStereoDelaySettings()
		: DelayMode(EStereoDelaySourceEffect::PingPong)
		, DelayTimeMsec(500.0f)
		, Feedback(0.1f)
		, DelayRatio(0.2f)
		, WetLevel(0.4f)
	{}
};

class SYNTHESIS_API FSourceEffectStereoDelay : public FSoundEffectSource
{
public:
	// Called on an audio effect at initialization on main thread before audio processing begins.
	virtual void Init(const FSoundEffectSourceInitData& InitData) override;
	
	// Called when an audio effect preset is changed
	virtual void OnPresetChanged() override;

	// Process the input block of audio. Called on audio thread.
	virtual void ProcessAudio(const FSoundEffectSourceInputData& InData, float* OutAudioBufferData) override;

protected:
	Audio::FDelayStereo DelayStereo;
};



UCLASS(ClassGroup = AudioSourceEffect, meta = (BlueprintSpawnableComponent))
class SYNTHESIS_API USourceEffectStereoDelayPreset : public USoundEffectSourcePreset
{
	GENERATED_BODY()

public:

	EFFECT_PRESET_METHODS(SourceEffectStereoDelay)

	virtual FColor GetPresetColor() const override { return FColor(23.0f, 121.0f, 225.0f); }

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects")
	void SetSettings(const FSourceEffectStereoDelaySettings& InSettings);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Effects")
	FSourceEffectStereoDelaySettings Settings;
};
