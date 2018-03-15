// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FrameTime.h"
#include "FrameRate.h"

#include "QualifiedFrameTime.generated.h"

/**
 * A frame time qualified by a frame rate context
 */
USTRUCT(BlueprintType)
struct FQualifiedFrameTime
{
	GENERATED_BODY()

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
	UPROPERTY(BlueprintReadWrite, Category="Time")
	FFrameTime Time;

	/** The rate that this frame time is in */
	UPROPERTY(BlueprintReadWrite, Category="Time")
	FFrameRate Rate;
};
