// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
//#if PLATFORM_WINDOWS
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

	enum class EResamplingMethod : uint8
	{
		BestSinc = 0,
		ModerateSinc = 1,
		FastSinc = 2,
		ZeroOrderHold = 3,
		Linear = 4
	};

	struct FResamplingParameters
	{
		EResamplingMethod ResamplerMethod;
		int NumChannels;
		float SourceSampleRate;
		float DestinationSampleRate;
		AlignedFloatBuffer& InputBuffer;
	};

	struct FResamplerResults
	{
		AlignedFloatBuffer* OutBuffer;

		float ResultingSampleRate;

		int InputFramesUsed;

		int OutputFramesGenerated;

		FResamplerResults()
			: OutBuffer(nullptr)
			, ResultingSampleRate(0.0f)
			, InputFramesUsed(0)
			, OutputFramesGenerated(0)
		{}
	};

	// Get how large the output buffer should be for a resampling operation.
	AUDIOPLATFORMCONFIGURATION_API int32 GetOutputBufferSize(const FResamplingParameters& InParameters);

	// Simple, inline resampler. Returns true on success, false otherwise.
	AUDIOPLATFORMCONFIGURATION_API bool Resample(const FResamplingParameters& InParameters, FResamplerResults& OutData);
	
}

//#endif //PLATFORM_WINDOWS