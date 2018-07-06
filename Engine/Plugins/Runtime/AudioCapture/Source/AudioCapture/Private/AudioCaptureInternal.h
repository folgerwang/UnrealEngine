// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioCapture.h"


#if PLATFORM_WINDOWS || PLATFORM_LUMIN

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"

THIRD_PARTY_INCLUDES_START
#include "RtAudio.h"
THIRD_PARTY_INCLUDES_END
#elif PLATFORM_LUMIN // #if PLATFORM_WINDOWS
#include "ml_audio.h"
#endif

namespace Audio
{
	class FAudioCaptureImpl
	{
	public:
		FAudioCaptureImpl();

		bool GetDefaultCaptureDeviceInfo(FCaptureDeviceInfo& OutInfo);
		bool OpenDefaultCaptureStream(const FAudioCaptureStreamParam& StreamParams);
		bool CloseStream();
		bool StartStream();
		bool StopStream();
		bool AbortStream();
		bool GetStreamTime(double& OutStreamTime);
		int32 GetSampleRate() const { return SampleRate; }
		bool IsStreamOpen() const;
		bool IsCapturing() const;
		void OnAudioCapture(void* InBuffer, uint32 InBufferFrames, double StreamTime, bool bOverflow);

#if PLATFORM_LUMIN
		TArray<float> FloatBuffer;
		MLHandle InputDeviceHandle;
		bool bStreamStarted;

		FCriticalSection ApplicationResumeCriticalSection;
		void OnApplicationSuspend();
		void OnApplicationResume();
#endif

	private:
		IAudioCaptureCallback* Callback;
		int32 NumChannels;
		int32 SampleRate;
#if PLATFORM_WINDOWS
		RtAudio CaptureDevice;
#endif
	};
}

#else // #if PLATFORM_WINDOWS || PLATFORM_LUMIN

namespace Audio
{
	// Null implementation for compiler
	class FAudioCaptureImpl
	{
	public:
		FAudioCaptureImpl() {}

		bool GetDefaultCaptureDeviceInfo(FCaptureDeviceInfo& OutInfo) { return false; }
		bool OpenDefaultCaptureStream(const FAudioCaptureStreamParam& StreamParams) { return false; }
		bool CloseStream() { return false; }
		bool StartStream() { return false; }
		bool StopStream() { return false; }
		bool AbortStream() { return false; }
		bool GetStreamTime(double& OutStreamTime)  { return false; }
		int32 GetSampleRate() const { return 0; }
		bool IsStreamOpen() const { return false; }
		bool IsCapturing() const { return false; }
	};

	// Return nullptr when creating impl
	TUniquePtr<FAudioCaptureImpl> FAudioCapture::CreateImpl()
	{
		return nullptr;
	}

} // namespace audio
#endif

