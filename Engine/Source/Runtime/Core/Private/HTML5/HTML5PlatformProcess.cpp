// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HTML5Process.cpp: HTML5 implementations of Process functions
=============================================================================*/

#include "HTML5/HTML5PlatformProcess.h"
#ifdef __EMSCRIPTEN_PTHREADS__
#include <unistd.h>
#include <sys/time.h>
#include <emscripten/threading.h>
#include "HAL/PThreadEvent.h"
#endif
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreStats.h"
#include "Misc/App.h"
#include "Misc/SingleThreadEvent.h"
#include "Misc/CoreDelegates.h"

#include <emscripten/emscripten.h>

const TCHAR* FHTML5PlatformProcess::ComputerName()
{
	return TEXT("Browser");
}

const TCHAR* FHTML5PlatformProcess::BaseDir()
{
	return TEXT("");
}

DECLARE_CYCLE_STAT(TEXT("CPU Stall - HTML5Sleep"),STAT_HTML5Sleep,STATGROUP_CPUStalls);

void FHTML5PlatformProcess::Sleep( float Seconds )
{
	SCOPE_CYCLE_COUNTER(STAT_HTML5Sleep);
	FThreadIdleStats::FScopeIdle Scope;
	SleepNoStats(Seconds);
}

void FHTML5PlatformProcess::SleepNoStats(float Seconds)
{
#ifdef __EMSCRIPTEN_PTHREADS__
	if (!emscripten_is_main_browser_thread())
	{
		usleep(Seconds * 1000);
	}
#endif
}

void FHTML5PlatformProcess::SleepInfinite()
{
	// stop this thread forever
#ifdef __EMSCRIPTEN_PTHREADS__
	if (!emscripten_is_main_browser_thread())
	{
		EM_ASM({ console.log("FHTML5PlatformProcess::SleepInfinite()"); });
		usleep(INFINITY);
	}
#else
	EM_ASM({
		console.log("FHTML5PlatformProcess::SleepInfinite()");
		calling_a_function_that_does_not_exist_in_javascript_will__stop__the_thread_forever();
	}); // =)
#endif
}

#include "HTML5/HTML5PlatformRunnableThread.h"

FRunnableThread* FHTML5PlatformProcess::CreateRunnableThread()
{
#ifdef __EMSCRIPTEN_PTHREADS__
	return new FHTML5RunnablePThread();
#else
	return new FHTML5RunnableThread();
#endif
}

class FEvent* FHTML5PlatformProcess::CreateSynchEvent( bool bIsManualReset /*= 0*/ )
{
#ifdef __EMSCRIPTEN_PTHREADS__
	FEvent* Event = NULL;
	if (FPlatformProcess::SupportsMultithreading())
	{
		// Allocate the new object
		Event = new FPThreadEvent();
	}
	else
	{
		// Fake event.
		Event = new FSingleThreadEvent();
	}
	// If the internal create fails, delete the instance and return NULL
	if (!Event->Create(bIsManualReset))
	{
		delete Event;
		Event = NULL;
	}
	return Event;
#else
	FEvent* Event = new FSingleThreadEvent();
	return Event;
#endif
}

bool FHTML5PlatformProcess::SupportsMultithreading()
{
#ifdef __EMSCRIPTEN_PTHREADS__
	// TODO: EMSCRITPEN_TOOLCHAIN_UPGRADE_CHECK - cache this when multi-threaded ASMFS is live
	bool bEnableMultithreading = EM_ASM_INT({
		if ( ENVIRONMENT_IS_WORKER )
		{	// worker threads do not have access to emscripten's Module object
			return true; // but, if here in UE4 -- this is a "worker thread"
		}
		return Module['UE4_MultiThreaded'];
	});
	if ( bEnableMultithreading )
	{	// if here, browser supports SharedArrayBuffer, allow project to override this
		GConfig->GetBool(TEXT("/Script/HTML5PlatformEditor.HTML5TargetSettings"), TEXT("EnableMultithreading"), bEnableMultithreading, GEngineIni);
	}
	return bEnableMultithreading;
#else
	return false;
#endif
}

void FHTML5PlatformProcess::LaunchURL(const TCHAR* URL, const TCHAR* Parms, FString* Error)
{
	if (FCoreDelegates::ShouldLaunchUrl.IsBound() && !FCoreDelegates::ShouldLaunchUrl.Execute(URL))
	{
		if (Error)
		{
			*Error = TEXT("LaunchURL cancelled by delegate");
		}
		return;
	}

	auto TmpURL = StringCast<ANSICHAR>(URL);
	MAIN_THREAD_EM_ASM({var InUrl = Pointer_stringify($0); console.log("Opening "+InUrl); window.open(InUrl);}, (ANSICHAR*)TmpURL.Get());
}

const TCHAR* FHTML5PlatformProcess::ExecutableName(bool bRemoveExtension)
{
	return FApp::GetProjectName();
}
