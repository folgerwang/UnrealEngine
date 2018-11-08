// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

/* Public dependencies
*****************************************************************************/

#include "CoreMinimal.h"
#include "AudioMixerBuffer.h"
#include "AudioMixerSubmix.h"
#include "AudioMixerBus.h"
#include "DSP/OnePole.h"
#include "DSP/Filter.h"
#include "DSP/EnvelopeFollower.h"
#include "DSP/ParamInterpolator.h"
#include "DSP/BufferVectorOperations.h"
#include "IAudioExtensionPlugin.h"
#include "Containers/Queue.h"
#include "AudioMixerSourceBuffer.h"


namespace Audio
{
	class FMixerSubmix;
	class FMixerDevice;
	class FMixerSourceVoice;
	class FMixerSourceBuffer;
	class ISourceListener;

	/** Struct defining a source voice buffer. */
	struct FMixerSourceVoiceBuffer
	{
		/** PCM float data. */
		TArray<float> AudioData;

		/** The amount of samples of audio data in the float buffer array. */
		uint32 Samples;

		/** How many times this buffer will loop. */
		int32 LoopCount;

		/** If this buffer is from real-time decoding and needs to make callbacks for more data. */
		uint32 bRealTimeBuffer : 1;

		FMixerSourceVoiceBuffer()
		{
			FMemory::Memzero(this, sizeof(*this));
		}
	};

	typedef TSharedPtr<FMixerSubmix, ESPMode::ThreadSafe> FMixerSubmixPtr;
	typedef TWeakPtr<FMixerSubmix, ESPMode::ThreadSafe> FMixerSubmixWeakPtr;

	class ISourceListener
	{
	public:
		// Called before a source begins to generate audio. 
		virtual void OnBeginGenerate() = 0;

		// Called when a loop point is hit
		virtual void OnLoopEnd() = 0;

		// Called when the source finishes on the audio render thread
		virtual void OnDone() = 0;

		// Called when the source's effect tails finish on the audio render thread.
		virtual void OnEffectTailsDone() = 0;

	};

	struct FMixerSourceSubmixSend
	{
		// The submix ptr
		FMixerSubmixWeakPtr Submix;

		// The amount of audio that is to be mixed into this submix
		float SendLevel;

		// Whather or not this is the primary send (i.e. first in the send chain)
		bool bIsMainSend;
	};

	// Struct holding mappings of bus ids (unique ids) to send level
	struct FMixerBusSend
	{
		uint32 BusId;
		float SendLevel;
	};

	struct FMixerSourceVoiceInitParams
	{
		TSharedPtr<FMixerSourceBuffer> MixerSourceBuffer;
		ISourceListener* SourceListener;
		TArray<FMixerSourceSubmixSend> SubmixSends;
		TArray<FMixerBusSend> BusSends[(int32)EBusSendType::Count];
		uint32 BusId;
		float BusDuration;
		uint32 SourceEffectChainId;
		TArray<FSourceEffectChainEntry> SourceEffectChain;
		FMixerSourceVoice* SourceVoice;
		int32 NumInputChannels;
		int32 NumInputFrames;
		float EnvelopeFollowerAttackTime;
		float EnvelopeFollowerReleaseTime;
		FString DebugName;
		USpatializationPluginSourceSettingsBase* SpatializationPluginSettings;
		UOcclusionPluginSourceSettingsBase* OcclusionPluginSettings;
		UReverbPluginSourceSettingsBase* ReverbPluginSettings;
		FName AudioComponentUserID;
		uint64 AudioComponentID;
		uint8 bPlayEffectChainTails : 1;
		uint8 bUseHRTFSpatialization : 1;
		uint8 bIsDebugMode : 1;
		uint8 bOutputToBusOnly : 1;
		uint8 bIsVorbis : 1;
		uint8 bIsAmbisonics : 1;
		uint8 bIsSeeking : 1;

		FMixerSourceVoiceInitParams()
			: MixerSourceBuffer(nullptr)
			, SourceListener(nullptr)
			, BusId(INDEX_NONE)
			, BusDuration(0.0f)
			, SourceEffectChainId(INDEX_NONE)
			, SourceVoice(nullptr)
			, NumInputChannels(0)
			, NumInputFrames(0)
			, EnvelopeFollowerAttackTime(10.0f)
			, EnvelopeFollowerReleaseTime(100.0f)
			, SpatializationPluginSettings(nullptr)
			, OcclusionPluginSettings(nullptr)
			, ReverbPluginSettings(nullptr)
			, AudioComponentID(0)
			, bPlayEffectChainTails(false)
			, bUseHRTFSpatialization(false)
			, bIsDebugMode(false)
			, bOutputToBusOnly(false)
			, bIsVorbis(false)
			, bIsAmbisonics(false)
			, bIsSeeking(false)
		{}
	};

	class FSourceChannelMap
	{
	public:
		FSourceChannelMap() {}

		FORCEINLINE void Reset()
		{
			ChannelValues.Reset();
		}

		FORCEINLINE void SetChannelMap(const Audio::AlignedFloatBuffer& ChannelMap, const int32 InNumInterpFrames)
		{
			if (ChannelValues.Num() != ChannelMap.Num())
			{
				ChannelValues.Reset();
				for (int32 i = 0; i < ChannelMap.Num(); ++i)
				{
					ChannelValues.Add(FParam());
					ChannelValues[i].SetValue(ChannelMap[i], InNumInterpFrames);
				}
			}
			else
			{
				for (int32 i = 0; i < ChannelMap.Num(); ++i)
				{
					ChannelValues[i].SetValue(ChannelMap[i], InNumInterpFrames);
				}
			}
		}

		FORCEINLINE void UpdateChannelMap()
		{
			const int32 NumChannelValues = ChannelValues.Num();
			for (int32 i = 0; i < NumChannelValues; ++i)
			{
				ChannelValues[i].Update();
			}
		}

		FORCEINLINE void ResetInterpolation()
		{
			const int32 NumChannelValues = ChannelValues.Num();
			for (int32 i = 0; i < NumChannelValues; ++i)
			{
				ChannelValues[i].Reset();
			}
		}

		FORCEINLINE float GetChannelValue(int ChannelIndex)
		{
			return ChannelValues[ChannelIndex].GetValue();
		}

		FORCEINLINE void PadZeroes(const int32 ToSize, const int32 InNumInterpFrames)
		{
			int32 CurrentSize = ChannelValues.Num();
			for (int32 i = CurrentSize; i < ToSize; ++i)
			{
				ChannelValues.Add(FParam());
				ChannelValues[i].SetValue(0.0f, InNumInterpFrames);
			}
		}

	private:
		TArray<FParam> ChannelValues;
	};

	struct FSourceManagerInitParams
	{
		// Total number of sources to use in the source manager
		int32 NumSources;

		// Number of worker threads to use for the source manager.
		int32 NumSourceWorkers;
	};

	class FMixerSourceManager
	{
	public:
		FMixerSourceManager(FMixerDevice* InMixerDevice);
		~FMixerSourceManager();

		void Init(const FSourceManagerInitParams& InitParams);
		void Update();

		bool GetFreeSourceId(int32& OutSourceId);
		int32 GetNumActiveSources() const;
		int32 GetNumActiveBuses() const;

		void ReleaseSourceId(const int32 SourceId);
		void InitSource(const int32 SourceId, const FMixerSourceVoiceInitParams& InitParams);

		void Play(const int32 SourceId);
		void Stop(const int32 SourceId);
		void StopFade(const int32 SourceId, const int32 NumFrames);
		void Pause(const int32 SourceId);
		void SetPitch(const int32 SourceId, const float Pitch);
		void SetVolume(const int32 SourceId, const float Volume);
		void SetDistanceAttenuation(const int32 SourceId, const float DistanceAttenuation);
		void SetSpatializationParams(const int32 SourceId, const FSpatializationParams& InParams);
		void SetChannelMap(const int32 SourceId, const ESubmixChannelFormat SubmixChannelType, const Audio::AlignedFloatBuffer& InChannelMap, const bool bInIs3D, const bool bInIsCenterChannelOnly);
		void SetLPFFrequency(const int32 SourceId, const float Frequency);
		void SetHPFFrequency(const int32 SourceId, const float Frequency);

		void SetListenerTransforms(const TArray<FTransform>& ListenerTransforms);
		const TArray<FTransform>* GetListenerTransforms() const;

		int64 GetNumFramesPlayed(const int32 SourceId) const;
		float GetEnvelopeValue(const int32 SourceId) const;
		bool NeedsSpeakerMap(const int32 SourceId) const;
		void ComputeNextBlockOfSamples();
		void ClearStoppingSounds();
		void MixOutputBuffers(const int32 SourceId, const ESubmixChannelFormat InSubmixChannelType, const float SendLevel, AlignedFloatBuffer& OutWetBuffer) const;

		void SetSubmixSendInfo(const int32 SourceId, const FMixerSourceSubmixSend& SubmixSend);

		void UpdateDeviceChannelCount(const int32 InNumOutputChannels);

		void UpdateSourceEffectChain(const uint32 SourceEffectChainId, const TArray<FSourceEffectChainEntry>& SourceEffectChain, const bool bPlayEffectChainTails);

		const float* GetPreDistanceAttenuationBuffer(const int32 SourceId) const;
		const float* GetPreEffectBuffer(const int32 SourceId) const;
		const float* GetPreviousBusBuffer(const int32 SourceId) const;
		int32 GetNumChannels(const int32 SourceId) const;
		int32 GetNumOutputFrames() const { return NumOutputFrames; }
		bool IsBus(const int32 SourceId) const;
		void PumpCommandQueue();
		void UpdatePendingReleaseData(bool bForceWait = false);
		void FlushCommandQueue();
	private:

		void ReleaseSource(const int32 SourceId);
		void BuildSourceEffectChain(const int32 SourceId, FSoundEffectSourceInitData& InitData, const TArray<FSourceEffectChainEntry>& SourceEffectChain);
		void ResetSourceEffectChain(const int32 SourceId);
		void ReadSourceFrame(const int32 SourceId);

		void GenerateSourceAudio(const bool bGenerateBuses);
		void GenerateSourceAudio(const bool bGenerateBuses, const int32 SourceIdStart, const int32 SourceIdEnd);

		void ComputeSourceBuffersForIdRange(const bool bGenerateBuses, const int32 SourceIdStart, const int32 SourceIdEnd);
		void ComputePostSourceEffectBufferForIdRange(const bool bGenerateBuses, const int32 SourceIdStart, const int32 SourceIdEnd);
		void ComputeOutputBuffersForIdRange(const bool bGenerateBuses, const int32 SourceIdStart, const int32 SourceIdEnd);

		void ComputeBuses();
		void UpdateBuses();

		void AudioMixerThreadCommand(TFunction<void()> InFunction);

		static const int32 NUM_BYTES_PER_SAMPLE = 2;

		// Private class which perform source buffer processing in a worker task
		class FAudioMixerSourceWorker : public FNonAbandonableTask
		{
			FMixerSourceManager* SourceManager;
			int32 StartSourceId;
			int32 EndSourceId;
			bool bGenerateBuses;

		public:
			FAudioMixerSourceWorker(FMixerSourceManager* InSourceManager, const int32 InStartSourceId, const int32 InEndSourceId)
				: SourceManager(InSourceManager)
				, StartSourceId(InStartSourceId)
				, EndSourceId(InEndSourceId)
				, bGenerateBuses(false)
			{
			}

			void SetGenerateBuses(bool bInGenerateBuses)
			{
				bGenerateBuses = bInGenerateBuses;
			}

			void DoWork()
			{
				SourceManager->GenerateSourceAudio(bGenerateBuses, StartSourceId, EndSourceId);
			}

			FORCEINLINE TStatId GetStatId() const
			{
				RETURN_QUICK_DECLARE_CYCLE_STAT(FAudioMixerSourceWorker, STATGROUP_ThreadPoolAsyncTasks);
			}
		};

		FMixerDevice* MixerDevice;

		// Cached ptr to an optional spatialization plugin
		TAudioSpatializationPtr SpatializationPlugin;

		// Array of pointers to game thread audio source objects
		TArray<FMixerSourceVoice*> MixerSources;

		// A command queue to execute commands from audio thread (or game thread) to audio mixer device thread.
		struct FCommands
		{
			TArray<TFunction<void()>> SourceCommandQueue;
		};

		FCommands CommandBuffers[2];
		FThreadSafeCounter AudioThreadCommandBufferIndex;
		FThreadSafeCounter RenderThreadCommandBufferIndex;

		FEvent* CommandsProcessedEvent;

		TArray<int32> DebugSoloSources;

		struct FSubmixChannelTypeInfo
		{
			// Channel map parameter
			FSourceChannelMap ChannelMapParam;

			// Output buffer based on channel map param
			TArray<float> OutputBuffer;

			// Whether or not this channel type is used
			bool bUsed;

			FSubmixChannelTypeInfo()
				: bUsed(false)
			{}
		};

		struct FSourceInfo
		{
			FSourceInfo() {}
			~FSourceInfo() {}

			// Object which handles source buffer decoding
			TSharedPtr<FMixerSourceBuffer> MixerSourceBuffer;
			ISourceListener* SourceListener;

			// Data used for rendering sources
			TSharedPtr<FMixerSourceVoiceBuffer> CurrentPCMBuffer;
			int32 CurrentAudioChunkNumFrames;

			// The post-attenuation source buffer, used to send audio to submixes
			Audio::AlignedFloatBuffer SourceBuffer;
			Audio::AlignedFloatBuffer PreEffectBuffer;
			Audio::AlignedFloatBuffer PreDistanceAttenuationBuffer;
			Audio::AlignedFloatBuffer SourceEffectScratchBuffer;

			TArray<float> CurrentFrameValues;
			TArray<float> NextFrameValues;
			float CurrentFrameAlpha;
			int32 CurrentFrameIndex;
			int64 NumFramesPlayed;

			// The number of frames to wait before starting the source
			double StartTime;

			TArray<FMixerSourceSubmixSend> SubmixSends;

			// What bus Id this source is, if it is a bus. This is INDEX_NONE for sources which are not buses.
			uint32 BusId;

			// Number of samples to count for bus
			int64 BusDurationFrames;

			// What buses this source is sending its audio to. Used to remove this source from the bus send list.
			TArray<uint32> BusSends[(int32)EBusSendType::Count];

			// Interpolated source params
			FParam PitchSourceParam;
			float VolumeSourceStart;
			float VolumeSourceDestination;
			float VolumeFadeSlope;
			float VolumeFadeStart;
			int32 VolumeFadeFramePosition;
			int32 VolumeFadeNumFrames;

			float DistanceAttenuationSourceStart;
			float DistanceAttenuationSourceDestination;
			FParam LPFCutoffFrequencyParam;
			FParam HPFCutoffFrequencyParam;

			// One-Pole LPFs and HPFs per source
			Audio::FOnePoleLPFBank LowPassFilter;
			Audio::FOnePoleFilter HighPassFilter;

			// Source effect instances
			uint32 SourceEffectChainId;
			TArray<FSoundEffectSource*> SourceEffects;
			TArray<USoundEffectSourcePreset*> SourceEffectPresets;
			bool bEffectTailsDone;
			FSoundEffectSourceInputData SourceEffectInputData;

			FAudioPluginSourceOutputData AudioPluginOutputData;

			// A DSP object which tracks the amplitude envelope of a source.
			Audio::FEnvelopeFollower SourceEnvelopeFollower;
			float SourceEnvelopeValue;

			FSpatializationParams SpatParams;
			Audio::AlignedFloatBuffer ScratchChannelMap;

			// Output data, after computing a block of sample data, this is read back from mixers
			Audio::AlignedFloatBuffer ReverbPluginOutputBuffer;
			Audio::AlignedFloatBuffer* PostEffectBuffers;

			// Data needed for outputting to submixes
			FSubmixChannelTypeInfo SubmixChannelInfo[(int32) ESubmixChannelFormat::Count];

			// State management
			uint8 bIs3D:1;
			uint8 bIsCenterChannelOnly:1;
			uint8 bIsActive:1;
			uint8 bIsPlaying:1;
			uint8 bIsPaused:1;
			uint8 bIsStopping:1;
			uint8 bHasStarted:1;
			uint8 bIsBusy:1;
			uint8 bUseHRTFSpatializer:1;
			uint8 bUseOcclusionPlugin:1;
			uint8 bUseReverbPlugin:1;
			uint8 bIsDone:1;
			uint8 bIsLastBuffer:1;
			uint8 bOutputToBusOnly:1;
			uint8 bIsVorbis:1;
			uint8 bIsBypassingLPF:1;
			uint8 bIsBypassingHPF:1;
			uint8 bIsDebugMode:1;

			FString DebugName;

			// Source format info
			int32 NumInputChannels;
			int32 NumPostEffectChannels;
			int32 NumInputFrames;

			// ID for associated Audio Component if there is one, 0 otherwise
			uint64 AudioComponentID;
		};

		void ApplyDistanceAttenuation(FSourceInfo& InSourceInfo, int32 NumSamples);
		void ComputePluginAudio(FSourceInfo& InSourceInfo, int32 SourceId, int32 NumSamples);

		// Array of listener transforms
		TArray<FTransform> ListenerTransforms;

		// Array of source infos.
		TArray<FSourceInfo> SourceInfos;

		// Map of bus object Id's to bus data. 
		TMap<uint32, FMixerBus> Buses;

		// Async task workers for processing sources in parallel
		TArray<FAsyncTask<FAudioMixerSourceWorker>*> SourceWorkers;

		// Array of task data waiting to finished. Processed on audio render thread.
		TArray<TSharedPtr<FMixerSourceBuffer>> PendingSourceBuffers;

		// General information about sources in source manager accessible from game thread
		struct FGameThreadInfo
		{
			TArray<int32> FreeSourceIndices;
			TArray<bool> bIsBusy;
			TArray <bool> bNeedsSpeakerMap;
			TArray<bool> bIsDebugMode;
		} GameThreadInfo;

		int32 NumActiveSources;
		int32 NumTotalSources;
		int32 NumOutputFrames;
		int32 NumOutputSamples;
		int32 NumSourceWorkers;

		uint8 bInitialized : 1;
		uint8 bUsingSpatializationPlugin : 1;

		friend class FMixerSourceVoice;
		// Set to true when the audio source manager should pump the command queue
		FThreadSafeBool bPumpQueue;
	};
}
