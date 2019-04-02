// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	HTML5Time.h: HTML5 platform Time functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformTime.h"
#include "HTML5/HTML5SystemIncludes.h"

#include <time.h>
#include <emscripten/emscripten.h>

struct CORE_API FHTML5PlatformTime : public FGenericPlatformTime
{
	static double emscripten_t0;

	static double InitTiming()
	{
		emscripten_t0 = emscripten_get_now();
		SecondsPerCycle = 1.0f / 1000000.0f;
		SecondsPerCycle64 = 1.0 / 1000000.0;
		return FHTML5PlatformTime::Seconds();
	}

	// for HTML5 - this returns the time since startup.
	static FORCEINLINE double Seconds()
	{
		return (emscripten_get_now() - emscripten_t0) * 0.001;
	}

	static FORCEINLINE uint32 Cycles()
	{
		return (uint32)((emscripten_get_now() - emscripten_t0) * 1000.0);
	}
	static FORCEINLINE uint64 Cycles64()
	{
		return (uint64)((emscripten_get_now() - emscripten_t0) * 1000.0);
	}

};

typedef FHTML5PlatformTime FPlatformTime;
