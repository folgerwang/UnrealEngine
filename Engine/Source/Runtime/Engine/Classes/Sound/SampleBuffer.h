// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Dsp.h"
#include "Misc/Paths.h"
#include "Sound/SoundEffectBase.h"
#include "Async/AsyncWork.h"
#include "Templates/UnrealTypeTraits.h"

class USoundWave;
class FAudioDevice;

namespace Audio
{
	typedef int16 DefaultUSoundWaveSampleType;

	// An object which fully loads/decodes a sound wave and allows direct access to sound wave data.	
	/************************************************************************/
	/* TSampleBuffer<class SampleType>                                      */
	/* This class owns an audio buffer.                                     */
	/* To convert between fixed Q15 buffers and float buffers,              */
	/* Use the assignment operator. Example:                                */
	/*                                                                      */
	/* TSampleBuffer<float> AFloatBuffer;                                   */
	/* TSampleBuffer<int16> AnIntBuffer = AFloatBuffer;                     */
	/************************************************************************/
	template <class SampleType = DefaultUSoundWaveSampleType>
	class ENGINE_API TSampleBuffer
	{
	public:
		// Ptr to raw PCM data buffer
		TUniquePtr<SampleType[]> RawPCMData;
		// The number of samples in the buffer
		int32 NumSamples;
		// The number of frames in the buffer
		int32 NumFrames;
		// The number of channels in the buffer
		int32 NumChannels;
		// The sample rate of the buffer	
		int32 SampleRate;
		// The duration of the buffer in seconds
		float SampleDuration;

	public:
		TSampleBuffer()
			: RawPCMData(nullptr)
			, NumSamples(0)
			, NumFrames(0)
			, NumChannels(0)
			, SampleRate(0)
			, SampleDuration(0.0f)
		{}

		FORCEINLINE TSampleBuffer(const TSampleBuffer& Other)
		{
			NumSamples = Other.NumSamples;
			NumFrames = Other.NumFrames;
			NumChannels = Other.NumChannels;
			SampleRate = Other.SampleRate;
			SampleDuration = Other.SampleDuration;

			RawPCMData.Reset(new SampleType[NumSamples]);
			FMemory::Memcpy(RawPCMData.Get(), Other.RawPCMData.Get(), NumSamples * sizeof(SampleType));
		}

		FORCEINLINE TSampleBuffer(AlignedFloatBuffer& InData, int32 InNumChannels, int32 InSampleRate)
		{
			NumSamples = InData.Num();
			NumFrames = NumSamples / InNumChannels;
			NumChannels = InNumChannels;
			SampleRate = InSampleRate;
			SampleDuration = ((float)NumFrames) / SampleRate;

			RawPCMData.Reset(new SampleType[NumSamples]);
			float* InBufferPtr = InData.GetData();

			if (TIsSame<SampleType, float>::Value)
			{
				FMemory::Memcpy(RawPCMData.Get(), InBufferPtr, NumSamples * sizeof(float));
			}
			else if (TIsSame<SampleType, int16>::Value)
			{
				// Convert from float to int:
				for (int32 SampleIndex = 0; SampleIndex < NumSamples; SampleIndex++)
				{
					RawPCMData[SampleIndex] = (int16)(InBufferPtr[SampleIndex] * 32767.0f);
				}
			}
			else
			{
				// rely on casts:
				for (int32 SampleIndex = 0; SampleIndex < NumSamples; SampleIndex++)
				{
					RawPCMData[SampleIndex] = (SampleType)(InBufferPtr[SampleIndex]);
				}
			}
		}

		// Vanilla assignment operator:
		FORCEINLINE TSampleBuffer& operator=(const TSampleBuffer& Other)
		{
			NumSamples = Other.NumSamples;
			NumFrames = Other.NumFrames;
			NumChannels = Other.NumChannels;
			SampleRate = Other.SampleRate;
			SampleDuration = Other.SampleDuration;

			RawPCMData.Reset(new SampleType[NumSamples]);
			FMemory::Memcpy(RawPCMData.Get(), Other.RawPCMData.Get(), NumSamples * sizeof(SampleType));

			return *this;
		}

		//SampleType converting assignment operator:
		template<class OtherSampleType>
		FORCEINLINE TSampleBuffer& operator=(const TSampleBuffer<OtherSampleType>& Other)
		{
			NumSamples = Other.NumSamples;
			NumFrames = Other.NumFrames;
			NumChannels = Other.NumChannels;
			SampleRate = Other.SampleRate;
			SampleDuration = Other.SampleDuration;

			RawPCMData.Reset(new SampleType[NumSamples]);
			if (TIsSame<SampleType, OtherSampleType>::Value)
			{
				// If buffers are of the same type, copy over:
				FMemory::Memcpy(RawPCMData.Get(), Other.RawPCMData.Get(), NumSamples * sizeof(SampleType));
			}
			else if (TIsSame<SampleType, int16>::Value && TIsSame<OtherSampleType, float>::Value)
			{
				// Convert from float to int:
				for (int32 SampleIndex = 0; SampleIndex < NumSamples; SampleIndex++)
				{
					RawPCMData[SampleIndex] = (int16)(Other.RawPCMData[SampleIndex] * 32767.0f);
				}
			}
			else if (TIsSame<SampleType, float>::Value && TIsSame<OtherSampleType, int16>::Value)
			{
				// Convert from int to float:
				for (int32 SampleIndex = 0; SampleIndex < NumSamples; SampleIndex++)
				{
					RawPCMData[SampleIndex] = ((float)Other.RawPCMData[SampleIndex]) / 32767.0f;
				}
			}
			else
			{
				// for any other types, we don't know how to explicitly convert, so we fall back to casts:
				for (int32 SampleIndex = 0; SampleIndex < NumSamples; SampleIndex++)
				{
					RawPCMData[SampleIndex] = Other.RawPCMData[SampleIndex];
				}
			}

			return *this;
		}

		~TSampleBuffer() {};

		// Gets the raw PCM data of the sound wave
		FORCEINLINE const SampleType* GetData() const
		{
			return RawPCMData.Get();
		}

		// Gets the number of samples of the sound wave
		FORCEINLINE int32 GetNumSamples() const
		{
			return NumSamples;
		}

		// Gets the number of frames of the sound wave
		FORCEINLINE int32 GetNumFrames() const
		{
			return NumFrames;
		}

		// Gets the number of channels of the sound wave
		FORCEINLINE int32 GetNumChannels() const
		{
			return NumChannels;
		}

		// Gets the sample rate of the sound wave
		FORCEINLINE int32 GetSampleRate() const
		{
			return SampleRate;
		}

		FORCEINLINE void MixBufferToChannels(int32 InNumChannels)
		{
			if (!RawPCMData.IsValid() || InNumChannels <= 0)
			{
				return;
			}

			TUniquePtr<SampleType[]> TempBuffer;
			TempBuffer.Reset(new SampleType[InNumChannels * NumFrames]);
			FMemory::Memset(TempBuffer.Get(), 0, InNumChannels * NumFrames * sizeof(SampleType));

			const SampleType* SrcBuffer = GetData();

			// Downmixing using the channel modulo assumption:
			// TODO: Use channel matrix for channel conversions.
			for (int32 FrameIndex = 0; FrameIndex < NumFrames; FrameIndex++)
			{
				for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ChannelIndex++)
				{
					const int32 DstSampleIndex = FrameIndex * InNumChannels + (ChannelIndex % InNumChannels);
					const int32 SrcSampleIndex = FrameIndex * NumChannels + ChannelIndex;

					TempBuffer[DstSampleIndex] += SrcBuffer[SrcSampleIndex];
				}
			}

			NumChannels = InNumChannels;
			NumSamples = NumFrames * NumChannels;
			
			// Resize our buffer and copy the result in:
			RawPCMData.Reset(new SampleType[NumChannels * NumFrames]);
			FMemory::Memcpy(RawPCMData.Get(), TempBuffer.Get(), NumSamples * sizeof(SampleType));
		}
	};

	// FSampleBuffer is a strictly defined TSampleBuffer that uses the same sample format we use for USoundWaves.
	typedef TSampleBuffer<> FSampleBuffer;

	/************************************************************************/
	/* FSoundWavePCMLoader                                                  */
	/* This class loads and decodes a USoundWave asset into a TSampleBuffer.*/
	/* To use, call LoadSoundWave with the sound wave you'd like to load    */
	/* and call Update on every tick until it returns true, at which point  */
	/* you may call GetSampleBuffer to get the decoded audio.               */
	/************************************************************************/
	class ENGINE_API FSoundWavePCMLoader
	{
	public:
		FSoundWavePCMLoader();

		// Intialize loader with audio device
		void Init(FAudioDevice* InAudioDevice);

		// Loads a USoundWave, call on game thread.
		void LoadSoundWave(USoundWave* InSoundWave);

		// Update the loading state. Returns true once the sound wave is loaded/decoded.
		// Call on game thread.
		bool Update();

		// Returns the sample buffer data once the sound wave is loaded/decoded. Call on game thread thread.
		void GetSampleBuffer(TSampleBuffer<>& OutSampleBuffer);

		// Empties pending sound wave load references. Call on audio rendering thread.
		void Reset();

		// Queries whether the current sound wave has finished loading/decoding
		bool IsSoundWaveLoaded() const { return bIsLoaded; }

	private:
		
		// Ptr to the audio device to use to do the decoding
		FAudioDevice* AudioDevice;
		// Reference to current loading sound wave
		USoundWave* SoundWave;	
		// Struct to meta-data of decoded PCM buffer and ptr to PCM data
		TSampleBuffer<> SampleBuffer;
		// Queue of sound wave ptrs to hold references to them until fully released in audio render thread
		TQueue<USoundWave*> PendingStoppingSoundWaves;
		// Whether the sound wave load/decode is in-flight
		bool bIsLoading;
		// Whether or not the sound wave has already been loaded
		bool bIsLoaded;
	};

	// Enum used to express the current state of a FSoundWavePCMWriter's current operation.
	enum class ESoundWavePCMWriterState : uint8
	{
		Idle,
		Generating,
		WritingToDisk,
		Suceeded,
		Failed,
		Cancelled
	};

	// Enum used internally by the FSoundWavePCMWriter.
	enum class ESoundWavePCMWriteTaskType : uint8
	{
		GenerateSoundWave,
		GenerateAndWriteSoundWave,
		WriteSoundWave,
		WriteWavFile
	};

	/************************************************************************/
	/* FAsyncSoundWavePCMWriteWorker                                        */
	/* This class is used by FSoundWavePCMWriter to handle async writing.   */
	/************************************************************************/
	class ENGINE_API FAsyncSoundWavePCMWriteWorker : public FNonAbandonableTask
	{
	protected:
		class FSoundWavePCMWriter* Writer;
		ESoundWavePCMWriteTaskType TaskType;

		FCriticalSection NonAbandonableSection;

		TFunction<void(const USoundWave*)> CallbackOnSuccess;

	public:
		
		FAsyncSoundWavePCMWriteWorker(FSoundWavePCMWriter* InWriter, ESoundWavePCMWriteTaskType InTaskType, TFunction<void(const USoundWave*)> OnSuccess);
		~FAsyncSoundWavePCMWriteWorker();

		/**
		* Performs write operations async.
		*/
		void DoWork();

		bool CanAbandon() 
		{ 
			return true;
		}

		void Abandon();

		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncSoundWavePCMWriteWorker, STATGROUP_ThreadPoolAsyncTasks);
		}
	};

	typedef FAsyncTask<FAsyncSoundWavePCMWriteWorker> FAsyncSoundWavePCMWriterTask;

	// This is the default chunk size, in bytes that FSoundWavePCMWriter writes to the disk at once.
	static const int32 WriterDefaultChunkSize = 8192;


	/************************************************************************/
	/* FSoundWavePCMWriter                                                  */
	/* This class can be used to save a TSampleBuffer to either a wav  file */
	/* or a USoundWave using BeginGeneratingSoundWaveFromBuffer,            */
	/* BeginWriteToSoundWave, or BeginWriteToWavFile on the game thread.    */
	/* This class uses an async task to generate and write the file to disk.*/
	/************************************************************************/
	class ENGINE_API FSoundWavePCMWriter
	{
	public:
		friend class FAsyncSoundWavePCMWriteWorker;

		FSoundWavePCMWriter(int32 InChunkSize = WriterDefaultChunkSize);
		~FSoundWavePCMWriter();

		// This kicks off an operation to write InSampleBuffer to SoundWaveToSaveTo.
		// If InSoundWave is not nullptr, the audio will be written directly into
		// Returns true on a successful start, false otherwise.
		bool BeginGeneratingSoundWaveFromBuffer(const TSampleBuffer<>& InSampleBuffer, USoundWave* InSoundWave = nullptr, TFunction<void(const USoundWave*)> OnSuccess = [](const USoundWave* ResultingWave){});

		// This kicks off an operation to write InSampleBuffer to a USoundWave asset
		// at the specified file path relative to the project directory.
		// This function should only be used in the editor.
		// If a USoundWave asset already exists 
		bool BeginWriteToSoundWave(const FString& FileName, const TSampleBuffer<>& InSampleBuffer, FString InPath, TFunction<void(const USoundWave*)> OnSuccess = [](const USoundWave* ResultingWave) {});
	
		// This writes out the InSampleBuffer as a wav file at a path relative to
		// the BouncedWavFiles folder in the Saved directory. This can be used
		// in non-editor builds.
		bool BeginWriteToWavFile(const TSampleBuffer<>& InSampleBuffer, const FString& FileName, FString& FilePath, TFunction<void()> OnSuccess = []() {});

		// This is a blocking call that will return the SoundWave generated from InSampleBuffer.
		// Optionally, if you're using the editor, you can also write the resulting soundwave out to the content browser using the FileName and FilePath parameters.
		USoundWave* SynchronouslyWriteSoundWave(const TSampleBuffer<>& InSampleBuffer, const FString* FileName = nullptr, const FString* FilePath = nullptr);

		// Call this on the game thread to continue the write operation. Optionally provide a pointer
		// to an ESoundWavePCMWriterState which will be written to with the current state of the write operation.
		// Returns a float value from 0 to 1 indicating how complete the write operation is.
		float CheckStatus(ESoundWavePCMWriterState* OutCurrentState = nullptr);

		// Aborts the current write operation.
		void CancelWrite();

		// Whether we have finished the write operation, by either succeeding, failing, or being cancelled.
		bool IsDone();

		// Clean up all resources used.
		void Reset();

		// Used to grab the a handle to the soundwave. 
		USoundWave* GetFinishedSoundWave();

		// This function can be used after generating a USoundWave by calling BeginGeneratingSoundWaveFromBuffer
		// to save the generated soundwave to an asset.
		// This is handy if you'd like to preview or edit the USoundWave before saving it to disk.
		void SaveFinishedSoundWaveToPath(const FString& FileName, FString InPath = FPaths::EngineContentDir());

	private:
		// Current pending buffer.
		TSampleBuffer<> CurrentBuffer;

		// Sound wave currently being written to.
		USoundWave* CurrentSoundWave;

		// Current state of the buffer.
		ESoundWavePCMWriterState CurrentState;

		// Current Absolute File Path we are writing to.
		FString AbsoluteFilePath;

		bool bWasPreviouslyAddedToRoot;

		TUniquePtr<FAsyncSoundWavePCMWriterTask> CurrentOperation;

		// Internal buffer for holding the serialized wav file in memory.
		TArray<uint8> SerializedWavData;

		// Internal progress
		FThreadSafeCounter Progress;

		int32 ChunkSize;

		UPackage* CurrentPackage;

	private:

		//  This is used to emplace CurrentBuffer in CurrentSoundWave.
		void ApplyBufferToSoundWave();

		// This is used to save CurrentSoundWave within CurrentPackage.
		void SerializeSoundWaveToAsset();

		// This is used to write a WavFile in disk.
		void SerializeBufferToWavFile();

		// This checks to see if a directory exists and, if it does not, recursively adds the directory.
		bool CreateDirectoryIfNeeded(FString& DirectoryPath);
	};

	/************************************************************************/
	/* FAudioRecordingData                                                  */
	/* This is used by USoundSubmix and the AudioMixerBlueprintLibrary      */
	/* to contain FSoundWavePCMWriter operations.                           */
	/************************************************************************/
	struct FAudioRecordingData
	{
		TSampleBuffer<int16> InputBuffer;
		FSoundWavePCMWriter Writer;

		~FAudioRecordingData() {};
	};

}
