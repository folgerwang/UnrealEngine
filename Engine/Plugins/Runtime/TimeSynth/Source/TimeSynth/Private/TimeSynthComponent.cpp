// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TimeSynthComponent.h"
#include "AudioThread.h"
#include "TimeSynthModule.h"
 
static_assert((int32)Audio::EEventQuantization::Count == (int32)ETimeSynthEventQuantization::Count, "These enumerations need to match");

void FTimeSynthEventListener::OnEvent(Audio::EEventQuantization EventQuantizationType, int32 Bars, float Beat)
{
	check(TimeSynth);
	TimeSynth->OnQuantizationEvent(EventQuantizationType, Bars, Beat);
}

UTimeSynthComponent::UTimeSynthComponent(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
	, TimeSynthEventListener(this)
{
	PrimaryComponentTick.bCanEverTick = true;
}

UTimeSynthComponent::~UTimeSynthComponent()
{
}

void UTimeSynthComponent::PostInitProperties()
{
	Super::PostInitProperties();

	// Copy the settings right away to the audio render thread version
	FilterSettings_AudioRenderThread[(int32)ETimeSynthFilter::FilterA] = FilterASettings;
	FilterSettings_AudioRenderThread[(int32)ETimeSynthFilter::FilterB] = FilterBSettings;
	bIsFilterEnabled_AudioRenderThread[(int32)ETimeSynthFilter::FilterA] = bIsFilterAEnabled;
	bIsFilterEnabled_AudioRenderThread[(int32)ETimeSynthFilter::FilterB] = bIsFilterBEnabled;

	EnvelopeFollowerSettings_AudioRenderThread = EnvelopeFollowerSettings;
	bIsEnvelopeFollowerEnabled_AudioRenderThread = bIsEnvelopeFollowerEnabled;

	SpectrumAnalyzerSettings.FFTSize = GetFFTSize(FFTSize);
	SpectrumAnalyzer.SetSettings(SpectrumAnalyzerSettings);

	// Randomize the seed on post init properties
	RandomStream.GenerateNewSeed();
}

void UTimeSynthComponent::BeginDestroy()
{
	Super::BeginDestroy();
}

void UTimeSynthComponent::OnRegister()
{
	Super::OnRegister();

	SetComponentTickEnabled(true);

	if (!IsRegistered())
	{
		RegisterComponent();
	}
}

void UTimeSynthComponent::OnUnregister()
{
	Super::OnUnregister();

	SetComponentTickEnabled(false);

	if (IsRegistered())
	{
		UnregisterComponent();
	}
}

bool UTimeSynthComponent::IsReadyForFinishDestroy()
{
	return SpectrumAnalysisCounter.GetValue() == 0;
}

void UTimeSynthComponent::AddQuantizationEventDelegate(ETimeSynthEventQuantization QuantizationType, const FOnQuantizationEventBP& OnQuantizationEvent)
{
	// Add a delegate for this event on the game thread data for this event slot
	EventNotificationDelegates_GameThread[(int32)QuantizationType].AddUnique(OnQuantizationEvent);

	// Send over to the audio render thread to tell it that we're listening to this event now
	SynthCommand([this, QuantizationType]
	{
		EventQuantizer.RegisterListenerForEvent(&TimeSynthEventListener, (Audio::EEventQuantization)QuantizationType);
	});
}

void UTimeSynthComponent::SetFilterSettings(ETimeSynthFilter InFilter, const FTimeSynthFilterSettings& InSettings)
{
	if (InFilter == ETimeSynthFilter::FilterA)
	{
		FilterASettings = InSettings;
	}
	else
	{
		FilterBSettings = InSettings;
	}

	SynthCommand([this, InFilter, InSettings]
	{
		FilterSettings_AudioRenderThread[(int32)InFilter] = InSettings;
		UpdateFilter((int32)InFilter);
	});
}

void UTimeSynthComponent::SetEnvelopeFollowerSettings(const FTimeSynthEnvelopeFollowerSettings& InSettings)
{
	EnvelopeFollowerSettings = InSettings;

	SynthCommand([this, InSettings]
	{
		EnvelopeFollowerSettings_AudioRenderThread = InSettings;
		UpdateEnvelopeFollower();
	});
}

void UTimeSynthComponent::SetFilterEnabled(ETimeSynthFilter InFilter, bool bInIsFilterEnabled)
{
	if (InFilter == ETimeSynthFilter::FilterA)
	{
		bIsFilterAEnabled = bInIsFilterEnabled;
	}
	else
	{
		bIsFilterBEnabled = bInIsFilterEnabled;
	}

	SynthCommand([this, InFilter, bInIsFilterEnabled]
	{
		bIsFilterEnabled_AudioRenderThread[(int32)InFilter] = bInIsFilterEnabled;
	});
}

void UTimeSynthComponent::SetEnvelopeFollowerEnabled(bool bInIsEnabled)
{
	bIsEnvelopeFollowerEnabled = bInIsEnabled;

	// Set the envelope value to 0.0 immediately if we're disabling the envelope follower
	if (!bInIsEnabled)
	{
		CurrentEnvelopeValue = 0.0f;
	}

	SynthCommand([this, bInIsEnabled]
	{
		bIsEnvelopeFollowerEnabled_AudioRenderThread = bInIsEnabled;
	});
}

Audio::FSpectrumAnalyzerSettings::EFFTSize UTimeSynthComponent::GetFFTSize(ETimeSynthFFTSize InSize) const
{
	switch (InSize)
	{
		case ETimeSynthFFTSize::Min_64: return Audio::FSpectrumAnalyzerSettings::EFFTSize::Min_64;
		case ETimeSynthFFTSize::Small_256: return Audio::FSpectrumAnalyzerSettings::EFFTSize::Small_256;
		case ETimeSynthFFTSize::Medium_512: return Audio::FSpectrumAnalyzerSettings::EFFTSize::Medium_512;
		case ETimeSynthFFTSize::Large_1024: return Audio::FSpectrumAnalyzerSettings::EFFTSize::Large_1024;
		break;
	}
	// return default
	return Audio::FSpectrumAnalyzerSettings::EFFTSize::Medium_512;
}

void UTimeSynthComponent::SetFFTSize(ETimeSynthFFTSize InFFTSize)
{
	Audio::FSpectrumAnalyzerSettings::EFFTSize NewFFTSize = GetFFTSize(InFFTSize);

	SynthCommand([this, NewFFTSize]
	{
		SpectrumAnalyzerSettings.FFTSize = NewFFTSize;
		SpectrumAnalyzer.SetSettings(SpectrumAnalyzerSettings);
	});
}

void UTimeSynthComponent::OnQuantizationEvent(Audio::EEventQuantization EventQuantizationType, int32 Bars, float Beat)
{
	// When this happens, we want to queue up the event data so it can be safely consumed on the game thread
	GameCommand([this, EventQuantizationType, Bars, Beat]()
	{
		EventNotificationDelegates_GameThread[(int32)EventQuantizationType].Broadcast((ETimeSynthEventQuantization)EventQuantizationType, Bars, Beat);
	});	
}

void UTimeSynthComponent::PumpGameCommandQueue()
{
	TFunction<void()> Command;
	while (GameCommandQueue.Dequeue(Command))
	{
		Command();
	}
}

void UTimeSynthComponent::GameCommand(TFunction<void()> Command)
{
	GameCommandQueue.Enqueue(MoveTemp(Command));
}

void UTimeSynthComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	// Pump the command queue for any event data that is coming back from the audio render thread/callback
	PumpGameCommandQueue();

	// Broadcast the playback time
	if (OnPlaybackTime.IsBound())
	{
		const float PlaybacktimeSeconds = EventQuantizer.GetPlaybacktimeSeconds();
		OnPlaybackTime.Broadcast(PlaybacktimeSeconds);
	}

	// Perform volume group math to update volume group volume values and then set the volumes on the clips
	for (auto& Entry : VolumeGroupData)
	{
		FVolumeGroupData& VolumeGroup = Entry.Value;
		
		// If we've reached our terminating condition, just set to the target volume 
		if (VolumeGroup.CurrentTime >= VolumeGroup.TargetFadeTime)
		{
			VolumeGroup.CurrentVolumeDb = VolumeGroup.TargetVolumeDb;
		}
		else
		{
			check(VolumeGroup.TargetFadeTime > 0.0f);
			const float FadeFraction = VolumeGroup.CurrentTime / VolumeGroup.TargetFadeTime;

			VolumeGroup.CurrentVolumeDb = VolumeGroup.StartVolumeDb + FadeFraction * (VolumeGroup.TargetVolumeDb - VolumeGroup.StartVolumeDb);
			VolumeGroup.CurrentTime += DeltaTime;
		}

		for (FTimeSynthClipHandle& ClipHandle : VolumeGroup.Clips)
		{
			float LinearVolume = Audio::ConvertToLinear(VolumeGroup.CurrentVolumeDb);

			SynthCommand([this, ClipHandle, LinearVolume]
			{
				int32* PlayingClipIndex = ClipIdToClipIndexMap_AudioRenderThread.Find(ClipHandle.ClipId);
				if (PlayingClipIndex)
				{
					FPlayingClipInfo& PlayingClipInfo = PlayingClipsPool_AudioRenderThread[*PlayingClipIndex];
					Audio::FDecodingSoundSourceHandle& DecodingSoundSourceHandle = PlayingClipInfo.DecodingSoundSourceHandle;
					SoundWaveDecoder.SetSourceVolumeScale(DecodingSoundSourceHandle, LinearVolume);
				}
			});
		}
	}

	// If the spectrum analyzer is running, grab the desired magnitude spectral data
	if (bEnableSpectralAnalysis)
	{
		SpectralData.Reset();
		SpectrumAnalyzer.LockOutputBuffer();
		for (float Frequency : FrequenciesToAnalyze)
		{
			FTimeSynthSpectralData Data;
			Data.FrequencyHz = Frequency;
			Data.Magnitude = SpectrumAnalyzer.GetMagnitudeForFrequency(Frequency);
			SpectralData.Add(Data);
		}
		SpectrumAnalyzer.UnlockOutputBuffer();
	}

	// Update the synth component on the audio thread
	FAudioThread::RunCommandOnAudioThread([this]()
	{
		SoundWaveDecoder.Update(); 
	});
}

void UTimeSynthComponent::UpdateFilter(int32 FilterIndex)
{
	Filter[FilterIndex].SetFilterType((Audio::EFilter::Type)FilterSettings_AudioRenderThread[FilterIndex].FilterType);
	Filter[FilterIndex].SetFrequency(FilterSettings_AudioRenderThread[FilterIndex].CutoffFrequency);
	Filter[FilterIndex].SetQ(FilterSettings_AudioRenderThread[FilterIndex].FilterQ);
	Filter[FilterIndex].Update();
}

void UTimeSynthComponent::UpdateEnvelopeFollower()
{
	EnvelopeFollower.SetAnalog(EnvelopeFollowerSettings_AudioRenderThread.bIsAnalogMode);
	EnvelopeFollower.SetAttackTime(EnvelopeFollowerSettings_AudioRenderThread.AttackTime);
	EnvelopeFollower.SetReleaseTime(EnvelopeFollowerSettings_AudioRenderThread.ReleaseTime);
	EnvelopeFollower.SetMode((Audio::EPeakMode::Type)EnvelopeFollowerSettings_AudioRenderThread.PeakMode);
}

bool UTimeSynthComponent::Init(int32& InSampleRate)
{ 
	SampleRate = InSampleRate;
	SoundWaveDecoder.Init(GetAudioDevice(), InSampleRate);
	NumChannels = 2;

	// Initialize the settings for the spectrum analyzer
	SpectrumAnalyzer.Init(InSampleRate);

	// Init and update the filter settings
	for (int32 i = 0; i < 2; ++i)
	{
		Filter[i].Init(InSampleRate, 2);
		UpdateFilter(i);
	}

	DynamicsProcessor.Init(InSampleRate, NumChannels);
	DynamicsProcessor.SetLookaheadMsec(3.0f);
	DynamicsProcessor.SetAttackTime(5.0f);
	DynamicsProcessor.SetReleaseTime(100.0f);
	DynamicsProcessor.SetThreshold(-15.0f);
	DynamicsProcessor.SetRatio(5.0f);
	DynamicsProcessor.SetKneeBandwidth(10.0f);
	DynamicsProcessor.SetInputGain(0.0f);
	DynamicsProcessor.SetOutputGain(0.0f);
	DynamicsProcessor.SetChannelLinked(true);
	DynamicsProcessor.SetAnalogMode(true);
	DynamicsProcessor.SetPeakMode(Audio::EPeakMode::Peak);
	DynamicsProcessor.SetProcessingMode(Audio::EDynamicsProcessingMode::Compressor);

	// Init and update the envelope follower settings
	EnvelopeFollower.Init(InSampleRate);
	UpdateEnvelopeFollower();

	// Set the default quantization settings
	SetQuantizationSettings(QuantizationSettings);

	// Create a pool of playing clip runtime infos
	CurrentPoolSize = 20;

	PlayingClipsPool_AudioRenderThread.AddDefaulted(CurrentPoolSize);
	FreePlayingClipIndices_AudioRenderThread.AddDefaulted(CurrentPoolSize);

	for (int32 Index = 0; Index < CurrentPoolSize; ++Index)
	{
		FreePlayingClipIndices_AudioRenderThread[Index] = Index;
	}

	return true; 
}

void UTimeSynthComponent::ShutdownPlayingClips()
{
	SoundWaveDecoder.UpdateRenderThread();

	// Loop through all acitve loops and render their audio
	for (int32 i = ActivePlayingClipIndices_AudioRenderThread.Num() - 1; i >= 0; --i)
	{
		// Grab the playing clip at the active index
		int32 ClipIndex = ActivePlayingClipIndices_AudioRenderThread[i];
		FPlayingClipInfo& PlayingClip = PlayingClipsPool_AudioRenderThread[ClipIndex];

		// Block until the decoder has initialized
		while (!SoundWaveDecoder.IsInitialized(PlayingClip.DecodingSoundSourceHandle))
		{
			FPlatformProcess::Sleep(0);
		}

		SoundWaveDecoder.RemoveDecodingSource(PlayingClip.DecodingSoundSourceHandle);
		ActivePlayingClipIndices_AudioRenderThread.RemoveAtSwap(i, 1, false);
		FreePlayingClipIndices_AudioRenderThread.Add(ClipIndex);
	}
}

void UTimeSynthComponent::OnEndGenerate() 
{
	ShutdownPlayingClips();
}

int32 UTimeSynthComponent::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	// Update the decoder
	SoundWaveDecoder.UpdateRenderThread();

	int32 NumFrames = NumSamples / NumChannels;

	// Perform event quantization notifications
	// This will use the NumFramesPerCallback to evaluate what queued up events need 
	// to begin rendering. THe lambda callback will then enqueue any new rendering clips
	// to the list of active clips. So we only need to loop through active clip indices to render the audio output
	EventQuantizer.NotifyEvents(NumFrames);

	// Loop through all acitve loops and render their audio
	for (int32 i = ActivePlayingClipIndices_AudioRenderThread.Num() - 1; i >= 0; --i)
	{
		// Grab the playing clip at the active index
		int32 ClipIndex = ActivePlayingClipIndices_AudioRenderThread[i];
		FPlayingClipInfo& PlayingClip = PlayingClipsPool_AudioRenderThread[ClipIndex];

		// Compute the number of frames we need to read
		int32 NumFramesToRead = NumFrames - PlayingClip.StartFrameOffset;
		check(NumFramesToRead > 0 && NumFramesToRead <= NumFrames);

		if (!SoundWaveDecoder.IsInitialized(PlayingClip.DecodingSoundSourceHandle))
		{
			continue;
		}

		AudioScratchBuffer.Reset();
		if (SoundWaveDecoder.GetSourceBuffer(PlayingClip.DecodingSoundSourceHandle, NumFramesToRead, NumChannels, AudioScratchBuffer))
		{
			// Make sure we read the appropriate amount of audio frames
			check(AudioScratchBuffer.Num() == NumFramesToRead * NumChannels);
			// Now mix in the retrieved audio at the appropriate sample index
			float* DecodeSourceAudioPtr = AudioScratchBuffer.GetData();
			float FadeVolume = 1.0f;
			int32 OutputSampleIndex = PlayingClip.StartFrameOffset * NumChannels;
			int32 SourceSampleIndex = 0;
			for (int32 FrameIndex = PlayingClip.StartFrameOffset; FrameIndex < NumFrames; ++FrameIndex)
			{
				// Check the fade in condition
				if (PlayingClip.CurrentFrameCount < PlayingClip.FadeInDurationFrames)
				{
					FadeVolume = (float)PlayingClip.CurrentFrameCount / PlayingClip.FadeInDurationFrames;
				}
				// Check the fade out condition
				else if (PlayingClip.CurrentFrameCount >= PlayingClip.DurationFrames && PlayingClip.FadeOutDurationFrames > 0)
				{
					int32 FadeOutFrameCount = PlayingClip.CurrentFrameCount - PlayingClip.DurationFrames;
					FadeVolume = 1.0f - (float)FadeOutFrameCount / PlayingClip.FadeOutDurationFrames;
				}

				FadeVolume = FMath::Clamp(FadeVolume, 0.0f, 1.0f);
				for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex, ++OutputSampleIndex, ++SourceSampleIndex)
				{
					OutAudio[OutputSampleIndex] += FadeVolume * DecodeSourceAudioPtr[SourceSampleIndex];
				}

				++PlayingClip.CurrentFrameCount;
			}

			// Reset the start frame offset so that when this clip continues playing,
			// it won't start part-way through the audio buffer
			PlayingClip.StartFrameOffset = 0;

			bool bIsClipDurationFinished = PlayingClip.CurrentFrameCount > PlayingClip.DurationFrames + PlayingClip.FadeOutDurationFrames;

			// If the clip finished by artificial clip duration settings or if it naturally finished (file length), remove it from the active list
			if (bIsClipDurationFinished || SoundWaveDecoder.IsFinished(PlayingClip.DecodingSoundSourceHandle))
			{
				SoundWaveDecoder.RemoveDecodingSource(PlayingClip.DecodingSoundSourceHandle);
				ActivePlayingClipIndices_AudioRenderThread.RemoveAtSwap(i, 1, false);
				FreePlayingClipIndices_AudioRenderThread.Add(ClipIndex);

				// If this clip was playing in a volume group, we need to remove it from the volume group
				if (PlayingClip.VolumeGroupId != INDEX_NONE)
				{
					FTimeSynthClipHandle Handle = PlayingClip.Handle;
					VolumeGroupUniqueId VolumeGroupId = PlayingClip.VolumeGroupId;

					GameCommand([this, Handle, VolumeGroupId]()
					{
						FVolumeGroupData* VolumeGroup = VolumeGroupData.Find(VolumeGroupId);
						if (VolumeGroup)
						{
							VolumeGroup->Clips.Remove(Handle);
						}
					});
				}
			}
		}
	}

	// Feed audio through filter
	for (int32 i = 0; i < 2; ++i)
	{
		if (bIsFilterEnabled_AudioRenderThread[i])
		{
			Filter[i].ProcessAudio(OutAudio, NumSamples, OutAudio);
		}
	}

	// Feed audio through the envelope follower if it's enabled
	if (bIsEnvelopeFollowerEnabled_AudioRenderThread)
	{
		for (int32 SampleIndex = 0; SampleIndex < NumSamples; SampleIndex += NumChannels)
		{
			float InputSample = 0.5f * (OutAudio[SampleIndex] + OutAudio[SampleIndex + 1]);
			CurrentEnvelopeValue = EnvelopeFollower.ProcessAudio(InputSample);
		}
	}

	if (bEnableSpectralAnalysis)
	{
		// If we have stereo audio, sum to mono before sending to analyzer
		if (NumChannels == 2)
		{
			// Use the scratch buffer to sum the audio to mono
			AudioScratchBuffer.Reset();
			AudioScratchBuffer.AddUninitialized(NumFrames);
			float* AudioScratchBufferPtr = AudioScratchBuffer.GetData();
			int32 SampleIndex = 0;
			for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex, SampleIndex += NumChannels)
			{
				AudioScratchBufferPtr[FrameIndex] = 0.5f * (OutAudio[SampleIndex] + OutAudio[SampleIndex + 1]);
			}
			SpectrumAnalyzer.PushAudio(AudioScratchBufferPtr, NumFrames);
		}
		else
		{
			SpectrumAnalyzer.PushAudio(OutAudio, NumSamples);
		}

		// Launch an analysis task with this audio
		(new FAutoDeleteAsyncTask<FTimeSynthSpectrumAnalysisTask>(&SpectrumAnalyzer, &SpectrumAnalysisCounter))->StartBackgroundTask();
	}

	// Limit the output to prevent clipping
	for (int32 SampleIndex = 0; SampleIndex < NumSamples; SampleIndex += NumChannels)
	{
		DynamicsProcessor.ProcessAudio(&OutAudio[SampleIndex], NumChannels, &OutAudio[SampleIndex]);
	}


 	return NumSamples; 
}

void UTimeSynthComponent::SetQuantizationSettings(const FTimeSynthQuantizationSettings& InQuantizationSettings)
{
	// Store the quantizaton on the UObject for BP querying
	QuantizationSettings = InQuantizationSettings;

	// Local store what the global quantization is so we can assign it to clips using global quantization
	GlobalQuantization = (Audio::EEventQuantization)InQuantizationSettings.GlobalQuantization;

	// Translate the TimeSynth version of quantization settings to the non-UObject quantization settings
	Audio::FEventQuantizationSettings Settings;
	Settings.SampleRate = SampleRate;
	Settings.NumChannels = NumChannels;
	Settings.BeatsPerMinute = FMath::Max(1.0f, InQuantizationSettings.BeatsPerMinute);
	Settings.BeatsPerBar = (uint32)FMath::Max(InQuantizationSettings.BeatsPerBar, 1);
	Settings.GlobalQuantization = GlobalQuantization;
	Settings.BeatDivision = FMath::Pow(2, (int32)InQuantizationSettings.BeatDivision);

	SynthCommand([this, Settings]
	{
		EventQuantizer.SetQuantizationSettings(Settings);
	});
}

void UTimeSynthComponent::SetBPM(const float InBeatsPerMinute)
{
	QuantizationSettings.BeatsPerMinute = InBeatsPerMinute;

	SynthCommand([this, InBeatsPerMinute]
	{
		EventQuantizer.SetBPM(InBeatsPerMinute);
	});
}

int32 UTimeSynthComponent::GetBPM() const
{
	return QuantizationSettings.BeatsPerMinute;
}

void UTimeSynthComponent::SetSeed(int32 InSeed)
{
	RandomStream.Initialize(InSeed);
}

void UTimeSynthComponent::ResetSeed()
{
	RandomStream.Reset();
}

FTimeSynthClipHandle UTimeSynthComponent::PlayClip(UTimeSynthClip* InClip, UTimeSynthVolumeGroup* InVolumeGroup)
{
	if (!InClip)
	{
		UE_LOG(LogTimeSynth, Warning, TEXT("Failed to play clip. Null UTimeSynthClip object."));
		return FTimeSynthClipHandle();
	}

	// Validate the clip
	if (InClip->Sounds.Num() == 0)
	{
		UE_LOG(LogTimeSynth, Warning, TEXT("Failed to play clip: needs to have sounds to choose from."));
		return FTimeSynthClipHandle();
	}

	if (!bIsActive)
	{
		SetActive(true);
	}

	// Get this time synth components transform
	const FTransform& ThisComponentTransform = GetComponentTransform();

	// Get the distance to nearest listener using this transform
	const FAudioDevice* OwningAudioDevice = GetAudioDevice();
	const float DistanceToListener = OwningAudioDevice->GetDistanceToNearestListener(ThisComponentTransform.GetTranslation());

	TArray<FTimeSynthClipSound> ValidSounds;

	// Make sure at least one of the entries in the sound array has a USoundWave asset ref
	for (const FTimeSynthClipSound& ClipSound : InClip->Sounds)
	{
		if (ClipSound.SoundWave)
		{
			// Now check if this clip sound is in range of the distance to the listener
			if (ClipSound.DistanceRange.X != 0 || ClipSound.DistanceRange.Y != 0)
			{
				float MinDist = FMath::Min(ClipSound.DistanceRange.X, ClipSound.DistanceRange.Y);
				float MaxDist = FMath::Max(ClipSound.DistanceRange.X, ClipSound.DistanceRange.Y);

				if (DistanceToListener >= MinDist && DistanceToListener < MaxDist)
				{
					ValidSounds.Add(ClipSound);
				}
			}
			else
			{
				ValidSounds.Add(ClipSound);
			}
		}
	}

	// We didn't have any valid sounds to play for this clip or component was out of range from listener
	if (!ValidSounds.Num())
	{
		return FTimeSynthClipHandle();
	}

	// Calculate the linear volume
	const float VolumeMin = FMath::Clamp(InClip->VolumeScaleDb.X, -60.0f, 20.0f);
	const float VolumeMax = FMath::Clamp(InClip->VolumeScaleDb.Y, -60.0f, 20.0f);
	const float VolumeDb = RandomStream.FRandRange(VolumeMin, VolumeMax);
	float VolumeScale = Audio::ConvertToLinear(VolumeDb);

	// Calculate the pitch scale
	const float PitchMin = FMath::Clamp(InClip->PitchScaleSemitones.X, -24.0f, 24.0f);
	const float PitchMax = FMath::Clamp(InClip->PitchScaleSemitones.Y, -24.0f, 24.0f);
	const float PitchSemitones = RandomStream.FRandRange(PitchMin, PitchMax);
	const float PitchScale = Audio::GetFrequencyMultiplier(PitchSemitones);

	// Only need to find a random-weighted one if there's more than valid sound
	int32 ChosenSoundIndex = 0;
	if (ValidSounds.Num() > 1)
	{
		float SumWeight = 0.0f;
		for (FTimeSynthClipSound& Sound : ValidSounds)
		{
			SumWeight += Sound.RandomWeight;
		}

		const float Choice = RandomStream.FRandRange(0.0f, SumWeight);
		SumWeight = 0.0f;

		for (int32 Index = 0; Index < ValidSounds.Num(); ++Index)
		{
			const FTimeSynthClipSound& Sound = ValidSounds[Index];
			const float NextTotal = SumWeight + Sound.RandomWeight;
			if (Choice >= SumWeight && Choice < NextTotal)
			{
				ChosenSoundIndex = Index;
				break;
			}
			SumWeight = NextTotal;
		}
	}

	const FTimeSynthClipSound& ChosenSound = ValidSounds[ChosenSoundIndex];

	// Now have a chosen sound, so we can create a new decoder handle on the game thread
	Audio::FDecodingSoundSourceHandle NewDecoderHandle = SoundWaveDecoder.CreateSourceHandle(ChosenSound.SoundWave);
	DecodingSounds_GameThread.Add(NewDecoderHandle);

	// Generate a new handle for this clip.
	// This handle is used by game thread to control this clip.
	static int32 ClipIds = 0;
	FTimeSynthClipHandle NewHandle;
	NewHandle.ClipName = InClip->GetFName();
	NewHandle.ClipId = ClipIds++;

	// New struct for a playing clip handle. This is internal.
	FPlayingClipInfo NewClipInfo;

	// Setup an entry for the playing clip in its volume group if it was set
	if (InVolumeGroup)
	{
		VolumeGroupUniqueId Id = InVolumeGroup->GetUniqueID();
		NewClipInfo.VolumeGroupId = Id;

		FVolumeGroupData* VolumeGroup = VolumeGroupData.Find(Id);
		if (!VolumeGroup)
		{
			FVolumeGroupData NewData;
			NewData.Clips.Add(NewHandle);
			VolumeGroupData.Add(Id, NewData);
		}
		else
		{
			// Get the current volume group value and "scale" it into the volume scale
			VolumeScale *= Audio::ConvertToLinear(VolumeGroup->CurrentVolumeDb);
			VolumeGroup->Clips.Add(NewHandle);
		}
	}

	Audio::FSourceDecodeInit DecodeInit;
	DecodeInit.Handle = NewDecoderHandle;
	DecodeInit.PitchScale = PitchScale;
	DecodeInit.VolumeScale = VolumeScale;
	DecodeInit.SoundWave = ChosenSound.SoundWave;
	DecodeInit.SeekTime = 0;

	// Update the synth component on the audio thread
	FAudioThread::RunCommandOnAudioThread([this, DecodeInit]()
	{
		SoundWaveDecoder.InitDecodingSource(DecodeInit);
	});


	NewClipInfo.bIsGloballyQuantized = InClip->ClipQuantization == ETimeSynthEventClipQuantization::Global;

	if (NewClipInfo.bIsGloballyQuantized)
	{
		NewClipInfo.ClipQuantization = GlobalQuantization;
	}
	else
	{
		// Our Audio::EEventQuantization enumeration is 1 greater than the ETimeSynthEventClipQuantization to account for
		// the "Global" enumeration slot which is presented to users. We need to special-case it here. 
		int32 ClipQuantizationEnumIndex = (int32)InClip->ClipQuantization;
		check(ClipQuantizationEnumIndex >= 1);
		NewClipInfo.ClipQuantization = (Audio::EEventQuantization)(ClipQuantizationEnumIndex - 1);
	}

	// Pass this off to the clip info. This is going to use this to trigger the follow clip if it exists.
	NewClipInfo.SynthClip = InClip;
	NewClipInfo.VolumeScale = VolumeScale;
	NewClipInfo.PitchScale = PitchScale;
	NewClipInfo.DecodingSoundSourceHandle = NewDecoderHandle;
	NewClipInfo.StartFrameOffset = 0;
	NewClipInfo.CurrentFrameCount = 0;

	// Pass the handle to the clip
	NewClipInfo.Handle = NewHandle;

	FTimeSynthTimeDef ClipDuration = InClip->ClipDuration;
	FTimeSynthTimeDef FadeInTime = InClip->FadeInTime;
	FTimeSynthTimeDef FadeOutTime = InClip->FadeOutTime;

	// Send this new clip over to the audio render thread
	SynthCommand([this, NewClipInfo, ClipDuration, FadeInTime, FadeOutTime]
	{
		// Immediately create a mapping for this clip id to a free clip slot
		// It's possible that the clip might get state changes before it starts playing if
		// we're playing a very long-duration quantization
		int32 FreeClipIndex = -1;
		if (FreePlayingClipIndices_AudioRenderThread.Num() > 0)
		{
			FreeClipIndex = FreePlayingClipIndices_AudioRenderThread.Pop(false);
		}
		else
		{
			// Grow the pool size if we ran out of clips in the pool
			CurrentPoolSize++;
			FreePlayingClipIndices_AudioRenderThread.Add(CurrentPoolSize);
			FreeClipIndex = FreePlayingClipIndices_AudioRenderThread.Pop(false);
		}
		check(FreeClipIndex >= 0);

		// Copy over the clip info to the slot
		check(FreeClipIndex < PlayingClipsPool_AudioRenderThread.Num());
		PlayingClipsPool_AudioRenderThread[FreeClipIndex] = NewClipInfo;

		// Add a mapping of the clip handle id to the free index
		// This will allow us to reference the playing clip from BP, etc.
		ClipIdToClipIndexMap_AudioRenderThread.Add(NewClipInfo.Handle.ClipId, FreeClipIndex);

		// Queue an event quantization event up. 
		// The Event quantizer will execute the lambda on the exact frame of the quantization enumeration.
		// It's NumFramesOffset will be the number of frames within the current audio buffer to begin rendering the
		// audio at.
		EventQuantizer.EnqueueEvent(NewClipInfo.ClipQuantization, 

			[this, FreeClipIndex, ClipDuration, FadeInTime, FadeOutTime](uint32 NumFramesOffset)
			{
				FPlayingClipInfo& PlayingClipInfo = PlayingClipsPool_AudioRenderThread[FreeClipIndex];

				// Setup the duration of various things using the event quantizer
				PlayingClipInfo.DurationFrames = EventQuantizer.GetDurationInFrames(ClipDuration.NumBars, (float)ClipDuration.NumBeats);
				PlayingClipInfo.FadeInDurationFrames = EventQuantizer.GetDurationInFrames(FadeInTime.NumBars, (float)FadeInTime.NumBeats);
				PlayingClipInfo.FadeOutDurationFrames = EventQuantizer.GetDurationInFrames(FadeOutTime.NumBars, (float)FadeOutTime.NumBeats);
				PlayingClipInfo.StartFrameOffset = NumFramesOffset;

				// Add this clip to the list of active playing clips so it begins rendering
				ActivePlayingClipIndices_AudioRenderThread.Add(FreeClipIndex);
			});
	});
	
	return NewHandle;
}

void UTimeSynthComponent::StopClip(FTimeSynthClipHandle InClipHandle, ETimeSynthEventClipQuantization EventQuantization)
{
	Audio::EEventQuantization StopQuantization = GlobalQuantization;
	if (EventQuantization != ETimeSynthEventClipQuantization::Global)
	{
		int32 ClipQuantizationEnumIndex = (int32)EventQuantization;
		check(ClipQuantizationEnumIndex >= 1);
		StopQuantization = (Audio::EEventQuantization)(ClipQuantizationEnumIndex - 1);
	}

	SynthCommand([this, InClipHandle, StopQuantization]
	{
		EventQuantizer.EnqueueEvent(StopQuantization,

			[this, InClipHandle](uint32 NumFramesOffset)
			{
				int32* PlayingClipIndex = ClipIdToClipIndexMap_AudioRenderThread.Find(InClipHandle.ClipId);
				if (PlayingClipIndex)
				{
					// Grab the clip info
					FPlayingClipInfo& PlayingClipInfo = PlayingClipsPool_AudioRenderThread[*PlayingClipIndex];

					// Only do anything if the clip is not yet already fading
					if (PlayingClipInfo.CurrentFrameCount < PlayingClipInfo.DurationFrames)
					{
						// Adjust the duration of the clip to "spoof" it's code which triggers a fade this render callback block.
						PlayingClipInfo.DurationFrames = PlayingClipInfo.CurrentFrameCount + NumFramesOffset;
					}
				}
			});
	});
}

void UTimeSynthComponent::StopClipWithFadeOverride(FTimeSynthClipHandle InClipHandle, ETimeSynthEventClipQuantization EventQuantization, const FTimeSynthTimeDef& FadeTime)
{
	Audio::EEventQuantization StopQuantization = GlobalQuantization;
	if (EventQuantization != ETimeSynthEventClipQuantization::Global)
	{
		int32 ClipQuantizationEnumIndex = (int32)EventQuantization;
		check(ClipQuantizationEnumIndex >= 1);
		StopQuantization = (Audio::EEventQuantization)(ClipQuantizationEnumIndex - 1);
	}

	SynthCommand([this, InClipHandle, StopQuantization, FadeTime]
	{
		EventQuantizer.EnqueueEvent(StopQuantization,

			[this, InClipHandle, FadeTime](uint32 NumFramesOffset)
			{
				int32* PlayingClipIndex = ClipIdToClipIndexMap_AudioRenderThread.Find(InClipHandle.ClipId);
				if (PlayingClipIndex)
				{
					// Grab the clip info
					FPlayingClipInfo& PlayingClipInfo = PlayingClipsPool_AudioRenderThread[*PlayingClipIndex];

					// Only do anything if the clip is not yet already fading
					if (PlayingClipInfo.CurrentFrameCount < PlayingClipInfo.DurationFrames)
					{
						// Adjust the duration of the clip to "spoof" it's code which triggers a fade this render callback block.
						PlayingClipInfo.DurationFrames = PlayingClipInfo.CurrentFrameCount + NumFramesOffset;

						// Override the clip's fade out duration (but prevent pops so we can do a brief fade out at least)
						PlayingClipInfo.FadeOutDurationFrames = FMath::Max(EventQuantizer.GetDurationInFrames(FadeTime.NumBars, (float)FadeTime.NumBeats), 100u);
					}
				}
			});
	});
}

void UTimeSynthComponent::SetVolumeGroupInternal(FVolumeGroupData& InData, float VolumeDb, float FadeTimeSec)
{
	if (FadeTimeSec == 0.0f)
	{
		InData.CurrentVolumeDb = VolumeDb;
		InData.StartVolumeDb = VolumeDb;
	}
	else
	{
		InData.StartVolumeDb = InData.CurrentVolumeDb;
	}
	InData.TargetVolumeDb = VolumeDb;

	InData.CurrentTime = 0.0f;
	InData.TargetFadeTime = FadeTimeSec;
}

void UTimeSynthComponent::SetVolumeGroup(UTimeSynthVolumeGroup* InVolumeGroup, float VolumeDb, float FadeTimeSec)
{
	VolumeGroupUniqueId Id = InVolumeGroup->GetUniqueID();
	FVolumeGroupData* VolumeGroup = VolumeGroupData.Find(Id);

	// If no volume group exists, there are no clips playing on that volume group, just create a slot for it.
	// New clips that are playing on this group will just get the volume set here.
	if (!VolumeGroup)
	{
		FVolumeGroupData NewData;
		SetVolumeGroupInternal(NewData, VolumeDb, FadeTimeSec);
		VolumeGroupData.Add(Id, NewData);
	}
	else
	{
		SetVolumeGroupInternal(*VolumeGroup, VolumeDb, FadeTimeSec);
	}
}

void UTimeSynthComponent::StopSoundsOnVolumeGroup(UTimeSynthVolumeGroup* InVolumeGroup, ETimeSynthEventClipQuantization EventQuantization)
{
	VolumeGroupUniqueId Id = InVolumeGroup->GetUniqueID();
	FVolumeGroupData* VolumeGroupEntry = VolumeGroupData.Find(Id);

	if (VolumeGroupEntry)
	{
		for (FTimeSynthClipHandle& ClipHandle : VolumeGroupEntry->Clips)
		{
			StopClip(ClipHandle, EventQuantization);
		}
	}
}

void UTimeSynthComponent::StopSoundsOnVolumeGroupWithFadeOverride(UTimeSynthVolumeGroup* InVolumeGroup, ETimeSynthEventClipQuantization EventQuantization, const FTimeSynthTimeDef& FadeTime)
{
	VolumeGroupUniqueId Id = InVolumeGroup->GetUniqueID();
	FVolumeGroupData* VolumeGroupEntry = VolumeGroupData.Find(Id);

	if (VolumeGroupEntry)
	{
		for (FTimeSynthClipHandle& ClipHandle : VolumeGroupEntry->Clips)
		{
			StopClipWithFadeOverride(ClipHandle, EventQuantization, FadeTime);
		}
	}
}

TArray<FTimeSynthSpectralData> UTimeSynthComponent::GetSpectralData() const
{
	if (bEnableSpectralAnalysis)
	{
		return SpectralData;
	}
	// Return empty array if not analyzing spectra
	return TArray<FTimeSynthSpectralData>();
}

