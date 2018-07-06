// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AudioCaptureInternal.h"
#include "Misc/CoreDelegates.h"

#if PLATFORM_LUMIN

namespace Audio 
{

FAudioCaptureImpl::FAudioCaptureImpl()
	: Callback(nullptr)
	, NumChannels(1)
	, SampleRate(16000)
	, InputDeviceHandle(ML_INVALID_HANDLE)
	, bStreamStarted(false)
{
}

static void OnAudioCaptureCallback(MLHandle Handle, void* CallbackContext)
{
	(void)Handle;
	FAudioCaptureImpl* AudioCapture = (FAudioCaptureImpl*)CallbackContext;
	check(MLHandleIsValid(AudioCapture->InputDeviceHandle));
	MLAudioBuffer OutputBuffer;
	MLResult Result = MLAudioGetInputStreamBuffer(AudioCapture->InputDeviceHandle, &OutputBuffer);
	if (Result == MLResult_Ok)
	{
		AudioCapture->OnAudioCapture(OutputBuffer.ptr, OutputBuffer.size / sizeof(int16), 0.0, false);
		Result = MLAudioReleaseInputStreamBuffer(AudioCapture->InputDeviceHandle);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogAudioCapture, Warning, TEXT("MLAudioReleaseInputStreamBuffer failed with error %d"), Result);
		}
	}
	else
	{
		UE_LOG(LogAudioCapture, Warning, TEXT("MLAudioGetInputStreamBuffer failed with error %d"), Result);
	}
}

void FAudioCaptureImpl::OnAudioCapture(void* InBuffer, uint32 InBufferFrames, double StreamTime, bool bOverflow)
{
	check(Callback);

	FScopeLock ScopeLock(&ApplicationResumeCriticalSection);

	int32 NumSamples = (int32)(InBufferFrames * NumChannels);

	//TODO: Check to see if float conversion is needed:
	FloatBuffer.Reset(InBufferFrames);
	FloatBuffer.AddUninitialized(NumSamples);

	int16* InBufferData = (int16*)InBuffer;
	float* FloatBufferPtr = FloatBuffer.GetData();

	for (int32 i = 0; i < NumSamples; ++i)
	{
		FloatBufferPtr[i] = (((float)InBufferData[i]) / 32767.0f);
	};

	Callback->OnAudioCapture(FloatBufferPtr, InBufferFrames, NumChannels, StreamTime, bOverflow);
}

bool FAudioCaptureImpl::GetDefaultCaptureDeviceInfo(FCaptureDeviceInfo& OutInfo)
{
	// set up variables to be populated by ML Audio
	uint32 ChannelCount = NumChannels;
	MLAudioBufferFormat DefaultBufferFormat;
	uint32 BufferSize = 0;
	uint32 MinBufferSize = 0;
	uint32 UnsignedSampleRate = SampleRate;

	MLResult Result = MLAudioGetInputStreamDefaults(ChannelCount, UnsignedSampleRate, &DefaultBufferFormat, &BufferSize, &MinBufferSize);
	if (Result == MLResult_Ok)
	{
		OutInfo.DeviceName = TEXT("MLAudio Microphones");
		OutInfo.InputChannels = ChannelCount;
		OutInfo.PreferredSampleRate = SampleRate;
		UE_LOG(LogAudioCapture, Warning, TEXT("MICROPHONE Settings!!\n Channels: %u\nBuffer Size: %u"), ChannelCount, BufferSize);
	}
	else
	{
		UE_LOG(LogAudioCapture, Error, TEXT("Unable to retrieve settings from MLAudio error: %d! Sample rate %u"), Result, UnsignedSampleRate);
		return false;
	}
	
	return true;
}

bool FAudioCaptureImpl::OpenDefaultCaptureStream(const FAudioCaptureStreamParam& StreamParams)
{
	UE_LOG(LogAudioCapture, Warning, TEXT("OPENING STREAM"));
	if (MLHandleIsValid(InputDeviceHandle))
	{
		UE_LOG(LogAudioCapture, Error, TEXT("Capture Stream Already Opened"));
	}

	if (!StreamParams.Callback)
	{
		UE_LOG(LogAudioCapture, Error, TEXT("Need a callback object passed to open a capture stream"));
		return false;
	}

	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(this, &FAudioCaptureImpl::OnApplicationSuspend);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FAudioCaptureImpl::OnApplicationResume);

	// set up variables to be populated by ML Audio
	uint32 ChannelCount = NumChannels;
	MLAudioBufferFormat DefaultBufferFormat;
	uint32 RecommendedBufferSize = 0;
	uint32 MinBufferSize = 0;
	uint32 UnsignedSampleRate = SampleRate;
	uint32 NumFrames = StreamParams.NumFramesDesired;
	uint32 BufferSize = StreamParams.NumFramesDesired * NumChannels * sizeof(int16);

	MLResult Result = MLAudioGetInputStreamDefaults(ChannelCount, UnsignedSampleRate, &DefaultBufferFormat, &RecommendedBufferSize, &MinBufferSize);
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogAudioCapture, Error, TEXT("MLAudioGetInputStreamDefaults failed with error %d"), Result);
		return false;
	}

	if (BufferSize < RecommendedBufferSize)
	{
		UE_LOG(LogAudioCapture, Warning, TEXT("Requested buffer size of %u is smaller than the recommended buffer size, reverting to a buffer size of %u"), BufferSize, RecommendedBufferSize);
		BufferSize = RecommendedBufferSize;
	}
	UE_LOG(LogAudioCapture, Display, TEXT("Using buffer size of %u"), BufferSize);

	DefaultBufferFormat.bits_per_sample = 16;
	DefaultBufferFormat.sample_format = MLAudioSampleFormat_Int;
	DefaultBufferFormat.channel_count = ChannelCount;
	DefaultBufferFormat.samples_per_second = SampleRate;

	Callback = StreamParams.Callback;

	// Open up new audio stream
	Result = MLAudioCreateInputFromVoiceComm(&DefaultBufferFormat, BufferSize, &OnAudioCaptureCallback, this, &InputDeviceHandle);
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogAudioCapture, Warning, TEXT("MLAudioCreateInputFromVoiceComm failed with code %d"), Result);
		return false;
	}
	else if (!MLHandleIsValid(InputDeviceHandle))
	{
		UE_LOG(LogAudioCapture, Warning, TEXT("MLAudioCreateInputFromVoiceComm failed to generate an input device handle."));
		return false;
	}
	return true;
}

bool FAudioCaptureImpl::CloseStream()
{
	UE_LOG(LogAudioCapture, Warning, TEXT("CLOSING STREAM"));
	MLResult Result = MLAudioDestroyInput(InputDeviceHandle);
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogAudioCapture, Warning, TEXT("MLAudioDestroyInput failed with code %d"), Result);
		return false;
	}

	InputDeviceHandle = ML_INVALID_HANDLE;

	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.RemoveAll(this);

	return true;
}

bool FAudioCaptureImpl::StartStream()
{
	UE_LOG(LogAudioCapture, Warning, TEXT("STARTING STREAM"));
	MLResult Result = MLAudioStartInput(InputDeviceHandle);
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogAudioCapture, Warning, TEXT("MLAudioStartInput failed with code %d"), Result);
		return false;
	}

	bStreamStarted = true;
	return true;
}

bool FAudioCaptureImpl::StopStream()
{
	UE_LOG(LogAudioCapture, Warning, TEXT("STOPPING STREAM"));
	MLResult Result = MLAudioStopInput(InputDeviceHandle);
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogAudioCapture, Warning, TEXT("MLAudioStopInput failed with code %d"), Result);
		return false;
	}

	bStreamStarted = false;
	return true;
}

bool FAudioCaptureImpl::AbortStream()
{
	UE_LOG(LogAudioCapture, Warning, TEXT("ABORTING STREAM"));
	StopStream();
	CloseStream();
	return true;
}

bool FAudioCaptureImpl::GetStreamTime(double& OutStreamTime)
{
	OutStreamTime = 0.0;
	return true;
}

bool FAudioCaptureImpl::IsStreamOpen() const
{
	return MLHandleIsValid(InputDeviceHandle);
}

bool FAudioCaptureImpl::IsCapturing() const
{
	return bStreamStarted;
}

TUniquePtr<FAudioCaptureImpl> FAudioCapture::CreateImpl()
{
	return TUniquePtr<FAudioCaptureImpl>(new FAudioCaptureImpl());
}

void FAudioCaptureImpl::OnApplicationSuspend()
{
	FScopeLock ScopeLock(&ApplicationResumeCriticalSection);
	MLResult Result = MLAudioStopInput(InputDeviceHandle);
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogAudioCapture, Warning, TEXT("MLAudioStopInput failed with code %d"), Result);
		return;
	}
}

void FAudioCaptureImpl::OnApplicationResume()
{
	MLResult Result = MLAudioStartInput(InputDeviceHandle);
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogAudioCapture, Warning, TEXT("MLAudioStartInput failed with code %d"), Result);
		return;
	}
}

} // namespace Audio

#endif // PLATFORM_LUMIN
