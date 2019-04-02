// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AudioMixerBlueprintLibrary.h"
#include "Engine/World.h"
#include "AudioDevice.h"
#include "AudioMixerDevice.h"
#include "CoreMinimal.h"
#include "DSP/SpectrumAnalyzer.h"


// This is our global recording task:
static TUniquePtr<Audio::FAudioRecordingData> RecordingData;


static FAudioDevice* GetAudioDeviceFromWorldContext(const UObject* WorldContextObject)
{
	UWorld* ThisWorld = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!ThisWorld || !ThisWorld->bAllowAudioPlayback || ThisWorld->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}

	return ThisWorld->GetAudioDevice();
}

static Audio::FMixerDevice* GetAudioMixerDeviceFromWorldContext(const UObject* WorldContextObject)
{
	if (FAudioDevice* AudioDevice = GetAudioDeviceFromWorldContext(WorldContextObject))
	{
		if (!AudioDevice->IsAudioMixerEnabled())
		{
			return nullptr;
		}
		else
		{
			return static_cast<Audio::FMixerDevice*>(AudioDevice);
		}
	}
	return nullptr;
}

void UAudioMixerBlueprintLibrary::AddMasterSubmixEffect(const UObject* WorldContextObject, USoundEffectSubmixPreset* SubmixEffectPreset)
{
	if (!SubmixEffectPreset)
	{
		UE_LOG(LogAudioMixer, Warning, TEXT("AddMasterSubmixEffect was passed invalid submix effect preset"));
		return;
	}

	if (Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(WorldContextObject))
	{
		// Immediately create a new sound effect base here before the object becomes potentially invalidated
		FSoundEffectBase* SoundEffectBase = SubmixEffectPreset->CreateNewEffect();

		// Cast it to a sound effect submix type
		FSoundEffectSubmix* SoundEffectSubmix = static_cast<FSoundEffectSubmix*>(SoundEffectBase);

		FSoundEffectSubmixInitData InitData;
		InitData.SampleRate = MixerDevice->GetSampleRate();

		// Initialize and set the preset immediately
		SoundEffectSubmix->Init(InitData);
		SoundEffectSubmix->SetPreset(SubmixEffectPreset);
		SoundEffectSubmix->SetEnabled(true);

		// Get a unique id for the preset object on the game thread. Used to refer to the object on audio render thread.
		uint32 SubmixPresetUniqueId = SubmixEffectPreset->GetUniqueID();

		MixerDevice->AddMasterSubmixEffect(SubmixPresetUniqueId, SoundEffectSubmix);
	}
}

void UAudioMixerBlueprintLibrary::RemoveMasterSubmixEffect(const UObject* WorldContextObject, USoundEffectSubmixPreset* SubmixEffectPreset)
{
	if (!SubmixEffectPreset)
	{
		UE_LOG(LogAudioMixer, Warning, TEXT("RemoveMasterSubmixEffect was passed invalid submix effect preset"));
		return;
	}

	if (Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(WorldContextObject))
	{
		// Get the unique id for the preset object on the game thread. Used to refer to the object on audio render thread.
		uint32 SubmixPresetUniqueId = SubmixEffectPreset->GetUniqueID();

		MixerDevice->RemoveMasterSubmixEffect(SubmixPresetUniqueId);
	}
}

void UAudioMixerBlueprintLibrary::ClearMasterSubmixEffects(const UObject* WorldContextObject)
{
	if (Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(WorldContextObject))
	{
		MixerDevice->ClearMasterSubmixEffects();
	}
}

void UAudioMixerBlueprintLibrary::StartRecordingOutput(const UObject* WorldContextObject, float ExpectedDuration, USoundSubmix* SubmixToRecord)
{
	if (Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(WorldContextObject))
	{
		MixerDevice->StartRecording(SubmixToRecord, ExpectedDuration);
	}
	else
	{
		UE_LOG(LogAudioMixer, Error, TEXT("Output recording is an audio mixer only feature. Please run the game with -audiomixer to enable this feature."));
	}
}

USoundWave* UAudioMixerBlueprintLibrary::StopRecordingOutput(const UObject* WorldContextObject, EAudioRecordingExportType ExportType, const FString& Name, FString Path, USoundSubmix* SubmixToRecord, USoundWave* ExistingSoundWaveToOverwrite)
{
	if (RecordingData.IsValid())
	{
		UE_LOG(LogAudioMixer, Warning, TEXT("Abandoning existing write operation. If you'd like to export multiple submix recordings at the same time, use Start/Finish Recording Submix Output instead."));
	}

	if (Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(WorldContextObject))
	{
		float SampleRate;
		float ChannelCount;

		// call the thing here.
		Audio::AlignedFloatBuffer& RecordedBuffer = MixerDevice->StopRecording(SubmixToRecord, ChannelCount, SampleRate);

		if (RecordedBuffer.Num() == 0)
		{
			UE_LOG(LogAudioMixer, Warning, TEXT("No audio data. Did you call Start Recording Output?"));
			return nullptr;
		}

		// Pack output data into a TSampleBuffer and record out:
		RecordingData.Reset(new Audio::FAudioRecordingData());
		RecordingData->InputBuffer = Audio::TSampleBuffer<int16>(RecordedBuffer, ChannelCount, SampleRate);

		switch (ExportType)
		{
		case EAudioRecordingExportType::SoundWave:
		{
			USoundWave* ResultingSoundWave = RecordingData->Writer.SynchronouslyWriteSoundWave(RecordingData->InputBuffer, &Name, &Path);
			RecordingData.Reset();
			return ResultingSoundWave;
			break;
		}
		case EAudioRecordingExportType::WavFile:
		{
			RecordingData->Writer.BeginWriteToWavFile(RecordingData->InputBuffer, Name, Path, [SubmixToRecord]()
			{
				if (SubmixToRecord && SubmixToRecord->OnSubmixRecordedFileDone.IsBound())
				{
					SubmixToRecord->OnSubmixRecordedFileDone.Broadcast(nullptr);
				}

				// I'm gonna try this, but I do not feel great about it.
				RecordingData.Reset();
			});
			break;
		}
		default:
			break;
		}	
	}
	else
	{
		UE_LOG(LogAudioMixer, Error, TEXT("Output recording is an audio mixer only feature. Please run the game with -audiomixer to enable this feature."));
	}

	return nullptr;
}

void UAudioMixerBlueprintLibrary::PauseRecordingOutput(const UObject* WorldContextObject, USoundSubmix* SubmixToPause /* = nullptr */)
{
	if (Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(WorldContextObject))
	{
		MixerDevice->PauseRecording(SubmixToPause);
	}
	else
	{
		UE_LOG(LogAudioMixer, Error, TEXT("Output recording is an audio mixer only feature. Please run the game with -audiomixer to enable this feature."));
	}
}

void UAudioMixerBlueprintLibrary::ResumeRecordingOutput(const UObject* WorldContextObject, USoundSubmix* SubmixToResume /* = nullptr */)
{
	if (Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(WorldContextObject))
	{
		MixerDevice->ResumeRecording(SubmixToResume);
	}
	else
	{
		UE_LOG(LogAudioMixer, Error, TEXT("Output recording is an audio mixer only feature. Please run the game with -audiomixer to enable this feature."));
	}
}

void UAudioMixerBlueprintLibrary::StartAnalyzingOutput(const UObject* WorldContextObject, USoundSubmix* SubmixToAnalyze /*= nullptr*/, EFFTSize FFTSize /*= EFFTSize::Default*/, EFFTPeakInterpolationMethod InterpolationMethod /*= Linear*/, EFFTWindowType WindowType /*= EFFTWindowType::Hamming*/, float HopSize /*= 0*/)
{
	if (Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(WorldContextObject))
	{
		Audio::FSpectrumAnalyzerSettings Settings = Audio::FSpectrumAnalyzerSettings();
		PopulateSpectrumAnalyzerSettings(FFTSize, InterpolationMethod, WindowType, HopSize, Settings);
		MixerDevice->StartSpectrumAnalysis(SubmixToAnalyze, Settings);
	}
	else
	{
		UE_LOG(LogAudioMixer, Error, TEXT("Spectrum Analysis is an audio mixer only feature. Please run the game with -audiomixer to enable this feature."));
	}
}

void UAudioMixerBlueprintLibrary::StopAnalyzingOutput(const UObject* WorldContextObject, USoundSubmix* SubmixToStopAnalyzing /*= nullptr*/)
{
	if (Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(WorldContextObject))
	{
		MixerDevice->StopSpectrumAnalysis(SubmixToStopAnalyzing);
	}
	else
	{
		UE_LOG(LogAudioMixer, Error, TEXT("Spectrum Analysis is an audio mixer only feature. Please run the game with -audiomixer to enable this feature."));
	}
}

void UAudioMixerBlueprintLibrary::GetMagnitudeForFrequencies(const UObject* WorldContextObject, const TArray<float>& Frequencies, TArray<float>& Magnitudes, USoundSubmix* SubmixToAnalyze /*= nullptr*/)
{
	if (Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(WorldContextObject))
	{
		MixerDevice->GetMagnitudesForFrequencies(SubmixToAnalyze, Frequencies, Magnitudes);
	}
	else
	{
		UE_LOG(LogAudioMixer, Error, TEXT("Output recording is an audio mixer only feature. Please run the game with -audiomixer to enable this feature."));
	}
}

void UAudioMixerBlueprintLibrary::GetPhaseForFrequencies(const UObject* WorldContextObject, const TArray<float>& Frequencies, TArray<float>& Phases, USoundSubmix* SubmixToAnalyze /*= nullptr*/)
{
	if (Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(WorldContextObject))
	{
		MixerDevice->GetPhasesForFrequencies(SubmixToAnalyze, Frequencies, Phases);
	}
	else
	{
		UE_LOG(LogAudioMixer, Error, TEXT("Output recording is an audio mixer only feature. Please run the game with -audiomixer to enable this feature."));
	}
}

void UAudioMixerBlueprintLibrary::AddSourceEffectToPresetChain(const UObject* WorldContextObject, USoundEffectSourcePresetChain* PresetChain, FSourceEffectChainEntry Entry)
{
	if (!PresetChain)
	{
		UE_LOG(LogAudioMixer, Warning, TEXT("AddSourceEffectToPresetChain was passed invalid preset chain"));
		return;
	}

	if (Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(WorldContextObject))
	{
		TArray<FSourceEffectChainEntry> Chain;

		uint32 PresetChainId = PresetChain->GetUniqueID();

		if (!MixerDevice->GetCurrentSourceEffectChain(PresetChainId, Chain))
		{
			Chain = PresetChain->Chain;
		}

		Chain.Add(Entry);
		MixerDevice->UpdateSourceEffectChain(PresetChainId, Chain, PresetChain->bPlayEffectChainTails);
	}
}

void UAudioMixerBlueprintLibrary::RemoveSourceEffectFromPresetChain(const UObject* WorldContextObject, USoundEffectSourcePresetChain* PresetChain, int32 EntryIndex)
{
	if (!PresetChain)
	{
		UE_LOG(LogAudioMixer, Warning, TEXT("RemoveSourceEffectFromPresetChain was passed invalid preset chain"));
		return;
	}

	if (Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(WorldContextObject))
	{
		TArray<FSourceEffectChainEntry> Chain;

		uint32 PresetChainId = PresetChain->GetUniqueID();

		if (!MixerDevice->GetCurrentSourceEffectChain(PresetChainId, Chain))
		{
			Chain = PresetChain->Chain;
		}

		if (EntryIndex < Chain.Num())
		{
			Chain.RemoveAt(EntryIndex);
		}

		MixerDevice->UpdateSourceEffectChain(PresetChainId, Chain, PresetChain->bPlayEffectChainTails);
	}

}

void UAudioMixerBlueprintLibrary::SetBypassSourceEffectChainEntry(const UObject* WorldContextObject, USoundEffectSourcePresetChain* PresetChain, int32 EntryIndex, bool bBypassed)
{
	if (!PresetChain)
	{
		UE_LOG(LogAudioMixer, Warning, TEXT("SetBypassSourceEffectChainEntry was passed invalid preset chain"));
		return;
	}

	if (Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(WorldContextObject))
	{
		TArray<FSourceEffectChainEntry> Chain;

		uint32 PresetChainId = PresetChain->GetUniqueID();

		if (!MixerDevice->GetCurrentSourceEffectChain(PresetChainId, Chain))
		{
			Chain = PresetChain->Chain;
		}

		if (EntryIndex < Chain.Num())
		{
			Chain[EntryIndex].bBypass = bBypassed;
		}

		MixerDevice->UpdateSourceEffectChain(PresetChainId, Chain, PresetChain->bPlayEffectChainTails);
	}
}

int32 UAudioMixerBlueprintLibrary::GetNumberOfEntriesInSourceEffectChain(const UObject* WorldContextObject, USoundEffectSourcePresetChain* PresetChain)
{
	if (!PresetChain)
	{
		UE_LOG(LogAudioMixer, Warning, TEXT("GetNumberOfEntriesInSourceEffectChain was passed invalid preset chain"));
		return 0;
	}

	if (Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(WorldContextObject))
	{
		TArray<FSourceEffectChainEntry> Chain;

		uint32 PresetChainId = PresetChain->GetUniqueID();

		if (!MixerDevice->GetCurrentSourceEffectChain(PresetChainId, Chain))
		{
			return PresetChain->Chain.Num();
		}

		return Chain.Num();
	}

	return 0;
}

void UAudioMixerBlueprintLibrary::PopulateSpectrumAnalyzerSettings(EFFTSize FFTSize, EFFTPeakInterpolationMethod InterpolationMethod, EFFTWindowType WindowType, float HopSize, Audio::FSpectrumAnalyzerSettings &OutSettings)
{
	switch (FFTSize)
	{
	case EFFTSize::DefaultSize:
		OutSettings.FFTSize = Audio::FSpectrumAnalyzerSettings::EFFTSize::Default;
		break;
	case EFFTSize::Min:
		OutSettings.FFTSize = Audio::FSpectrumAnalyzerSettings::EFFTSize::Min_64;
		break;
	case EFFTSize::Small:
		OutSettings.FFTSize = Audio::FSpectrumAnalyzerSettings::EFFTSize::Small_256;
		break;
	case EFFTSize::Medium:
		OutSettings.FFTSize = Audio::FSpectrumAnalyzerSettings::EFFTSize::Medium_512;
		break;
	case EFFTSize::Large:
		OutSettings.FFTSize = Audio::FSpectrumAnalyzerSettings::EFFTSize::Large_1024;
		break;
	case EFFTSize::Max:
		OutSettings.FFTSize = Audio::FSpectrumAnalyzerSettings::EFFTSize::TestLarge_4096;
		break;
	default:
		break;
	}

	switch (InterpolationMethod)
	{
	case EFFTPeakInterpolationMethod::NearestNeighbor:
		OutSettings.InterpolationMethod = Audio::FSpectrumAnalyzerSettings::EPeakInterpolationMethod::NearestNeighbor;
		break;
	case EFFTPeakInterpolationMethod::Linear:
		OutSettings.InterpolationMethod = Audio::FSpectrumAnalyzerSettings::EPeakInterpolationMethod::Linear;
		break;
	case EFFTPeakInterpolationMethod::Quadratic:
		OutSettings.InterpolationMethod = Audio::FSpectrumAnalyzerSettings::EPeakInterpolationMethod::Quadratic;
		break;
	default:
		break;
	}

	switch (WindowType)
	{
	case EFFTWindowType::None:
		OutSettings.WindowType = Audio::EWindowType::None;
		break;
	case EFFTWindowType::Hamming:
		OutSettings.WindowType = Audio::EWindowType::Hamming;
		break;
	case EFFTWindowType::Hann:
		OutSettings.WindowType = Audio::EWindowType::Hann;
		break;
	case EFFTWindowType::Blackman:
		OutSettings.WindowType = Audio::EWindowType::Blackman;
		break;
	default:
		break;
	}

	OutSettings.HopSize = HopSize;
}
