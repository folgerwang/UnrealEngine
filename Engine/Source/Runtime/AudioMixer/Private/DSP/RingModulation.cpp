// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DSP/RingModulation.h"
#include "DSP/Dsp.h"

namespace Audio
{
	FRingModulation::FRingModulation()
		: ModulationFrequency(800.0f)
		, ModulationDepth(0.5f)
		, DryLevel(0.0f)
		, WetLevel(1.0f)
		, NumChannels(0)
	{

	}

	FRingModulation::~FRingModulation()
	{

	}

	void FRingModulation::Init(const float InSampleRate, const int32 InNumChannels)
	{
		Osc.Init(InSampleRate);
		Osc.SetFrequency(ModulationFrequency);
		Osc.Update();
		Osc.Start();

		NumChannels = InNumChannels;
	}

	void FRingModulation::SetModulatorWaveType(const EOsc::Type InType)
	{
		Osc.SetType(InType);
	}

	void FRingModulation::SetModulationFrequency(const float InModulationFrequency)
	{
		Osc.SetFrequency(FMath::Clamp(InModulationFrequency, 10.0f, 10000.0f));
		Osc.Update();
	}

	void FRingModulation::SetModulationDepth(const float InModulationDepth)
	{
		ModulationDepth = FMath::Clamp(InModulationDepth, -1.0f, 1.0f);
	}

	void FRingModulation::ProcessAudioFrame(const float* InFrame, float* OutFrame)
	{
		float OscOut = Osc.Generate();
		for (int32 Channel = 0; Channel < NumChannels; ++Channel)
		{
			OutFrame[Channel] = DryLevel * InFrame[Channel] + WetLevel * InFrame[Channel] * OscOut * ModulationDepth;
		}
	}

	void FRingModulation::ProcessAudio(const float* InBuffer, const int32 InNumSamples, float* OutBuffer)
	{
		for (int32 SampleIndex = 0; SampleIndex < InNumSamples; SampleIndex += NumChannels)
		{
			ProcessAudioFrame(&InBuffer[SampleIndex], &OutBuffer[SampleIndex]);
		}
	}


}
