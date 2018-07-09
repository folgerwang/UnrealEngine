// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AudioMixerSourceBuffer.h"

namespace Audio
{
	bool FRawPCMDataBuffer::GetNextBuffer(FMixerSourceVoiceBuffer* OutSourceBufferPtr, const uint32 NumSampleToGet)
	{
		// TODO: support loop counts
		float* OutBufferPtr = OutSourceBufferPtr->AudioData.GetData();
		int16* DataPtr = (int16*)Data;

		if (LoopCount == Audio::LOOP_FOREVER)
		{
			bool bLooped = false;
			for (uint32 Sample = 0; Sample < NumSampleToGet; ++Sample)
			{
				OutBufferPtr[Sample] = DataPtr[CurrentSample++] / 32768.0f;

				// Loop around if we're looping
				if (CurrentSample >= NumSamples)
				{
					CurrentSample = 0;
					bLooped = true;
				}
			}
			return bLooped;
		}
		else if (CurrentSample < NumSamples)
		{
			uint32 Sample = 0;
			while (Sample < NumSampleToGet && CurrentSample < NumSamples)
			{
				OutBufferPtr[Sample++] = (float)DataPtr[CurrentSample++] / 32768.0f;
			}

			// Zero out the rest of the buffer
			while (Sample < NumSampleToGet)
			{
				OutBufferPtr[Sample++] = 0.0f;
			}
		}
		else
		{
			for (uint32 Sample = 0; Sample < NumSampleToGet; ++Sample)
			{
				OutBufferPtr[Sample] = 0.0f;
			}
		}

		// If the current sample is greater or equal to num samples we hit the end of the buffer
		return CurrentSample >= NumSamples;
	}

	FMixerSourceBuffer::FMixerSourceBuffer()
		: NumBuffersQeueued(0)
		, CurrentBuffer(0)
		, MixerBuffer(nullptr)
		, SoundWave(nullptr)
		, AsyncRealtimeAudioTask(nullptr)
		, LoopingMode(ELoopingMode::LOOP_Never)
		, bInitialized(false)
		, bBufferFinished(false)
		, bPlayedCachedBuffer(false)
		, bIsSeeking(false)
		, bLoopCallback(false)
	{
	}

	FMixerSourceBuffer::~FMixerSourceBuffer()
	{
		OnEndGenerate();
	}

	bool FMixerSourceBuffer::PreInit(FMixerBuffer* InBuffer, USoundWave* InWave, ELoopingMode InLoopingMode, bool bInIsSeeking)
	{
		LLM_SCOPE(ELLMTag::AudioMixer);

		// Mixer source buffer now owns this buffer
		MixerBuffer = InBuffer;
		check(MixerBuffer);

		// May or may not be nullptr
		SoundWave = InWave;

		LoopingMode = InLoopingMode;
		bIsSeeking = bInIsSeeking;
		bLoopCallback = false;

		BufferQueue.Empty();

		const uint32 TotalSamples = MONO_PCM_BUFFER_SAMPLES * MixerBuffer->NumChannels;
		for (int32 BufferIndex = 0; BufferIndex < Audio::MAX_BUFFERS_QUEUED; ++BufferIndex)
		{
			SourceVoiceBuffers.Add(TSharedPtr<FMixerSourceVoiceBuffer>(new FMixerSourceVoiceBuffer()));

			SourceVoiceBuffers[BufferIndex]->AudioData.AddZeroed(TotalSamples);
			SourceVoiceBuffers[BufferIndex]->Samples = TotalSamples;
			SourceVoiceBuffers[BufferIndex]->bRealTimeBuffer = true;
			SourceVoiceBuffers[BufferIndex]->LoopCount = 0;
		}
		return true;
	}

	bool FMixerSourceBuffer::Init()
	{
		check(SoundWave);
		if (SoundWave->bProcedural && SoundWave->GetNumSoundsActive() > 0)
		{
			UE_LOG(LogAudioMixer, Warning, TEXT("Procedural sound wave is reinitializing even though it is currently actively generating audio. Please stop sound before trying to play it again."));
			return false;
		}

		// We flag that this sound wave is active for the lifetime of this object since we use it for decoding, etc.
		SoundWave->IncrementNumSounds();

		// We have successfully initialized which means our SoundWave has been flagged as bIsActive
		// GC can run between PreInit and Init so when cleaning up FMixerSourceBuffer, we don't want to touch SoundWave unless bInitailized is true.
		// SoundWave->bIsSoundActive will prevent GC until it is released in audio render thread
		bInitialized = true;

		const EBufferType::Type BufferType = MixerBuffer->GetType();
		switch (BufferType)
		{
			case EBufferType::PCM:
			case EBufferType::PCMPreview:
				SubmitInitialPCMBuffers();
				break;

			case EBufferType::PCMRealTime:
			case EBufferType::Streaming:
				SubmitInitialRealtimeBuffers();
				break;

			case EBufferType::Invalid:
				break;
		}

		return true;
	}

	void FMixerSourceBuffer::OnBufferEnd()
	{
		if ((NumBuffersQeueued == 0 && bBufferFinished) || !SoundWave)
		{
			return;
		}

		ProcessRealTimeSource();
	}

	TSharedPtr<FMixerSourceVoiceBuffer> FMixerSourceBuffer::GetNextBuffer()
	{
		TSharedPtr<FMixerSourceVoiceBuffer> NewBufferPtr;
		BufferQueue.Dequeue(NewBufferPtr);
		--NumBuffersQeueued;
		return NewBufferPtr;
	}

	void FMixerSourceBuffer::SubmitInitialPCMBuffers()
	{
		CurrentBuffer = 0;

		RawPCMDataBuffer.Data = nullptr;
		RawPCMDataBuffer.DataSize = 0;
		MixerBuffer->GetPCMData(&RawPCMDataBuffer.Data, &RawPCMDataBuffer.DataSize);

		RawPCMDataBuffer.NumSamples = RawPCMDataBuffer.DataSize / sizeof(int16);
		RawPCMDataBuffer.CurrentSample = 0;

		// Only submit data if we've successfully loaded it
		if (!RawPCMDataBuffer.Data || !RawPCMDataBuffer.DataSize)
		{
			return;
		}

		RawPCMDataBuffer.LoopCount = (LoopingMode != LOOP_Never) ? Audio::LOOP_FOREVER : 0;

		// Submit the first two format-converted chunks to the source voice
		const uint32 NumSamplesPerBuffer = MONO_PCM_BUFFER_SAMPLES * MixerBuffer->NumChannels;
		int16* RawPCMBufferDataPtr = (int16*)RawPCMDataBuffer.Data;

		RawPCMDataBuffer.GetNextBuffer(SourceVoiceBuffers[0].Get(), NumSamplesPerBuffer);

		SubmitBuffer(SourceVoiceBuffers[0]);

		CurrentBuffer = 1;
	}

	void FMixerSourceBuffer::SubmitInitialRealtimeBuffers()
	{
		CurrentBuffer = 0;

		bPlayedCachedBuffer = false;
		if (!bIsSeeking && SoundWave && SoundWave->CachedRealtimeFirstBuffer)
		{
			bPlayedCachedBuffer = true;

			// Format convert the first cached buffers
			const uint32 NumSamples = MONO_PCM_BUFFER_SAMPLES * MixerBuffer->NumChannels;
			const uint32 BufferSize = MONO_PCM_BUFFER_SIZE * MixerBuffer->NumChannels;

			int16* CachedBufferPtr0 = (int16*)SoundWave->CachedRealtimeFirstBuffer;
			int16* CachedBufferPtr1 = (int16*)(SoundWave->CachedRealtimeFirstBuffer + BufferSize);
			float* AudioData0 = SourceVoiceBuffers[0]->AudioData.GetData();
			float* AudioData1 = SourceVoiceBuffers[1]->AudioData.GetData();
			for (uint32 Sample = 0; Sample < NumSamples; ++Sample)
			{
				AudioData0[Sample] = CachedBufferPtr0[Sample] / 32768.0f;
				AudioData1[Sample] = CachedBufferPtr1[Sample] / 32768.0f;
			}

			// Submit the already decoded and cached audio buffers
			SubmitBuffer(SourceVoiceBuffers[0]);
			SubmitBuffer(SourceVoiceBuffers[1]);

			CurrentBuffer = 2;
		}
		else if (SoundWave && !SoundWave->bIsBus)
		{
			// We should have already kicked off and finished a task. 
			check(AsyncRealtimeAudioTask != nullptr);

			ProcessRealTimeSource();
		}
	}

	bool FMixerSourceBuffer::ReadMoreRealtimeData(const int32 BufferIndex, EBufferReadMode BufferReadMode)
	{
		if (SoundWave && SoundWave->bProcedural)
		{
			const int32 MaxSamples = MONO_PCM_BUFFER_SAMPLES * MixerBuffer->NumChannels;

			FProceduralAudioTaskData NewTaskData;
			NewTaskData.ProceduralSoundWave = SoundWave;
			NewTaskData.AudioData = SourceVoiceBuffers[BufferIndex]->AudioData.GetData();
			NewTaskData.NumSamples = MaxSamples;
			NewTaskData.NumChannels = MixerBuffer->NumChannels;
			check(!AsyncRealtimeAudioTask);
			AsyncRealtimeAudioTask = CreateAudioTask(NewTaskData);

			// Procedural sound waves never loop
			return false;
		}
		else if (!MixerBuffer->IsRealTimeBuffer())
		{
			check(RawPCMDataBuffer.Data != nullptr);

			// Read the next raw PCM buffer into the source buffer index. This converts raw PCM to float.
			const uint32 NumSamplesPerBuffer = MONO_PCM_BUFFER_SAMPLES * MixerBuffer->NumChannels;
			return RawPCMDataBuffer.GetNextBuffer(SourceVoiceBuffers[BufferIndex].Get(), NumSamplesPerBuffer);
		}

		FDecodeAudioTaskData NewTaskData;
		NewTaskData.MixerBuffer = MixerBuffer;
		NewTaskData.AudioData = SourceVoiceBuffers[BufferIndex]->AudioData.GetData();
		NewTaskData.bLoopingMode = LoopingMode != LOOP_Never;
		NewTaskData.bSkipFirstBuffer = (BufferReadMode == EBufferReadMode::AsynchronousSkipFirstFrame);
		NewTaskData.NumFramesToDecode = MONO_PCM_BUFFER_SAMPLES;

		check(!AsyncRealtimeAudioTask);
		AsyncRealtimeAudioTask = CreateAudioTask(NewTaskData);

		return false;
	}

	void FMixerSourceBuffer::SubmitRealTimeSourceData(const bool bLooped)
	{
		// Have we reached the end of the sound
		if (bLooped)
		{
			switch (LoopingMode)
			{
			case LOOP_Never:
				// Play out any queued buffers - once there are no buffers left, the state check at the beginning of IsFinished will fire
				bBufferFinished = true;
				break;

			case LOOP_WithNotification:
				// If we have just looped, and we are looping, send notification
				// This will trigger a WaveInstance->NotifyFinished() in the FXAudio2SoundSournce::IsFinished() function on main thread.
				bLoopCallback = true;
				break;

			case LOOP_Forever:
				// Let the sound loop indefinitely
				break;
			}
		}

		if (SourceVoiceBuffers[CurrentBuffer]->AudioData.Num() > 0)
		{
			SubmitBuffer(SourceVoiceBuffers[CurrentBuffer]);
		}
	}

	void FMixerSourceBuffer::ProcessRealTimeSource()
	{
		if (AsyncRealtimeAudioTask)
		{
			AsyncRealtimeAudioTask->EnsureCompletion();

			bool bLooped = false;

			switch (AsyncRealtimeAudioTask->GetType())
			{
			case EAudioTaskType::Decode:
			{
				FDecodeAudioTaskResults TaskResult;
				AsyncRealtimeAudioTask->GetResult(TaskResult);

				SourceVoiceBuffers[CurrentBuffer]->Samples = MONO_PCM_BUFFER_SAMPLES * MixerBuffer->NumChannels;
				bLooped = TaskResult.bLooped;
			}
			break;

			case EAudioTaskType::Procedural:
			{
				FProceduralAudioTaskResults TaskResult;
				AsyncRealtimeAudioTask->GetResult(TaskResult);

				SourceVoiceBuffers[CurrentBuffer]->Samples = TaskResult.NumSamplesWritten;
			}
			break;
			}

			delete AsyncRealtimeAudioTask;
			AsyncRealtimeAudioTask = nullptr;

			SubmitRealTimeSourceData(bLooped);
		}

		if (!AsyncRealtimeAudioTask)
		{
			// Update the buffer index
			if (++CurrentBuffer > 2)
			{
				CurrentBuffer = 0;
			}

			EBufferReadMode DataReadMode;
			if (bPlayedCachedBuffer)
			{
				bPlayedCachedBuffer = false;
				DataReadMode = EBufferReadMode::AsynchronousSkipFirstFrame;
			}
			else
			{
				DataReadMode = EBufferReadMode::Asynchronous;
			}

			const bool bLooped = ReadMoreRealtimeData(CurrentBuffer, DataReadMode);

			// If this was a synchronous read, then immediately write it
			if (AsyncRealtimeAudioTask == nullptr)
			{
				SubmitRealTimeSourceData(bLooped);
			}
		}
	}

	void FMixerSourceBuffer::SubmitBuffer(TSharedPtr<FMixerSourceVoiceBuffer> InSourceVoiceBuffer)
	{
		NumBuffersQeueued++;
		BufferQueue.Enqueue(InSourceVoiceBuffer);
	}

	bool FMixerSourceBuffer::IsAsyncTaskInProgress() const
	{ 
		return AsyncRealtimeAudioTask != nullptr; 
	}

	bool FMixerSourceBuffer::IsAsyncTaskDone() const 
	{
		if (AsyncRealtimeAudioTask)
		{
			return AsyncRealtimeAudioTask->IsDone();
		}
		return true; 
	}
	
	void FMixerSourceBuffer::EnsureAsyncTaskFinishes() 
	{
		if (AsyncRealtimeAudioTask) 
		{ 
			AsyncRealtimeAudioTask->EnsureCompletion(); 
		}
	}

	bool FMixerSourceBuffer::IsBeginDestroy()
	{
		return SoundWave && SoundWave->bIsBeginDestroy;
	}

	void FMixerSourceBuffer::ClearSoundWave()
	{
		// Call on end generate right now, before destructor
		OnEndGenerate();
	}

	void FMixerSourceBuffer::OnBeginGenerate()
	{
		if (SoundWave)
		{
			if (SoundWave->bProcedural)
			{
				SoundWave->OnBeginGenerate();
			}
		
		}
	}

	void FMixerSourceBuffer::OnEndGenerate()
	{
		// Make sure the async task finishes!
		EnsureAsyncTaskFinishes();

		// Only need to call OnEndGenerate and access SoundWave here if we successfully initialized
		if (bInitialized && SoundWave)
		{
			if (SoundWave->bProcedural)
			{
				SoundWave->OnEndGenerate();
			}

			SoundWave->DecrementNumSounds();
			SoundWave = nullptr;
		}

		if (MixerBuffer)
		{
			EBufferType::Type BufferType = MixerBuffer->GetType();
			if (BufferType == EBufferType::PCMRealTime || BufferType == EBufferType::Streaming)
			{
				delete MixerBuffer;
			}
	
			MixerBuffer = nullptr;
		}
	}

}
