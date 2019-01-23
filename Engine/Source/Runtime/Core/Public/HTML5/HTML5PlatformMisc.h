// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


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

#ifdef __EMSCRIPTEN_PTHREADS__
#include <emscripten/threading.h>
#endif

#ifdef __EMSCRIPTEN_PTHREADS__
// Set the following define to 1 to build UE4 with rendering on a separate thread enabled.
// Currently this requires falling back to proxying all WebGL commands to the main browser
// thread, so it likely reduces performance, so disabled by default. When this is *disabled*,
// the OffscreenCanvas API is used instead.

// XXX(kainino0x): Enabling this seems to cause UE to attempt to use multiple contexts
// (CONTEXT_Shared and CONTEXT_Rendering). In WebGL, this is not possible (no resource sharing).
#define HTML5_ENABLE_RENDERER_THREAD PLATFORM_RHITHREAD_DEFAULT_BYPASS
										// 0 for renderer running in main thread (OFFSCREENCANVAS_SUPPORT)
										// 1 for renderer proxied to main thread (OFFSCREEN_FRAMEBUFFER)
#else
#define HTML5_ENABLE_RENDERER_THREAD 0 // non-multi-threading builds
#endif

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
#ifdef __EMSCRIPTEN_PTHREADS__
		return 4;
//		return emscripten_num_logical_cores();
#else
		return 1;
#endif
	}

#ifdef __EMSCRIPTEN_PTHREADS__
	static int32 NumberOfWorkerThreadsToSpawn()
	{
		// XXX EMSCRIPTEN THREAD LIMIT - Limit number of threads at runtime. Besides, PTHREAD_POOL_SIZE preallocate before init.
		// TODO: Remove this override function once Wasm threads consume much less memory.
		return 1;
	}
#endif

	static int32 NumberOfIOWorkerThreadsToSpawn()
	{
#ifdef __EMSCRIPTEN_PTHREADS__
		// XXX EMSCRIPTEN THREAD LIMIT - Limit number of threads at runtime. Besides, PTHREAD_POOL_SIZE preallocate before init.
		// TODO: Remove this override function once Wasm threads consume much less memory.
		return 1;
#else
		return 4;
#endif
	}

	static bool AllowThreadHeartBeat()
	{
		return false;
	}

	FORCEINLINE static void MemoryBarrier()
	{
#ifdef __EMSCRIPTEN_PTHREADS__
		emscripten_atomic_fence();
#endif
	}

	/** Return true if a debugger is present */
	FORCEINLINE static bool IsDebuggerPresent()
	{
		return true;
	}

	static bool AllowRenderThread()
	{
#ifdef __EMSCRIPTEN_PTHREADS__
		return !!HTML5_ENABLE_RENDERER_THREAD;
#else
		return false;
#endif
	}

	static bool AllowAudioThread()
	{
#ifdef __EMSCRIPTEN_PTHREADS__
		return true;
#else
		return false; // note to self: GenericPlatform is true -- meaning, used fake threads if multithreading is not supported...
#endif
	}

	static void LocalPrint(const TCHAR* Str);
};

typedef FHTML5Misc FPlatformMisc;
