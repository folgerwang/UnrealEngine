// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SourceEffects/SourceEffectChorus.h"

void FSourceEffectChorus::Init(const FSoundEffectSourceInitData& InitData)
{
	bIsActive = true;

	Chorus.Init(InitData.SampleRate, InitData.NumSourceChannels, 2.0f, 64);
}

void FSourceEffectChorus::OnPresetChanged()
{
	GET_EFFECT_SETTINGS(SourceEffectChorus);

	Chorus.SetDepth(Audio::EChorusDelays::Left, Settings.Depth);
	Chorus.SetDepth(Audio::EChorusDelays::Center, Settings.Depth);
	Chorus.SetDepth(Audio::EChorusDelays::Right, Settings.Depth);

	Chorus.SetFeedback(Audio::EChorusDelays::Left, Settings.Feedback);
	Chorus.SetFeedback(Audio::EChorusDelays::Center, Settings.Feedback);
	Chorus.SetFeedback(Audio::EChorusDelays::Right, Settings.Feedback);

	Chorus.SetFrequency(Audio::EChorusDelays::Left, Settings.Frequency);
	Chorus.SetFrequency(Audio::EChorusDelays::Center, Settings.Frequency);
	Chorus.SetFrequency(Audio::EChorusDelays::Right, Settings.Frequency);

	Chorus.SetWetLevel(Settings.WetLevel);
	Chorus.SetDryLevel(Settings.DryLevel);
	Chorus.SetSpread(Settings.Spread);
}

void FSourceEffectChorus::ProcessAudio(const FSoundEffectSourceInputData& InData, float* OutAudioBufferData)
{
	Chorus.ProcessAudio(InData.InputSourceEffectBufferPtr, InData.NumSamples, OutAudioBufferData);
}

void USourceEffectChorusPreset::SetSettings(const FSourceEffectChorusSettings& InSettings)
{
	UpdateSettings(InSettings);
}