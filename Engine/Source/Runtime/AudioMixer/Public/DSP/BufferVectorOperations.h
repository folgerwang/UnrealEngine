// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_SWITCH
// Switch uses page alignment for submitted buffers
#define AUDIO_BUFFER_ALIGNMENT 4096
#else
#define AUDIO_BUFFER_ALIGNMENT 16
#endif

namespace Audio
{
	typedef TArray<float, TAlignedHeapAllocator<AUDIO_BUFFER_ALIGNMENT>> AlignedFloatBuffer;
	typedef TArray<uint8, TAlignedHeapAllocator<AUDIO_BUFFER_ALIGNMENT>> AlignedByteBuffer;

	/** Multiplies the input aligned float buffer with the given value. */
	void BufferMultiplyByConstant(const AlignedFloatBuffer& InFloatBuffer, float InValue, AlignedFloatBuffer& OutFloatBuffer);

	/* Takes a float buffer and quickly interpolates it's gain from StartValue to EndValue. */
	/* This operation completely ignores channel counts, so avoid using this function on buffers that are not mono, stereo or quad */
	/* if the buffer needs to fade all channels uniformly. */
	void FadeBufferFast(AlignedFloatBuffer& OutFloatBuffer, const float StartValue, const float EndValue);
	void FadeBufferFast(float* OutFloatBuffer, int32 NumSamples, const float StartValue, const float EndValue);

	// Takes buffer InFloatBuffer, optionally multiplies it by Gain, and adds it to BufferToSumTo.
	void MixInBufferFast(const AlignedFloatBuffer& InFloatBuffer, AlignedFloatBuffer& BufferToSumTo, const float Gain);
	void MixInBufferFast(const float* InFloatBuffer, float* BufferToSumTo, int32 NumSamples, const float Gain);
	void MixInBufferFast(const float* InFloatBuffer, float* BufferToSumTo, int32 NumSamples);

	// Sums two buffers together and places the result in the resulting buffer.
	void SumBuffers(const AlignedFloatBuffer& InFloatBuffer1, const AlignedFloatBuffer& InFloatBuffer2, AlignedFloatBuffer& OutputBuffer);
	void SumBuffers(const float* InFloatBuffer1, const float* InFloatBuffer2, float* OutputBuffer, int32 NumSamples);

	// Takes an audio buffer and returns the magnitude across that buffer.
	float GetMagnitude(const AlignedFloatBuffer& Buffer);
	float GetMagnitude(const float* Buffer, int32 NumSamples);

	// Takes an audio buffer and gets the average absolute amplitude across that buffer.
	float GetAverageAmplitude(const AlignedFloatBuffer& Buffer);
	float GetAverageAmplitude(const float* Buffer, int32 NumSamples);
}
