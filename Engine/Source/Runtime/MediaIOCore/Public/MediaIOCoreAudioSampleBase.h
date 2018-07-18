// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaAudioSample.h"
#include "MediaObjectPool.h"

/*
 * Implements a media audio sample.
 */
class FMediaIOCoreAudioSampleBase
	: public IMediaAudioSample
	, public IMediaPoolable
{
public:

	/** Default constructor. */
	FMediaIOCoreAudioSampleBase()
		: Channels(0)
		, Duration(0)
		, SampleRate(0)
		, Time(FTimespan::MinValue())
	{ }

public:

	//~ IMediaAudioSample interface

	virtual const void* GetBuffer() override
	{
		return Buffer.GetData();
	}

	virtual uint32 GetChannels() const override
	{
		return Channels;
	}

	virtual FTimespan GetDuration() const override
	{
		return Duration;
	}

	virtual EMediaAudioSampleFormat GetFormat() const override
	{
		return EMediaAudioSampleFormat::Int32;
	}

	virtual uint32 GetFrames() const override
	{
		return Buffer.Num() / Channels;
	}

	virtual uint32 GetSampleRate() const override
	{
		return SampleRate;
	}

	virtual FTimespan GetTime() const override
	{
		return Time;
	}

public:

	//~ IMediaPoolable interface

	virtual void ShutdownPoolable() override
	{
		FreeSample();
	}

protected:
	virtual void FreeSample()
	{
		Buffer.Reset();
	}

protected:
	/** The sample's frame buffer. */
	TArray<int32> Buffer;

	/** Number of audio channels. */
	uint32 Channels;

	/** The duration for which the sample is valid. */
	FTimespan Duration;

	/** Audio sample rate (in samples per second). */
	uint32 SampleRate;

	/** Sample time. */
	FTimespan Time;
};
