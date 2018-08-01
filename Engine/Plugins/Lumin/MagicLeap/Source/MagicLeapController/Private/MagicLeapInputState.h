// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

//-------------------------------------------------------------------------------------------------
// FMagicLeapControllerState - Input state for the magic leap motion controller
//-------------------------------------------------------------------------------------------------

struct FMagicLeapControllerState
{
	/** True if the device is connected, otherwise false */
	bool bIsConnected;

	/** True if position is being tracked, otherwise false */
	bool bIsPositionTracked;

	/** True if orientation is being tracked, otherwise false */
	bool bIsOrientationTracked;

	/** Whether or not we're playing a haptic effect.  If true, force feedback calls will be early-outed in favor of the haptic effect */
	bool bPlayingHapticEffect;
	/** Haptic frequency (zero to disable) */
	float HapticFrequency;
	/** Haptic amplitude (zero to disable) */
	float HapticAmplitude;

	/** Explicit constructor sets up sensible defaults */
	FMagicLeapControllerState(const EControllerHand Hand)
		: bIsConnected(false),
		bIsPositionTracked(false),
		bIsOrientationTracked(false),
		bPlayingHapticEffect(false),
		HapticFrequency(0.0f),
		HapticAmplitude(0.0f)
	{
	}

	/** Default constructor does nothing.  Don't use it.  This only exists because we cannot initialize an array of objects with no default constructor on non-C++ 11 compliant compilers (VS 2013) */
	FMagicLeapControllerState()
	{
	}
};


