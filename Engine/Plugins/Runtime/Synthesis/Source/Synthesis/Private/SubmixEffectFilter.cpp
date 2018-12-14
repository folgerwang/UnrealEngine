// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SubmixEffects/SubmixEffectFilter.h"
#include "AudioMixer.h"

PRAGMA_DISABLE_OPTIMIZATION

FSubmixEffectFilter::FSubmixEffectFilter()
	: SampleRate(0.0f)
	, CurrentFilter(nullptr)
	, FilterAlgorithm(ESubmixFilterAlgorithm::OnePole)
	, FilterType(ESubmixFilterType::LowPass)
	, FilterFrequency(0.0f)
	, FilterFrequencyMod(0.0f)
	, FilterQ(0.0f)
	, FilterQMod(0.0f)
	, NumChannels(0)
{
}

FSubmixEffectFilter::~FSubmixEffectFilter()
{
}

void FSubmixEffectFilter::Init(const FSoundEffectSubmixInitData& InData)
{
	SampleRate = InData.SampleRate;
	CurrentFilter = &OnePoleFilter;
	NumChannels = 2;

	InitFilter();

}

void FSubmixEffectFilter::InitFilter()
{
	OnePoleFilter.Init(SampleRate, NumChannels);
	StateVariableFilter.Init(SampleRate, NumChannels);
	LadderFilter.Init(SampleRate, NumChannels);

	// Reset all the things on the current filter
	CurrentFilter->SetFilterType((Audio::EFilter::Type)FilterType);
	CurrentFilter->SetFrequency(FilterFrequency);
	CurrentFilter->SetQ(FilterQ);
	CurrentFilter->SetFrequencyMod(FilterFrequencyMod);
	CurrentFilter->SetQMod(FilterQMod);
}

void FSubmixEffectFilter::OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData)
{
	CurrentFilter->Update();

	if (NumChannels != InData.NumChannels)
	{
		NumChannels = InData.NumChannels;
		InitFilter();
	}

	float* InAudioBuffer = InData.AudioBuffer->GetData();
	float* OutAudioBuffer = OutData.AudioBuffer->GetData();
	const int32 NumSamples = InData.AudioBuffer->Num();

	CurrentFilter->ProcessAudio(InAudioBuffer, NumSamples, OutAudioBuffer);
}

void FSubmixEffectFilter::OnPresetChanged()
{
	GET_EFFECT_SETTINGS(SubmixEffectFilter);

	FSubmixEffectFilterSettings NewSettings;
	NewSettings = Settings;

	if (NewSettings.FilterAlgorithm != FilterAlgorithm)
	{
		FilterFrequency = NewSettings.FilterFrequency;
		FilterType = NewSettings.FilterType;
		FilterQ = NewSettings.FilterQ;

		SetFilterAlgorithm(NewSettings.FilterAlgorithm);
	}
	else
	{
		SetFilterCutoffFrequency(NewSettings.FilterFrequency);
		SetFilterQ(NewSettings.FilterQ);
		SetFilterType(NewSettings.FilterType);
	}
}

void FSubmixEffectFilter::SetFilterType(ESubmixFilterType InType)
{
	if (FilterType != InType)
	{
		FilterType = InType;
		CurrentFilter->SetFilterType((Audio::EFilter::Type)FilterType);
	}
}

void FSubmixEffectFilter::SetFilterAlgorithm(ESubmixFilterAlgorithm InAlgorithm)
{
	if (InAlgorithm != FilterAlgorithm)
	{
		FilterAlgorithm = InAlgorithm;

		switch (FilterAlgorithm)
		{
			case ESubmixFilterAlgorithm::OnePole:
				CurrentFilter = &OnePoleFilter;
				break;

			case ESubmixFilterAlgorithm::StateVariable:
				CurrentFilter = &StateVariableFilter;
				break;

			case ESubmixFilterAlgorithm::Ladder:
				CurrentFilter = &LadderFilter;
				break;
		}

		CurrentFilter->SetFilterType((Audio::EFilter::Type)FilterType);
		CurrentFilter->SetFrequency(FilterFrequency);
		CurrentFilter->SetQ(FilterQ);
		CurrentFilter->SetFrequencyMod(FilterFrequencyMod);
		CurrentFilter->SetQMod(FilterQMod);
	}
}

void FSubmixEffectFilter::SetFilterCutoffFrequency(float InFrequency)
{
	if (!FMath::IsNearlyEqual(InFrequency, FilterFrequency))
	{
		FilterFrequency = InFrequency;
		CurrentFilter->SetFrequency(FilterFrequency);
	}
}

void FSubmixEffectFilter::SetFilterCutoffFrequencyMod(float InFrequency)
{
	if (!FMath::IsNearlyEqual(InFrequency, FilterFrequencyMod))
	{
		FilterFrequencyMod = InFrequency;
		CurrentFilter->SetFrequencyMod(FilterFrequencyMod);
	}
}

void FSubmixEffectFilter::SetFilterQ(float InQ)
{
	if (!FMath::IsNearlyEqual(InQ, FilterQ))
	{
		FilterQ = InQ;
		CurrentFilter->SetQ(FilterQ);
	}
}

void FSubmixEffectFilter::SetFilterQMod(float InQ)
{
	if (!FMath::IsNearlyEqual(InQ, FilterQMod))
	{
		FilterQMod = InQ;
		CurrentFilter->SetQMod(FilterQMod);
	}
}

void USubmixEffectFilterPreset::SetSettings(const FSubmixEffectFilterSettings& InSettings)
{
	UpdateSettings(InSettings);
}

void USubmixEffectFilterPreset::SetFilterType(ESubmixFilterType InType)
{
	for (FSoundEffectBase* EffectBaseInstance : Instances)
	{
		FSubmixEffectFilter* FilterEffect = (FSubmixEffectFilter*)EffectBaseInstance;
		EffectBaseInstance->EffectCommand([FilterEffect, InType]()
		{
			FilterEffect->SetFilterType(InType);
		});
	}
}

void USubmixEffectFilterPreset::SetFilterAlgorithm(ESubmixFilterAlgorithm InAlgorithm)
{
	for (FSoundEffectBase* EffectBaseInstance : Instances)
	{
		FSubmixEffectFilter* FilterEffect = (FSubmixEffectFilter*)EffectBaseInstance;
		EffectBaseInstance->EffectCommand([FilterEffect, InAlgorithm]()
		{
			FilterEffect->SetFilterAlgorithm(InAlgorithm);
		});
	}
}

void USubmixEffectFilterPreset::SetFilterCutoffFrequency(float InFrequency)
{
	for (FSoundEffectBase* EffectBaseInstance : Instances)
	{
		FSubmixEffectFilter* FilterEffect = (FSubmixEffectFilter*)EffectBaseInstance;
		EffectBaseInstance->EffectCommand([FilterEffect, InFrequency]()
		{
			FilterEffect->SetFilterCutoffFrequency(InFrequency);
		});
	}
}

void USubmixEffectFilterPreset::SetFilterCutoffFrequencyMod(float InFrequency)
{
	for (FSoundEffectBase* EffectBaseInstance : Instances)
	{
		FSubmixEffectFilter* FilterEffect = (FSubmixEffectFilter*)EffectBaseInstance;
		EffectBaseInstance->EffectCommand([FilterEffect, InFrequency]()
		{
			FilterEffect->SetFilterCutoffFrequencyMod(InFrequency);
		});
	}
}

void USubmixEffectFilterPreset::SetFilterQ(float InQ)
{
	for (FSoundEffectBase* EffectBaseInstance : Instances)
	{
		FSubmixEffectFilter* FilterEffect = (FSubmixEffectFilter*)EffectBaseInstance;
		EffectBaseInstance->EffectCommand([FilterEffect, InQ]()
		{
			FilterEffect->SetFilterQ(InQ);
		});
	}
}

void USubmixEffectFilterPreset::SetFilterQMod(float InQ)
{
	for (FSoundEffectBase* EffectBaseInstance : Instances)
	{
		FSubmixEffectFilter* FilterEffect = (FSubmixEffectFilter*)EffectBaseInstance;
		EffectBaseInstance->EffectCommand([FilterEffect, InQ]()
		{
			FilterEffect->SetFilterQMod(InQ);
		});
	}

}

PRAGMA_ENABLE_OPTIMIZATION