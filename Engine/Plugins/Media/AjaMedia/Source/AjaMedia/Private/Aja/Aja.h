// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/App.h"
#include "Misc/Timecode.h"
#include "Misc/Timespan.h"

namespace AJA
{
	struct FTimecode;
}
struct FFrameRate;

class FAja
{
public:
	static bool Initialize();
	static bool IsInitialized();
	static void Shutdown();

	// Helpers
	static FTimespan ConvertAJATimecode2Timespan(const AJA::FTimecode& InTimecode, const AJA::FTimecode& PreviousTimeCode, const FTimespan& PreviousTimespan, const FFrameRate& InFPS);
	static FTimecode ConvertAJATimecode2Timecode(const AJA::FTimecode& InTimecode, const FFrameRate& InFPS);

	static bool CanUseAJACard() { return (FApp::CanEverRender() || bCanForceAJAUsage); }

private:
	static void LogInfo(const TCHAR* InFormat, ...);
	static void LogWarning(const TCHAR* InFormat, ...);
	static void LogError(const TCHAR* InFormat, ...);

private:
	static void* LibHandle;
	static bool bCanForceAJAUsage;
};
