// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	HTML5Misc.h: HTML5 platform misc functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformMisc.h"
#include "CoreTypes.h"
#include "HTML5/HTML5DebugLogging.h"
#include "HTML5/HTML5SystemIncludes.h"
#include <emscripten/emscripten.h>

#define UE_DEBUG_BREAK_IMPL() PLATFORM_BREAK()

/**
 * HTML5 implementation of the misc OS functions
 */
struct CORE_API FHTML5Misc : public FGenericPlatformMisc
{
	static void PlatformInit();
	static const TCHAR* GetPlatformFeaturesModuleName();
	static FString GetDefaultLocale();
	static void SetCrashHandler(void (* CrashHandler)(const FGenericCrashContext& Context));
	static EAppReturnType::Type MessageBoxExt( EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption );

	FORCEINLINE static int32 GetMaxPathLength()
	{
		return HTML5_MAX_PATH;
	}

	static bool GetUseVirtualJoysticks()
	{
		return false;
	}

	FORCEINLINE static int32 NumberOfCores()
	{
		return 1;
	}

	static bool AllowThreadHeartBeat()
	{
		return false;
	}

	FORCEINLINE static void MemoryBarrier()
	{
		// Do nothing on x86; the spec requires load/store ordering even in the absence of a memory barrier.

		// @todo HTML5: Will this be applicable for final?
	}
	/** Return true if a debugger is present */
	FORCEINLINE static bool IsDebuggerPresent()
	{
		return true;
	}

	static void LocalPrint(const TCHAR* Str);
};

typedef FHTML5Misc FPlatformMisc;
