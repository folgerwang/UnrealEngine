// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MagicLeapEyeTrackerTypes.h"


class IMagicLeapVREyeTracker
{
public:
	/**
	* Gets all gaze rays. Both for the individual eyes, as well as an averaged version.
	*
	* @return gaze ray data in world space.
	*/
	virtual const FMagicLeapVREyeTrackingData& GetVREyeTrackingData() = 0;

	/**
	* Gets the status of the eye tracking device.
	*
	* @return the device status.
	*/
	virtual EMagicLeapEyeTrackingStatus GetEyeTrackingStatus() = 0;
};
