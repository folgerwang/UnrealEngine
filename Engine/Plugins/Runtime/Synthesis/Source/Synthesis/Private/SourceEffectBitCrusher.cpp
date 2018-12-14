// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SourceEffects/SourceEffectBitCrusher.h"

void FSourceEffectBitCrusher::Init(const FSoundEffectSourceInitData& InitData)
{
	bIsActive = true;
	BitCrusher.Init(InitData.SampleRate, InitData.NumSourceChannels);
}

void FSourceEffectBitCrusher::OnPresetChanged()
{
	GET_EFFECT_SETTINGS(SourceEffectBitCrusher);

	BitCrusher.SetBitDepthCrush(Settings.CrushedBits);
	BitCrusher.SetSampleRateCrush(Settings.CrushedSampleRate);
}

void FSourceEffectBitCrusher::ProcessAudio(const FSoundEffectSourceInputData& InData, float* OutAudioBufferData)
{
	BitCrusher.ProcessAudio(InData.InputSourceEffectBufferPtr, InData.NumSamples, OutAudioBufferData);
}

void USourceEffectBitCrusherPreset::SetSettings(const FSourceEffectBitCrusherSettings& InSettings)
{
	UpdateSettings(InSettings);
}