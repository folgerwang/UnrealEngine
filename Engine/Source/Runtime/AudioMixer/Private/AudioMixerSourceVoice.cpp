// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AudioMixerSourceVoice.h"
#include "AudioMixerSource.h"
#include "AudioMixerSourceManager.h"
#include "AudioMixerDevice.h"

namespace Audio
{

	/**
	* FMixerSourceVoice Implementation
	*/

	FMixerSourceVoice::FMixerSourceVoice()
	{
		Reset(nullptr);
	}

	FMixerSourceVoice::~FMixerSourceVoice()
	{
	}

	void FMixerSourceVoice::Reset(FMixerDevice* InMixerDevice)
	{
		if (InMixerDevice)
		{
			MixerDevice = InMixerDevice;
			SourceManager = MixerDevice->GetSourceManager();
		}
		else
		{
			MixerDevice = nullptr;
			SourceManager = nullptr;
		}

		Pitch = -1.0f;
		Volume = -1.0f;
		DistanceAttenuation = -1.0f;
		Distance = -1.0f;
		LPFFrequency = -1.0f;
		HPFFrequency = -1.0f;
		SourceId = INDEX_NONE;
		bIsPlaying = false;
		bIsPaused = false;
		bIsActive = false;
		bIsBus = false;
		bOutputToBusOnly = false;
		bStopFadedOut = false;
		SubmixSends.Reset();
	}

	bool FMixerSourceVoice::Init(const FMixerSourceVoiceInitParams& InitParams)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		if (SourceManager->GetFreeSourceId(SourceId))
		{
			AUDIO_MIXER_CHECK(InitParams.SourceListener != nullptr);
			AUDIO_MIXER_CHECK(InitParams.NumInputChannels > 0);

			bOutputToBusOnly = InitParams.bOutputToBusOnly;
			bIsBus = InitParams.BusId != INDEX_NONE;

			for (int32 i = 0; i < InitParams.SubmixSends.Num(); ++i)
			{
				FMixerSubmixPtr SubmixPtr = InitParams.SubmixSends[i].Submix.Pin();
				if (SubmixPtr.IsValid())
				{
					SubmixSends.Add(SubmixPtr->GetId(), InitParams.SubmixSends[i]);
				}
			}

			bStopFadedOut = false;
			SourceManager->InitSource(SourceId, InitParams);
			return true;
		}

		return false;
	}

	void FMixerSourceVoice::Release()
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		SourceManager->ReleaseSourceId(SourceId);
	}

	void FMixerSourceVoice::SetPitch(const float InPitch)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		if (Pitch != InPitch)
		{
			Pitch = InPitch;
			SourceManager->SetPitch(SourceId, InPitch);
		}
	}

	void FMixerSourceVoice::SetVolume(const float InVolume)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		if (Volume != InVolume)
		{
			Volume = InVolume;
			SourceManager->SetVolume(SourceId, InVolume);
		}
	}

	void FMixerSourceVoice::SetDistanceAttenuation(const float InDistanceAttenuation)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		if (DistanceAttenuation != InDistanceAttenuation)
		{
			DistanceAttenuation = InDistanceAttenuation;
			SourceManager->SetDistanceAttenuation(SourceId, InDistanceAttenuation);
		}
	}

	void FMixerSourceVoice::SetLPFFrequency(const float InLPFFrequency)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		if (LPFFrequency != InLPFFrequency)
		{
			LPFFrequency = InLPFFrequency;
			SourceManager->SetLPFFrequency(SourceId, LPFFrequency);
		}
	}

	void FMixerSourceVoice::SetHPFFrequency(const float InHPFFrequency)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		if (HPFFrequency != InHPFFrequency)
		{
			HPFFrequency = InHPFFrequency;
			SourceManager->SetHPFFrequency(SourceId, HPFFrequency);
		}
	}

	void FMixerSourceVoice::SetChannelMap(ESubmixChannelFormat InChannelType, const uint32 NumInputChannels, const Audio::AlignedFloatBuffer& InChannelMap, const bool bInIs3D, const bool bInIsCenterChannelOnly)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		SourceManager->SetChannelMap(SourceId, InChannelType, NumInputChannels, InChannelMap, bInIs3D, bInIsCenterChannelOnly);
	}

	void FMixerSourceVoice::SetSpatializationParams(const FSpatializationParams& InParams)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		SourceManager->SetSpatializationParams(SourceId, InParams);
	}

	void FMixerSourceVoice::Play()
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		bIsPlaying = true;
		bIsPaused = false;
		bIsActive = true;

		SourceManager->Play(SourceId);
	}

	void FMixerSourceVoice::Stop()
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		bIsPlaying = false;
		bIsPaused = false;
		bIsActive = false;
		// We are instantly fading out with this stop command
		bStopFadedOut = true;
		SourceManager->Stop(SourceId);
	}

	void FMixerSourceVoice::StopFade(int32 NumFrames)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		bIsPaused = false;
		SourceManager->StopFade(SourceId, NumFrames);
	}

	void FMixerSourceVoice::Pause()
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		bIsPaused = true;
		bIsActive = false;
		SourceManager->Pause(SourceId);
	}

	bool FMixerSourceVoice::IsPlaying() const
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		return bIsPlaying;
	}

	bool FMixerSourceVoice::IsPaused() const
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		return bIsPaused;
	}

	bool FMixerSourceVoice::IsActive() const
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		return bIsActive;
	}

	bool FMixerSourceVoice::NeedsSpeakerMap() const
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		return SourceManager->NeedsSpeakerMap(SourceId);
	}

	int64 FMixerSourceVoice::GetNumFramesPlayed() const
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		return SourceManager->GetNumFramesPlayed(SourceId);
	}

	float FMixerSourceVoice::GetEnvelopeValue() const
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		return SourceManager->GetEnvelopeValue(SourceId);
	}

	void FMixerSourceVoice::MixOutputBuffers(const ESubmixChannelFormat InSubmixChannelType, const float SendLevel, AlignedFloatBuffer& OutWetBuffer) const
	{
		AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(MixerDevice);

		check(!bOutputToBusOnly);

		return SourceManager->MixOutputBuffers(SourceId, InSubmixChannelType, SendLevel, OutWetBuffer);
	}

	void FMixerSourceVoice::SetSubmixSendInfo(FMixerSubmixWeakPtr Submix, const float SendLevel)
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(MixerDevice);

		if (!bOutputToBusOnly)
		{
			FMixerSubmixPtr SubmixPtr = Submix.Pin();
			if (SubmixPtr.IsValid())
			{
				FMixerSourceSubmixSend* SubmixSend = SubmixSends.Find(SubmixPtr->GetId());

				if (!SubmixSend)
				{
					FMixerSourceSubmixSend NewSubmixSend;
					NewSubmixSend.Submix = Submix;
					NewSubmixSend.SendLevel = SendLevel;
					NewSubmixSend.bIsMainSend = false;
					SubmixSends.Add(SubmixPtr->GetId(), NewSubmixSend);
					SourceManager->SetSubmixSendInfo(SourceId, NewSubmixSend);
				}
				else if (!FMath::IsNearlyEqual(SubmixSend->SendLevel, SendLevel))
				{
					SubmixSend->SendLevel = SendLevel;
					SourceManager->SetSubmixSendInfo(SourceId, *SubmixSend);
				}
			}
		}
	}

	void FMixerSourceVoice::OnMixBus(FMixerSourceVoiceBuffer* OutMixerSourceBuffer)
	{
		AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(MixerDevice);

		check(OutMixerSourceBuffer->AudioData.Num() > 0);

		for (int32 i = 0; i < OutMixerSourceBuffer->AudioData.Num(); ++i)
		{
			OutMixerSourceBuffer->AudioData[i] = 0.0f;
		}
	}
}
