// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/CriticalSection.h"
#include "Templates/SharedPointer.h"

#import <AVFoundation/AVFoundation.h>

class FAvfMediaTextureSamplePool;
class FMediaSamples;


/**
 * Creates samples from video frames.
 */
class FAvfMediaVideoSampler
{
public:

	/** Default constructor. */
	FAvfMediaVideoSampler(FMediaSamples& InSamples);

	/** Virtual destructor. */
	virtual ~FAvfMediaVideoSampler();

public:

	/**
	 * Set the video output object to be sampled.
	 *
	 * This method must be called on the render thread.
	 *
	 * @param Output The output object.
	 */
	void SetOutput(AVPlayerItemVideoOutput* Output, float InFrameRate, bool bFullRange = false);

	/** Tick the video sampler (on the render thread). */
	void Tick();

private:

	/** Mutex to ensure thread-safe access */
	FCriticalSection CriticalSection;

	/** The track's video output handle */
	AVPlayerItemVideoOutput* Output;

	/** The media sample queue. */
	FMediaSamples& Samples;

	/** Video sample object pool. */
	FAvfMediaTextureSamplePool* VideoSamplePool;

	/** Current Video cached decode information */
	float FrameDuration;
	FMatrix const* ColorTransform;

#if WITH_ENGINE

	/** The Metal texture cache for unbuffered texture uploads. */
	CVMetalTextureCacheRef MetalTextureCache;

#endif
};
