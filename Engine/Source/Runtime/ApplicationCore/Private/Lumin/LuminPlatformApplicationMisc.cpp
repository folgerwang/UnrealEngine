// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Lumin/LuminPlatformApplicationMisc.h"
#include "Lumin/LuminApplication.h"

// #include "Android/AndroidApplication.h"
// #include "Android/AndroidErrorOutputDevice.h"
// #include "Android/AndroidInputInterface.h"
// #include "HAL/PlatformMisc.h"
// #include "Misc/ConfigCacheIni.h"
// #include "Internationalization/Regex.h"
// #include "Modules/ModuleManager.h"

GenericApplication* FLuminPlatformApplicationMisc::CreateApplication()
{
	return FLuminApplication::CreateLuminApplication();
}
