// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/TaskGraphInterfaces.h"
#include "Templates/SharedPointer.h"
#include "WebMMediaAudioSample.h"

class FMediaSamples;
class FWebMMediaAudioSamplePool;
struct FWebMFrame;
struct OpusDecoder;

class FWebMAudioDecoder
{
public:
	FWebMAudioDecoder(TSharedPtr<FMediaSamples, ESPMode::ThreadSafe> InSamples);
	~FWebMAudioDecoder();

public:
	void Initialize(int32 InSampleRate, int32 InChannels);
	void DecodeAudioFramesAsync(const TArray<TSharedPtr<FWebMFrame>>& AudioFrames);

private:
	TSharedPtr<FMediaSamples, ESPMode::ThreadSafe> Samples;
	TUniquePtr<FWebMMediaAudioSamplePool> AudioSamplePool;
	FGraphEventRef AudioDecodingTask;
	TArray<uint8> DecodeBuffer;
	OpusDecoder* Decoder;
	int32 FrameSize;
	int32 SampleRate;
	int32 Channels;

	void DoDecodeAudioFrames(const TArray<TSharedPtr<FWebMFrame>>& AudioFrames);
};
