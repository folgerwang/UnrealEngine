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
	FString AjaDllPath = *FPaths::Combine(FPaths::EngineDir(), TEXT("../Enterprise/Binaries/ThirdParty/AJA/Win64"), AjaDll);
	if (!FPaths::FileExists(AjaDllPath))
	{
		AjaDllPath = *FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries/ThirdParty/AJA/Win64"), AjaDll);
	}
	if (!FPaths::FileExists(AjaDllPath))
	{
		AjaDllPath = *FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/ThirdParty/AJA/Win64"), AjaDll);
	}
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

namespace AJATimecodeEncoder
{
	void Pattern(FColor* ColorBuffer, uint32 ColorBufferWidth, int32 HeightIndex, int32 Amount)
	{
		for (int32 Index = 0; Index < Amount; ++Index)
		{
			*(ColorBuffer + (ColorBufferWidth * HeightIndex) + Index) = (Index % 2) ? FColor::Red : FColor::Black;
		}
	}

	void Time(FColor* ColorBuffer, uint32 ColorBufferWidth, int32 HeightIndex, int32 Time)
	{
		int32 Tenth = (Time / 10);
		int32 Unit = (Time % 10);
		if (Tenth > 0)
		{
			*(ColorBuffer + (ColorBufferWidth * HeightIndex) + Tenth - 1) = FColor::White;
		}
		*(ColorBuffer + (ColorBufferWidth * (1 + HeightIndex)) + Unit) = FColor::White;
	}
}

void FAja::EncodeTimecode(const AJA::FTimecode& Timecode, FColor* ColorBuffer, uint32 ColorBufferWidth, uint32 ColorBufferHeight)
{
	const int32 FillWidth = 12;
	const int32 FillHeight = 6 * 2;

	if (ColorBufferWidth > FillWidth && ColorBufferHeight > FillHeight)
	{
		for (int32 IndexHeight = 0; IndexHeight < FillHeight; ++IndexHeight)
		{
			for (int32 IndexWidth = 0; IndexWidth < FillWidth; ++IndexWidth)
			{
				*(ColorBuffer + ColorBufferWidth * IndexHeight + IndexWidth) = FColor::Black;
			}
		}

		AJATimecodeEncoder::Pattern(ColorBuffer, ColorBufferWidth, 0, 2);	//hh
		AJATimecodeEncoder::Pattern(ColorBuffer, ColorBufferWidth, 1, 10);
		AJATimecodeEncoder::Pattern(ColorBuffer, ColorBufferWidth, 3, 6);	//mm
		AJATimecodeEncoder::Pattern(ColorBuffer, ColorBufferWidth, 4, 10);
		AJATimecodeEncoder::Pattern(ColorBuffer, ColorBufferWidth, 6, 6);	//ss
		AJATimecodeEncoder::Pattern(ColorBuffer, ColorBufferWidth, 7, 10);
		AJATimecodeEncoder::Pattern(ColorBuffer, ColorBufferWidth, 9, 6);	//ff
		AJATimecodeEncoder::Pattern(ColorBuffer, ColorBufferWidth, 10, 10);

		AJATimecodeEncoder::Time(ColorBuffer, ColorBufferWidth, 0, Timecode.Hours);
		AJATimecodeEncoder::Time(ColorBuffer, ColorBufferWidth, 3, Timecode.Minutes);
		AJATimecodeEncoder::Time(ColorBuffer, ColorBufferWidth, 6, Timecode.Seconds);
		AJATimecodeEncoder::Time(ColorBuffer, ColorBufferWidth, 9, Timecode.Frames);
	}
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



