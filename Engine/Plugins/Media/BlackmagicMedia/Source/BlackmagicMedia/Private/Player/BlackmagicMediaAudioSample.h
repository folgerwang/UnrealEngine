// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaIOCoreAudioSampleBase.h"
#include "BlackmagicMediaPrivate.h"

/*
 * Implements a media audio sample.
 */
class FBlackmagicMediaAudioSample : public FMediaIOCoreAudioSampleBase
{
public:

	/** Default constructor. */
	FBlackmagicMediaAudioSample()
	{ }

public:

	/**
	 * Initialize the sample.
	 *
	 * @param InFrame The audio frame data.
	 * @param InTime The sample time (in the player's own clock).
	 * @result true on success, false otherwise.
	 */
	bool Initialize(const BlackmagicDevice::FFrame InFrame, FTimespan InTime)
	{
		uint32_t TmpSize, TmpNumberOfChannels, TmpAudioRate, TmpNumSamples;
		int32_t* TmpAudioBuffer = BlackmagicDevice::VideoIOFrameAudioBuffer(InFrame, TmpSize, TmpNumberOfChannels, TmpAudioRate, TmpNumSamples);

		if (TmpAudioBuffer)
		{
			Channels = TmpNumberOfChannels;
			SampleRate = TmpAudioRate;
			Time = InTime;
			Duration = (TmpSize * ETimespan::TicksPerSecond) / (Channels * SampleRate * sizeof(int32));

			Buffer.Reset(TmpSize);
			Buffer.Append(TmpAudioBuffer, TmpSize);
			return true;
		}
		else
		{
			Channels = SampleRate = 0;
			Buffer.Reset();
		}
		return false;
	}
};

/*
 * Implements a pool for NDI audio sample objects. 
 */

class FBlackmagicMediaAudioSamplePool : public TMediaObjectPool<FBlackmagicMediaAudioSample>
{
};
