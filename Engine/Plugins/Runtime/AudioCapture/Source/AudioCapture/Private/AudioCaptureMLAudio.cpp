// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AudioCaptureInternal.h"
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

static void OnAudioCaptureCallback(void* CallbackContext)
{
	FAudioCaptureImpl* AudioCapture = (FAudioCaptureImpl*)CallbackContext;
	check(MLHandleIsValid(AudioCapture->InputDeviceHandle));
	MLAudioBuffer OutputBuffer;
	MLAudioError Result;
	if (MLAudioGetInputStreamBuffer(AudioCapture->InputDeviceHandle, &OutputBuffer, &Result))
	{
		AudioCapture->OnAudioCapture(OutputBuffer.ptr, OutputBuffer.size / sizeof(int16), 0.0, false);
		MLAudioReleaseInputStreamBuffer(AudioCapture->InputDeviceHandle, &Result);
	}
	else
	{
		UE_LOG(LogAudioCapture, Warning, TEXT("CALLBACK ERROR %u"), Result);
	}
}

void FAudioCaptureImpl::OnAudioCapture(void* InBuffer, uint32 InBufferFrames, double StreamTime, bool bOverflow)
{
	check(Callback);

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
	MLAudioError Result;
	uint32 UnsignedSampleRate = SampleRate;
	MLAudioGetInputStreamDefaults(ChannelCount, UnsignedSampleRate, &DefaultBufferFormat, &BufferSize, &MinBufferSize, &Result);

	if (Result == MLAudioError_Success)
	{
		OutInfo.DeviceName = TEXT("MLAudio Microphones");
		OutInfo.InputChannels = ChannelCount;
		OutInfo.PreferredSampleRate = SampleRate;
		UE_LOG(LogAudioCapture, Warning, TEXT("MICROPHONE Settings!!\n Channels: %u\nBuffer Size: %u"), ChannelCount, BufferSize);
	}
	else
	{
		UE_LOG(LogAudioCapture, Error, TEXT("Unable to retrieve settings from MLAudio error: %u! Sample rate %u"), Result, UnsignedSampleRate);
	}
	

	return true;
}

bool FAudioCaptureImpl::OpenDefaultCaptureStream(const FAudioCaptureStreamParam& StreamParams)
{
	UE_LOG(LogAudioCapture, Warning, TEXT("OPENING STREAM"));
	if (InputDeviceHandle != ML_INVALID_HANDLE)
	{
		UE_LOG(LogAudioCapture, Error, TEXT("Capture Stream Already Opened"));
	}

	if (!StreamParams.Callback)
	{
		UE_LOG(LogAudioCapture, Error, TEXT("Need a callback object passed to open a capture stream"));
		return false;
	}

	// set up variables to be populated by ML Audio
	uint32 ChannelCount = NumChannels;
	MLAudioBufferFormat DefaultBufferFormat;
	uint32 BufferSize = 0;
	uint32 MinBufferSize = 0;
	uint32 UnsignedSampleRate = SampleRate;
	MLAudioError Result;

	MLAudioGetInputStreamDefaults(ChannelCount, UnsignedSampleRate, &DefaultBufferFormat, &BufferSize, &MinBufferSize, &Result);
	
	uint32 NumFrames = StreamParams.NumFramesDesired;
	uint32 RequestedBufferSize = StreamParams.NumFramesDesired * NumChannels * sizeof(int16);

	if (RequestedBufferSize < MinBufferSize)
	{
		UE_LOG(LogAudioCapture, Warning, TEXT("Requested buffer size of %u is smaller than the minimum buffer size, reverting to a buffer size of %u"), RequestedBufferSize, BufferSize);
	}
	else
	{
		BufferSize = StreamParams.NumFramesDesired * NumChannels;
		UE_LOG(LogAudioCapture, Display, TEXT("Using buffer size of %u"), StreamParams.NumFramesDesired * NumChannels);
	}

	DefaultBufferFormat.bits_per_sample = 16;
	DefaultBufferFormat.sample_format = MLAudioSampleFormat_Int;
	DefaultBufferFormat.channel_count = ChannelCount;
	DefaultBufferFormat.samples_per_second = SampleRate;

	Callback = StreamParams.Callback;

	// Open up new audio stream
	MLAudioCreateInputFromVoiceComm(&DefaultBufferFormat, BufferSize, &OnAudioCaptureCallback, this, &InputDeviceHandle, &Result);
	if (Result != MLAudioError_Success)
	{
		UE_LOG(LogAudioCapture, Warning, TEXT("MLAudioCreateInputFromVoiceComm failed with code %u"), Result);
	}
	else if (InputDeviceHandle == ML_INVALID_HANDLE)
	{
		UE_LOG(LogAudioCapture, Warning, TEXT("MLAudioCreateInputFromVoiceComm failed to generate an input device handle"), Result);
	}
	return true;
}

bool FAudioCaptureImpl::CloseStream()
{
	UE_LOG(LogAudioCapture, Warning, TEXT("CLOSING STREAM"));
	MLAudioError Result;
	if (!MLAudioDestroyInput(InputDeviceHandle, &Result))
	{
		UE_LOG(LogAudioCapture, Warning, TEXT("MLAudioDestroyInput failed with code %u"), Result);
		return false;
	}

	InputDeviceHandle = ML_INVALID_HANDLE;
	return true;
}

bool FAudioCaptureImpl::StartStream()
{
	UE_LOG(LogAudioCapture, Warning, TEXT("STARTING STREAM"));
	MLAudioError Result;
	if (!MLAudioStartInput(InputDeviceHandle, &Result))
	{
		UE_LOG(LogAudioCapture, Warning, TEXT("MLAudioStartInput failed with code %u"), Result);
		return false;
	}

	bStreamStarted = true;
	return true;
}

bool FAudioCaptureImpl::StopStream()
{
	UE_LOG(LogAudioCapture, Warning, TEXT("STOPPING STREAM"));
	MLAudioError Result;
	if (!MLAudioStopInput(InputDeviceHandle, &Result))
	{
		UE_LOG(LogAudioCapture, Warning, TEXT("MLAudioStopInput failed with code %u"), Result);
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
	return InputDeviceHandle != ML_INVALID_HANDLE;
}

bool FAudioCaptureImpl::IsCapturing() const
{
	return bStreamStarted;
}

TUniquePtr<FAudioCaptureImpl> FAudioCapture::CreateImpl()
{
	return TUniquePtr<FAudioCaptureImpl>(new FAudioCaptureImpl());
}

} // namespace Audio

#endif // PLATFORM_LUMIN
