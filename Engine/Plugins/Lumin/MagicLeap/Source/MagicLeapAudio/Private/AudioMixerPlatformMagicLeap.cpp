// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AudioMixerPlatformMagicLeap.h"
#include "Modules/ModuleManager.h"
#include "AudioMixer.h"
#include "AudioMixerDevice.h"
#include "CoreGlobals.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "VorbisAudioInfo.h"
#include "ADPCMAudioInfo.h"


DECLARE_LOG_CATEGORY_EXTERN(LogAudioMixerMagicLeap, Log, All);
DEFINE_LOG_CATEGORY(LogAudioMixerMagicLeap);

// Macro to check result for failure, get string version, log, and return false
#define MLAUDIO_RETURN_ON_FAIL(Result)						\
	if (Result != MLAudioError_Success)						\
	{														\
		const TCHAR* ErrorString = FMixerPlatformMagicLeap::GetErrorString(Result);	\
		AUDIO_PLATFORM_ERROR(ErrorString);					\
		return false;										\
	}

#define MLAUDIO_CHECK_ON_FAIL(Result)						\
	if (Result != MLAudioError_Success)						\
	{														\
		const TCHAR* ErrorString = FMixerPlatformMagicLeap::GetErrorString(Result);	\
		AUDIO_PLATFORM_ERROR(ErrorString);					\
		check(false);										\
	}

#define MLAUDIO_LOG_ON_FAIL(Result)							\
	if (Result != MLAudioError_Success)						\
	{														\
		const TCHAR* ErrorString = FMixerPlatformMagicLeap::GetErrorString(Result);	\
		UE_LOG(LogAudioMixerMagicLeap, Error, TEXT("Error in %s, line %s: %s"), __FILE__, __LINE__,ErrorString); \
	}

namespace Audio
{
	// ML1 currently only has stereo speakers and stereo aux support.
	const uint32 DefaultNumChannels = 2;
	// TODO: @Epic check the value tp be used. Setting default for now.
	const uint32 DefaultSamplesPerSecond = 48000;  // presumed 48KHz and 16 bits for the sample

#if WITH_MLSDK
	static void GetMLDeviceDefaults(MLAudioBufferFormat& OutFormat, uint32& OutSize, uint32& OutMinSize, MLAudioError& Result)
	{
		static bool bRetrievedDeviceDefaults = false;
		static uint32 CachedSize = 0;
		static uint32 CachedMinSize = 0;
		static MLAudioBufferFormat CachedBufferFormat;
		static MLAudioError CachedResult;

		if (!bRetrievedDeviceDefaults)
		{
			bRetrievedDeviceDefaults = MLAudioGetOutputStreamDefaults(DefaultNumChannels, DefaultSamplesPerSecond, &CachedBufferFormat, &CachedSize, &CachedMinSize, &CachedResult);
		}

		OutFormat = CachedBufferFormat;
		OutSize = CachedSize;
		OutMinSize = CachedMinSize;
		Result = CachedResult;
	}
#endif //WITH_MLSDK

	FMixerPlatformMagicLeap::FMixerPlatformMagicLeap()
		: CachedBufferHandle(nullptr)
		, bSuspended(false)
		, bInitialized(false)
		, bInCallback(false)
#if WITH_MLSDK
		, StreamHandle(ML_INVALID_HANDLE)
#endif //WITH_MLSDK
	{
	}

	FMixerPlatformMagicLeap::~FMixerPlatformMagicLeap()
	{
		if (bInitialized)
		{
			TeardownHardware();
		}
	}

#if WITH_MLSDK
	const TCHAR* FMixerPlatformMagicLeap::GetErrorString(MLAudioError Result)
	{
		switch (Result)
		{
		case MLAudioError_Success:					return TEXT("MLAudioError_Success");
		case MLAudioError_NotImplemented:			return TEXT("MLAudioError_NotImplemented");
		case MLAudioError_UnspecifiedFailure:       return TEXT("MLAudioError_UnspecifiedFailure");
		case MLAudioError_UnauthorizedCommand:      return TEXT("MLAudioError_UnauthorizedCommand");
		case MLAudioError_HandleNotFound:           return TEXT("MLAudioError_HandleNotFound");
		case MLAudioError_InvalidArgument:          return TEXT("MLAudioError_InvalidArgument");
		case MLAudioError_InvalidSampleRate:        return TEXT("MLAudioError_InvalidSampleRate");
		case MLAudioError_InvalidBitsPerSample:     return TEXT("MLAudioError_InvalidBitsPerSample");
		case MLAudioError_InvalidValidBits:         return TEXT("MLAudioError_InvalidValidBits");
		case MLAudioError_InvalidSampleFormat:      return TEXT("MLAudioError_InvalidSampleFormat");
		case MLAudioError_InvalidChannelCount:      return TEXT("MLAudioError_InvalidChannelCount");
		case MLAudioError_InvalidChannelFormat:     return TEXT("MLAudioError_InvalidChannelFormat");
		case MLAudioError_InvalidBufferSize:        return TEXT("MLAudioError_InvalidBufferSize");
		case MLAudioError_BufferAllocFailure:       return TEXT("MLAudioError_BufferAllocFailure");
		case MLAudioError_BufferNotReady:           return TEXT("MLAudioError_BufferNotReady");
		case MLAudioError_FileNotFound:             return TEXT("MLAudioError_FileNotFound");
		case MLAudioError_FileNotRecognized:        return TEXT("MLAudioError_FileNotRecognized");
		default:                                return TEXT("MlAudioError_UnknownError");
		}
	}
#endif //WITH_MLSDK

	bool FMixerPlatformMagicLeap::InitializeHardware()
	{
		if (bInitialized)
		{
			return false;
		}

		// Register application lifecycle delegates
		FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(this, &FMixerPlatformMagicLeap::SuspendContext);
		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FMixerPlatformMagicLeap::ResumeContext);

		bInitialized = true;

		return true;
	}

	bool FMixerPlatformMagicLeap::TeardownHardware()
	{
		if (!bInitialized)
		{
			return true;
		}

		// Unregister application lifecycle delegates
		FCoreDelegates::ApplicationWillEnterBackgroundDelegate.RemoveAll(this);
		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.RemoveAll(this);

		bInitialized = false;
		return true;
	}

	bool FMixerPlatformMagicLeap::IsInitialized() const
	{
		return bInitialized;
	}

	bool FMixerPlatformMagicLeap::GetNumOutputDevices(uint32& OutNumOutputDevices)
	{
		// ML1 will always have just one device.
		OutNumOutputDevices = 1;
		return true;
	}

	bool FMixerPlatformMagicLeap::GetOutputDeviceInfo(const uint32 InDeviceIndex, FAudioPlatformDeviceInfo& OutInfo)
	{
#if WITH_MLSDK
		MLAudioError Result = MLAudioError_Success;
		MLAudioBufferFormat DesiredBufferFormat;
		uint32 OutSize;
		uint32 OutMinimalSize;

		GetMLDeviceDefaults(DesiredBufferFormat, OutSize, OutMinimalSize, Result);

		OutInfo.Name = TEXT("Magic Leap Audio Device");
		OutInfo.DeviceId = 0;
		OutInfo.bIsSystemDefault = true;
		OutInfo.SampleRate = DesiredBufferFormat.samples_per_second;
		OutInfo.NumChannels = DefaultNumChannels;

		if (DesiredBufferFormat.sample_format == MLAudioSampleFormat_Float && DesiredBufferFormat.bits_per_sample == 32)
		{
			OutInfo.Format = EAudioMixerStreamDataFormat::Float;
		}
		else if (DesiredBufferFormat.sample_format == MLAudioSampleFormat_Int && DesiredBufferFormat.bits_per_sample == 16)
		{
			OutInfo.Format = EAudioMixerStreamDataFormat::Int16;
		}
		else
		{
			//Unknown format:
			UE_LOG(LogAudioMixerMagicLeap, Error, TEXT("Invalid sample type requested. "));
			return false;
		}

		OutInfo.OutputChannelArray.SetNum(2);
		OutInfo.OutputChannelArray[0] = EAudioMixerChannel::FrontLeft;
		OutInfo.OutputChannelArray[1] = EAudioMixerChannel::FrontRight;
#endif //WITH_MLSDK

		return true;
	}

	bool FMixerPlatformMagicLeap::GetDefaultOutputDeviceIndex(uint32& OutDefaultDeviceIndex) const
	{
		OutDefaultDeviceIndex = 0;
		return true;
	}

	bool FMixerPlatformMagicLeap::OpenAudioStream(const FAudioMixerOpenStreamParams& Params)
	{
#if WITH_MLSDK
		if (!bInitialized || AudioStreamInfo.StreamState != EAudioOutputStreamState::Closed)
		{
			return false;
		}

		if (!GetOutputDeviceInfo(AudioStreamInfo.OutputDeviceIndex, AudioStreamInfo.DeviceInfo))
		{
			return false;
		}

		MLAudioError Result = MLAudioError_Success;
		MLAudioBufferFormat DesiredBufferFormat;
		uint32 OutSize;
		uint32 OutMinimalSize;

		GetMLDeviceDefaults(DesiredBufferFormat, OutSize, OutMinimalSize, Result);

		OpenStreamParams = Params;

		// Number of frames is defined by the default buffer size, divided by the size of a single frame,
		// which is the number of channels times the number of bytes in a single sample.
		OpenStreamParams.NumFrames = OutSize / (DefaultNumChannels * (DesiredBufferFormat.bits_per_sample / 8));

		AudioStreamInfo.Reset();

		AudioStreamInfo.OutputDeviceIndex = 0;
		AudioStreamInfo.NumOutputFrames = OpenStreamParams.NumFrames;
		AudioStreamInfo.NumBuffers = OpenStreamParams.NumBuffers;
		AudioStreamInfo.AudioMixer = OpenStreamParams.AudioMixer;
		GetOutputDeviceInfo(0, AudioStreamInfo.DeviceInfo);

		DesiredBufferFormat.channel_count = DefaultNumChannels;

		if (!MLAudioCreateSoundWithOutputStream(&DesiredBufferFormat, OutSize, &MLAudioCallback, this, &StreamHandle, &Result))
		{
			MLAUDIO_LOG_ON_FAIL(Result);
			return false;
		}

		AudioStreamInfo.StreamState = EAudioOutputStreamState::Open;
#endif //WITH_MLSDK

		return true;
	}

	bool FMixerPlatformMagicLeap::CloseAudioStream()
	{
#if WITH_MLSDK
		check(MLHandleIsValid(StreamHandle));

		if (!bInitialized || (AudioStreamInfo.StreamState != EAudioOutputStreamState::Open && AudioStreamInfo.StreamState != EAudioOutputStreamState::Stopped))
		{
			return false;
		}

		MLAudioError Result = MLAudioError_Success;

		if (!MLAudioDestroySound(StreamHandle, &Result))
		{
			MLAUDIO_LOG_ON_FAIL(Result);
			return false;
		}

		AudioStreamInfo.StreamState = EAudioOutputStreamState::Closed;
		StreamHandle = ML_INVALID_HANDLE;
#endif //WITH_MLSDK

		return true;
	}

	bool FMixerPlatformMagicLeap::StartAudioStream()
	{
#if WITH_MLSDK
		BeginGeneratingAudio();

		check(MLHandleIsValid(StreamHandle));

		MLAudioError Result = MLAudioError_Success;

		//Pre buffer with two zeroed buffers:
		static const int32 NumberOfBuffersToPrecache = 2;
		for (int32 BufferIndex = 0; BufferIndex < NumberOfBuffersToPrecache; BufferIndex++)
		{
			MLAudioBuffer PrecacheBuffer;

			if (!MLAudioGetOutputStreamBuffer(StreamHandle, &PrecacheBuffer, &Result))
			{
				MLAUDIO_LOG_ON_FAIL(Result);
				break;
			}

			FMemory::Memzero(PrecacheBuffer.ptr, PrecacheBuffer.size);

			if (!MLAudioReleaseOutputStreamBuffer(StreamHandle, &Result))
			{
				MLAUDIO_LOG_ON_FAIL(Result);
				break;
			}
		}

		if (!MLAudioStartSound(StreamHandle, &Result))
		{
			MLAUDIO_LOG_ON_FAIL(Result);
			return false;
		}
#endif //WITH_MLSDK

		return true;
	}

	bool FMixerPlatformMagicLeap::StopAudioStream()
	{
#if WITH_MLSDK
		MLAudioError Result;
		if (!bInitialized || AudioStreamInfo.StreamState != EAudioOutputStreamState::Running)
		{
			return false;
		}

		if (!MLAudioStopSound(StreamHandle, &Result))
		{
			MLAUDIO_LOG_ON_FAIL(Result);
			return false;
		}

		if (AudioStreamInfo.StreamState == EAudioOutputStreamState::Running)
		{
			StopGeneratingAudio();
		}

		check(AudioStreamInfo.StreamState == EAudioOutputStreamState::Stopped);
#endif //WITH_MLSDK

		return true;
	}

	FAudioPlatformDeviceInfo FMixerPlatformMagicLeap::GetPlatformDeviceInfo() const
	{
		check(AudioStreamInfo.DeviceInfo.NumChannels == 2);
		return AudioStreamInfo.DeviceInfo;
	}

	FAudioPlatformSettings FMixerPlatformMagicLeap::GetPlatformSettings() const
	{
#if WITH_MLSDK
		MLAudioError Result = MLAudioError_Success;
		MLAudioBufferFormat DesiredBufferFormat;
		uint32 OutSize;
		uint32 OutMinimalSize;

		GetMLDeviceDefaults(DesiredBufferFormat, OutSize, OutMinimalSize, Result);

		FAudioPlatformSettings PlatformSettings;
		PlatformSettings.CallbackBufferFrameSize = OutSize / (DefaultNumChannels * (DesiredBufferFormat.bits_per_sample / 8));
		PlatformSettings.MaxChannels = 0;
		PlatformSettings.NumBuffers = 2;
		PlatformSettings.SampleRate = DesiredBufferFormat.samples_per_second;

		return PlatformSettings;
#else
		return FAudioPlatformSettings();
#endif //WITH_MLSDK
	}

	void FMixerPlatformMagicLeap::SubmitBuffer(const uint8* Buffer)
	{
		CachedBufferHandle = (uint8*)Buffer;
	}

	FName FMixerPlatformMagicLeap::GetRuntimeFormat(USoundWave* InSoundWave)
	{
#if WITH_OGGVORBIS
		static FName NAME_OGG(TEXT("OGG"));
		if (InSoundWave->HasCompressedData(NAME_OGG))
		{
			return NAME_OGG;
		}
#endif

		static FName NAME_ADPCM(TEXT("ADPCM"));

		return NAME_ADPCM;
	}

	bool FMixerPlatformMagicLeap::HasCompressedAudioInfoClass(USoundWave* InSoundWave)
	{
		return true;
	}

	ICompressedAudioInfo* FMixerPlatformMagicLeap::CreateCompressedAudioInfo(USoundWave* InSoundWave)
	{
#if WITH_OGGVORBIS
		static FName NAME_OGG(TEXT("OGG"));
		if (InSoundWave->HasCompressedData(NAME_OGG))
		{
			return new FVorbisAudioInfo();
		}
#endif
		static FName NAME_ADPCM(TEXT("ADPCM"));
		return new FADPCMAudioInfo();
	}

	FString FMixerPlatformMagicLeap::GetDefaultDeviceName()
	{
		return FString(TEXT("MLAudio"));
	}

	void FMixerPlatformMagicLeap::ResumeContext()
	{
#if WITH_MLSDK
		if (bSuspended)
		{
			if (MLHandleIsValid(StreamHandle))
			{
				MLAudioError Result;

				if (!MLAudioStartSound(StreamHandle, &Result))
				{
					MLAUDIO_LOG_ON_FAIL(Result);
					return;
				}
			}

			bSuspended = false;
		}
#endif //WITH_MLSDK
	}

	int32 FMixerPlatformMagicLeap::GetNumFrames(const int32 InNumReqestedFrames)
	{
#if WITH_MLSDK
		MLAudioError Result = MLAudioError_Success;
		MLAudioBufferFormat DesiredBufferFormat;
		uint32 OutSize;
		uint32 OutMinimalSize;

		GetMLDeviceDefaults(DesiredBufferFormat, OutSize, OutMinimalSize, Result);
		return OutSize / (DefaultNumChannels * (DesiredBufferFormat.bits_per_sample / 8));
#else
		return 0;
#endif //WITH_MLSDK
	}

	void FMixerPlatformMagicLeap::SuspendContext()
	{
#if WITH_MLSDK
		if (!bSuspended)
		{
			if (MLHandleIsValid(StreamHandle))
			{
				MLAudioError Result;

				if (!MLAudioStopSound(StreamHandle, &Result))
				{
					MLAUDIO_LOG_ON_FAIL(Result);
					return;
				}
			}

			bSuspended = true;
		}
#endif //WITH_MLSDK
	}

	void FMixerPlatformMagicLeap::MLAudioCallback(void* CallbackContext)
	{
#if WITH_MLSDK
		FMixerPlatformMagicLeap* InPlatform = (FMixerPlatformMagicLeap*)CallbackContext;
		MLAudioError Result;

		MLAudioBuffer CallbackBuffer;

		check(MLHandleIsValid(InPlatform->StreamHandle));

		// Get the callback buffer from MLAudio 
		if (!MLAudioGetOutputStreamBuffer(InPlatform->StreamHandle, &CallbackBuffer, &Result))
		{
			MLAUDIO_LOG_ON_FAIL(Result);
			return;
		}

		if (InPlatform->CachedBufferHandle == nullptr)
		{
			InPlatform->ReadNextBuffer();
		}

		// It is possible that ReadNextBuffer() doesn't call SubmitBuffer()
		// in which case CachedBufferHandle will still be null and memcpy will segfault.
		if (InPlatform->CachedBufferHandle != nullptr)
		{
			// Fill the callback buffer:
			FMemory::Memcpy(CallbackBuffer.ptr, InPlatform->CachedBufferHandle, CallbackBuffer.size);
		}

		if (!MLAudioReleaseOutputStreamBuffer(InPlatform->StreamHandle, &Result))
		{
			MLAUDIO_LOG_ON_FAIL(Result);
			return;
		}

		InPlatform->CachedBufferHandle = nullptr;
#endif //WITH_MLSDK
	}

}
