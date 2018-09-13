// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "WebMAudioDecoder.h"
#include "WebMMediaPrivate.h"
#include "WebMMediaFrame.h"
#include "MediaSamples.h"

THIRD_PARTY_INCLUDES_START
#include <opus.h>
THIRD_PARTY_INCLUDES_END

FWebMAudioDecoder::FWebMAudioDecoder(TSharedPtr<FMediaSamples, ESPMode::ThreadSafe> InSamples)
	: Samples(InSamples)
	, AudioSamplePool(new FWebMMediaAudioSamplePool)
	, Decoder(nullptr)
{
}

FWebMAudioDecoder::~FWebMAudioDecoder()
{
	if (AudioDecodingTask && !AudioDecodingTask->IsComplete())
	{
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(AudioDecodingTask);
	}

	if (Decoder)
	{
		opus_decoder_destroy(Decoder);
	}
}

void FWebMAudioDecoder::Initialize(int32 InSampleRate, int32 InChannels)
{
	SampleRate = InSampleRate;
	Channels = InChannels;
	// Max supported frame is 120ms
	FrameSize = 120 * SampleRate / 1000;

	int32 ErrorCode = 0;
	Decoder = opus_decoder_create(SampleRate, Channels, &ErrorCode);
	check(Decoder);
	check(ErrorCode == 0);

	DecodeBuffer.SetNumUninitialized(FrameSize * Channels * sizeof(opus_int16));
}

void FWebMAudioDecoder::DecodeAudioFramesAsync(const TArray<TSharedPtr<FWebMFrame>>& AudioFrames)
{
	FGraphEventRef PreviousDecodingTask = AudioDecodingTask;

	AudioDecodingTask = FFunctionGraphTask::CreateAndDispatchWhenReady([this, PreviousDecodingTask, AudioFrames]()
	{
		if (PreviousDecodingTask && !PreviousDecodingTask->IsComplete())
		{
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(PreviousDecodingTask);
		}

		DoDecodeAudioFrames(AudioFrames);
	}, TStatId(), nullptr, ENamedThreads::AnyThread);
}

void FWebMAudioDecoder::DoDecodeAudioFrames(const TArray<TSharedPtr<FWebMFrame>>& AudioFrames)
{
	for (const TSharedPtr<FWebMFrame>& AudioFrame : AudioFrames)
	{
		const TSharedRef<FWebMMediaAudioSample, ESPMode::ThreadSafe> AudioSample = AudioSamplePool->AcquireShared();

		int32 NumOfSamplesDecoded = opus_decode(Decoder, AudioFrame->Data.GetData(), AudioFrame->Data.Num(), (opus_int16*) DecodeBuffer.GetData(), FrameSize, 0);
		if (NumOfSamplesDecoded <= 0)
		{
			UE_LOG(LogWebMMedia, Display, TEXT("Error decoding audio frame"));
			return;
		}

		AudioSample->Initialize(DecodeBuffer.GetData(), NumOfSamplesDecoded * Channels * 2, Channels, SampleRate, AudioFrame->Time, FTimespan::FromHours(1));
		Samples->AddAudio(AudioSample);
	}
}
