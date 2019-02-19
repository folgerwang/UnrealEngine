//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononReverb.h"
#include "SteamAudioModule.h"
#include "SteamAudioSettings.h"
#include "PhononReverbSourceSettings.h"
#include "Sound/SoundEffectSubmix.h"
#include "Sound/SoundSubmix.h"
#include "DSP/Dsp.h"
#include "Misc/ScopeLock.h"
#include "PhononPluginManager.h"
#include "SteamAudioEnvironment.h"

#include "AudioDevice.h"

namespace SteamAudio
{
	//==============================================================================================================================================
	// FPhononReverb
	//==============================================================================================================================================

	FPhononReverb::FPhononReverb()
		: BinauralRenderer(nullptr)
		, IndirectBinauralEffect(nullptr)
		, IndirectPanningEffect(nullptr)
		, ReverbConvolutionEffect(nullptr)
		, AmbisonicsChannels(0)
		, IndirectOutDeinterleaved(nullptr)
		, CachedSpatializationMethod(EIplSpatializationMethod::PANNING)
		, Environment(nullptr)
	{
	}

	FPhononReverb::~FPhononReverb()
	{
		for (auto& ReverbSource : ReverbSources)
		{
			if (ReverbSource.ConvolutionEffect)
			{
				iplDestroyConvolutionEffect(&ReverbSource.ConvolutionEffect);
			}
		}

		if (ReverbConvolutionEffect)
		{
			iplDestroyConvolutionEffect(&ReverbConvolutionEffect);
		}

		if (IndirectBinauralEffect)
		{
			iplDestroyAmbisonicsBinauralEffect(&IndirectBinauralEffect);
		}

		if (IndirectPanningEffect)
		{
			iplDestroyAmbisonicsPanningEffect(&IndirectPanningEffect);
		}

		if (BinauralRenderer)
		{
			iplDestroyBinauralRenderer(&BinauralRenderer);
		}

		if (IndirectOutDeinterleaved)
		{
			for (int32 i = 0; i < AmbisonicsChannels; ++i)
			{
				delete[] IndirectOutDeinterleaved[i];
			}
			delete[] IndirectOutDeinterleaved;
			IndirectOutDeinterleaved = nullptr;
		}
	}

	// Just makes a copy of the init params - actual initialization needs to be deferred until the environment is created.
	// This is because we do not know if we should fall back to Phonon settings from the TAN overrides until the compute
	// device has been created.
	void FPhononReverb::Initialize(const FAudioPluginInitializationParams InitializationParams)
	{
		AudioPluginInitializationParams = InitializationParams;

		InputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_MONO;
		InputAudioFormat.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
		InputAudioFormat.channelOrder = IPL_CHANNELORDER_INTERLEAVED;
		InputAudioFormat.numSpeakers = 1;
		InputAudioFormat.speakerDirections = nullptr;
		InputAudioFormat.ambisonicsOrder = -1;
		InputAudioFormat.ambisonicsNormalization = IPL_AMBISONICSNORMALIZATION_N3D;
		InputAudioFormat.ambisonicsOrdering = IPL_AMBISONICSORDERING_ACN;

		ReverbSources.SetNum(AudioPluginInitializationParams.NumSources);
		for (FReverbSource& ReverbSource : ReverbSources)
		{
			ReverbSource.InBuffer.format = InputAudioFormat;
			ReverbSource.InBuffer.numSamples = AudioPluginInitializationParams.BufferLength;
		}
	}

	void FPhononReverb::SetEnvironment(FEnvironment* InEnvironment)
	{
		if (!InEnvironment)
		{
			return;
		}

		Environment = InEnvironment;

		const int32 IndirectImpulseResponseOrder = Environment->GetSimulationSettings().ambisonicsOrder;
		AmbisonicsChannels = (IndirectImpulseResponseOrder + 1) * (IndirectImpulseResponseOrder + 1);

		ReverbInputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_STEREO;
		ReverbInputAudioFormat.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
		ReverbInputAudioFormat.channelOrder = IPL_CHANNELORDER_INTERLEAVED;
		ReverbInputAudioFormat.numSpeakers = 2;
		ReverbInputAudioFormat.speakerDirections = nullptr;
		ReverbInputAudioFormat.ambisonicsOrder = -1;
		ReverbInputAudioFormat.ambisonicsNormalization = IPL_AMBISONICSNORMALIZATION_N3D;
		ReverbInputAudioFormat.ambisonicsOrdering = IPL_AMBISONICSORDERING_ACN;

		IndirectOutputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_MONO;
		IndirectOutputAudioFormat.channelLayoutType = IPL_CHANNELLAYOUTTYPE_AMBISONICS;
		IndirectOutputAudioFormat.channelOrder = IPL_CHANNELORDER_DEINTERLEAVED;
		IndirectOutputAudioFormat.numSpeakers = (IndirectImpulseResponseOrder + 1) * (IndirectImpulseResponseOrder + 1);
		IndirectOutputAudioFormat.speakerDirections = nullptr;
		IndirectOutputAudioFormat.ambisonicsOrder = IndirectImpulseResponseOrder;
		IndirectOutputAudioFormat.ambisonicsNormalization = IPL_AMBISONICSNORMALIZATION_N3D;
		IndirectOutputAudioFormat.ambisonicsOrdering = IPL_AMBISONICSORDERING_ACN;

		// Assume stereo output - if wrong, will be dynamically changed in the mixer processing
		BinauralOutputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_STEREO;
		BinauralOutputAudioFormat.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
		BinauralOutputAudioFormat.channelOrder = IPL_CHANNELORDER_INTERLEAVED;
		BinauralOutputAudioFormat.numSpeakers = 2;
		BinauralOutputAudioFormat.speakerDirections = nullptr;
		BinauralOutputAudioFormat.ambisonicsOrder = -1;
		BinauralOutputAudioFormat.ambisonicsNormalization = IPL_AMBISONICSNORMALIZATION_N3D;
		BinauralOutputAudioFormat.ambisonicsOrdering = IPL_AMBISONICSORDERING_ACN;

		IPLHrtfParams HrtfParams;
		HrtfParams.hrtfData = nullptr;
		HrtfParams.loadCallback = nullptr;
		HrtfParams.lookupCallback = nullptr;
		HrtfParams.unloadCallback = nullptr;
		HrtfParams.numHrirSamples = 0;
		HrtfParams.type = IPL_HRTFDATABASETYPE_DEFAULT;

		// The binaural renderer always uses Phonon convolution even if TAN is available.
		IPLRenderingSettings BinauralRenderingSettings = Environment->GetRenderingSettings();
		BinauralRenderingSettings.convolutionType = IPL_CONVOLUTIONTYPE_PHONON;

		iplCreateBinauralRenderer(SteamAudio::GlobalContext, BinauralRenderingSettings, HrtfParams, &BinauralRenderer);
		iplCreateAmbisonicsBinauralEffect(BinauralRenderer, IndirectOutputAudioFormat, BinauralOutputAudioFormat, &IndirectBinauralEffect);
		iplCreateAmbisonicsPanningEffect(BinauralRenderer, IndirectOutputAudioFormat, BinauralOutputAudioFormat, &IndirectPanningEffect);

		IndirectOutDeinterleaved = new float*[AmbisonicsChannels];
		for (int32 i = 0; i < AmbisonicsChannels; ++i)
		{
			IndirectOutDeinterleaved[i] = new float[AudioPluginInitializationParams.BufferLength];
		}
		IndirectIntermediateBuffer.format = IndirectOutputAudioFormat;
		IndirectIntermediateBuffer.numSamples = AudioPluginInitializationParams.BufferLength;
		IndirectIntermediateBuffer.interleavedBuffer = nullptr;
		IndirectIntermediateBuffer.deinterleavedBuffer = IndirectOutDeinterleaved;

		DryBuffer.format = ReverbInputAudioFormat;
		DryBuffer.numSamples = AudioPluginInitializationParams.BufferLength;
		DryBuffer.interleavedBuffer = nullptr;
		DryBuffer.deinterleavedBuffer = nullptr;

		IndirectOutArray.SetNumZeroed(AudioPluginInitializationParams.BufferLength * BinauralOutputAudioFormat.numSpeakers);
		IndirectOutBuffer.format = BinauralOutputAudioFormat;
		IndirectOutBuffer.numSamples = AudioPluginInitializationParams.BufferLength;
		IndirectOutBuffer.interleavedBuffer = IndirectOutArray.GetData();
		IndirectOutBuffer.deinterleavedBuffer = nullptr;

		ReverbIndirectContribution = 1.0f;

		CachedSpatializationMethod = GetDefault<USteamAudioSettings>()->IndirectSpatializationMethod;
	}

	void FPhononReverb::OnInitSource(const uint32 SourceId, const FName& AudioComponentUserId, const uint32 NumChannels, UReverbPluginSourceSettingsBase* InSettings)
	{
		if (!Environment || !Environment->GetEnvironmentalRenderer())
		{
			UE_LOG(LogSteamAudio, Error, TEXT("Unable to find environmental renderer for reverb. Reverb will not be applied. Make sure to export the scene."));
			return;
		}

		FString SourceString = AudioComponentUserId.ToString().ToLower();
		IPLBakedDataIdentifier SourceIdentifier;
		SourceIdentifier.type = IPL_BAKEDDATATYPE_STATICSOURCE;
		SourceIdentifier.identifier = Environment->GetBakedIdentifierMap().Get(SourceString);

		UE_LOG(LogSteamAudio, Log, TEXT("Creating reverb effect for %s"), *SourceString);

		UPhononReverbSourceSettings* Settings = static_cast<UPhononReverbSourceSettings*>(InSettings);
		FReverbSource& ReverbSource = ReverbSources[SourceId];

		ReverbSource.IndirectContribution = Settings->IndirectContribution;

		InputAudioFormat.numSpeakers = NumChannels;
		switch (NumChannels)
		{
			case 1: InputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_MONO; break;
			case 2: InputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_STEREO; break;
			case 4: InputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_QUADRAPHONIC; break;
			case 6: InputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_FIVEPOINTONE; break;
			case 8: InputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_SEVENPOINTONE; break;
		}

		ReverbSource.InBuffer.format = InputAudioFormat;

		switch (Settings->IndirectSimulationType)
		{
		case EIplSimulationType::BAKED:
			iplCreateConvolutionEffect(Environment->GetEnvironmentalRenderer(), SourceIdentifier, IPL_SIMTYPE_BAKED, InputAudioFormat, IndirectOutputAudioFormat,
				&ReverbSource.ConvolutionEffect);
			break;
		case EIplSimulationType::REALTIME:
			iplCreateConvolutionEffect(Environment->GetEnvironmentalRenderer(), SourceIdentifier, IPL_SIMTYPE_REALTIME, InputAudioFormat, IndirectOutputAudioFormat,
				&ReverbSource.ConvolutionEffect);
			break;
		case EIplSimulationType::DISABLED:
		default:
			break;
		}
	}

	void FPhononReverb::OnReleaseSource(const uint32 SourceId)
	{
		UE_LOG(LogSteamAudio, Log, TEXT("Destroying reverb effect."));

		check((int32)SourceId < ReverbSources.Num());

		if (ReverbSources[SourceId].ConvolutionEffect)
		{
			iplDestroyConvolutionEffect(&ReverbSources[SourceId].ConvolutionEffect);
		}
	}

	void FPhononReverb::ProcessSourceAudio(const FAudioPluginSourceInputData& InputData, FAudioPluginSourceOutputData& OutputData)
	{
		if (!Environment || !Environment->GetEnvironmentalRenderer() || !Environment->GetEnvironmentCriticalSectionHandle())
		{
			return;
		}

		FScopeLock EnvironmentLock(Environment->GetEnvironmentCriticalSectionHandle());

		FReverbSource& ReverbSource = ReverbSources[InputData.SourceId];
		IPLVector3 Position = SteamAudio::UnrealToPhononIPLVector3(InputData.SpatializationParams->EmitterWorldPosition);

		if (ReverbSource.ConvolutionEffect)
		{
			ReverbSource.IndirectInArray.SetNumUninitialized(InputData.AudioBuffer->Num());
			for (int32 i = 0; i < InputData.AudioBuffer->Num(); ++i)
			{
				ReverbSource.IndirectInArray[i] = (*InputData.AudioBuffer)[i] * ReverbSource.IndirectContribution;
			}
			ReverbSource.InBuffer.interleavedBuffer = ReverbSource.IndirectInArray.GetData();

			iplSetDryAudioForConvolutionEffect(ReverbSource.ConvolutionEffect, Position, ReverbSource.InBuffer);
		}
	}

	void FPhononReverb::ProcessMixedAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData)
	{
		if (!Environment || !Environment->GetEnvironmentalRenderer() || !Environment->GetEnvironmentCriticalSectionHandle())
		{
			return;
		}

		FScopeLock EnvironmentLock(Environment->GetEnvironmentCriticalSectionHandle());

		if (IndirectOutBuffer.format.numSpeakers != OutData.NumChannels)
		{
			iplDestroyAmbisonicsBinauralEffect(&IndirectBinauralEffect);
			iplDestroyAmbisonicsPanningEffect(&IndirectPanningEffect);

			IndirectOutBuffer.format.numSpeakers = OutData.NumChannels;
			switch (OutData.NumChannels)
			{
				case 1: IndirectOutBuffer.format.channelLayout = IPL_CHANNELLAYOUT_MONO; break;
				case 2: IndirectOutBuffer.format.channelLayout = IPL_CHANNELLAYOUT_STEREO; break;
				case 4: IndirectOutBuffer.format.channelLayout = IPL_CHANNELLAYOUT_QUADRAPHONIC; break;
				case 6: IndirectOutBuffer.format.channelLayout = IPL_CHANNELLAYOUT_FIVEPOINTONE; break;
				case 8: IndirectOutBuffer.format.channelLayout = IPL_CHANNELLAYOUT_SEVENPOINTONE; break;
			}

			IndirectOutArray.SetNumZeroed(OutData.AudioBuffer->Num());
			IndirectOutBuffer.interleavedBuffer = IndirectOutArray.GetData();

			iplCreateAmbisonicsBinauralEffect(BinauralRenderer, IndirectOutputAudioFormat, IndirectOutBuffer.format, &IndirectBinauralEffect);
			iplCreateAmbisonicsPanningEffect(BinauralRenderer, IndirectOutputAudioFormat, IndirectOutBuffer.format, &IndirectPanningEffect);
		}

		if (ReverbConvolutionEffect)
		{
			ReverbIndirectInArray.SetNumUninitialized(InData.AudioBuffer->Num());
			for (int32 i = 0; i < InData.AudioBuffer->Num(); ++i)
			{
				ReverbIndirectInArray[i] = (*InData.AudioBuffer)[i] * ReverbIndirectContribution;
			}

			DryBuffer.interleavedBuffer = ReverbIndirectInArray.GetData();
			iplSetDryAudioForConvolutionEffect(ReverbConvolutionEffect, ListenerPosition, DryBuffer);
		}

		iplGetMixedEnvironmentalAudio(Environment->GetEnvironmentalRenderer(), ListenerPosition, ListenerForward, ListenerUp, IndirectIntermediateBuffer);

		switch (CachedSpatializationMethod)
		{
		case EIplSpatializationMethod::HRTF:
			iplApplyAmbisonicsBinauralEffect(IndirectBinauralEffect, IndirectIntermediateBuffer, IndirectOutBuffer);
			break;
		case EIplSpatializationMethod::PANNING:
			iplApplyAmbisonicsPanningEffect(IndirectPanningEffect, IndirectIntermediateBuffer, IndirectOutBuffer);
			break;
		}

		FMemory::Memcpy(OutData.AudioBuffer->GetData(), IndirectOutArray.GetData(), sizeof(float) * IndirectOutArray.Num());
	}

	void FPhononReverb::CreateReverbEffect()
	{
		check(Environment && Environment->GetEnvironmentalRenderer() && Environment->GetEnvironmentCriticalSectionHandle());

		FScopeLock Lock(Environment->GetEnvironmentCriticalSectionHandle());

		IPLBakedDataIdentifier ReverbIdentifier;
		ReverbIdentifier.type = IPL_BAKEDDATATYPE_REVERB;
		ReverbIdentifier.identifier = 0;

		ReverbIndirectContribution = GetDefault<USteamAudioSettings>()->IndirectContribution;
		switch (GetDefault<USteamAudioSettings>()->ReverbSimulationType)
		{
		case EIplSimulationType::BAKED:
			iplCreateConvolutionEffect(Environment->GetEnvironmentalRenderer(), ReverbIdentifier, IPL_SIMTYPE_BAKED, ReverbInputAudioFormat, IndirectOutputAudioFormat,
				&ReverbConvolutionEffect);
			break;
		case EIplSimulationType::REALTIME:
			iplCreateConvolutionEffect(Environment->GetEnvironmentalRenderer(), ReverbIdentifier, IPL_SIMTYPE_REALTIME, ReverbInputAudioFormat, IndirectOutputAudioFormat,
				&ReverbConvolutionEffect);
			break;
		case EIplSimulationType::DISABLED:
		default:
			break;
		}
	}

	void FPhononReverb::UpdateListener(const FVector& Position, const FVector& Forward, const FVector& Up)
	{
		ListenerPosition = SteamAudio::UnrealToPhononIPLVector3(Position);
		ListenerForward = SteamAudio::UnrealToPhononIPLVector3(Forward, false);
		ListenerUp = SteamAudio::UnrealToPhononIPLVector3(Up, false);
	}

	FSoundEffectSubmix* FPhononReverb::GetEffectSubmix(USoundSubmix* Submix)
	{
		USubmixEffectReverbPluginPreset* ReverbPluginPreset = NewObject<USubmixEffectReverbPluginPreset>(Submix, TEXT("Master Reverb Plugin Effect Preset"));
		auto Effect = static_cast<FSubmixEffectReverbPlugin*>(ReverbPluginPreset->CreateNewEffect());
		Effect->SetPhononReverbPlugin(this);
		return static_cast<FSoundEffectSubmix*>(Effect);
	}

	//==============================================================================================================================================
	// FReverbSource
	//==============================================================================================================================================

	FReverbSource::FReverbSource()
		: ConvolutionEffect(nullptr)
		, IndirectContribution(1.0f)
	{
	}

}

//==================================================================================================================================================
// FSubmixEffectReverbPlugin
//==================================================================================================================================================

FSubmixEffectReverbPlugin::FSubmixEffectReverbPlugin()
	: PhononReverbPlugin(nullptr)
{}

void FSubmixEffectReverbPlugin::Init(const FSoundEffectSubmixInitData& InSampleRate)
{
}

void FSubmixEffectReverbPlugin::OnPresetChanged()
{
}

uint32 FSubmixEffectReverbPlugin::GetDesiredInputChannelCountOverride() const
{
	return 2;
}

void FSubmixEffectReverbPlugin::OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData)
{
	PhononReverbPlugin->ProcessMixedAudio(InData, OutData);
}

void FSubmixEffectReverbPlugin::SetPhononReverbPlugin(SteamAudio::FPhononReverb* InPhononReverbPlugin)
{
	PhononReverbPlugin = InPhononReverbPlugin;
}
