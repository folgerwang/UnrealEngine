// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Aja.h"
#include "AjaMediaPrivate.h"

#include "Misc/FrameRate.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

 //~ Static initialization
 //--------------------------------------------------------------------
void* FAja::LibHandle = nullptr; 
bool FAja::bCanForceAJAUsage = false; 

//~ Initialization functions implementation
//--------------------------------------------------------------------
bool FAja::Initialize()
{
#if AJAMEDIA_DLL_PLATFORM
	check(LibHandle == nullptr);

#if AJAMEDIA_DLL_DEBUG
	const FString AjaDll = TEXT("AJAd.dll");
#else
	const FString AjaDll = TEXT("AJA.dll");
#endif //AJAMEDIA_DLL_DEBUG

	// determine directory paths
	FString AjaDllPath = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("AjaMedia"))->GetBaseDir(), TEXT("/Binaries/ThirdParty/Win64"));
	FPlatformProcess::PushDllDirectory(*AjaDllPath);
	AjaDllPath = FPaths::Combine(AjaDllPath, AjaDll);

	if (!FPaths::FileExists(AjaDllPath))
	{
		UE_LOG(LogAjaMedia, Error, TEXT("Failed to find the binary folder for the AJA dll. Plug-in will not be functional."));
		return false;
	}

	LibHandle = FPlatformProcess::GetDllHandle(*AjaDllPath);

	if (LibHandle == nullptr)
	{
		UE_LOG(LogAjaMedia, Error, TEXT("Failed to load required library %s. Plug-in will not be functional."), *AjaDllPath);
		return false;
	}

	//Check if command line argument to force AJA card usage is there
	bCanForceAJAUsage = FParse::Param(FCommandLine::Get(), TEXT("forceajausage"));

#if !NO_LOGGING
	AJA::SetLoggingCallbacks(&LogInfo, &LogWarning, &LogError);
#endif // !NO_LOGGING
	return true;
#else
	return false;
#endif // AJAMEDIA_DLL_PLATFORM
}

bool FAja::IsInitialized()
{
	return (LibHandle != nullptr);
}

void FAja::Shutdown()
{
#if AJAMEDIA_DLL_PLATFORM
	if (LibHandle != nullptr)
	{
#if !NO_LOGGING
		AJA::SetLoggingCallbacks(nullptr, nullptr, nullptr);
#endif // !NO_LOGGING
		FPlatformProcess::FreeDllHandle(LibHandle);
		LibHandle = nullptr;
	}
#endif // AJAMEDIA_DLL_PLATFORM
}

//~ Conversion functions implementation
//--------------------------------------------------------------------
FTimespan FAja::ConvertAJATimecode2Timespan(const AJA::FTimecode& InTimecode, const AJA::FTimecode& PreviousTimeCode, const FTimespan& PreviousTimespan, const FFrameRate& InFPS)
{
	check(InFPS.IsValid());

	//With FrameRate faster than 30FPS, max frame number will still be small than 30
	//Get by how much we need to divide the actual count.
	const float FrameRate = InFPS.AsDecimal();
	const float DividedFrameRate = FrameRate > 30.0f ? (FrameRate * 30.0f) / FrameRate : FrameRate;

	FTimespan NewTimespan;
	if (PreviousTimeCode == InTimecode)
	{
		NewTimespan = PreviousTimespan + FTimespan::FromSeconds(InFPS.AsInterval());
	}
	else
	{
		NewTimespan = FTimespan(0, InTimecode.Hours, InTimecode.Minutes, InTimecode.Seconds, static_cast<int32>((ETimespan::TicksPerSecond * InTimecode.Frames) / DividedFrameRate) * ETimespan::NanosecondsPerTick);
	}

	return NewTimespan;
}

FTimecode FAja::ConvertAJATimecode2Timecode(const AJA::FTimecode& InTimecode, const FFrameRate& InFPS)
{
	return FTimecode(InTimecode.Hours, InTimecode.Minutes, InTimecode.Seconds, InTimecode.Frames, FTimecode::IsDropFormatTimecodeSupported(InFPS));
}

//~ Log functions implementation
//--------------------------------------------------------------------
void FAja::LogInfo(const TCHAR* InFormat, ...)
{
#if !NO_LOGGING
	TCHAR TempString[1024];
	va_list Args;

	va_start(Args, InFormat );
	FCString::GetVarArgs(TempString, ARRAY_COUNT(TempString), ARRAY_COUNT(TempString) - 1, InFormat, Args);
	va_end(Args);

	UE_LOG(LogAjaMedia, Log, TempString);
#endif // !NO_LOGGING
}

void FAja::LogWarning(const TCHAR* InFormat, ...)
{
#if !NO_LOGGING
	TCHAR TempString[1024];
	va_list Args;

	va_start(Args, InFormat );
	FCString::GetVarArgs(TempString, ARRAY_COUNT(TempString), ARRAY_COUNT(TempString) - 1, InFormat, Args);
	va_end(Args);

	UE_LOG(LogAjaMedia, Warning, TempString);
#endif // !NO_LOGGING
}

void FAja::LogError(const TCHAR* InFormat, ...)
{
#if !NO_LOGGING
	TCHAR TempString[1024];
	va_list Args;

	va_start(Args, InFormat );
	FCString::GetVarArgs(TempString, ARRAY_COUNT(TempString), ARRAY_COUNT(TempString) - 1, InFormat, Args);
	va_end(Args);

	UE_LOG(LogAjaMedia, Error, TempString);
#endif // !NO_LOGGING
}



