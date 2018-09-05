// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaIOCoreTextureSampleBase.h"
#include "BlackmagicMediaPrivate.h"

/**
 * Implements a media texture sample for Blackmagic.
 */
class FBlackmagicMediaTextureSample : public FMediaIOCoreTextureSampleBase
{

public:
	/** Default constructor. */
	FBlackmagicMediaTextureSample()
		: Frame(nullptr)
	{ 
	}

	/** Default destructor. */
	virtual ~FBlackmagicMediaTextureSample()
	{
		FreeSample();
	}

public:
	/**
	 * Initialize the sample.
	 *
	 * @param InReceiverInstance The receiver instance that generated the sample.
	 * @param InFrame The video frame data.
	 * @param InSampleFormat The sample format.
	 * @param InTime The sample time (in the player's own clock).
	 */
	bool Initialize(BlackmagicDevice::FFrame InFrame, EMediaTextureSampleFormat InSampleFormat, FTimespan InTime)
	{
		FreeSample();

		if ((InFrame == nullptr) || (InSampleFormat == EMediaTextureSampleFormat::Undefined))
		{
			return false;
		}

		Frame = InFrame;
		Stride = BlackmagicDevice::VideoIOFrameDimensions(Frame, Width, Height);

		uint32_t Size;
		PixelBuffer = BlackmagicDevice::VideoIOFrameVideoBuffer(Frame, Size);

		Duration = FTimespan(0);
		SampleFormat = InSampleFormat;
		Time = InTime;
		return true;
	}

protected:
	/** Free the video frame data. */
	virtual void FreeSample() override
	{
		if (Frame)
		{
			PixelBuffer = nullptr;
			BlackmagicDevice::VideoIOReleaseFrame(Frame);
			Frame = nullptr;
		}
	}

protected:
	BlackmagicDevice::FFrame Frame;
};
