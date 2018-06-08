// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/FrameRate.h"

/**
 * Utility struct that acts as a cache of the current snapping metrics for the curve editor
 */
struct FCurveEditorSnapMetrics
{
	FCurveEditorSnapMetrics()
	{
		bSnapOutputValues = 0;
		bSnapInputValues = 0;
		OutputSnapInterval = 1.0;
	}

	/** Whether we are snapping to the output snap interval */
	uint8 bSnapOutputValues : 1;

	/** Whether we are snapping to the input snap rate */
	uint8 bSnapInputValues : 1;

	/** The output snap interval */
	double OutputSnapInterval;

	/** The input snap rate */
	FFrameRate InputSnapRate;

	/** Snap the specified input time to the input snap rate if necessary */
	FORCEINLINE double SnapInputSeconds(double InputTime)
	{
		return bSnapInputValues ? (InputTime * InputSnapRate).RoundToFrame() / InputSnapRate : InputTime;
	}

	/** Snap the specified output value to the output snap interval if necessary */
	FORCEINLINE double SnapOutput(double OutputValue)
	{
		return bSnapOutputValues ? FMath::RoundToDouble(OutputValue / OutputSnapInterval) * OutputSnapInterval : OutputValue;
	}
};
