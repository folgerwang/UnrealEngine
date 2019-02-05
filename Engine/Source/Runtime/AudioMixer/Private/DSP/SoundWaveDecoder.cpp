// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DSP/SoundWaveDecoder.h"
#include "Engine/Public/AudioThread.h"
#include "AudioMixer/Public/AudioMixer.h"

namespace Audio
{
	FDecodingSoundSource::FDecodingSoundSource(FAudioDevice* AudioDevice, const FSourceDecodeInit& InitData)
		: Handle(InitData.Handle)
		, SoundWave(InitData.SoundWave)
		, MixerBuffer(nullptr)
		, SampleRate(INDEX_NONE)
		, SeekTime(InitData.SeekTime)
	{
		SourceInfo.VolumeParam.Init();
		SourceInfo.VolumeParam.SetValue(InitData.VolumeScale);

		SourceInfo.PitchScale = InitData.PitchScale;

		MixerBuffer = FMixerBuffer::Init(AudioDevice, InitData.SoundWave, InitData.SeekTime > 0.0f);
	}

	FDecodingSoundSource::~FDecodingSoundSource()
	{
		MixerSourceBuffer.ClearSoundWave();
	}

	bool FDecodingSoundSource::PreInit(int32 InSampleRate)
	{
		SampleRate = InSampleRate;

#if AUDIO_SOURCE_DECODER_DEBUG
		SineTone[0].Init(InSampleRate, 220.0f, 0.5f);
		SineTone[1].Init(InSampleRate, 440.0f, 0.5f);
#endif

		ELoopingMode LoopingMode = SoundWave->bLooping ? ELoopingMode::LOOP_Forever : ELoopingMode::LOOP_Never;

		return MixerSourceBuffer.PreInit(MixerBuffer, SoundWave, LoopingMode, SeekTime > 0.0f);
	}

	bool FDecodingSoundSource::IsReadyToInit()
	{
		if (MixerBuffer && MixerBuffer->IsRealTimeSourceReady())
		{
			// Check if we have a realtime audio task already (doing first decode)
			if (MixerSourceBuffer.IsAsyncTaskInProgress())
			{
				// not ready
				return MixerSourceBuffer.IsAsyncTaskDone();
			}
			else
			{
				// Now check to see if we need to kick off a decode the first chunk of audio
				const EBufferType::Type BufferType = MixerBuffer->GetType();
				if ((BufferType == EBufferType::PCMRealTime || BufferType == EBufferType::Streaming) && SoundWave)
				{
					// If any of these conditions meet, we need to do an initial async decode before we're ready to start playing the sound
					if (SeekTime > 0.0f || !SoundWave->CachedRealtimeFirstBuffer)
					{
						// Before reading more PCMRT data, we first need to seek the buffer
						if (SeekTime > 0.0f)
						{
							MixerBuffer->Seek(SeekTime);
						}

						MixerSourceBuffer.ReadMoreRealtimeData(0, EBufferReadMode::Asynchronous);

						// not ready
						return false;
					}
				}
			}

			return true;
		}
		return false;
	}

	void FDecodingSoundSource::Init()
	{
		if (MixerBuffer->GetNumChannels() > 0 && MixerBuffer->GetNumChannels() <= 2)
		{
			SourceInfo.NumSourceChannels = MixerBuffer->GetNumChannels();
			SourceInfo.TotalNumFrames = MixerBuffer->GetNumFrames();

			SourceInfo.CurrentFrameValues.AddZeroed(SourceInfo.NumSourceChannels);
			SourceInfo.NextFrameValues.AddZeroed(SourceInfo.NumSourceChannels);

			SourceInfo.BasePitchScale = MixerBuffer->GetSampleRate() / SampleRate;
		
			SourceInfo.PitchParam.Init();
			SourceInfo.PitchParam.SetValue(SourceInfo.BasePitchScale * SourceInfo.PitchScale);

			MixerSourceBuffer.Init();

			bInitialized = true;
		}
	}

	void FDecodingSoundSource::SetPitchScale(float InPitchScale, uint32 NumFrames)
	{
		SourceInfo.PitchParam.SetValue(SourceInfo.BasePitchScale * InPitchScale, NumFrames);
		SourceInfo.PitchResetFrame = SourceInfo.NumFramesGenerated + NumFrames;
	}

	void FDecodingSoundSource::SetVolumeScale(float InVolumeScale, uint32 NumFrames)
	{
		SourceInfo.VolumeParam.SetValue(InVolumeScale, NumFrames);
		SourceInfo.VolumeResetFrame = SourceInfo.NumFramesGenerated + NumFrames;
	}

	void FDecodingSoundSource::ReadFrame()
	{
		bool bNextFrameOutOfRange = (SourceInfo.CurrentFrameIndex + 1) >= SourceInfo.CurrentAudioChunkNumFrames;
		bool bCurrentFrameOutOfRange = SourceInfo.CurrentFrameIndex >= SourceInfo.CurrentAudioChunkNumFrames;

		bool bReadCurrentFrame = true;

		while (bNextFrameOutOfRange || bCurrentFrameOutOfRange)
		{
			if (bNextFrameOutOfRange && !bCurrentFrameOutOfRange)
			{
				bReadCurrentFrame = false;

				const float* AudioData = SourceInfo.CurrentPCMBuffer->AudioData.GetData();
				const int32 CurrentSampleIndex = SourceInfo.CurrentFrameIndex * SourceInfo.NumSourceChannels;

				for (int32 Channel = 0; Channel < SourceInfo.NumSourceChannels; ++Channel)
				{
					SourceInfo.CurrentFrameValues[Channel] = AudioData[CurrentSampleIndex + Channel];
				}
			}

			if (SourceInfo.CurrentPCMBuffer.IsValid())
			{
				if (SourceInfo.CurrentPCMBuffer->LoopCount == Audio::LOOP_FOREVER && !SourceInfo.CurrentPCMBuffer->bRealTimeBuffer)
				{
					SourceInfo.CurrentFrameIndex = FMath::Max(SourceInfo.CurrentFrameIndex - SourceInfo.CurrentAudioChunkNumFrames, 0);
					break;
				}

				MixerSourceBuffer.OnBufferEnd();
			}

			if (MixerSourceBuffer.GetNumBuffersQueued() > 0)
			{
				SourceInfo.CurrentPCMBuffer = MixerSourceBuffer.GetNextBuffer();
				SourceInfo.CurrentAudioChunkNumFrames = SourceInfo.CurrentPCMBuffer->AudioData.Num() / SourceInfo.NumSourceChannels;

				if (bReadCurrentFrame)
				{
					SourceInfo.CurrentFrameIndex = FMath::Max(SourceInfo.CurrentFrameIndex - SourceInfo.CurrentAudioChunkNumFrames, 0);
				}
				else
				{
					SourceInfo.CurrentFrameIndex = INDEX_NONE;
				}
			}
			else
			{
				SourceInfo.bIsLastBuffer = true;
				return;
			}

			bNextFrameOutOfRange = (SourceInfo.CurrentFrameIndex + 1) >= SourceInfo.CurrentAudioChunkNumFrames;
			bCurrentFrameOutOfRange = SourceInfo.CurrentFrameIndex >= SourceInfo.CurrentAudioChunkNumFrames;
		}

		if (SourceInfo.CurrentPCMBuffer.IsValid())
		{
			const float* AudioData = SourceInfo.CurrentPCMBuffer->AudioData.GetData();
			const int32 NextSampleIndex = (SourceInfo.CurrentFrameIndex + 1)  * SourceInfo.NumSourceChannels;

			if (bReadCurrentFrame)
			{
				const int32 CurrentSampleIndex = SourceInfo.CurrentFrameIndex * SourceInfo.NumSourceChannels;
				for (int32 Channel = 0; Channel < SourceInfo.NumSourceChannels; ++Channel)
				{
					SourceInfo.CurrentFrameValues[Channel] = AudioData[CurrentSampleIndex + Channel];
					SourceInfo.NextFrameValues[Channel] = AudioData[NextSampleIndex + Channel];
				}
			}
			else
			{
				for (int32 Channel = 0; Channel < SourceInfo.NumSourceChannels; ++Channel)
				{
					SourceInfo.NextFrameValues[Channel] = AudioData[NextSampleIndex + Channel];
				}
			}
		}
	}

	void FDecodingSoundSource::GetAudioBufferInternal(const int32 InNumFrames, const int32 InNumChannels, AlignedFloatBuffer& OutAudioBuffer)
	{
#if AUDIO_SOURCE_DECODER_DEBUG
		int32 SampleIndex = 0;
		float* OutAudioBufferPtr = OutAudioBuffer.GetData();
		for (int32 FrameIndex = 0; FrameIndex < InNumFrames; ++FrameIndex)
		{
			for (int32 ChannelIndex = 0; ChannelIndex < InNumChannels; ++ChannelIndex)
			{
				OutAudioBufferPtr[SampleIndex++] = SineTone[ChannelIndex].ProcessAudio();
			}
		}
#else
		int32 SampleIndex = 0;
		float* OutAudioBufferPtr = OutAudioBuffer.GetData();
		float* CurrentFrameValuesPtr = SourceInfo.CurrentFrameValues.GetData();
		float* NextFrameValuesPtr = SourceInfo.NextFrameValues.GetData();

		for (int32 FrameIndex = 0; FrameIndex < InNumFrames; ++FrameIndex)
		{
			if (SourceInfo.bIsLastBuffer)
			{
				break;
			}

			bool bReadFrame = !SourceInfo.bHasStarted;
			SourceInfo.bHasStarted = true;

			while (SourceInfo.CurrentFrameAlpha >= 1.0f)
			{
				bReadFrame = true;
				SourceInfo.CurrentFrameIndex++;
				SourceInfo.NumFramesRead++;
				SourceInfo.CurrentFrameAlpha -= 1.0f;
			}

			if (bReadFrame)
			{
				ReadFrame();
			}

			const float CurrentVolumeScale = SourceInfo.VolumeParam.Update();

			for (int32 Channel = 0; Channel < SourceInfo.NumSourceChannels; ++Channel)
			{
				const float CurrFrameValue = CurrentFrameValuesPtr[Channel];
				const float NextFrameValue = NextFrameValuesPtr[Channel];
				const float CurrentAlpha = SourceInfo.CurrentFrameAlpha;

				OutAudioBufferPtr[SampleIndex++] = CurrentVolumeScale * FMath::Lerp(CurrFrameValue, NextFrameValue, CurrentAlpha);
			}
			const float CurrentPitchScale = SourceInfo.PitchParam.Update();
			SourceInfo.CurrentFrameAlpha += CurrentPitchScale;

			SourceInfo.NumFramesGenerated++;

			if (SourceInfo.NumFramesGenerated >= SourceInfo.PitchResetFrame)
			{
				SourceInfo.PitchResetFrame = INDEX_NONE;
				SourceInfo.PitchParam.Reset();
			}

			if (SourceInfo.NumFramesGenerated >= SourceInfo.VolumeResetFrame)
			{
				SourceInfo.VolumeResetFrame = INDEX_NONE;
				SourceInfo.VolumeParam.Reset();
			}
		}
#endif
	}

	bool FDecodingSoundSource::GetAudioBuffer(const int32 InNumFrames, const int32 InNumChannels, AlignedFloatBuffer& OutAudioBuffer)
	{
		if (!bInitialized)
		{
			return false;
		}

		OutAudioBuffer.Reset();
		OutAudioBuffer.AddZeroed(InNumFrames * InNumChannels);

		if (SourceInfo.bIsLastBuffer)
		{
			return false;
		}

		if (InNumChannels == SourceInfo.NumSourceChannels)
		{
			GetAudioBufferInternal(InNumFrames, InNumChannels, OutAudioBuffer);
		}
		else
		{
			static AlignedFloatBuffer ScratchBuffer;
			ScratchBuffer.AddZeroed(InNumFrames * SourceInfo.NumSourceChannels);

			GetAudioBufferInternal(InNumFrames, InNumChannels, ScratchBuffer);

			float* BufferPtr = OutAudioBuffer.GetData();
			float* ScratchBufferPtr = ScratchBuffer.GetData();

			int32 OutputSampleIndex = 0;
			int32 InputSampleIndex = 0;

			// Need to upmix the audio
			if (InNumChannels == 2 && SourceInfo.NumSourceChannels == 1)
			{
				for (int32 FrameIndex = 0; FrameIndex < InNumFrames; ++FrameIndex, ++InputSampleIndex)
				{
					for (int32 ChannelIndex = 0; ChannelIndex < InNumChannels; ++ChannelIndex)
					{
						BufferPtr[OutputSampleIndex++] = 0.5f * ScratchBufferPtr[InputSampleIndex];
					}
				}
			}
			// Need to downmix the audio
			else
			{
				check(InNumChannels == 1 && SourceInfo.NumSourceChannels == 2);

				for (int32 FrameIndex = 0; FrameIndex < InNumFrames; ++FrameIndex, ++InputSampleIndex)
				{
					for (int32 ChannelIndex = 0; ChannelIndex < InNumChannels; ++ChannelIndex)
					{
						BufferPtr[OutputSampleIndex++] = 0.5f * (ScratchBufferPtr[InputSampleIndex] + ScratchBufferPtr[InputSampleIndex + 1]);
					}
				}

			}
		}

		return true;
	}

	FSoundSourceDecoder::FSoundSourceDecoder()
		: AudioThreadId(0)
		, AudioDevice(nullptr)
		, SampleRate(0)
	{
	}

	FSoundSourceDecoder::~FSoundSourceDecoder()
	{

	}

	void FSoundSourceDecoder::Init(FAudioDevice* InAudioDevice, int32 InSampleRate)
	{
		AudioDevice = InAudioDevice;
		SampleRate = InSampleRate;
	}

	FDecodingSoundSourceHandle FSoundSourceDecoder::CreateSourceHandle(USoundWave* InSoundWave)
	{
		// Init the handle ids
		static int32 SoundWaveDecodingHandles = 0;

		// Create a new handle
		FDecodingSoundSourceHandle Handle;
		Handle.Id = SoundWaveDecodingHandles++;
		Handle.SoundWaveName = InSoundWave->GetFName();
		return Handle;
	}

	void FSoundSourceDecoder::EnqueueDecoderCommand(TFunction<void()> Command)
	{
		CommandQueue.Enqueue(Command);
	}

	void FSoundSourceDecoder::PumpDecoderCommandQueue()
	{
		TFunction<void()> Command;
		while (CommandQueue.Dequeue(Command))
		{
			Command();
		}
	}

	bool FSoundSourceDecoder::InitDecodingSourceInternal(const FSourceDecodeInit& InitData)
	{
		FDecodingSoundSourcePtr DecodingSoundWaveDataPtr = FDecodingSoundSourcePtr(new FDecodingSoundSource(AudioDevice, InitData));

		if (DecodingSoundWaveDataPtr->PreInit(SampleRate))
		{
			InitializingDecodingSources.Add(InitData.Handle.Id, DecodingSoundWaveDataPtr);

			// Add this decoding sound wave to a data structure we can access safely from audio render thread
			EnqueueDecoderCommand([this, InitData, DecodingSoundWaveDataPtr]()
			{
				DecodingSources.Add(InitData.Handle.Id, DecodingSoundWaveDataPtr);

				UE_LOG(LogTemp, Log, TEXT("Decoding sources size %d."), DecodingSources.Num());
			});

			return true;
		}

		UE_LOG(LogAudioMixer, Warning, TEXT("Failed to initialize sound wave %s."), *InitData.SoundWave->GetName());
		return false;
	}

	bool FSoundSourceDecoder::InitDecodingSource(const FSourceDecodeInit& InitData)
	{
		check(IsInAudioThread());

		if (InitData.SoundWave == nullptr)
		{
			UE_LOG(LogAudioMixer, Error, TEXT("Cannot Decode NULL SoundWave"));
			return false;
		}

		if (InitData.SoundWave->NumChannels == 0)
		{
			UE_LOG(LogAudioMixer, Error, TEXT("Cannot Decode invalid or corrupt sound wave %s. NumChannels = 0"), *InitData.SoundWave->GetName());
			return false;
		}

		if (InitData.SoundWave->NumChannels <= 0 || InitData.SoundWave->NumChannels > 2)
		{
			UE_LOG(LogAudioMixer, Error, TEXT("Only supporting 1 or 2 channel decodes in sound source decoder."), *InitData.SoundWave->GetName());
			return false;
		}


		if (InitData.SoundWave->bIsBus || InitData.SoundWave->bProcedural)
		{
			UE_LOG(LogAudioMixer, Warning, TEXT("Sound wave decoder does not support buses or procedural sounds."));
			return false;
		}

		// Start the soundwave precache
		const ESoundWavePrecacheState PrecacheState = InitData.SoundWave->GetPrecacheState();
		if (PrecacheState == ESoundWavePrecacheState::NotStarted)
		{
			AudioDevice->Precache(InitData.SoundWave);
			PrecachingSources.Add(InitData.Handle.Id, InitData);
			return true;
		}
		else if (PrecacheState != ESoundWavePrecacheState::Done)
		{
			if (!PrecachingSources.Contains(InitData.Handle.Id))
			{
				PrecachingSources.Add(InitData.Handle.Id, InitData);
			}
			return true;
		}
		else
		{
			return InitDecodingSourceInternal(InitData);
		}

		return false;
	}

	void FSoundSourceDecoder::RemoveDecodingSource(const FDecodingSoundSourceHandle& Handle)
	{
		DecodingSources.Remove(Handle.Id);
	}

	void FSoundSourceDecoder::SetSourcePitchScale(const FDecodingSoundSourceHandle& Handle, float InPitchScale)
	{

	}

	void FSoundSourceDecoder::SetSourceVolumeScale(const FDecodingSoundSourceHandle& InHandle, float InVolumeScale)
	{
		FDecodingSoundSourcePtr* DecodingSoundWaveDataPtr = DecodingSources.Find(InHandle.Id);
		if (!DecodingSoundWaveDataPtr)
		{
			return;
		}
		(*DecodingSoundWaveDataPtr)->SetVolumeScale(InVolumeScale);
	}

	void FSoundSourceDecoder::Update()
	{
		check(IsInAudioThread());

		TArray<int32> TempIds;

		for (auto& Entry : PrecachingSources)
		{
			int32 Id = Entry.Key;
			FSourceDecodeInit& InitData = Entry.Value;
			if (InitData.SoundWave->GetPrecacheState() == ESoundWavePrecacheState::Done)
			{
				InitDecodingSourceInternal(InitData);
				TempIds.Add(Id);
			}
		}

		// Remove the Id's that have initialized
		for (int32 Id : TempIds)
		{
			PrecachingSources.Remove(Id);
		}

		TempIds.Reset();
		for (auto& Entry : InitializingDecodingSources)
		{
			int32 Id = Entry.Key;
			FDecodingSoundSourcePtr DecodingSoundSourcePtr = Entry.Value;

			if (DecodingSoundSourcePtr->IsReadyToInit())
			{
				DecodingSoundSourcePtr->Init();

				// Add to local array here to clean up the map quickly
				TempIds.Add(Id);
			}
		}
		
		// Remove the Id's that have initialized
		for (int32 Id : TempIds)
		{
			InitializingDecodingSources.Remove(Id);
		}

	}

	void FSoundSourceDecoder::UpdateRenderThread()
	{
		PumpDecoderCommandQueue();
	}

	bool FSoundSourceDecoder::IsFinished(const FDecodingSoundSourceHandle& InHandle) const
	{
		const FDecodingSoundSourcePtr* DecodingSoundWaveDataPtr = DecodingSources.Find(InHandle.Id);
		if (!DecodingSoundWaveDataPtr)
		{
			return true;
		}

		return (*DecodingSoundWaveDataPtr)->IsFinished();
	}

	bool FSoundSourceDecoder::IsInitialized(const FDecodingSoundSourceHandle& InHandle) const
	{
		const FDecodingSoundSourcePtr* DecodingSoundWaveDataPtr = DecodingSources.Find(InHandle.Id);
		if (!DecodingSoundWaveDataPtr)
		{
			return true;
		}

		return (*DecodingSoundWaveDataPtr)->IsInitialized();
	}


	bool FSoundSourceDecoder::GetSourceBuffer(const FDecodingSoundSourceHandle& InHandle, const int32 NumOutFrames, const int32 NumOutChannels, AlignedFloatBuffer& OutAudioBuffer)
	{
		check(InHandle.Id != INDEX_NONE);

		FDecodingSoundSourcePtr* DecodingSoundWaveDataPtr = DecodingSources.Find(InHandle.Id);
		if (!DecodingSoundWaveDataPtr)
		{
			return false;
		}

		(*DecodingSoundWaveDataPtr)->GetAudioBuffer(NumOutFrames, NumOutChannels, OutAudioBuffer);

		return true;
	}

}
