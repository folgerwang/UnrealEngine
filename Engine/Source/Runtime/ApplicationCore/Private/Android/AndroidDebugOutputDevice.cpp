// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Android/AndroidDebugOutputDevice.h"
#include "Misc/OutputDeviceHelper.h"
#include "CoreGlobals.h"
#include "Containers/UnrealString.h"

FAndroidDebugOutputDevice::FAndroidDebugOutputDevice()
{
}

void FAndroidDebugOutputDevice::Serialize( const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	FPlatformMisc::LowLevelOutputDebugString(*FOutputDeviceHelper::FormatLogLine(Verbosity, Category, Msg, GPrintLogTimes));
}
