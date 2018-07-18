// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SourceEffects/SourceEffectSimpleDelay.h"

void FSourceEffectSimpleDelay::Init(const FSoundEffectSourceInitData& InitData)
{
	bIsActive = true;

	for (int32 i = 0; i < 2; ++i)
	{
		DelayLines[i].Init(InitData.SampleRate, 2.0f);
		FeedbackSamples[i] = 0.0f;
	}
}

void FSourceEffectSimpleDelay::OnPresetChanged()
{
	GET_EFFECT_SETTINGS(SourceEffectSimpleDelay);

	SettingsCopy = Settings;

	// If we are manually setting the delay, lets set it now on the delay line
	if (!SettingsCopy.bDelayBasedOnDistance)
	{
		for (int32 i = 0; i < 2; ++i)
		{
			DelayLines[i].SetEasedDelayMsec(SettingsCopy.DelayAmount * 1000.0f);
		}
	}
}

void FSourceEffectSimpleDelay::ProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData)
{
	const int32 NumChannels = InData.AudioFrame.Num();

	if (SettingsCopy.bDelayBasedOnDistance)
	{
		const float DistanceMeters = InData.Distance * 100.0f;
		const float DelayAmountMsec = 1000.0f * DistanceMeters / SettingsCopy.SpeedOfSound;

		for (int32 i = 0; i < NumChannels; ++i)
		{
			DelayLines[i].SetDelayMsec(DelayAmountMsec * 1000.0f);
		}
	}

	const float* InAudioBufferFrame = InData.AudioFrame.GetData();
	float* OutAudioBufferFrame = OutData.AudioFrame.GetData();

	for (int32 i = 0; i < NumChannels; ++i)
	{
		OutAudioBufferFrame[i] = DelayLines[i].ProcessAudio(InAudioBufferFrame[i] + FeedbackSamples[i] * SettingsCopy.Feedback);
		FeedbackSamples[i] = OutAudioBufferFrame[i];
	}
}

void USourceEffectSimpleDelayPreset::SetSettings(const FSourceEffectSimpleDelaySettings& InSettings)
{
	UpdateSettings(InSettings);
}