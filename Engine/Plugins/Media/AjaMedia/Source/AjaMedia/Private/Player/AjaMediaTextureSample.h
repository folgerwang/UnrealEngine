// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaIOCoreTextureSampleBase.h"

/**
 * Implements a media texture sample for AjaMedia.
 */
class FAjaMediaTextureSample
	: public FMediaIOCoreTextureSampleBase
{
public:
	/** Virtual destructor. */
	virtual ~FAjaMediaTextureSample()
	{
		FreeSample();
	}

public:

	/**
	 * Initialize the sample.
	 *
	 * @param InVideoData The video frame data.
	 * @param InSampleFormat The sample format.
	 * @param InTime The sample time (in the player's own clock).
	 */
	bool InitializeProgressive(const AJA::AJAVideoFrameData& InVideoData, EMediaTextureSampleFormat InSampleFormat, FTimespan InTime)
	{
		FreeSample();

		if ((InVideoData.VideoBuffer == nullptr) || (InSampleFormat == EMediaTextureSampleFormat::Undefined))
		{
			return false;
		}

		Buffer.Reset(InVideoData.VideoBufferSize);
		Buffer.Append(InVideoData.VideoBuffer, InVideoData.VideoBufferSize);
		Stride = InVideoData.Stride;
		Width = InVideoData.Width;
		Height = InVideoData.Height;
		SampleFormat = InSampleFormat;
		Time = InTime;
		PixelBuffer = Buffer.GetData();

		return true;
	}

	/**
	 * Initialize the sample.
	 *
	 * @param InVideoData The video frame data.
	 * @param InSampleFormat The sample format.
	 * @param InTime The sample time (in the player's own clock).
	 * @param bEven Only take the even frame from the image.
	 */
	bool InitializeInterlaced_Halfed(const AJA::AJAVideoFrameData& InVideoData, EMediaTextureSampleFormat InSampleFormat, FTimespan InTime, bool bInEven)
	{
		FreeSample();

		if ((InVideoData.VideoBuffer == nullptr) || (InSampleFormat == EMediaTextureSampleFormat::Undefined))
		{
			return false;
		}

		Buffer.Reset(InVideoData.VideoBufferSize/2);
		Stride = InVideoData.Stride;
		Width = InVideoData.Width;
		Height = InVideoData.Height / 2;
		SampleFormat = InSampleFormat;
		Time = InTime;

		for (uint32 IndexY = (bInEven ? 0 : 1); IndexY < InVideoData.Height; IndexY += 2)
		{
			uint8* Source = InVideoData.VideoBuffer + (IndexY*Stride);
			Buffer.Append(Source, Stride);
		}

		PixelBuffer = Buffer.GetData();
		return true;
	}

protected:

	/** Free the video frame data. */
	virtual void FreeSample() override
	{
		Buffer.Reset();
	}

private:
	/** Image buffer */
	TArray<uint8> Buffer;
};

/*
 * Implements a pool for AJA texture sample objects.
 */

class FAjaMediaTextureSamplePool : public TMediaObjectPool<FAjaMediaTextureSample> { };
