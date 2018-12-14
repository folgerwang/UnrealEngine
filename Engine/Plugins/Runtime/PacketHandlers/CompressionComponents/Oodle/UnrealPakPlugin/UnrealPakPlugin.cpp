// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

// These are normally defined by UBT, so we have to define them manually

#define UE_BUILD_DEVELOPMENT 1
#define WITH_EDITOR 0
#define WITH_ENGINE 0
#define WITH_UNREAL_DEVELOPER_TOOLS 0
#define WITH_PLUGIN_SUPPORT 0
#define UE_BUILD_MINIMAL 1
#define IS_MONOLITHIC 1
#define IS_PROGRAM 1

#if defined __APPLE__ && __APPLE__
	#include "TargetConditionals.h"
	#if TARGET_OS_MAC
		#define PLATFORM_MAC 1
	#else
		#error Unknown platform
	#endif
#elif _WINDOWS
	#define PLATFORM_WINDOWS 1
#else
	#error Unknown platform
#endif

#define CORE_API
#define HAS_OODLE_SDK 1

#include <stdlib.h>
#include "../Source/OodleHandlerComponent/Private/OodleCustomCompressor.cpp"

extern "C"
{
	DLLEXPORT ICustomCompressor* CreateCustomCompressor(const TCHAR* Arguments)
	{
		return CreateOodleCustomCompressor();
	}
}
