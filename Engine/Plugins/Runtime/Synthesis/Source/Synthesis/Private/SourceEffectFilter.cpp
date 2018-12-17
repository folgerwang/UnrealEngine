// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SourceEffects/SourceEffectFilter.h"


FSourceEffectFilter::FSourceEffectFilter()
	: CurrentFilter(nullptr)
	, CutoffFrequency(8000.0f)
	, FilterQ(2.0f)
	, CircuitType(ESourceEffectFilterCircuit::StateVariable)
	, FilterType(ESourceEffectFilterType::LowPass)
{
	FMemory::Memzero(AudioInput, 2 * sizeof(float));
	FMemory::Memzero(AudioOutput, 2 * sizeof(float));
}

void FSourceEffectFilter::Init(const FSoundEffectSourceInitData& InitData)
{
	bIsActive = true;
	NumChannels = InitData.NumSourceChannels;
	StateVariableFilter.Init(InitData.SampleRate, NumChannels);
	LadderFilter.Init(InitData.SampleRate, NumChannels);
	OnePoleFilter.Init(InitData.SampleRate, NumChannels);

	UpdateFilter();
}

void FSourceEffectFilter::UpdateFilter()
{
	switch (CircuitType)
	{
		default:
		case ESourceEffectFilterCircuit::OnePole:
		{
			CurrentFilter = &OnePoleFilter;
		}
		break;

		case ESourceEffectFilterCircuit::StateVariable:
		{
			CurrentFilter = &StateVariableFilter;
		}
		break;

		case ESourceEffectFilterCircuit::Ladder:
		{
			CurrentFilter = &LadderFilter;
		}
		break;
	}

	switch (FilterType)
	{
		default:
		case ESourceEffectFilterType::LowPass:
			CurrentFilter->SetFilterType(Audio::EFilter::LowPass);
			break;

		case ESourceEffectFilterType::HighPass:
			CurrentFilter->SetFilterType(Audio::EFilter::HighPass);
			break;

		case ESourceEffectFilterType::BandPass:
			CurrentFilter->SetFilterType(Audio::EFilter::BandPass);
			break;

		case ESourceEffectFilterType::BandStop:
			CurrentFilter->SetFilterType(Audio::EFilter::BandStop);
			break;
	}

	CurrentFilter->SetFrequency(CutoffFrequency);
	CurrentFilter->SetQ(FilterQ);
	CurrentFilter->Update();
}

void FSourceEffectFilter::OnPresetChanged()
{
	GET_EFFECT_SETTINGS(SourceEffectFilter);

	CircuitType = Settings.FilterCircuit;
	FilterType = Settings.FilterType;
	CutoffFrequency = Settings.CutoffFrequency;
	FilterQ = Settings.FilterQ;

	UpdateFilter();
}

void FSourceEffectFilter::ProcessAudio(const FSoundEffectSourceInputData& InData, float* OutAudioBufferData)
{
	CurrentFilter->ProcessAudio(InData.InputSourceEffectBufferPtr, InData.NumSamples, OutAudioBufferData);
}

void USourceEffectFilterPreset::SetSettings(const FSourceEffectFilterSettings& InSettings)
{
	UpdateSettings(InSettings);
}