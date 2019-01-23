// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "WindowsRunnableThread.h"
#include "Misc/OutputDeviceError.h"
#include "Stats/Stats.h"
#include "Misc/FeedbackContext.h"
#include "HAL/ExceptionHandling.h"

DEFINE_LOG_CATEGORY_STATIC(LogThreadingWindows, Log, All);

#if !PLATFORM_XBOXONE		
void FRunnableThreadWin::SetThreadDescription(HANDLE hThread, PCWSTR lpThreadDescription)
{
	// SetThreadDescription is only available from Windows 10 version 1607 / Windows Server 2016
	//
	// So in order to be compatible with older Windows versions we probe for the API at runtime
	// and call it only if available.

	typedef HRESULT(WINAPI *SetThreadDescriptionFnPtr)(HANDLE hThread, PCWSTR lpThreadDescription);

#pragma warning( push )
#pragma warning( disable: 4191 )	// unsafe conversion from 'type of expression' to 'type required'
	static SetThreadDescriptionFnPtr RealSetThreadDescription = (SetThreadDescriptionFnPtr) GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "SetThreadDescription");
#pragma warning( pop )

	if (RealSetThreadDescription)
	{
		RealSetThreadDescription(hThread, lpThreadDescription);
	}
}
#endif

uint32 FRunnableThreadWin::GuardedRun()
{
	uint32 ExitCode = 0;

	FPlatformProcess::SetThreadAffinityMask(ThreadAffinityMask);

#if UE_BUILD_DEBUG
	if (true && !GAlwaysReportCrash)
#else
	if (FPlatformMisc::IsDebuggerPresent() && !GAlwaysReportCrash)
#endif // UE_BUILD_DEBUG
	{
		ExitCode = Run();
	}
	else
	{
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
		__try
#endif // !PLATFORM_SEH_EXCEPTIONS_DISABLED
		{
			ExitCode = Run();
		}
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
		__except (ReportCrash( GetExceptionInformation() ))
		{
			// Make sure the information which thread crashed makes it into the log.
			UE_LOG( LogThreadingWindows, Error, TEXT( "Runnable thread %s crashed." ), *ThreadName );
			GWarn->Flush();

			// Append the thread name at the end of the error report.
			FCString::Strncat( GErrorHist, LINE_TERMINATOR TEXT( "Crash in runnable thread " ), ARRAY_COUNT( GErrorHist ) );
			FCString::Strncat( GErrorHist, *ThreadName, ARRAY_COUNT( GErrorHist ) );

			// Crashed.
			ExitCode = 1;		
			GError->HandleError();
			FPlatformMisc::RequestExit( true );
		}
#endif // !PLATFORM_SEH_EXCEPTIONS_DISABLED
	}

	return ExitCode;
}


uint32 FRunnableThreadWin::Run()
{
	// Assume we'll fail init
	uint32 ExitCode = 1;
	check(Runnable);

	// Initialize the runnable object
	if (Runnable->Init() == true)
	{
		// Initialization has completed, release the sync event
		ThreadInitSyncEvent->Trigger();

		// Setup TLS for this thread, used by FTlsAutoCleanup objects.
		SetTls();

		// Now run the task that needs to be done
		ExitCode = Runnable->Run();
		// Allow any allocated resources to be cleaned up
		Runnable->Exit();

#if STATS
		FThreadStats::Shutdown();
#endif
		FreeTls();
	}
	else
	{
		// Initialization has failed, release the sync event
		ThreadInitSyncEvent->Trigger();
	}

	return ExitCode;
}
