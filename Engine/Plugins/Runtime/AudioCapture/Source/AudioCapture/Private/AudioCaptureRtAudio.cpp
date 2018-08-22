// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AudioCapture.h"

#if PLATFORM_WINDOWS

#include "AudioCaptureInternal.h"

namespace Audio 
{

FAudioCaptureImpl::FAudioCaptureImpl()
	: Callback(nullptr)
	, NumChannels(0)
	, SampleRate(0)
{
}

static int32 OnAudioCaptureCallback(void *OutBuffer, void* InBuffer, uint32 InBufferFrames, double StreamTime, RtAudioStreamStatus AudioStreamStatus, void* InUserData)
{
	FAudioCaptureImpl* AudioCapture = (FAudioCaptureImpl*)InUserData;

	AudioCapture->OnAudioCapture(InBuffer, InBufferFrames, StreamTime, AudioStreamStatus == RTAUDIO_INPUT_OVERFLOW);
	return 0; 
}

void FAudioCaptureImpl::OnAudioCapture(void* InBuffer, uint32 InBufferFrames, double StreamTime, bool bOverflow)
{
	check(Callback);

	float* InBufferData = (float*)InBuffer;
	Callback->OnAudioCapture(InBufferData, InBufferFrames, NumChannels, StreamTime, bOverflow);
}

bool FAudioCaptureImpl::GetDefaultCaptureDeviceInfo(FCaptureDeviceInfo& OutInfo)
{
	uint32 DefaultInputDeviceId = CaptureDevice.getDefaultInputDevice();

	RtAudio::DeviceInfo DeviceInfo = CaptureDevice.getDeviceInfo(DefaultInputDeviceId);

	const char* NameStr = DeviceInfo.name.c_str();

	OutInfo.DeviceName = FString(ANSI_TO_TCHAR(NameStr));
	OutInfo.InputChannels = DeviceInfo.inputChannels;
	OutInfo.PreferredSampleRate = DeviceInfo.preferredSampleRate;

	return true;
}

bool FAudioCaptureImpl::OpenDefaultCaptureStream(const FAudioCaptureStreamParam& StreamParams)
{
	if (!StreamParams.Callback)
	{
		UE_LOG(LogAudioCapture, Error, TEXT("Need a callback object passed to open a capture stream"));
		return false;
	}

	uint32 DefaultInputDeviceId = CaptureDevice.getDefaultInputDevice();
	RtAudio::DeviceInfo DeviceInfo = CaptureDevice.getDeviceInfo(DefaultInputDeviceId);

	RtAudio::StreamParameters RtAudioStreamParams;
	RtAudioStreamParams.deviceId = DefaultInputDeviceId;
	RtAudioStreamParams.firstChannel = 0;
	RtAudioStreamParams.nChannels = FMath::Min((int32)DeviceInfo.inputChannels, 2);

	if (CaptureDevice.isStreamOpen())
	{
		CaptureDevice.stopStream();
		CaptureDevice.closeStream();
	}

	uint32 NumFrames = StreamParams.NumFramesDesired;
	NumChannels = RtAudioStreamParams.nChannels;
	SampleRate = DeviceInfo.preferredSampleRate;
	Callback = StreamParams.Callback;

	// Open up new audio stream
	CaptureDevice.openStream(nullptr, &RtAudioStreamParams, RTAUDIO_FLOAT32, SampleRate, &NumFrames, &OnAudioCaptureCallback, this);

	if (CaptureDevice.isStreamOpen())
	{
		SampleRate = CaptureDevice.getStreamSampleRate();
	}

	return true;
}

bool FAudioCaptureImpl::CloseStream()
{
	if (CaptureDevice.isStreamOpen())
	{
		CaptureDevice.closeStream();
	}
	return true;
}

bool FAudioCaptureImpl::StartStream()
{
	CaptureDevice.startStream();
	return true;
}

bool FAudioCaptureImpl::StopStream()
{
	if (CaptureDevice.isStreamOpen())
	{
		CaptureDevice.stopStream();
	}
	return true;
}

bool FAudioCaptureImpl::AbortStream()
{
	if (CaptureDevice.isStreamOpen())
	{
		CaptureDevice.abortStream();
	}
	return true;
}

bool FAudioCaptureImpl::GetStreamTime(double& OutStreamTime)
{
	OutStreamTime = CaptureDevice.getStreamTime();
	return true;
}

bool FAudioCaptureImpl::IsStreamOpen() const
{
	return CaptureDevice.isStreamOpen();
}

bool FAudioCaptureImpl::IsCapturing() const
{
	return CaptureDevice.isStreamRunning();
}

TUniquePtr<FAudioCaptureImpl> FAudioCapture::CreateImpl()
{
	return TUniquePtr<FAudioCaptureImpl>(new FAudioCaptureImpl());
}

} // namespace Audio

#endif // PLATFORM_WINDOWS
