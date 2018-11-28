// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Dsp.h"
#include "DSP/AudioFFT.h"
#include "Sound/SampleBuffer.h"

namespace Audio
{
	
	namespace SpectrumAnalyzerSettings
	{
		// Actual FFT size used. For FSpectrumAnalyzer, we never zero pad the input buffer.
		enum class EFFTSize : uint16
		{
			Default = 512,
			TestingMin_8 = 8,
			Min_64 = 64,
			Small_256 = 256,
			Medium_512 = 512,
			Large_1024 = 1024,
			TestLarge_4096 = 4096
		};

		// Peak interpolation method. If the EFFTSize is small but will be densely sampled,
		// it's worth using a linear or quadratic interpolation method.
		enum class EPeakInterpolationMethod : uint16
		{
			NearestNeighbor,
			Linear,
			Quadratic
		};

		struct FSettings
		{
			EWindowType WindowType;
			EFFTSize FFTSize;
			EPeakInterpolationMethod InterpolationMethod;

			/**
			 * Hop size as a percentage of FFTSize.
			 * 1.0 indicates a full hop.
			 * Keeping this as 0.0 will use whatever hop size
			 * can be used for WindowType to maintain COLA.
			 */
			float HopSize;

			FSettings()
				: WindowType(EWindowType::Hann)
				, FFTSize(EFFTSize::Default)
				, InterpolationMethod(EPeakInterpolationMethod::Linear)
				, HopSize(0.0f)
			{}
		};
	}

	/**
	 * This struct contains the output results from a singular FFT operation.
	 * Stored in FSpectrumAnalyzerBuffer.
	 */
	struct AUDIOMIXER_API FSpectrumAnalyzerFrequencyVector
	{
		FSpectrumAnalyzerFrequencyVector(int32 InFFTSize);

		AlignedFloatBuffer RealFrequencies;
		AlignedFloatBuffer ImagFrequencies;

	private:
		FSpectrumAnalyzerFrequencyVector();
	};

	/**
	 * This class locks an input buffer (for writing) and an output buffer (for reading).
	 * Uses triple buffering semantics.
	 */
	class FSpectrumAnalyzerBuffer
	{
	public:
		FSpectrumAnalyzerBuffer();
		FSpectrumAnalyzerBuffer(const SpectrumAnalyzerSettings::FSettings& InSettings);

		void Reset(const SpectrumAnalyzerSettings::FSettings& InSettings);

		// Input. Used on analysis thread to lock a buffer to write to.
		FSpectrumAnalyzerFrequencyVector* StartWorkOnBuffer();
		void StopWorkOnBuffer();
		
		// Output. Used to lock the most recent buffer we analyzed.
		const FSpectrumAnalyzerFrequencyVector* LockMostRecentBuffer();
		void UnlockBuffer();

	private:
		TArray<FSpectrumAnalyzerFrequencyVector> FrequencyVectors;

		// Private functions. Either increments or decrements the respective counter,
		// based on which index is currently in use. Mutually locked.
		void IncrementInputIndex();
		void IncrementOutputIndex();

		volatile int32 OutputIndex;
		volatile int32 InputIndex;

		// This mutex is locked when we increment either the input or output index.
		FCriticalSection BufferIndicesCriticalSection;
	};

	/**
	 * Class built to be a rolling spectrum analyzer for arbitrary, monaural audio data.
	 * Class is meant to scale accuracy with CPU and memory budgets.
	 * Typical usage is to either call PushAudio() and then PerformAnalysisIfPossible immediately afterwards,
	 * or have a seperate thread call PerformAnalysisIfPossible().
	 */
	class AUDIOMIXER_API FSpectrumAnalyzer
	{
	public:
		// Default constructor needs to call Init before using
		FSpectrumAnalyzer();
		FSpectrumAnalyzer(float InSampleRate);
		FSpectrumAnalyzer(const SpectrumAnalyzerSettings::FSettings& InSettings, float InSampleRate);

		// Initialize sample rate of analyzer if not known at time of construction
		void Init(float InSampleRate);
		void Init(const SpectrumAnalyzerSettings::FSettings& InSettings, float InSampleRate);

		// Update the settings used by this Spectrum Analyzer. Safe to call on any thread, but should not be called every tick.
		void SetSettings(const SpectrumAnalyzerSettings::FSettings& InSettings);

		// Get the current settings used by this Spectrum Analyzer.
		void GetSettings(SpectrumAnalyzerSettings::FSettings& OutSettings);

		// Samples magnitude (linearly) for a given frequency, in Hz.
		float GetMagnitudeForFrequency(float InFrequency);

		// Samples phase for a given frequency, in Hz.
		float GetPhaseForFrequency(float InFrequency);

		// You can call this function to ensure that you're sampling the same window of frequency data,
		// Then call UnlockOutputBuffer when you're done.
		// Otherwise, GetMagnitudeForFrequency and GetPhaseForFrequency will always use the latest window
		// of frequency data.
		void LockOutputBuffer();
		void UnlockOutputBuffer();
		
		// Push audio to queue. Returns false if the queue is already full.
		bool PushAudio(const TSampleBuffer<float>& InBuffer);
		bool PushAudio(const float* InBuffer, int32 NumSamples);

		// Thread safe call to perform actual FFT. Returns true if it performed the FFT, false otherwise.
		bool PerformAnalysisIfPossible();

	private:

		// Called on analysis thread.
		void ResetSettings();

		// Called in GetMagnitudeForFrequency and GetPhaseForFrequency.
		void PerformInterpolation(const FSpectrumAnalyzerFrequencyVector* InFrequencies, SpectrumAnalyzerSettings::EPeakInterpolationMethod InMethod, const float InFreq, float& OutReal, float& OutImag);

		// Cached current settings. Only actually used in ResetSettings().
		SpectrumAnalyzerSettings::FSettings CurrentSettings;
		volatile bool bSettingsWereUpdated;

		float SampleRate;

		// Cached window that is applied prior to running the FFT.
		FWindow Window;
		int32 FFTSize;
		int32 HopInSamples;

		TArray<float> AnalysisTimeDomainBuffer;
		TCircularAudioBuffer<float> InputQueue;
		FSpectrumAnalyzerBuffer FrequencyBuffer;

		// if non-null, owns pointer to locked frequency vector we're using.
		const FSpectrumAnalyzerFrequencyVector* LockedFrequencyVector;
	};
}
