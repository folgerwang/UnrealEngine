// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	LinuxPlatformProperties.h - Basic static properties of a platform 
	These are shared between:
		the runtime platform - via FPlatformProperties
		the target platforms - via ITargetPlatform
==================================================================================*/

#pragma once

#include "CoreTypes.h"
#include "Android/AndroidProperties.h"


/**
 * Implements Lumin platform properties.
 */
struct FLuminPlatformProperties : public FAndroidPlatformProperties
{
	static FORCEINLINE const char* IniPlatformName( )
	{
		return "Lumin";
	}

	static FORCEINLINE const char* PlatformName( )
	{
		return "Lumin";
	}

	static FORCEINLINE bool SupportsAutoSDK()
	{
		return false;
	}
};
