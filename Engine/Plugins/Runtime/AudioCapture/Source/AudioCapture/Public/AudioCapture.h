// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "HAL/ThreadSafeBool.h"
#include "DSP/Delay.h"
#include "DSP/EnvelopeFollower.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAudioCapture, Log, All);

namespace Audio
{

	struct FCaptureDeviceInfo
	{
		FString DeviceName;
		int32 InputChannels;
		int32 PreferredSampleRate;
	};

	class AUDIOCAPTURE_API IAudioCaptureCallback
	{
	public:
		/** 
		* Called when audio capture has received a new capture buffer. 
		*/
		virtual void OnAudioCapture(float* AudioData, int32 NumFrames, int32 NumChannels, double StreamTime, bool bOverflow) = 0;
		virtual ~IAudioCaptureCallback() {}
	};

	struct FAudioCaptureStreamParam
	{
		IAudioCaptureCallback* Callback;
		uint32 NumFramesDesired;
	};

	class FAudioCaptureImpl;

	// Class which handles audio capture internally, implemented with a back-end per platform
	class AUDIOCAPTURE_API FAudioCapture
	{
	public:
		FAudioCapture();
		~FAudioCapture();

		// Returns the audio capture device information at the given Id.
		bool GetDefaultCaptureDeviceInfo(FCaptureDeviceInfo& OutInfo);

		// Opens the audio capture stream with the given parameters
		bool OpenDefaultCaptureStream(FAudioCaptureStreamParam& StreamParams);

		// Closes the audio capture stream
		bool CloseStream();

		// Start the audio capture stream
		bool StartStream();

		// Stop the audio capture stream
		bool StopStream();

		// Abort the audio capture stream (stop and close)
		bool AbortStream();

		// Get the stream time of the audio capture stream
		bool GetStreamTime(double& OutStreamTime) const;

		// Get the sample rate in use by the stream.
		int32 GetSampleRate() const;

		// Returns if the audio capture stream has been opened.
		bool IsStreamOpen() const;

		// Returns true if the audio capture stream is currently capturing audio
		bool IsCapturing() const;

	private:

		TUniquePtr<FAudioCaptureImpl> CreateImpl();
		TUniquePtr<FAudioCaptureImpl> Impl;
	};


	/** Class which contains an FAudioCapture object and performs analysis on the audio stream, only outputing audio if it matches a detection criteteria. */
	class FAudioCaptureSynth : public IAudioCaptureCallback
	{
	public:
		FAudioCaptureSynth();
		virtual ~FAudioCaptureSynth();

		//~ IAudioCaptureCallback Begin
		virtual void OnAudioCapture(float* AudioData, int32 NumFrames, int32 NumChannels, double StreamTime, bool bOverflow) override;
		//~ IAudioCaptureCallback End

		// Gets the default capture device info
		bool GetDefaultCaptureDeviceInfo(FCaptureDeviceInfo& OutInfo);

		// Opens up a stream to the default capture device
		bool OpenDefaultStream();

		// Starts capturing audio
		bool StartCapturing();

		// Stops capturing audio
		void StopCapturing();

		// Immediately stop capturing audio
		void AbortCapturing();

		// Returned if the capture synth is closed
		bool IsStreamOpen() const;

		// Returns true if the capture synth is capturing audio
		bool IsCapturing() const;

		// Retrieves audio data from the capture synth.
		// This returns audio only if there was non-zero audio since this function was last called.
		bool GetAudioData(TArray<float>& OutAudioData);

		// Returns the number of samples enqueued in the capture synth
		int32 GetNumSamplesEnqueued();

	private:

		// Number of samples enqueued
		int32 NumSamplesEnqueued;

		// Information about the default capture device we're going to use
		FCaptureDeviceInfo CaptureInfo;

		// Audio capture object dealing with getting audio callbacks
		FAudioCapture AudioCapture;

		// Critical section to prevent reading and writing from the captured buffer at the same time
		FCriticalSection CaptureCriticalSection;
		
		// Buffer of audio capture data, yet to be copied to the output 
		TArray<float> AudioCaptureData;


		// If the object has been initialized
		bool bInitialized;

		// If we're capturing data
		bool bIsCapturing;
	};

} // namespace Audio

class FAudioCaptureModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};