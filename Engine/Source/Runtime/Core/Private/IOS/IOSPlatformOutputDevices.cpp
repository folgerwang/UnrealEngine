// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "IOS/IOSPlatformOutputDevices.h"
#include "Misc/OutputDeviceFile.h"


class FOutputDevice*	FIOSPlatformOutputDevices::GetLog()
{
    static FOutputDeviceFile Singleton(nullptr, true);
    return &Singleton;
}

//class FFeedbackContext*				FIOSPlatformOutputDevices::GetWarn()
//{
//	static FOutputDeviceIOSDebug Singleton;
//	return &Singleton;
//}
