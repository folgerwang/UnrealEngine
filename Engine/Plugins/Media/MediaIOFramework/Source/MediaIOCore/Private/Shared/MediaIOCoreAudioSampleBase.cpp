// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MediaIOCoreAudioSampleBase.h"


FMediaIOCoreAudioSampleBase::FMediaIOCoreAudioSampleBase()
	: Channels(0)
	, Duration(0)
	, SampleRate(0)
	, Time(FTimespan::MinValue())
{ }


bool FMediaIOCoreAudioSampleBase::Initialize(const int32* InAudioBuffer, uint32 InBufferSize, uint32 InNumberOfChannels, uint32 InSampleRate, FTimespan InTime, const TOptional<FTimecode>& InTimecode)
{
	if (InAudioBuffer == nullptr || InNumberOfChannels * InSampleRate <= 0)
	{
		FreeSample();
		return false;
	}

	Buffer.Reset(InBufferSize);
	Buffer.Append(InAudioBuffer, InBufferSize);
	Time = InTime;
	Timecode = InTimecode;
	Channels = InNumberOfChannels;
	SampleRate = InSampleRate;
	Duration = (InBufferSize * ETimespan::TicksPerSecond) / (Channels * SampleRate);

	return true;
}


bool FMediaIOCoreAudioSampleBase::Initialize(TArray<int32> InAudioBuffer, uint32 InNumberOfChannels, uint32 InSampleRate, FTimespan InTime, const TOptional<FTimecode>& InTimecode)
{
	if (InAudioBuffer.Num() == 0 || InNumberOfChannels * InSampleRate <= 0)
	{
		FreeSample();
		return false;
	}

	Buffer = MoveTemp(InAudioBuffer);
	Time = InTime;
	Timecode = InTimecode;
	Channels = InNumberOfChannels;
	SampleRate = InSampleRate;
	Duration = (InAudioBuffer.Num() * ETimespan::TicksPerSecond) / (Channels * SampleRate);

	return true;
}
