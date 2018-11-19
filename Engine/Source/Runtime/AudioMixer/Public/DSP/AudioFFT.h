// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/BufferVectorOperations.h"

namespace Audio
{
	enum class EWindowType : uint8
	{
		None, // No window is applied. Technically a boxcar window.
		Hamming, // Mainlobe width of -3 dB and sidelove attenuation of ~-40 dB. Good for COLA.
		Hann, // Mainlobe width of -3 dB and sidelobe attenuation of ~-30dB. Good for COLA.
		Blackman // Mainlobe width of -3 dB and sidelobe attenuation of ~-60db. Tricky for COLA.
	};

	// Utility functions for generating different types of windows. Called in FWindow::Generate.
	AUDIOMIXER_API void GenerateHammingWindow(float* WindowBuffer, int32 NumFrames, int32 NumChannels, bool bIsPeriodic);
	AUDIOMIXER_API void GenerateHannWindow(float* WindowBuffer, int32 NumFrames, int32 NumChannels, bool bIsPeriodic);
	AUDIOMIXER_API void GenerateBlackmanWindow(float* WindowBuffer, int32 NumFrames, int32 NumChannels, bool bIsPeriodic);

	// Returns the hop size in samples necessary to maintain constant overlap add.
	// For more information on COLA, see the following page:
	// https://ccrma.stanford.edu/~jos/sasp/Overlap_Add_OLA_STFT_Processing.html
	AUDIOMIXER_API uint32 GetCOLAHopSizeForWindow(EWindowType InType, uint32 WindowLength);

	/**
	 * Class used to generate, contain and apply a DSP window of a given type.
	 */
	class AUDIOMIXER_API FWindow
	{
	public:
		/**
		 * Constructor. Allocates buffer and generates window inside of it.
		 * @param InType: The type of window that should be generated.
		 * @param InNumFrames: The number of samples that should be generated divided by the number of channels.
		 * @param InNumChannels: The amount of channels that will be used in the signal this is applied to.
		 * @param bIsPeriodic: If false, the window will be symmetrical. If true, the window will be periodic.
		 *                     Generally, set this to false if using this window with an STFT, but use true
		 *                     if this window will be used on an entire, self-contained signal.
		 */
		FWindow(EWindowType InType, int32 InNumFrames, int32 InNumChannels, bool bIsPeriodic);

		// Destructor. Releases memory used for window.
		~FWindow();

		// Apply this window to InBuffer, which is expected to be an interleaved buffer with the same amount of frames
		// and channels this window was constructed with.
		void ApplyToBuffer(float* InBuffer);

	private:
		// Purposefully hidden constructor.
		FWindow();

		// Generate the window. Called on constructor.
		void Generate(int32 NumFrames, int32 NumChannels, bool bIsPeriodic);

		EWindowType WindowType;
		AlignedFloatBuffer WindowBuffer;
		int32 NumSamples;
	};

	struct FFTInputParams
	{
		float* InBuffer; // Pointer to a single channel of floats.
		int32 NumSamples; // Number of samples in InBuffer divided by the number of channels. must be a power of 2.
	};

	struct FFTOutputParams
	{
		// arrays in which real and imaginary values will be populated.
		float* OutReal; // Should point to an already allocated array of floats that is FFTInputParams::NumSamples long.
		float* OutImag; // Should point to an already allocated array of floats that is FFTInputParams::NumSamples long.
	};

	// Performs a one-time FFT on a float buffer. Does not support complex signals.
	// This function assumes that, if you desire a window for your FFT, that window was already
	// applied to FFTInputParams.InBuffer.
	AUDIOMIXER_API void PerformFFT(const FFTInputParams& InputParams, FFTOutputParams& OutputParams);

}
