// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
// Copyright 2016 Magic Leap, Inc. All Rights Reserved.

#pragma once
#include "Android/AndroidProcess.h"

/**
* Android implementation of the Process OS functions
**/
struct CORE_API FLuminPlatformProcess : public FAndroidPlatformProcess
{
	static const TCHAR* ComputerName();
	static const TCHAR* UserSettingsDir();
	static const TCHAR* ApplicationSettingsDir();
	static const TCHAR* ExecutableName(bool bRemoveExtension=true);

	static void LaunchURL(const TCHAR* URL, const TCHAR* Parms, FString* Error);

	static void* GetDllHandle(const TCHAR* Filename);
	static void FreeDllHandle(void* DllHandle);
	static void* GetDllExport(void* DllHandle, const TCHAR* ProcName);
	static int32 GetDllApiVersion(const TCHAR* Filename);

	static const TCHAR* GetModulePrefix();
	static const TCHAR* GetModuleExtension();
	static const TCHAR* GetBinariesSubdirectory();
};

typedef FLuminPlatformProcess FPlatformProcess;
