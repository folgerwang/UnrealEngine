// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AndroidTargetDevice.h"

/**
* Implements a Lumin target device.
*/
class FLuminTargetDevice : public FAndroidTargetDevice
{
public:

	/**
	* Creates and initializes a new Lumin target device.
	*
	* @param InTargetPlatform - The target platform.
	* @param InSerialNumber - The ADB serial number of the target device.
	* @param InAndroidVariant - The variant of the Android platform, i.e. ATC, DXT or PVRTC.
	*/
	FLuminTargetDevice(const ITargetPlatform& InTargetPlatform, const FString& InSerialNumber, const FString& InAndroidVariant)
		: FAndroidTargetDevice(InTargetPlatform, InSerialNumber, InAndroidVariant)
	{ }

	// Return true if the devices can be grouped in an aggregate (All_<platform>_devices_on_<host>) proxy
	virtual bool IsPlatformAggregated() const override
	{
		return false;
	}
};
