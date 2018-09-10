// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Blackmagic.h"
#include "BlackmagicMediaPrivate.h"

#include "Interfaces/IPluginManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

/*
 * Static initialization
 */

void* FBlackmagic::LibHandle = nullptr;

bool FBlackmagic::Initialize()
{
#if BLACKMAGICMEDIA_DLL_PLATFORM
	check(LibHandle == nullptr);

#if BLACKMAGICMEDIA_DLL_DEBUG
	const FString VideoIODll = TEXT("BlackmagicLibd.dll");
#else
	const FString VideoIODll = TEXT("BlackmagicLib.dll");
#endif // BLACKMAGICMEDIA_DLL_DEBUG

	// determine directory paths
	FString DllPath = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("BlackmagicMedia"))->GetBaseDir(), TEXT("/Binaries/ThirdParty/Win64"));
	FPlatformProcess::PushDllDirectory(*DllPath);
	DllPath = FPaths::Combine(DllPath, VideoIODll);

	if (!FPaths::FileExists(DllPath))
	{
		UE_LOG(LogBlackmagicMedia, Error, TEXT("Failed to find the binary folder for the dll. Plug-in will not be functional."));
		return false;
	}

	LibHandle = FPlatformProcess::GetDllHandle(*DllPath);
	if (LibHandle == nullptr)
	{
		UE_LOG(LogBlackmagicMedia, Error, TEXT("Failed to load required library %s. Plug-in will not be functional."), *VideoIODll);
		return false;
	}

#if !NO_LOGGING
	BlackmagicDevice::VideoIOSetLoggingCallbacks(&LogInfo, &LogWarning, &LogError);
#endif // !NO_LOGGING

	return true;
#else
	return false;
#endif // BLACKMAGICMEDIA_DLL_PLATFORM
}

bool FBlackmagic::IsInitialized()
{
	return (LibHandle != nullptr);
}

void FBlackmagic::Shutdown()
{
#if BLACKMAGICMEDIA_DLL_PLATFORM
	if (LibHandle != nullptr)
	{
#if !NO_LOGGING
		BlackmagicDevice::VideoIOSetLoggingCallbacks(nullptr, nullptr, nullptr);
#endif // !NO_LOGGING
		FPlatformProcess::FreeDllHandle(LibHandle);
		LibHandle = nullptr;
	}
#endif // BLACKMAGICMEDIA_DLL_PLATFORM
}

void FBlackmagic::LogInfo(const TCHAR* InFormat, ...)
{
#if !NO_LOGGING
	TCHAR TempString[1024];
	va_list Args;

	va_start(Args, InFormat );
	FCString::GetVarArgs(TempString, ARRAY_COUNT(TempString), ARRAY_COUNT(TempString) - 1, InFormat, Args);
	va_end(Args);

	UE_LOG(LogBlackmagicMedia, Log, TempString);
#endif // !NO_LOGGIN
}

void FBlackmagic::LogWarning(const TCHAR* InFormat, ...)
{
#if !NO_LOGGING
	TCHAR TempString[1024];
	va_list Args;

	va_start(Args, InFormat );
	FCString::GetVarArgs(TempString, ARRAY_COUNT(TempString), ARRAY_COUNT(TempString) - 1, InFormat, Args);
	va_end(Args);

	UE_LOG(LogBlackmagicMedia, Warning, TempString);
#endif // !NO_LOGGIN
}

void FBlackmagic::LogError(const TCHAR* InFormat, ...)
{
#if !NO_LOGGING
	TCHAR TempString[1024];
	va_list Args;

	va_start(Args, InFormat );
	FCString::GetVarArgs(TempString, ARRAY_COUNT(TempString), ARRAY_COUNT(TempString) - 1, InFormat, Args);
	va_end(Args);

	UE_LOG(LogBlackmagicMedia, Error, TempString);
#endif // !NO_LOGGING
}
