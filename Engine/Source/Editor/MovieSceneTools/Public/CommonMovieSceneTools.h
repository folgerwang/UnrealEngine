// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Layout/Geometry.h"
#include "FrameRate.h"

/**
 * Utility for converting time units to slate pixel units and vice versa
 */
struct FTimeToPixel
{
public:
	FTimeToPixel( const FGeometry& AllottedGeometry, const TRange<double>& InLocalViewRange, const FFrameRate& InFrameResolution )
		: ViewRangeStartSeconds( InLocalViewRange.GetLowerBoundValue() )
		, FrameResolution( InFrameResolution )
	{
		double VisibleWidth = InLocalViewRange.Size<double>();

		const float MaxPixelsPerSecond = 1000.f;
		PixelsPerSecond = VisibleWidth > 0 ? AllottedGeometry.GetLocalSize().X / VisibleWidth : MaxPixelsPerSecond;
	}

	/**
	 * Converts a time to a pixel point relative to the geometry of a widget (passed into the constructor)
	 *
	 * @param Time	The time to convert
	 * @return The pixel equivalent of the time
	 */
	float SecondsToPixel( double Time ) const
	{
		return (Time - ViewRangeStartSeconds) * PixelsPerSecond;
	}

	/**
	 * Converts a pixel value to time 
	 *
	 * @param PixelX The x value of a pixel coordinate relative to the geometry that was passed into the constructor.
	 * @return The time where the pixel is located
	 */
	double PixelToSeconds( float PixelX ) const
	{
		return (PixelX/PixelsPerSecond) + ViewRangeStartSeconds;
	}

	/**
	 * Converts a frame time to a pixel point relative to the geometry of a widget (passed into the constructor)
	 *
	 * @param Time The time to convert
	 * @return The pixel equivalent of the frame time
	 */
	float FrameToPixel( const FFrameTime& Time ) const
	{
		return (Time / FrameResolution - ViewRangeStartSeconds) * PixelsPerSecond;
	}

	/**
	 * Converts a pixel value to frame time 
	 *
	 * @param PixelX The x value of a pixel coordinate relative to the geometry that was passed into the constructor.
	 * @return The frame time where the pixel is located
	 */
	FFrameTime PixelToFrame( float PixelX ) const
	{
		return ( PixelX / PixelsPerSecond + ViewRangeStartSeconds ) * FrameResolution;
	}

	/**
	 * Converts a pixel delta value to delta frame time 
	 *
	 * @param PixelDelta The delta value in pixel space
	 * @return The equivalent delta frame time
	 */
	FFrameTime PixelDeltaToFrame( float PixelDelta ) const
	{
		return ( PixelDelta / PixelsPerSecond ) * FrameResolution;
	}

	/**
	 * Retrieve the frame resolution of the current sequence
	 */
	FFrameRate GetFrameResolution() const
	{
		return FrameResolution;
	}

private:

	/** time range of the sequencer in seconds */
	double ViewRangeStartSeconds;
	/** The frame resolution of the current timeline */
	FFrameRate FrameResolution;
	/** The number of pixels in the view range */
	float PixelsPerSecond;
};
