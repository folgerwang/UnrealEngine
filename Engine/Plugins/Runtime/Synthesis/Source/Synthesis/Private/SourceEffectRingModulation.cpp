// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SourceEffects/SourceEffectRingModulation.h"

void FSourceEffectRingModulation::Init(const FSoundEffectSourceInitData& InitData)
{
	bIsActive = true;
	RingModulation.Init(InitData.SampleRate, InitData.NumSourceChannels);
}

void FSourceEffectRingModulation::OnPresetChanged()
{
	GET_EFFECT_SETTINGS(SourceEffectRingModulation);

	switch (Settings.ModulatorType)
	{
		default:
		case ERingModulatorTypeSourceEffect::Sine:
			RingModulation.SetModulatorWaveType(Audio::EOsc::Sine);
			break;

		case ERingModulatorTypeSourceEffect::Saw:
			RingModulation.SetModulatorWaveType(Audio::EOsc::Saw);
			break;

		case ERingModulatorTypeSourceEffect::Triangle:
			RingModulation.SetModulatorWaveType(Audio::EOsc::Triangle);
			break;

		case ERingModulatorTypeSourceEffect::Square:
			RingModulation.SetModulatorWaveType(Audio::EOsc::Square);
			break;
	}

	RingModulation.SetModulationDepth(Settings.Depth);
	RingModulation.SetModulationFrequency(Settings.Frequency);
	RingModulation.SetDryLevel(Settings.DryLevel);
	RingModulation.SetWetLevel(Settings.WetLevel);
}

void FSourceEffectRingModulation::ProcessAudio(const FSoundEffectSourceInputData& InData, float* OutAudioBufferData)
{
	RingModulation.ProcessAudio(InData.InputSourceEffectBufferPtr, InData.NumSamples, OutAudioBufferData);
}

void USourceEffectRingModulationPreset::SetSettings(const FSourceEffectRingModulationSettings& InSettings)
{
	UpdateSettings(InSettings);
}
