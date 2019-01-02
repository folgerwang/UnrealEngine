// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SourceEffects/SourceEffectFoldbackDistortion.h"

void FSourceEffectFoldbackDistortion::Init(const FSoundEffectSourceInitData& InitData)
{
	bIsActive = true;
	FoldbackDistortion.Init(InitData.SampleRate, InitData.NumSourceChannels);
}

void FSourceEffectFoldbackDistortion::OnPresetChanged()
{
	GET_EFFECT_SETTINGS(SourceEffectFoldbackDistortion);

	FoldbackDistortion.SetInputGainDb(Settings.InputGainDb);
	FoldbackDistortion.SetThresholdDb(Settings.ThresholdDb);
	FoldbackDistortion.SetOutputGainDb(Settings.OutputGainDb);
}

void FSourceEffectFoldbackDistortion::ProcessAudio(const FSoundEffectSourceInputData& InData, float* OutAudioBufferData)
{
	FoldbackDistortion.ProcessAudio(InData.InputSourceEffectBufferPtr, InData.NumSamples, OutAudioBufferData);
}

void USourceEffectFoldbackDistortionPreset::SetSettings(const FSourceEffectFoldbackDistortionSettings& InSettings)
{
	UpdateSettings(InSettings);
}