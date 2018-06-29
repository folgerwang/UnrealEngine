// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AudioMixerSource.h"
#include "AudioMixerDevice.h"
#include "AudioMixerSourceVoice.h"
#include "ActiveSound.h"
#include "IAudioExtensionPlugin.h"
#include "Sound/AudioSettings.h"
#include "ContentStreaming.h"

static int32 DisableHRTFCvar = 0;
FAutoConsoleVariableRef CVarDisableHRTF(
	TEXT("au.DisableHRTF"),
	DisableHRTFCvar,
	TEXT("Disables HRTF\n")
	TEXT("0: Not Disabled, 1: Disabled"),
	ECVF_Default);

namespace Audio
{
	FMixerSource::FMixerSource(FAudioDevice* InAudioDevice)
		: FSoundSource(InAudioDevice)
		, MixerDevice((FMixerDevice*)InAudioDevice)
		, MixerBuffer(nullptr)
		, MixerSourceBuffer(nullptr)
		, MixerSourceVoice(nullptr)		
		, PreviousAzimuth(-1.0f)
		, InitializationState(EMixerSourceInitializationState::NotInitialized)
		, bPlayedCachedBuffer(false)
		, bPlaying(false)
		, bLoopCallback(false)
		, bIsDone(false)
		, bIsEffectTailsDone(false)
		, bIsPlayingEffectTails(false)
		, bEditorWarnedChangedSpatialization(false)
		, bUsingHRTFSpatialization(false)
		, bIs3D(false)
		, bDebugMode(false)
		, bIsVorbis(false)
		, bIsStoppingVoicesEnabled(InAudioDevice->IsStoppingVoicesEnabled())
	{
	}

	FMixerSource::~FMixerSource()
	{
		FreeResources();
	}

	bool FMixerSource::Init(FWaveInstance* InWaveInstance)
	{
		AUDIO_MIXER_CHECK(MixerBuffer);
		AUDIO_MIXER_CHECK(MixerBuffer->IsRealTimeSourceReady());

		// We've already been passed the wave instance in PrepareForInitialization, make sure we have the same one
		AUDIO_MIXER_CHECK(WaveInstance && WaveInstance == InWaveInstance);

		LLM_SCOPE(ELLMTag::AudioMixer);

		FSoundSource::InitCommon();

		// Get the number of frames before creating the buffer
		int32 NumFrames = INDEX_NONE;

		AUDIO_MIXER_CHECK(WaveInstance->WaveData);

		if (WaveInstance->WaveData->DecompressionType != DTYPE_Procedural)
		{
			const int32 NumBytes = WaveInstance->WaveData->RawPCMDataSize;
			NumFrames = NumBytes / (WaveInstance->WaveData->NumChannels * sizeof(int16));
		}

		// Unfortunately, we need to know if this is a vorbis source since channel maps are different for 5.1 vorbis files
		bIsVorbis = WaveInstance->WaveData->bDecompressedFromOgg;

		bIsStoppingVoicesEnabled = ((FAudioDevice*)MixerDevice)->IsStoppingVoicesEnabled();
		
		bIsStopping = false;
		bIsEffectTailsDone = true;
		bIsDone = false;

		FSoundBuffer* SoundBuffer = static_cast<FSoundBuffer*>(MixerBuffer);
		if (SoundBuffer->NumChannels > 0)
		{
			SCOPE_CYCLE_COUNTER(STAT_AudioSourceInitTime);

			AUDIO_MIXER_CHECK(MixerDevice);
			MixerSourceVoice = MixerDevice->GetMixerSourceVoice();
			if (!MixerSourceVoice)
			{
				return false;
			}

			// Initialize the source voice with the necessary format information
			FMixerSourceVoiceInitParams InitParams;
			InitParams.SourceListener = this;
			InitParams.NumInputChannels = WaveInstance->WaveData->NumChannels;
			InitParams.NumInputFrames = NumFrames;
			InitParams.SourceVoice = MixerSourceVoice;
			InitParams.bUseHRTFSpatialization = UseObjectBasedSpatialization();
			InitParams.bIsAmbisonics = WaveInstance->bIsAmbisonics;
			if (InitParams.bIsAmbisonics)
			{
				checkf(InitParams.NumInputChannels == 4, TEXT("Only allow 4 channel source if file is ambisonics format."));
			}
			InitParams.AudioComponentUserID = WaveInstance->ActiveSound->GetAudioComponentUserID();

			InitParams.AudioComponentID = WaveInstance->ActiveSound->GetAudioComponentID();

			InitParams.EnvelopeFollowerAttackTime = WaveInstance->EnvelopeFollowerAttackTime;
			InitParams.EnvelopeFollowerReleaseTime = WaveInstance->EnvelopeFollowerReleaseTime;

			InitParams.SourceEffectChainId = 0;

			// Source manager needs to know if this is a vorbis source for rebuilding speaker maps
			InitParams.bIsVorbis = bIsVorbis;

			if (InitParams.NumInputChannels <= 2)
			{
				if (WaveInstance->SourceEffectChain)
				{
					InitParams.SourceEffectChainId = WaveInstance->SourceEffectChain->GetUniqueID();

					for (int32 i = 0; i < WaveInstance->SourceEffectChain->Chain.Num(); ++i)
					{
						InitParams.SourceEffectChain.Add(WaveInstance->SourceEffectChain->Chain[i]);
						InitParams.bPlayEffectChainTails = WaveInstance->SourceEffectChain->bPlayEffectChainTails;
					}
				}

				// Only need to care about effect chain tails finishing if we're told to play them
				if (InitParams.bPlayEffectChainTails)
				{
					bIsEffectTailsDone = false;
				}

				// Setup the bus Id if this source is a bus
				if (WaveInstance->WaveData->bIsBus)
				{
					InitParams.BusId = WaveInstance->WaveData->GetUniqueID();
					if (!WaveInstance->WaveData->IsLooping())
					{
						InitParams.BusDuration = WaveInstance->WaveData->GetDuration();
					}
				}

				// Toggle muting the source if sending only to output bus. 
				// This can get set even if the source doesn't have bus sends since bus sends can be dynamically enabled.
				InitParams.bOutputToBusOnly = WaveInstance->bOutputToBusOnly;

				// If this source is sending its audio to a bus
				for (int32 BusSendType = 0; BusSendType < (int32)EBusSendType::Count; ++BusSendType)
				{
					// And add all the source bus sends
					for (FSoundSourceBusSendInfo& SendInfo : WaveInstance->SoundSourceBusSends[BusSendType])
					{
						if (SendInfo.SoundSourceBus != nullptr)
						{
							FMixerBusSend BusSend;
							BusSend.BusId = SendInfo.SoundSourceBus->GetUniqueID();
							BusSend.SendLevel = SendInfo.SendLevel;
							InitParams.BusSends[BusSendType].Add(BusSend);
						}
					}
				}
			}

			// Don't set up any submixing if we're set to output to bus only
			if (!InitParams.bOutputToBusOnly)
			{
				// If we're spatializing using HRTF and its an external send, don't need to setup a default/base submix send to master or EQ submix
				// We'll only be using non-default submix sends (e.g. reverb).
				if (!(WaveInstance->SpatializationMethod == ESoundSpatializationAlgorithm::SPATIALIZATION_HRTF && MixerDevice->bSpatializationIsExternalSend))
				{
					// If this sound is an ambisonics file, we preempt the normal base submix routing and only send to master ambisonics submix
					if (WaveInstance->bIsAmbisonics)
					{
						FMixerSourceSubmixSend SubmixSend;
						SubmixSend.Submix = MixerDevice->GetMasterAmbisonicsSubmix();
						SubmixSend.SendLevel = 1.0f;
						SubmixSend.bIsMainSend = true;
						InitParams.SubmixSends.Add(SubmixSend);
					}
					else
					{
						// If we've overridden which submix we're sending the sound, then add that as the first send
						if (WaveInstance->SoundSubmix != nullptr)
						{
							FMixerSourceSubmixSend SubmixSend;
							SubmixSend.Submix = MixerDevice->GetSubmixInstance(WaveInstance->SoundSubmix);
							SubmixSend.SendLevel = 1.0f;
							SubmixSend.bIsMainSend = true;
							InitParams.SubmixSends.Add(SubmixSend);
						}
						else
						{
							// Send the voice to the EQ submix if it's enabled
							const bool bIsEQDisabled = GetDefault<UAudioSettings>()->bDisableMasterEQ;
							bool bUseMaster = true;
							if (!bIsEQDisabled && IsEQFilterApplied())
							{
								if (MixerDevice->GetMasterEQSubmix().IsValid())
								{
									// Default the submix to use to use the master submix if none are set
									FMixerSourceSubmixSend SubmixSend;
									SubmixSend.Submix = MixerDevice->GetMasterEQSubmix();
									SubmixSend.SendLevel = 1.0f;
									SubmixSend.bIsMainSend = true;
									InitParams.SubmixSends.Add(SubmixSend);
									bUseMaster = false;
								}
							}

							if (bUseMaster)
							{
								// Default the submix to use to use the master submix if none are set
								FMixerSourceSubmixSend SubmixSend;
								SubmixSend.Submix = MixerDevice->GetMasterSubmix();
								SubmixSend.SendLevel = 1.0f;
								SubmixSend.bIsMainSend = true;
								InitParams.SubmixSends.Add(SubmixSend);
							}
						}
					}
				}

				// Now add any addition submix sends for this source
				for (FSoundSubmixSendInfo& SendInfo : WaveInstance->SoundSubmixSends)
				{
					if (SendInfo.SoundSubmix != nullptr)
					{
						FMixerSourceSubmixSend SubmixSend;
						SubmixSend.Submix = MixerDevice->GetSubmixInstance(SendInfo.SoundSubmix);
						SubmixSend.SendLevel = SendInfo.SendLevel;
						SubmixSend.bIsMainSend = false;
						InitParams.SubmixSends.Add(SubmixSend);
					}
				}
			}

			// Loop through all submix sends to figure out what speaker maps this source is using
			for (FMixerSourceSubmixSend& Send : InitParams.SubmixSends)
			{
				ESubmixChannelFormat SubmixChannelType = Send.Submix->GetSubmixChannels();
				ChannelMaps[(int32)SubmixChannelType].bUsed = true;
				ChannelMaps[(int32)SubmixChannelType].ChannelMap.Reset();
			}

			// Check to see if this sound has been flagged to be in debug mode
#if AUDIO_MIXER_ENABLE_DEBUG_MODE
			InitParams.DebugName = WaveInstance->GetName();

			bool bIsDebug = false;
			FString WaveInstanceName = WaveInstance->GetName(); //-V595
			FString TestName = GEngine->GetAudioDeviceManager()->GetAudioMixerDebugSoundName();
			if (WaveInstanceName.Contains(TestName))
			{
				bDebugMode = true;
				InitParams.bIsDebugMode = bDebugMode;
			}
#endif

			// Whether or not we're 3D
			bIs3D = !UseObjectBasedSpatialization() && WaveInstance->bUseSpatialization && SoundBuffer->NumChannels < 3;

			// Grab the source's reverb plugin settings 
			InitParams.SpatializationPluginSettings = UseSpatializationPlugin() ? WaveInstance->SpatializationPluginSettings : nullptr;

			// Grab the source's occlusion plugin settings 
			InitParams.OcclusionPluginSettings = UseOcclusionPlugin() ? WaveInstance->OcclusionPluginSettings : nullptr;

			// Grab the source's reverb plugin settings 
			InitParams.ReverbPluginSettings = UseReverbPlugin() ? WaveInstance->ReverbPluginSettings : nullptr;

			// We support reverb
			SetReverbApplied(true);

			// Update the buffer sample rate to the wave instance sample rate in case it was serialized incorrectly
			MixerBuffer->InitSampleRate(WaveInstance->WaveData->GetSampleRateForCurrentPlatform());

			// Now we init the mixer source buffer
			MixerSourceBuffer->Init();

			// Hand off the mixer source buffer decoder
			InitParams.MixerSourceBuffer = MixerSourceBuffer;
			MixerSourceBuffer = nullptr;

			if (MixerSourceVoice->Init(InitParams))
			{
				InitializationState = EMixerSourceInitializationState::Initialized;

				Update();

				return true;
			}
			else
			{
				InitializationState = EMixerSourceInitializationState::NotInitialized;
			}
		}
		return false;
	}

	void FMixerSource::Update()
	{
		SCOPE_CYCLE_COUNTER(STAT_AudioUpdateSources);

		LLM_SCOPE(ELLMTag::AudioMixer);

		if (!WaveInstance || !MixerSourceVoice || Paused || InitializationState == EMixerSourceInitializationState::NotInitialized)
		{
			return;
		}

		++TickCount;

		UpdatePitch();

		UpdateVolume();

		UpdateSpatialization();

		UpdateEffects();

		UpdateChannelMaps();

		FSoundSource::DrawDebugInfo();
	}

	bool FMixerSource::PrepareForInitialization(FWaveInstance* InWaveInstance)
	{
		LLM_SCOPE(ELLMTag::AudioMixer);

		// We are currently not supporting playing audio on a controller
		if (InWaveInstance->OutputTarget == EAudioOutputTarget::Controller)
		{
			return false;
		}

		// We are not initialized yet. We won't be until the sound file finishes loading and parsing the header.
		InitializationState = EMixerSourceInitializationState::Initializing;

		//  Reset so next instance will warn if algorithm changes in-flight
		bEditorWarnedChangedSpatialization = false;

		check(InWaveInstance);
		check(MixerBuffer == nullptr);
		check(AudioDevice);

		bool bIsSeeking = InWaveInstance->StartTime > 0.0f;
		MixerBuffer = FMixerBuffer::Init(AudioDevice, InWaveInstance->WaveData, bIsSeeking);
		if (MixerBuffer)
		{
			Buffer = MixerBuffer;
			WaveInstance = InWaveInstance;

			LPFFrequency = MAX_FILTER_FREQUENCY;
			LastLPFFrequency = FLT_MAX;

			HPFFrequency = 0.0;
			LastHPFFrequency = FLT_MAX;

			bIsDone = false;

			// Not all wave data types have a non-zero duration
			if (InWaveInstance->WaveData->Duration > 0)
			{
				if (!InWaveInstance->WaveData->bIsBus)
				{
					NumTotalFrames = InWaveInstance->WaveData->Duration * InWaveInstance->WaveData->GetSampleRateForCurrentPlatform();
					check(NumTotalFrames > 0);
				}
				else if (!InWaveInstance->WaveData->IsLooping())
				{
					NumTotalFrames = InWaveInstance->WaveData->Duration * AudioDevice->GetSampleRate();
					check(NumTotalFrames > 0);
				}
			}

			check(!MixerSourceBuffer.IsValid());
			MixerSourceBuffer = TSharedPtr<FMixerSourceBuffer>(new FMixerSourceBuffer());

			if (MixerSourceBuffer->PreInit(MixerBuffer, InWaveInstance->WaveData, InWaveInstance->LoopingMode, bIsSeeking))
			{
				// We succeeded in preparing the buffer for initialization, but we are not technically initialized yet.
				// If the buffer is asynchronously preparing a file-handle, we may not yet initialize the source.
				return true;
			}
		}

		// Something went wrong with initializing the generator
		return false;
	}

	bool FMixerSource::IsPreparedToInit()
	{
		LLM_SCOPE(ELLMTag::AudioMixer);

		if (MixerBuffer && MixerBuffer->IsRealTimeSourceReady())
		{
			check(MixerSourceBuffer.IsValid());

			// Check if we have a realtime audio task already (doing first decode)
			if (MixerSourceBuffer->IsAsyncTaskInProgress())
			{
				// not ready
				return MixerSourceBuffer->IsAsyncTaskDone();
			}
			else if (WaveInstance)
			{
				if (WaveInstance->WaveData->bIsBus)
				{
					// Buses don't need to do anything to play audio
					return true;
				}
				else
				{
					// Now check to see if we need to kick off a decode the first chunk of audio
					const EBufferType::Type BufferType = MixerBuffer->GetType();
					if ((BufferType == EBufferType::PCMRealTime || BufferType == EBufferType::Streaming) && WaveInstance->WaveData)
					{
						// If any of these conditions meet, we need to do an initial async decode before we're ready to start playing the sound
						if (WaveInstance->StartTime > 0.0f || WaveInstance->WaveData->bProcedural || WaveInstance->WaveData->bIsBus || !WaveInstance->WaveData->CachedRealtimeFirstBuffer)
						{
							// Before reading more PCMRT data, we first need to seek the buffer
							if (WaveInstance->StartTime > 0.0f && !WaveInstance->WaveData->bIsBus && !WaveInstance->WaveData->bProcedural)
							{
								MixerBuffer->Seek(WaveInstance->StartTime);
							}

							check(MixerSourceBuffer.IsValid());
							MixerSourceBuffer->ReadMoreRealtimeData(0, EBufferReadMode::Asynchronous);

							// not ready
							return false;
						}
					}
				}
			}

			return true;
		}

		return false;
	}

	bool FMixerSource::IsInitialized() const
	{
		return InitializationState == EMixerSourceInitializationState::Initialized;
	}

	void FMixerSource::Play()
	{
		if (!WaveInstance)
		{
			return;
		}

		// It's possible if Pause and Play are called while a sound is async initializing. In this case
		// we'll just not actually play the source here. Instead we'll call play when the sound finishes loading.
		if (MixerSourceVoice && InitializationState == EMixerSourceInitializationState::Initialized)
		{
			MixerSourceVoice->Play();
		}

		bIsStopping = false;
		Paused = false;
		Playing = true;
		bLoopCallback = false;
		bIsDone = false;
	}

	void FMixerSource::Stop()
	{
		LLM_SCOPE(ELLMTag::AudioMixer);

		if (!MixerSourceVoice)
		{
			StopNow();
			return;
		}

		if (bIsDone)
		{
			StopNow();
		}
		else if (!bIsStopping)
		{
			// Otherwise, we need to do a quick fade-out of the sound and put the state
			// of the sound into "stopping" mode. This prevents this source from
			// being put into the "free" pool and prevents the source from freeing its resources
			// until the sound has finished naturally (i.e. faded all the way out)

			// StopFade will stop a sound with a very small fade to avoid discontinuities
			if (MixerSourceVoice && Playing)
			{
				if (bIsStoppingVoicesEnabled && !WaveInstance->WaveData->bProcedural)
				{
					// Let the wave instance know it's stopping
					WaveInstance->SetStopping(true);

					// TODO: parameterize the number of fades
					MixerSourceVoice->StopFade(512);
					bIsStopping = true;
				}
				else
				{
					StopNow();
				}
			}

			Paused = false;
		}
	}

	void FMixerSource::StopNow()
	{
		LLM_SCOPE(ELLMTag::AudioMixer);

		// Immediately stop the sound source

		InitializationState = EMixerSourceInitializationState::NotInitialized;
		
		IStreamingManager::Get().GetAudioStreamingManager().RemoveStreamingSoundSource(this);

		bIsStopping = false;

		if (WaveInstance)
		{
			if (MixerSourceVoice && Playing)
			{
				MixerSourceVoice->Stop();
			}

			Paused = false;
			Playing = false;

			FreeResources();
		}

		FSoundSource::Stop();
	}

	void FMixerSource::Pause()
	{
		if (!WaveInstance)
		{
			return;
		}

		if (MixerSourceVoice)
		{
			MixerSourceVoice->Pause();
		}

		Paused = true;
	}

	bool FMixerSource::IsFinished()
	{
		// A paused source is not finished.
		if (Paused)
		{
			return false;
		}

		if (InitializationState == EMixerSourceInitializationState::NotInitialized)
		{
			return true;
		}

		if (InitializationState == EMixerSourceInitializationState::Initializing)
		{
			return false;
		}

		if (WaveInstance && MixerSourceVoice)
		{
			if (bIsDone && bIsEffectTailsDone)
			{
				WaveInstance->NotifyFinished();
				bIsStopping = false;
				return true;
			}
			else if (bLoopCallback && WaveInstance->LoopingMode == LOOP_WithNotification)
			{
				WaveInstance->NotifyFinished();
				bLoopCallback = false;
			}

			return false;
		}
		return true;
	}

	FString FMixerSource::Describe(bool bUseLongName)
	{
		return FString(TEXT("Stub"));
	}

	float FMixerSource::GetPlaybackPercent() const
	{
		if (MixerSourceVoice && NumTotalFrames > 0)
		{
			int64 NumFrames = MixerSourceVoice->GetNumFramesPlayed();
			AUDIO_MIXER_CHECK(NumTotalFrames > 0);
			float PlaybackPercent = (float)NumFrames / NumTotalFrames;
			if (WaveInstance->LoopingMode == LOOP_Never)
			{
				PlaybackPercent = FMath::Min(PlaybackPercent, 1.0f);
			}
			return PlaybackPercent;
		}
		else
		{
			// If we don't have any frames, that means it's a procedural sound wave, which means
			// that we're never going to have a playback percentage.
			return 1.0f;
		}
	}

	float FMixerSource::GetEnvelopeValue() const
	{
		if (MixerSourceVoice)
		{
			return MixerSourceVoice->GetEnvelopeValue();
		}
		return 0.0f;
	}

	void FMixerSource::OnBeginGenerate()
	{
	}

	void FMixerSource::OnDone()
	{
		bIsDone = true;
	}


	void FMixerSource::OnEffectTailsDone()
	{
		bIsEffectTailsDone = true;
	}

	void FMixerSource::FreeResources()
	{
		LLM_SCOPE(ELLMTag::AudioMixer);

		if (MixerBuffer)
		{
			MixerBuffer->EnsureHeaderParseTaskFinished();
		}

		check(!bIsStopping);
		check(!Playing);

		// Make a new pending release data ptr to pass off release data
		if (MixerSourceVoice)
		{
			// We're now "releasing" so don't recycle this voice until we get notified that the source has finished
			bIsReleasing = true;

			// This will trigger FMixerSource::OnRelease from audio render thread.
			MixerSourceVoice->Release();
			MixerSourceVoice = nullptr;
		}

		MixerSourceBuffer = nullptr;
		MixerBuffer = nullptr;
		Buffer = nullptr;
		bLoopCallback = false;
		NumTotalFrames = 0;

		// Reset the source's channel maps
		for (int32 i = 0; i < (int32)ESubmixChannelFormat::Count; ++i)
		{
			ChannelMaps[i].bUsed = false;
			ChannelMaps[i].ChannelMap.Reset();
		}
	}

	void FMixerSource::UpdatePitch()
	{
		AUDIO_MIXER_CHECK(MixerBuffer);

		check(WaveInstance);

		Pitch = WaveInstance->Pitch;

		// Don't apply global pitch scale to UI sounds
		if (!WaveInstance->bIsUISound)
		{
			Pitch *= AudioDevice->GetGlobalPitchScale().GetValue();
		}

		Pitch = FMath::Clamp<float>(Pitch, AUDIO_MIXER_MIN_PITCH, AUDIO_MIXER_MAX_PITCH);

		// Scale the pitch by the ratio of the audio buffer sample rate and the actual sample rate of the hardware
		if (MixerBuffer)
		{
			const float MixerBufferSampleRate = MixerBuffer->GetSampleRate();
			const float AudioDeviceSampleRate = AudioDevice->GetSampleRate();
			Pitch *= MixerBufferSampleRate / AudioDeviceSampleRate;

			MixerSourceVoice->SetPitch(Pitch);
		}
	}

	void FMixerSource::UpdateVolume()
	{
		float CurrentVolume;
		if (AudioDevice->IsAudioDeviceMuted())
		{
			CurrentVolume = 0.0f;
		}
		else
		{
			CurrentVolume = WaveInstance->GetVolume();
			CurrentVolume *= WaveInstance->GetVolumeApp();
			CurrentVolume *= AudioDevice->GetPlatformAudioHeadroom();
			CurrentVolume = FMath::Clamp<float>(GetDebugVolume(CurrentVolume), 0.0f, MAX_VOLUME);
		}

		MixerSourceVoice->SetVolume(CurrentVolume);
		MixerSourceVoice->SetDistanceAttenuation(WaveInstance->GetDistanceAttenuation());
	}

	void FMixerSource::UpdateSpatialization()
	{
		SpatializationParams = GetSpatializationParams();
		if (WaveInstance->bUseSpatialization)
		{
			MixerSourceVoice->SetSpatializationParams(SpatializationParams);
		}
	}

	void FMixerSource::UpdateEffects()
	{
		// Update the default LPF filter frequency 
		SetFilterFrequency();

		if (LastLPFFrequency != LPFFrequency)
		{
			MixerSourceVoice->SetLPFFrequency(LPFFrequency);
			LastLPFFrequency = LPFFrequency;
		}

		if (LastHPFFrequency != HPFFrequency)
		{
			MixerSourceVoice->SetHPFFrequency(HPFFrequency);
			LastHPFFrequency = HPFFrequency;
		}

		// If reverb is applied, figure out how of the source to "send" to the reverb.
		if (bReverbApplied)
		{
			float ReverbSendLevel = 0.0f;
			ChannelMaps[(int32)ESubmixChannelFormat::Device].bUsed = true;

			if (WaveInstance->ReverbSendMethod == EReverbSendMethod::Manual)
			{
				ReverbSendLevel = FMath::Clamp(WaveInstance->ManualReverbSendLevel, 0.0f, 1.0f);
			}
			else
			{
				// The alpha value is determined identically between manual and custom curve methods
				const FVector2D& ReverbSendRadialRange = WaveInstance->ReverbSendLevelDistanceRange;
				const float Denom = FMath::Max(ReverbSendRadialRange.Y - ReverbSendRadialRange.X, 1.0f);
				const float Alpha = FMath::Clamp((WaveInstance->ListenerToSoundDistance - ReverbSendRadialRange.X) / Denom, 0.0f, 1.0f);

				if (WaveInstance->ReverbSendMethod == EReverbSendMethod::Linear)
				{
					ReverbSendLevel = FMath::Clamp(FMath::Lerp(WaveInstance->ReverbSendLevelRange.X, WaveInstance->ReverbSendLevelRange.Y, Alpha), 0.0f, 1.0f);
				}
				else
				{
					ReverbSendLevel = FMath::Clamp(WaveInstance->CustomRevebSendCurve.GetRichCurveConst()->Eval(Alpha), 0.0f, 1.0f);
				}
			}

			// Send the source audio to the reverb plugin if enabled
			if (UseReverbPlugin())
			{
				if (MixerDevice->GetMasterReverbPluginSubmix().IsValid())
				{
					MixerSourceVoice->SetSubmixSendInfo(MixerDevice->GetMasterReverbPluginSubmix(), ReverbSendLevel);
				}
			}
			else 
			{
				// Send the source audio to the master reverb
				if (MixerDevice->GetMasterReverbSubmix().IsValid())
				{
					MixerSourceVoice->SetSubmixSendInfo(MixerDevice->GetMasterReverbSubmix(), ReverbSendLevel);
				}
			}
		}

		for (FSoundSubmixSendInfo& SendInfo : WaveInstance->SoundSubmixSends)
		{
			if (SendInfo.SoundSubmix != nullptr)
			{
				FMixerSubmixPtr SubmixInstance = MixerDevice->GetSubmixInstance(SendInfo.SoundSubmix);
				MixerSourceVoice->SetSubmixSendInfo(SubmixInstance, SendInfo.SendLevel);

				// Make sure we flag that we're using this submix sends since these can be dynamically added from BP
				// If we don't flag this then these channel maps won't be generated for this channel format
				ChannelMaps[(int32)SendInfo.SoundSubmix->ChannelFormat].bUsed = true;
			}
		}
	}

	void FMixerSource::UpdateChannelMaps()
	{
		SetStereoBleed();

		SetLFEBleed();

		int32 NumOutputDeviceChannels = MixerDevice->GetNumDeviceChannels();
		const FAudioPlatformDeviceInfo& DeviceInfo = MixerDevice->GetPlatformDeviceInfo();

		// Compute a new speaker map for each possible output channel mapping for the source
		for (int32 i = 0; i < (int32)ESubmixChannelFormat::Count; ++i)
		{
			FChannelMapInfo& ChannelMapInfo = ChannelMaps[i];
			if (ChannelMapInfo.bUsed)
			{
				ESubmixChannelFormat ChannelType = (ESubmixChannelFormat)i;

				// We don't need to compute speaker maps for ambisonics channel maps since we're not doing downmixing on ambisonics sources
				if (ChannelType != ESubmixChannelFormat::Ambisonics)
				{
					check(Buffer);
					if (ComputeChannelMap(ChannelType, Buffer->NumChannels, ChannelMapInfo.ChannelMap))
					{
						MixerSourceVoice->SetChannelMap(ChannelType, ChannelMapInfo.ChannelMap, bIs3D, WaveInstance->bCenterChannelOnly);
					}
				}
			}
		}
	}

	bool FMixerSource::ComputeMonoChannelMap(const ESubmixChannelFormat SubmixChannelType, Audio::AlignedFloatBuffer& OutChannelMap)
	{
		if (UseObjectBasedSpatialization())
		{
			if (WaveInstance->SpatializationMethod != ESoundSpatializationAlgorithm::SPATIALIZATION_HRTF && !bEditorWarnedChangedSpatialization)
			{
				bEditorWarnedChangedSpatialization = true;
				UE_LOG(LogAudioMixer, Warning, TEXT("Changing the spatialization method on a playing sound is not supported (WaveInstance: %s)"), *WaveInstance->WaveData->GetFullName());
			}

			// Treat the source as if it is a 2D stereo source
			return ComputeStereoChannelMap(SubmixChannelType, OutChannelMap);
		}
		else if (WaveInstance->bUseSpatialization && (!FMath::IsNearlyEqual(WaveInstance->AbsoluteAzimuth, PreviousAzimuth, 0.01f) || MixerSourceVoice->NeedsSpeakerMap()))
		{
			// Don't need to compute the source channel map if the absolute azimuth hasn't changed much
			PreviousAzimuth = WaveInstance->AbsoluteAzimuth;
			OutChannelMap.Reset();
			MixerDevice->Get3DChannelMap(SubmixChannelType, WaveInstance, WaveInstance->AbsoluteAzimuth, SpatializationParams.NormalizedOmniRadius, OutChannelMap);
			return true;
		}
		else if (!OutChannelMap.Num())
		{
			// Only need to compute the 2D channel map once
			MixerDevice->Get2DChannelMap(bIsVorbis, SubmixChannelType, 1, WaveInstance->bCenterChannelOnly, OutChannelMap);
			return true;
		}

		// Return false means the channel map hasn't changed
		return false;
	}

	bool FMixerSource::ComputeStereoChannelMap(const ESubmixChannelFormat InSubmixChannelType, Audio::AlignedFloatBuffer& OutChannelMap)
	{
		if (!UseObjectBasedSpatialization() && WaveInstance->bUseSpatialization && (!FMath::IsNearlyEqual(WaveInstance->AbsoluteAzimuth, PreviousAzimuth, 0.01f) || MixerSourceVoice->NeedsSpeakerMap()))
		{
			// Make sure our stereo emitter positions are updated relative to the sound emitter position
			UpdateStereoEmitterPositions();

			float AzimuthOffset = 0.0f;
			if (WaveInstance->ListenerToSoundDistance > 0.0f)
			{
				AzimuthOffset = FMath::Atan(0.5f * WaveInstance->StereoSpread / WaveInstance->ListenerToSoundDistance);
				AzimuthOffset = FMath::RadiansToDegrees(AzimuthOffset);
			}

			float LeftAzimuth = WaveInstance->AbsoluteAzimuth - AzimuthOffset;
			if (LeftAzimuth < 0.0f)
			{
				LeftAzimuth += 360.0f;
			}

			float RightAzimuth = WaveInstance->AbsoluteAzimuth + AzimuthOffset;
			if (RightAzimuth > 360.0f)
			{
				RightAzimuth -= 360.0f;
			}

			// Reset the channel map, the stereo spatialization channel mapping calls below will append their mappings
			OutChannelMap.Reset();

			MixerDevice->Get3DChannelMap(InSubmixChannelType, WaveInstance, LeftAzimuth, SpatializationParams.NormalizedOmniRadius, OutChannelMap);
			MixerDevice->Get3DChannelMap(InSubmixChannelType, WaveInstance, RightAzimuth, SpatializationParams.NormalizedOmniRadius, OutChannelMap);

			return true;
		}
		else if (!OutChannelMap.Num())
		{
			MixerDevice->Get2DChannelMap(bIsVorbis, InSubmixChannelType, 2, WaveInstance->bCenterChannelOnly, OutChannelMap);
			return true;
		}

		return false;
	}

	bool FMixerSource::ComputeChannelMap(const ESubmixChannelFormat InSubmixChannelType, const int32 NumSourceChannels, Audio::AlignedFloatBuffer& OutChannelMap)
	{
		if (NumSourceChannels == 1)
		{
			return ComputeMonoChannelMap(InSubmixChannelType, OutChannelMap);
		}
		else if (NumSourceChannels == 2)
		{
			return ComputeStereoChannelMap(InSubmixChannelType, OutChannelMap);
		}
		else if (!OutChannelMap.Num())
		{
			MixerDevice->Get2DChannelMap(bIsVorbis, InSubmixChannelType, NumSourceChannels, WaveInstance->bCenterChannelOnly, OutChannelMap);
			return true;
		}
		return false;
	}

	bool FMixerSource::UseObjectBasedSpatialization() const
	{
		return (Buffer->NumChannels == 1 &&
				AudioDevice->IsSpatializationPluginEnabled() &&
				DisableHRTFCvar == 0 &&
				WaveInstance->SpatializationMethod == ESoundSpatializationAlgorithm::SPATIALIZATION_HRTF);
	}

	bool FMixerSource::UseSpatializationPlugin() const
	{
		return (Buffer->NumChannels == 1) &&
			AudioDevice->IsSpatializationPluginEnabled() &&
			WaveInstance->SpatializationPluginSettings != nullptr;
	}

	bool FMixerSource::UseOcclusionPlugin() const
	{
		return (Buffer->NumChannels == 1 || Buffer->NumChannels == 2) &&
				AudioDevice->IsOcclusionPluginEnabled() &&
				WaveInstance->OcclusionPluginSettings != nullptr;
	}

	bool FMixerSource::UseReverbPlugin() const
	{
		return (Buffer->NumChannels == 1 || Buffer->NumChannels == 2) &&
				AudioDevice->IsReverbPluginEnabled() &&
				WaveInstance->ReverbPluginSettings != nullptr;
	}

}
