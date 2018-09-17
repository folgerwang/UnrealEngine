// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaIOCoreTextureSampleBase.h"

/**
 * Implements a media texture sample for AjaMedia.
 */
class FAjaMediaTextureSample
	: public FMediaIOCoreTextureSampleBase
{
	using Super = FMediaIOCoreTextureSampleBase;

public:

	/**
	 * Initialize the sample.
	 *
	 * @param InVideoData The video frame data.
	 * @param InSampleFormat The sample format.
	 * @param InTime The sample time (in the player's own clock).
	 */
	bool InitializeProgressive(const AJA::AJAVideoFrameData& InVideoData, EMediaTextureSampleFormat InSampleFormat, FTimespan InTime, const TOptional<FTimecode>& InTimecode)
	{
		return Super::Initialize(InVideoData.VideoBuffer
			, InVideoData.VideoBufferSize
			, InVideoData.Stride
			, InVideoData.Width
			, InVideoData.Height
			, InSampleFormat
			, InTime
			, InTimecode);
	}

	/**
	 * Initialize the sample.
	 *
	 * @param InVideoData The video frame data.
	 * @param InSampleFormat The sample format.
	 * @param InTime The sample time (in the player's own clock).
	 * @param bEven Only take the even frame from the image.
	 */
	bool InitializeInterlaced_Halfed(const AJA::AJAVideoFrameData& InVideoData, EMediaTextureSampleFormat InSampleFormat, FTimespan InTime, const TOptional<FTimecode>& InTimecode, bool bInEven)
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
		Timecode = InTimecode;
		Time = InTime;

		for (uint32 IndexY = (bInEven ? 0 : 1); IndexY < InVideoData.Height; IndexY += 2)
		{
			uint8* Source = InVideoData.VideoBuffer + (IndexY*Stride);
			Buffer.Append(Source, Stride);
		}

		return true;
	}
};

/*
 * Implements a pool for AJA texture sample objects.
 */
class FAjaMediaTextureSamplePool : public TMediaObjectPool<FAjaMediaTextureSample> { };
