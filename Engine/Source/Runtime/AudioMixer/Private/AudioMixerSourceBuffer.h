// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AudioMixerBuffer.h"
#include "AudioMixerSourceManager.h"

namespace Audio
{
	struct FMixerSourceVoiceBuffer;

	static const int32 MAX_BUFFERS_QUEUED = 3;
	static const int32 LOOP_FOREVER = -1;

	struct FRawPCMDataBuffer
	{
		uint8* Data;
		uint32 DataSize;
		int32 LoopCount;
		uint32 CurrentSample;
		uint32 NumSamples;

		bool GetNextBuffer(FMixerSourceVoiceBuffer* OutSourceBufferPtr, const uint32 NumSampleToGet);

		FRawPCMDataBuffer()
			: Data(nullptr)
			, DataSize(0)
			, LoopCount(0)
			, CurrentSample(0)
			, NumSamples(0)
		{}
	};

	/** Enum describing the data-read mode of an audio buffer. */
	enum class EBufferReadMode : uint8
	{
		/** Read the next buffer asynchronously. */
		Asynchronous,

		/** Read the next buffer asynchronously but skip the first chunk of audio. */
		AsynchronousSkipFirstFrame
	};

	/** Class which handles decoding audio for a particular source buffer. */
	class FMixerSourceBuffer
	{
	public:
		FMixerSourceBuffer();
		~FMixerSourceBuffer();

		bool PreInit(FMixerBuffer* InBuffer, USoundWave* InWave, ELoopingMode InLoopingMode, bool bInIsSeeking);
		bool Init();

		// Called by source manager when needing more buffers
		void OnBufferEnd();

		// Return the number of buffers enqueued on the mixer source buffer
		int32 GetNumBuffersQueued() const { return NumBuffersQeueued; }
		
		// Returns the next enqueued buffer, returns nullptr if no buffers enqueued
		TSharedPtr<FMixerSourceVoiceBuffer> GetNextBuffer();

		// Returns if buffer looped
		bool DidBufferLoop() const { return bLoopCallback; }

		// Returns true if buffer finished
		bool DidBufferFinish() const { return bBufferFinished; }

		// Called to start an async task to read more data
		bool ReadMoreRealtimeData(const int32 BufferIndex, EBufferReadMode BufferReadMode);

		// Returns true if async task is in progress
		bool IsAsyncTaskInProgress() const;

		// Returns true if the async task is done
		bool IsAsyncTaskDone() const;

		// Ensures the async task finishes
		void EnsureAsyncTaskFinishes();

		// Checks if sound wave is flagged begin destroy
		bool IsBeginDestroy();

		// Clear the sound wave reference
		void ClearSoundWave();

		// Begin and end generation on the audio render thread (audio mixer only)
		void OnBeginGenerate();
		void OnEndGenerate();

	private:

		void SubmitInitialPCMBuffers();
		void SubmitInitialRealtimeBuffers();
		void SubmitRealTimeSourceData(const bool bLooped);
		void ProcessRealTimeSource();
		void SubmitBuffer(TSharedPtr<FMixerSourceVoiceBuffer> InSourceVoiceBuffer);


		int32 NumBuffersQeueued;
		FRawPCMDataBuffer RawPCMDataBuffer;

		TArray<TSharedPtr<FMixerSourceVoiceBuffer>> SourceVoiceBuffers;
		TQueue<TSharedPtr<FMixerSourceVoiceBuffer>> BufferQueue;
		int32 CurrentBuffer;
		FMixerBuffer* MixerBuffer;
		USoundWave* SoundWave;
		IAudioTask* AsyncRealtimeAudioTask;
		ELoopingMode LoopingMode;
		bool bInitialized;
		bool bBufferFinished;
		bool bPlayedCachedBuffer;
		bool bIsSeeking;
		bool bLoopCallback;

	};
}
