// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Android/AndroidApplicationMisc.h"

struct APPLICATIONCORE_API FLuminPlatformApplicationMisc : public FAndroidApplicationMisc
{
	// android overrides
// 	static void LoadPreInitModules();
// 	static class FOutputDeviceError* GetErrorOutputDevice();
// 	static class GenericApplication* CreateApplication();
// 	static void RequestMinimize();
// 	static bool ControlScreensaver(EScreenSaverAction Action);
// 	static void ResetGamepadAssignments();
// 	static void ResetGamepadAssignmentToController(int32 ControllerId);
// 	static bool IsControllerAssignedToGamepad(int32 ControllerId);
// 	static void ClipboardCopy(const TCHAR* Str);
// 	static void ClipboardPaste(class FString& Dest);
// 	static EScreenPhysicalAccuracy ComputePhysicalScreenDensity(int32& OutScreenDensity);

	static bool RequiresVirtualKeyboard()
	{
		return false;
	}

	static class GenericApplication* CreateApplication();
};

typedef FLuminPlatformApplicationMisc FPlatformApplicationMisc;

