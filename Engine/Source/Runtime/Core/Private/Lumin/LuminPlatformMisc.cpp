// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Lumin/LuminPlatformMisc.h"
#include "Lumin/LuminLifecycle.h"
#include "Android/AndroidPlatformStackWalk.h"
#include "Android/AndroidPlatformCrashContext.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Misc/VarargsHelper.h"
#include "Modules/ModuleManager.h"
#include "Misc/OutputDeviceError.h"
#include "EngineDefines.h"
#include "Misc/CallbackDevice.h"
#include "Async/TaskGraphInterfaces.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/FeedbackContext.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include <ml_lifecycle.h>
#include <ml_logging.h>

// @todo Lumin: this is super gross - but HMD plugin had it hardcoded as well
static int32 GBuiltinDisplayWidth = 2560;
static int32 GBuiltinDisplayHeight = 960;

FString FLuminPlatformMisc::WritableDirPath = "";
FString FLuminPlatformMisc::PackageDirPath = "";
FString FLuminPlatformMisc::PackageName = "";
FString FLuminPlatformMisc::ComponentName = "";
bool FLuminPlatformMisc::ApplicationPathsInitialized = false;

void FLuminPlatformMisc::InitLifecycle()
{
	FLuminLifecycle::Initialize();
}

void FLuminPlatformMisc::RequestExit(bool Force)
{
	UE_LOG(LogWindows, Log, TEXT("FLuminPlatformMisc::RequestExit(%i)"), Force);
	if (Force)
	{
		_exit(1);
	}
	else
	{
		GIsRequestingExit = 1;
	}
}

void FLuminPlatformMisc::PlatformPreInit()
{
	// base class version
	FAndroidMisc::PlatformPreInit();

	FLuminLifecycle::Initialize();
	InitApplicationPaths();
}


// The PhysX Android libraries refer to some Android only utilities.
// So we reproduce them here as a hack as we build our own PhysX as if it's Android.
extern "C" int android_getCpuCount()
{
	return FLuminPlatformMisc::NumberOfCores();
}

#if !UE_BUILD_SHIPPING
bool FLuminPlatformMisc::IsDebuggerPresent()
{
	// If a process is tracing this one then TracerPid in /proc/self/status will
	// be the id of the tracing process. Use SignalHandler safe functions 

	int StatusFile = open("/proc/self/status", O_RDONLY);
	if (StatusFile == -1)
	{
		// Failed - unknown debugger status.
		return false;
	}

	char Buffer[256];
	ssize_t Length = read(StatusFile, Buffer, sizeof(Buffer));

	bool bDebugging = false;
	const char* TracerString = "TracerPid:\t";
	const ssize_t LenTracerString = strlen(TracerString);
	int i = 0;

	while ((Length - i) > LenTracerString)
	{
		// TracerPid is found
		if (strncmp(&Buffer[i], TracerString, LenTracerString) == 0)
		{
			// 0 if no process is tracing.
			bDebugging = Buffer[i + LenTracerString] != '0';
			break;
		}

		++i;
	}

	close(StatusFile);
	return bDebugging;
}
#endif // !UE_BUILD_SHIPPING

bool FLuminPlatformMisc::AllowRenderThread()
{
	return true;
}

bool FLuminPlatformMisc::SupportsLocalCaching()
{
	return true;
}

bool FLuminPlatformMisc::SupportsMessaging()
{
	return true;
}

bool FLuminPlatformMisc::GetOverrideResolution(int32 &ResX, int32& ResY)
{
	ResX = GBuiltinDisplayWidth;
	ResY = GBuiltinDisplayHeight;

	return true;
}

bool FLuminPlatformMisc::ShouldUseVulkan()
{
	bool bUseVulkan = false;
	GConfig->GetBool(TEXT("/Script/LuminRuntimeSettings.LuminRuntimeSettings"), TEXT("bUseVulkan"), bUseVulkan, GEngineIni);

	return bUseVulkan;
}

bool FLuminPlatformMisc::ShouldUseDesktopVulkan()
{
	// @todo Lumin: Double check all this stuff after merging general android Vulkan SM5 from main
	bool bUseMobileRendering = false;
	GConfig->GetBool(TEXT("/Script/LuminRuntimeSettings.LuminRuntimeSettings"), TEXT("bUseMobileRendering"), bUseMobileRendering, GEngineIni);

	return ShouldUseVulkan() && !bUseMobileRendering;
}

const TCHAR* FLuminPlatformMisc::GetDefaultDeviceProfileName()
{
	if (ShouldUseDesktopVulkan() || ShouldUseDesktopOpenGL())
	{
		return TEXT("Lumin_Desktop");
	}
	return TEXT("Lumin");
}

bool FLuminPlatformMisc::ShouldUseDesktopOpenGL()
{
	bool bUseMobileRendering = false;
	GConfig->GetBool(TEXT("/Script/LuminRuntimeSettings.LuminRuntimeSettings"), TEXT("bUseMobileRendering"), bUseMobileRendering, GEngineIni);

	return !(ShouldUseVulkan() || bUseMobileRendering);
}

void FLuminPlatformMisc::GetValidTargetPlatforms(TArray<FString>& TargetPlatformNames)
{
	TargetPlatformNames.Add(TEXT("Lumin"));
}

void FLuminPlatformMisc::LocalPrint(const TCHAR* Message)
{
	LocalPrintWithVerbosity(Message, ELogVerbosity::Display);
}

void FLuminPlatformMisc::LowLevelOutputDebugString(const TCHAR *Message)
{
	LocalPrintWithVerbosity(Message, ELogVerbosity::Display);
}

void FLuminPlatformMisc::LowLevelOutputDebugStringWithVerbosity(const TCHAR *Message, ELogVerbosity::Type Verbosity)
{
	LocalPrintWithVerbosity(Message, Verbosity);
}

void FLuminPlatformMisc::LowLevelOutputDebugStringfWithVerbosity(ELogVerbosity::Type Verbosity, const TCHAR *Fmt, ...)
{
	GROWABLE_LOGF(
		FLuminPlatformMisc::LowLevelOutputDebugStringWithVerbosity(Buffer, Verbosity);
	);
}

// @todo override FOutputDevice and FOutputDeviceError to actually utilize this
void FLuminPlatformMisc::LocalPrintWithVerbosity(const TCHAR *Message, ELogVerbosity::Type Verbosity)
{
#if !UE_BUILD_SHIPPING
	MLLogLevel logLevel = MLLogLevel_Debug;
	switch (Verbosity)
	{
		case ELogVerbosity::Fatal:
			logLevel = MLLogLevel_Fatal;
			break;
		case ELogVerbosity::Error:
			logLevel = MLLogLevel_Error;
			break;
		case ELogVerbosity::Warning:
			logLevel = MLLogLevel_Warning;
			break;
		case ELogVerbosity::Display:
			logLevel = MLLogLevel_Info;
			break;
		case ELogVerbosity::Log:
			// lets keep it on Debug for now. Ideally this should only goto the log file and not to the console.
			logLevel = MLLogLevel_Debug;
			break;
		case ELogVerbosity::Verbose:
		case ELogVerbosity::VeryVerbose:
			logLevel = MLLogLevel_Verbose;
			break;
		default:
			logLevel = MLLogLevel_Debug;
			break;
	}

	const int MAX_LOG_LENGTH = 4096;
	// not static since may be called by different threads
	ANSICHAR MessageBuffer[MAX_LOG_LENGTH];

	const TCHAR* SourcePtr = Message;
	while (*SourcePtr)
	{
		ANSICHAR* WritePtr = MessageBuffer;
		int32 RemainingSpace = MAX_LOG_LENGTH;
		while (*SourcePtr && --RemainingSpace > 0)
		{
			if (*SourcePtr == TEXT('\r'))
			{
				// If next character is newline, skip it
				if (*(++SourcePtr) == TEXT('\n'))
					++SourcePtr;
				break;
			}
			else if (*SourcePtr == TEXT('\n'))
			{
				++SourcePtr;
				break;
			}
			else
			{
				*WritePtr++ = static_cast<ANSICHAR>(*SourcePtr++);
			}
		}
		*WritePtr = '\0';
		MLLoggingLog(logLevel, "UE4", MessageBuffer);
	}
#endif
}

const FString& FLuminPlatformMisc::GetApplicationWritableDirectoryPath()
{
	InitApplicationPaths();
	return WritableDirPath;
}

const FString& FLuminPlatformMisc::GetApplicationPackageDirectoryPath()
{
	InitApplicationPaths();
	return PackageDirPath;
}

const FString& FLuminPlatformMisc::GetApplicationApplicationPackageName()
{
	InitApplicationPaths();
	return PackageName;
}

const FString& FLuminPlatformMisc::GetApplicationComponentName()
{
	InitApplicationPaths();
	return ComponentName;
}


void FLuminPlatformMisc::InitApplicationPaths()
{
	if (ApplicationPathsInitialized)
	{
		return;
	}

	if (!FLuminLifecycle::IsLifecycleInitialized())
	{
		// Lifecycle services should be registered as early as possible.
		// The OS will kill the app if it does not register with lifecycle withing a given timeout.
		// That time frame, although good for a packaged app, is small when using cook-on-the-fly.
		FLuminLifecycle::Initialize();

		// Only try to initialize lifecycle service once or else it will cause a recursive crash.
		if (!FLuminLifecycle::IsLifecycleInitialized())
		{
			WritableDirPath = FPlatformProcess::BaseDir();
			PackageDirPath = FString(FPlatformProcess::BaseDir()) + FString(TEXT("../"));
			// TODO PackageName and ComponentName are currently not initialized. Investigate whether they should be.
			return;
		}
	}

	MLLifecycleSelfInfo* SelfInfo = nullptr;
	MLResult Result = MLLifecycleGetSelfInfo(&SelfInfo);
	if (Result != MLResult_Ok || SelfInfo == nullptr)
	{
		FLuminPlatformMisc::LowLevelOutputDebugStringWithVerbosity(TEXT("Could not get self info for the application. The application paths will be incorrect"), ELogVerbosity::Error);
		return;
	}

	WritableDirPath = FString(ANSI_TO_TCHAR(SelfInfo->writable_dir_path));
	WritableDirPath.RemoveFromEnd(TEXT("/"));
	PackageDirPath = FString(ANSI_TO_TCHAR(SelfInfo->package_dir_path));
	PackageDirPath.RemoveFromEnd(TEXT("/"));
	PackageName = FString(ANSI_TO_TCHAR(SelfInfo->package_name));
	ComponentName = FString(ANSI_TO_TCHAR(SelfInfo->component_name));

	MLLifecycleFreeSelfInfo(&SelfInfo);

	ApplicationPathsInitialized = true;
}
