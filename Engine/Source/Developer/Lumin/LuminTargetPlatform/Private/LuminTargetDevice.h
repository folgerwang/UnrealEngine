// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AndroidTargetDevice.h"

class FLuminTargetDevice : public FAndroidTargetDevice
{
public:
	FLuminTargetDevice(const ITargetPlatform& InTargetPlatform, const FString& InSerialNumber, const FString& InAndroidVariant)
	: FAndroidTargetDevice(InTargetPlatform, InSerialNumber, InAndroidVariant)
	{}

	virtual bool IsPlatformAggregated() const override
	{
		return false;
	}
};
