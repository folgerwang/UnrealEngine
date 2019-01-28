// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
// Copyright 2016 Magic Leap, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "LaunchEngineLoop.h"
#include "HAL/ExceptionHandling.h"
#include "Misc/CommandLine.h"
#include "HAL/PlatformProcess.h"
#include "Lumin/LuminPlatformMisc.h"
#include <ml_lifecycle.h>

#include <locale.h>
#include <sys/resource.h>

#include "IMessagingModule.h"
#include "ISessionServicesModule.h"
#include "ISessionService.h"


/** The global EngineLoop instance */
FEngineLoop	GEngineLoop;

extern int32 GuardedMain(const TCHAR* CmdLine);

static void InitCommandLine()
{
	static const uint32 CMD_LINE_MAX = 16384u;

	// It is possible that FLuminLifecycle::Initialize() is called before LaunchLumin::InitCommandLine().
	// If so, FLuminLifecycle::Initialize() would have initialized the command line and filled it with args passed via mldb launch.
	// So initialize command line here only if it hasnt been initialized already.
	if (!FCommandLine::IsInitialized())
	{
		// initialize the command line to an empty string
		FCommandLine::Set(TEXT(""));
	}

	// Adds command line args coming from lifecycle app init args.
	FLuminPlatformMisc::InitLifecycle();	

	// append space as UE4CommandLine.txt may or may not have a space at the start.
	FCommandLine::Append(TEXT(" "));

	// read in the command line text file from the sdcard if it exists
	FString CommandLineFilePath = FString(FPlatformProcess::BaseDir()) + FString("UE4CommandLine.txt");
	FILE* CommandLineFile = fopen(TCHAR_TO_UTF8(*CommandLineFilePath), "r");
	if (CommandLineFile == NULL)
	{
		// if that failed, try the lowercase version
		CommandLineFilePath = CommandLineFilePath.Replace(TEXT("UE4CommandLine.txt"), TEXT("ue4commandline.txt"));
		CommandLineFile = fopen(TCHAR_TO_UTF8(*CommandLineFilePath), "r");
	}

	if (CommandLineFile)
	{
		char CommandLine[CMD_LINE_MAX];
		FMemory::Memzero(CommandLine, CMD_LINE_MAX * sizeof(char));
		fgets(CommandLine, ARRAY_COUNT(CommandLine) - 1, CommandLineFile);
		
		fclose(CommandLineFile);
		
		// chop off trailing spaces
		while (*CommandLine && isspace(CommandLine[strlen(CommandLine) - 1]))
		{
			CommandLine[strlen(CommandLine) - 1] = 0;
		}
		
		FCommandLine::Append(UTF8_TO_TCHAR(CommandLine));
	}

	// This prevents duplicate logs in logcat as indicated in -
	// Engine/Source/Runtime/Core/Public/HAL/FeedbackContextAnsi.h
	FCommandLine::Append(TEXT(" -stdout "));
}

/**
* Increases (soft) limit on a specific resource
*
* @param DesiredLimit - max number of open files desired.
*/
static bool IncreaseLimit(int Resource, rlim_t DesiredLimit)
{
	rlimit Limit;
	if (getrlimit(Resource, &Limit) != 0)
	{
		fprintf(stderr, "getrlimit() failed with error %d (%s)\n", errno, strerror(errno));
		return false;
	}

	if (Limit.rlim_cur == RLIM_INFINITY || Limit.rlim_cur >= DesiredLimit)
	{
#if !UE_BUILD_SHIPPING
		printf("- Existing per-process limit (soft=%lu, hard=%lu) is enough for us (need only %lu)\n", Limit.rlim_cur, Limit.rlim_max, DesiredLimit);
#endif // !UE_BUILD_SHIPPING
		return true;
	}

	Limit.rlim_cur = DesiredLimit;
	if (setrlimit(Resource, &Limit) != 0)
	{
		fprintf(stderr, "setrlimit() failed with error %d (%s)\n", errno, strerror(errno));

		if (errno == EINVAL)
		{
			if (DesiredLimit == RLIM_INFINITY)
			{
				fprintf(stderr, "- Max per-process value allowed is %lu (we wanted infinity).\n", Limit.rlim_max);
			}
			else
			{
				fprintf(stderr, "- Max per-process value allowed is %lu (we wanted %lu).\n", Limit.rlim_max, DesiredLimit);
			}
		}
		return false;
	}

	return true;
}

/**
* Expects FCommandLine to be set up. Increases limit on
*  - number of open files to be no less than desired (if specified on command line, otherwise left alone)
*  - size of core file, so core gets dumped and we can debug crashed builds (unless overriden with -nocore)
*
*/
static bool IncreasePerProcessLimits()
{
	// honor the parameter if given, but don't change limits if not
	int32 FileHandlesToReserve = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("numopenfiles="), FileHandlesToReserve) && FileHandlesToReserve > 0)
	{
#if !UE_BUILD_SHIPPING
		printf("Increasing per-process limit of open file handles to %d\n", FileHandlesToReserve);
#endif // !UE_BUILD_SHIPPING
		if (!IncreaseLimit(RLIMIT_NOFILE, FileHandlesToReserve))
		{
			fprintf(stderr, "Could not adjust number of file handles, consider changing \"nofile\" in /etc/security/limits.conf and relogin.\nerror(%d): %s\n", errno, strerror(errno));
			return false;
		}
	}

#if !UE_BUILD_SHIPPING
	if (!FParse::Param(FCommandLine::Get(), TEXT("nocore")))
	{
		printf("Increasing per-process limit of core file size to infinity.\n");
		if (!IncreaseLimit(RLIMIT_CORE, RLIM_INFINITY))
		{
			fprintf(stderr, "Could not adjust core file size, consider changing \"core\" in /etc/security/limits.conf and relogin.\nerror(%d): %s\n", errno, strerror(errno));
			fprintf(stderr, "Alternatively, pass -nocore if you are unable or unwilling to do that.\n");
			return false;
		}
	}
#endif // !UE_BUILD_SHIPPING

	return true;
}


int main(int argc, char *argv[])
{
	FPlatformMisc::SetGracefulTerminationHandler();

	int ErrorLevel = 0;

	// read the command line file
	InitCommandLine();
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Final commandline: %s\n"), FCommandLine::Get());

#if !UE_BUILD_SHIPPING
	GAlwaysReportCrash = true;	// set by default and reverse the behavior
	if (FParse::Param(FCommandLine::Get(), TEXT("nocrashreports")) || FParse::Param(FCommandLine::Get(), TEXT("no-crashreports")))
	{
		GAlwaysReportCrash = false;
	}
#endif

	if (!IncreasePerProcessLimits())
	{
		fprintf(stderr, "Could not set desired per-process limits, consider changing system limits.\n");
		ErrorLevel = 1;
		return ErrorLevel;
	}
// 		if (!FPlatformMisc::IsDebuggerPresent() || GAlwaysReportCrash)
// 		{
// 			// Use default crash handler, but we still need to set this up to register signal handlers
// 			FPlatformMisc::SetCrashHandler(nullptr);
// 		}
// 		ErrorLevel = GuardedMain(FCommandLine::Get());


	// initialize the engine
	GEngineLoop.PreInit(0, NULL, FCommandLine::Get());

	// initialize HMDs 
	// @todo Lumin: I guess we don't need this?
	// InitHMDs();

	UE_LOG(LogAndroid, Display, TEXT("Passed PreInit()"));

	GLog->SetCurrentThreadAsMasterThread();

	GEngineLoop.Init();

	UE_LOG(LogAndroid, Log, TEXT("Passed GEngineLoop.Init()"));

#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("Messaging")))
	{
		// initialize messaging subsystem
		FModuleManager::LoadModuleChecked<IMessagingModule>("Messaging");
		TSharedPtr<ISessionService> SessionService = FModuleManager::LoadModuleChecked<ISessionServicesModule>("SessionServices").GetSessionService();
		SessionService->Start();

		// Initialize functional testing
		FModuleManager::Get().LoadModule("FunctionalTesting");
	}
#endif

	// tick until done
	while (!GIsRequestingExit)
	{
// 		FAppEventManager::GetInstance()->Tick();
// 		if(!FAppEventManager::GetInstance()->IsGamePaused())
		{
			GEngineLoop.Tick();
		}
// 		else
// 		{
// 			// use less CPU when paused
// 			FPlatformProcess::Sleep(0.10f);
// 		}

#if !UE_BUILD_SHIPPING
		// show console window on next game tick
// 		if (GShowConsoleWindowNextTick)
// 		{
// 			GShowConsoleWindowNextTick = false;
// 			AndroidThunkCpp_ShowConsoleWindow();
// 		}
#endif
	}
// 	FAppEventManager::GetInstance()->TriggerEmptyQueue();

	UE_LOG(LogAndroid, Log, TEXT("Exiting"));

	// exit out!
	GEngineLoop.Exit();
	FEngineLoop::AppExit();

	FPlatformMisc::LowLevelOutputDebugString(TEXT("Exiting is over"));

	if (ErrorLevel)
	{
		printf("Exiting abnormally (error code: %d)\n", ErrorLevel);
	}
	return ErrorLevel;
}
