// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SourceEffects/SourceEffectPanner.h"
#include "DSP/Dsp.h"

void FSourceEffectPanner::Init(const FSoundEffectSourceInitData& InitData)
{
	bIsActive = true;
	PanValue = 0.0f;
	NumChannels = InitData.NumSourceChannels;
}

void FSourceEffectPanner::OnPresetChanged()
{
	GET_EFFECT_SETTINGS(SourceEffectPanner);

	// Normalize the panning value to be between 0.0 and 1.0
	PanValue = 0.5f * (1.0f - Settings.Pan);

	// Convert to radians between 0.0 and PI/2
	PanValue *= 0.5f * PI;
}

void FSourceEffectPanner::ProcessAudio(const FSoundEffectSourceInputData& InData, float* OutAudioBufferData)
{
	if (NumChannels != 2)
	{
		FMemory::Memcpy(OutAudioBufferData, InData.InputSourceEffectBufferPtr, sizeof(float)*InData.NumSamples);
	}
	else
	{
		float PanGains[2];
		// Use the "cosine" equal power panning law to compute a smooth pan based off our parameter
		FMath::SinCos(&PanGains[0], &PanGains[1], PanValue);

		// Clamp this to be between 0.0 and 1.0 since SinCos is fast and may have values greater than 1.0 or less than 0.0
		for (int32 i = 0; i < 2; ++i)
		{
			PanGains[i] = FMath::Clamp(PanGains[i], 0.0f, 1.0f);
		}

		for (int32 SampleIndex = 0; SampleIndex < InData.NumSamples; SampleIndex += NumChannels)
		{
			// Then simply scale the pan value output with the channel inputs. Note we have to clamp the 
			// the output of the SinCos since 
			for (int32 i = 0; i < 2; ++i)
			{
				// Simply scale the input sample with the pan value 
				OutAudioBufferData[SampleIndex + i] = PanGains[i] * InData.InputSourceEffectBufferPtr[SampleIndex + i];
			}
		}
	}
}

void USourceEffectPannerPreset::SetSettings(const FSourceEffectPannerSettings& InSettings)
{
	UpdateSettings(InSettings);
}