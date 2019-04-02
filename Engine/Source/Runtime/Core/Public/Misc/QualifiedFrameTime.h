// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/FrameRate.h"
#include "Misc/FrameTime.h"
#include "Misc/Timecode.h"

/**
 * A frame time qualified by a frame rate context
 */
struct FQualifiedFrameTime
{

	/**
	 * Default construction for UObject purposes
	 */
	FQualifiedFrameTime()
		: Time(0), Rate(24, 1)
	{}

	/**
	 * User construction from a frame time and its frame rate
	 */
	FQualifiedFrameTime(const FFrameTime& InTime, const FFrameRate& InRate)
		: Time(InTime), Rate(InRate)
	{}

	/**
	 * User construction from a timecode and its frame rate
	 */
	FQualifiedFrameTime(const FTimecode& InTimecode, const FFrameRate& InRate)
		: Time(InTimecode.ToFrameNumber(InRate))
		, Rate(InRate)
	{
	}

public:

	/**
	 * Convert this frame time to a value in seconds
	 */
	double AsSeconds() const
	{
		return Time / Rate;
	}

	/**
	 * Convert this frame time to a different frame rate
	 */
	FFrameTime ConvertTo(FFrameRate DesiredRate) const
	{
		return  FFrameRate::TransformTime(Time, Rate, DesiredRate);
	}

public:

	/** The frame time */
	FFrameTime Time;

	/** The rate that this frame time is in */
	FFrameRate Rate;
};
