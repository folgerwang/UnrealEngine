// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AudioCapture.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "AudioCaptureInternal.h"

DEFINE_LOG_CATEGORY(LogAudioCapture);

namespace Audio 
{

FAudioCapture::FAudioCapture()
{
	Impl = CreateImpl();
}

FAudioCapture::~FAudioCapture()
{
}

bool FAudioCapture::GetDefaultCaptureDeviceInfo(FCaptureDeviceInfo& OutInfo)
{
	if (Impl.IsValid())
	{
		return Impl->GetDefaultCaptureDeviceInfo(OutInfo);
	}

	return false;
}

bool FAudioCapture::OpenDefaultCaptureStream(FAudioCaptureStreamParam& StreamParams)
{
	if (Impl.IsValid())
	{
		return Impl->OpenDefaultCaptureStream(StreamParams);
	}

	return false;
}

bool FAudioCapture::CloseStream()
{
	if (Impl.IsValid())
	{
		return Impl->CloseStream();
	}
	return false;
}

bool FAudioCapture::StartStream()
{
	if (Impl.IsValid())
	{
		return Impl->StartStream();
	}
	return false;
}

bool FAudioCapture::StopStream()
{
	if (Impl.IsValid())
	{
		return Impl->StopStream();
	}
	return false;
}

bool FAudioCapture::AbortStream()
{
	if (Impl.IsValid())
	{
		return Impl->AbortStream();
	}
	return false;
}

bool FAudioCapture::GetStreamTime(double& OutStreamTime) const
{
	if (Impl.IsValid())
	{
		return Impl->GetStreamTime(OutStreamTime);
	}
	return false;
}

int32 FAudioCapture::GetSampleRate() const
{
	if (Impl.IsValid())
	{
		return Impl->GetSampleRate();
	}
	return 0;
}

bool FAudioCapture::IsStreamOpen() const
{
	if (Impl.IsValid())
	{
		return Impl->IsStreamOpen();
	}
	return false;
}

bool FAudioCapture::IsCapturing() const
{
	if (Impl.IsValid())
	{
		return Impl->IsCapturing();
	}
	return false;
}

FAudioCaptureSynth::FAudioCaptureSynth()
	: bInitialized(false)
	, bIsCapturing(false)
	, NumSamplesEnqueued(0)
{
}

FAudioCaptureSynth::~FAudioCaptureSynth()
{
}

void FAudioCaptureSynth::OnAudioCapture(float* AudioData, int32 NumFrames, int32 NumChannels, double StreamTime, bool bOverflow)
{
	int32 NumSamples = NumChannels * NumFrames;

	FScopeLock Lock(&CaptureCriticalSection);

	if (bIsCapturing)
	{
		// Append the audio memory to the capture data buffer
		int32 Index = AudioCaptureData.AddUninitialized(NumSamples);
		float* AudioCaptureDataPtr = AudioCaptureData.GetData();
		FMemory::Memcpy(&AudioCaptureDataPtr[Index], AudioData, NumSamples * sizeof(float));
	}
}

bool FAudioCaptureSynth::GetDefaultCaptureDeviceInfo(FCaptureDeviceInfo& OutInfo)
{
	return AudioCapture.GetDefaultCaptureDeviceInfo(OutInfo);
}

bool FAudioCaptureSynth::OpenDefaultStream()
{
	check(!AudioCapture.IsStreamOpen());

	FAudioCaptureStreamParam StreamParam;
	StreamParam.Callback = this;
	StreamParam.NumFramesDesired = 1024;

	// Prepare the audio buffer memory for 2 seconds of stereo audio at 48k SR to reduce chance for allocation in callbacks
	AudioCaptureData.Reserve(2 * 2 * 48000);

	// Start the stream here to avoid hitching the audio render thread. 
	if (AudioCapture.OpenDefaultCaptureStream(StreamParam))
	{
		AudioCapture.StartStream();
		return true;
	}
	return false;
}

bool FAudioCaptureSynth::StartCapturing()
{
	FScopeLock Lock(&CaptureCriticalSection);

	AudioCaptureData.Reset();

	check(AudioCapture.IsStreamOpen());

	bIsCapturing = true;
	return true;
}

void FAudioCaptureSynth::StopCapturing()
{
	check(AudioCapture.IsStreamOpen());
	check(AudioCapture.IsCapturing());
	FScopeLock Lock(&CaptureCriticalSection);
	bIsCapturing = false;
}

void FAudioCaptureSynth::AbortCapturing()
{
	AudioCapture.AbortStream();
	AudioCapture.CloseStream();
}

bool FAudioCaptureSynth::IsStreamOpen() const
{
	return AudioCapture.IsStreamOpen();
}

bool FAudioCaptureSynth::IsCapturing() const
{
	return bIsCapturing;
}

int32 FAudioCaptureSynth::GetNumSamplesEnqueued()
{
	FScopeLock Lock(&CaptureCriticalSection);
	return AudioCaptureData.Num();
}

bool FAudioCaptureSynth::GetAudioData(TArray<float>& OutAudioData)
{
	FScopeLock Lock(&CaptureCriticalSection);

	int32 CaptureDataSamples = AudioCaptureData.Num();
	if (CaptureDataSamples > 0)
	{
		// Append the capture audio to the output buffer
		int32 OutIndex = OutAudioData.AddUninitialized(CaptureDataSamples);
		float* OutDataPtr = OutAudioData.GetData();
		FMemory::Memcpy(&OutDataPtr[OutIndex], AudioCaptureData.GetData(), CaptureDataSamples * sizeof(float));

		// Reset the capture data buffer since we copied the audio out
		AudioCaptureData.Reset();
		return true;
	}
	return false;
}

} // namespace audio

void FAudioCaptureModule::StartupModule()
{
}

void FAudioCaptureModule::ShutdownModule()
{
}


IMPLEMENT_MODULE(FAudioCaptureModule, AudioCapture);