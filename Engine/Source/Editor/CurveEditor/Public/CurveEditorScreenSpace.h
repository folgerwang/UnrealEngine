// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/FrameRate.h"

template<typename> class TRange;
struct FGeometry;

/**
 * Utility struct used for converting to/from curve editor screen space
 */
struct FCurveEditorScreenSpace
{
	/**
	 * Construction from a physical size, and input/output range
	 */
	FCurveEditorScreenSpace(FVector2D InPixelSize, float InInputMin, float InInputMax, float InOutputMin, float InOutputMax)
		: PixelSize(InPixelSize)
		, InputMin(InInputMin), InputMax(InInputMax)
		, OutputMin(InOutputMin), OutputMax(InOutputMax)
	{
	}

public:

	/** Convert a horizontal screen position in slate units to a value in seconds */
	FORCEINLINE double ScreenToSeconds(float ScreenPosition) const
	{
		return InputMin + ScreenPosition / PixelsPerInput();
	}

	/** Convert a value in seconds to a horizontal screen position in slate units */
	FORCEINLINE float SecondsToScreen(double InSeconds) const
	{
		return (InSeconds - InputMin) * PixelsPerInput();
	}

	/** Convert a vertical screen position in slate units to a value */
	FORCEINLINE double ScreenToValue(float ScreenPosition) const
	{
		return OutputMin + (PixelSize.Y - ScreenPosition) / PixelsPerOutput();
	}

	/** Convert a value to a vertical screen position in slate units */
	FORCEINLINE float ValueToScreen(double InValue) const
	{
		return PixelSize.Y - (InValue - OutputMin) * PixelsPerOutput();
	}

public:

	/** Retrieve the number of slate units per input value */
	FORCEINLINE float PixelsPerInput() const
	{
		float InputDiff = FMath::Max(InputMax - InputMin, 1e-10);
		return PixelSize.X / InputDiff;
	}

	/** Retrieve the number of slate units per output value */
	FORCEINLINE float PixelsPerOutput() const
	{
		float OutputDiff = FMath::Max(OutputMax - OutputMin, 1e-10);
		return PixelSize.Y / OutputDiff;
	}

public:

	/** Get the minimum input value displayed on the screen */
	FORCEINLINE float GetInputMin() const { return InputMin; }
	/** Get the maximum input value displayed on the screen */
	FORCEINLINE float GetInputMax() const { return InputMax; }
	/** Get the minimum output value displayed on the screen */
	FORCEINLINE float GetOutputMin() const { return OutputMin; }
	/** Get the maximum output value displayed on the screen */
	FORCEINLINE float GetOutputMax() const { return OutputMax; }
	/** Get the physical size of the screen */
	FORCEINLINE FVector2D GetPhysicalSize() const { return PixelSize; }
	/** Get the physical width of the screen */
	FORCEINLINE float GetPhysicalWidth() const { return PixelSize.X; }
	/** Get the physical height of the screen */
	FORCEINLINE float GetPhysicalHeight() const { return PixelSize.Y; }

private:

	FVector2D PixelSize;
	double InputMin, InputMax;
	double OutputMin, OutputMax;
};