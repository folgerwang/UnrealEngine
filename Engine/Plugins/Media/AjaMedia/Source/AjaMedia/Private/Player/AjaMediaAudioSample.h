// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaIOCoreAudioSampleBase.h"
#include "AjaMediaPrivate.h"

/*
 * Implements a media audio sample for AjaMedia.
 */
class FAjaMediaAudioSample
	: public FMediaIOCoreAudioSampleBase
{
public:

	bool Initialize(const AJA::AJAAudioFrameData& InAudioData, FTimespan InTime)
	{
		if (InAudioData.AudioBuffer)
		{
			Buffer.Reset(InAudioData.AudioBufferSize);
			Buffer.Append(reinterpret_cast<const int32*>(InAudioData.AudioBuffer), InAudioData.AudioBufferSize/sizeof(int32));
		}
		else
		{
			Buffer.Reset();
			return false;
		}

		Channels = InAudioData.NumChannels;
		SampleRate = InAudioData.AudioRate;
		Time = InTime;
		Duration = (InAudioData.AudioBufferSize * ETimespan::TicksPerSecond) / (Channels * SampleRate * sizeof(int32));

		return true;
	}
};

/*
 * Implements a pool for AJA audio sample objects. 
 */

class FAjaMediaAudioSamplePool : public TMediaObjectPool<FAjaMediaAudioSample> { };
