// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "AudioMixerDevice.h"

// Forward declaration
class FStreamer;

class FAudioEncoder final : public ISubmixBufferListener
{
public:
	FAudioEncoder(FStreamer& Outer);
	~FAudioEncoder();
	FAudioEncoder(FAudioEncoder& Other) = delete;
	FAudioEncoder& operator=(FAudioEncoder& Other) = delete;
	void Init();
private:

	// ISubmixBufferListener interface
	virtual void OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate, double AudioClock) override;

	FStreamer& Outer;
	bool bInitialized;
	bool bFormatChecked;

	// Used as scratchpad to convert the floats to pcm-16bits, to avoid
	// reallocating memory
	TArray<int16> PCM16;
};
