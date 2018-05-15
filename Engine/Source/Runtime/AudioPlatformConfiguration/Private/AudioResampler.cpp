// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AudioResampler.h"

// Convenience macro for the case in which LibSampleRate needs to be built for limited platforms.
#define WITH_LIBSAMPLERATE (WITH_EDITOR && !PLATFORM_LINUX)

#if WITH_LIBSAMPLERATE
#include "samplerate.h"
#endif // WITH_LIBSAMPLERATE

namespace Audio
{
	// Helper function to ensure that buffers are appropriately set up.
	bool CheckBufferValidity(const FResamplingParameters& InParameters, FResamplerResults& OutData)
	{
		if (OutData.OutBuffer == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("Please specify an output buffer when using Resample()."));
			return false;
		}

		if (InParameters.SourceSampleRate <= 0.0f || InParameters.DestinationSampleRate <= 0.0f)
		{
			UE_LOG(LogTemp, Error, TEXT("Please use non-zero, positive sample rates when calling Resample()."));
			return false;
		}

		if (OutData.OutBuffer->Num() < GetOutputBufferSize(InParameters))
		{
			UE_LOG(LogTemp, Error, TEXT("Insufficient space in output buffer: Please allocate space for %d samples."), GetOutputBufferSize(InParameters));
			return false;
		}

		return true;
	}

	int32 GetOutputBufferSize(const FResamplingParameters& InParameters)
	{
		const float Ratio = InParameters.DestinationSampleRate / InParameters.SourceSampleRate;
		return InParameters.InputBuffer.Num() * Ratio;
	}

	bool Resample(const FResamplingParameters& InParameters, FResamplerResults& OutData)
	{
#if WITH_LIBSAMPLERATE
		// Check validity of buffers.
		if (!CheckBufferValidity(InParameters, OutData))
		{
			return false;
		}

		// Create new converter
		int32 Error = 0;
		SRC_STATE* Converter = src_new(SRC_SINC_BEST_QUALITY, InParameters.NumChannels, &Error);
		if (Converter == nullptr || Error != 0)
		{
			UE_LOG(LogTemp, Error, TEXT("Error creating sample converter: %s"), src_strerror(Error));
			return false;
		}

		SRC_DATA SrcData;
		SrcData.data_in = InParameters.InputBuffer.GetData();
		SrcData.data_out = OutData.OutBuffer->GetData();
		SrcData.input_frames = InParameters.InputBuffer.Num() / InParameters.NumChannels;
		SrcData.output_frames = OutData.OutBuffer->Num() / InParameters.NumChannels;
		SrcData.src_ratio = InParameters.DestinationSampleRate / InParameters.SourceSampleRate;

		Error = src_process(Converter, &SrcData);

		if (Error != 0)
		{
			UE_LOG(LogTemp, Error, TEXT("Error on Resampling process: %s"), src_strerror(Error));
			return false;
		}

		OutData.InputFramesUsed = SrcData.input_frames_used;
		OutData.OutputFramesGenerated = SrcData.output_frames_gen;

		// Clean up:
		src_delete(Converter);
#endif //WITH_LIBSAMPLERATE
		return true;
	}
}
