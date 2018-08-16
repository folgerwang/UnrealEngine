// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "FramePro/FrameProProfiler.h"

#if FRAMEPRO_ENABLED

#include "CoreTypes.h"
#include "HAL/ThreadSingleton.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "HAL/ThreadManager.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/Paths.h"
#include "Runtime/Launch/Resources/Version.h"

DEFINE_LOG_CATEGORY_STATIC(LogFramePro, Log, All);

static int32 GFrameProEnabled = 0;
static FAutoConsoleVariableRef CVarVerboseScriptStats(
	TEXT("framepro.enabled"),
	GFrameProEnabled,
	TEXT("Enable FramePro named events.\n"),
	ECVF_Default
);

#if PLATFORM_TCHAR_IS_CHAR16
#define FP_TEXT_PASTE(x) L ## x
#define WTEXT(x) FP_TEXT_PASTE(x)
#else
#define WTEXT TEXT
#endif

/** One entry in timer scope stack */
struct FFrameProProfilerScope
{
	int64 StartTime;
	FramePro::StringId StatStringId;

	FFrameProProfilerScope()
		: StartTime(0)
		, StatStringId(0)
	{}

	FORCEINLINE void BeginScope()
	{
		FRAMEPRO_GET_CLOCK_COUNT(StartTime);
	}

	FORCEINLINE void BeginScope(const ANSICHAR* Text)
	{
		StatStringId = FramePro::RegisterString(Text);
		FRAMEPRO_GET_CLOCK_COUNT(StartTime);
	}

	FORCEINLINE void BeginScope(const TCHAR* Text)
	{

		StatStringId = FramePro::RegisterString(TCHAR_TO_WCHAR(Text));
		FRAMEPRO_GET_CLOCK_COUNT(StartTime);
	}

private:
	FORCEINLINE void EndScopeImpl(int64 EndTime)
	{
		int64 Duration = EndTime - StartTime;
		if (Duration < 0)
		{
			UE_LOG(LogFramePro, Warning, TEXT("Invalid duration scope! Start:%lld End:%lld"), StartTime, EndTime);
		}
		else if (FramePro::IsConnected() && (Duration > FramePro::GetConditionalScopeMinTime()))
		{
			FramePro::AddTimeSpan(StatStringId, "none", StartTime, EndTime);
		}
	}

public:
	FORCEINLINE void EndScope()
	{
		int64 EndTime;
		FRAMEPRO_GET_CLOCK_COUNT(EndTime);

		if (StartTime == 0)
		{
			UE_LOG(LogFramePro, Warning, TEXT("EndScope called before BeginScope!"));
			return;
		}

		EndScopeImpl(EndTime);
	}

	/** Allows override of scope name at point of closure */
	FORCEINLINE void EndScope(const ANSICHAR* Override)
	{
		int64 EndTime;
		FRAMEPRO_GET_CLOCK_COUNT(EndTime);

		StatStringId = FramePro::RegisterString(Override);

		EndScopeImpl(EndTime);
	}

	/** Allows override of scope name at point of closure */
	FORCEINLINE void EndScope(const TCHAR* Override)
	{
		int64 EndTime;
		FRAMEPRO_GET_CLOCK_COUNT(EndTime);

		StatStringId = FramePro::RegisterString(TCHAR_TO_WCHAR(Override));

		EndScopeImpl(EndTime);
	}
};

/** TSL storage for per-thread scope stack */
class FFrameProProfilerContext : public TThreadSingleton<FFrameProProfilerContext>
{
	friend TThreadSingleton<FFrameProProfilerContext>;

	FFrameProProfilerContext()
	: TThreadSingleton<FFrameProProfilerContext>()
	{
		FString ThreadName;
		if (IsInGameThread())
		{
			ThreadName = FName(NAME_GameThread).GetPlainNameString();
		}
		else
		{
			ThreadName = FThreadManager::Get().GetThreadName(ThreadId);
		}

		if (ThreadName.Len())
		{
			FramePro::SetThreadName(TCHAR_TO_ANSI(*ThreadName));
		}
	}
	
	/** Array that represents thread of scopes */
	TArray<FFrameProProfilerScope> ProfilerScopes;

public:

	FORCEINLINE void PushScope()
	{
		ProfilerScopes.AddDefaulted();
		ProfilerScopes.Last().BeginScope();
	}

	FORCEINLINE void PushScope(const ANSICHAR* Text)
	{
		ProfilerScopes.AddDefaulted();
		ProfilerScopes.Last().BeginScope(Text);
	}

	FORCEINLINE void PushScope(const TCHAR* Text)
	{
		ProfilerScopes.AddDefaulted();
		ProfilerScopes.Last().BeginScope(Text);
	}

	FORCEINLINE void PopScope()
	{
		if (ProfilerScopes.Num() > 0)
		{
			ProfilerScopes.Last().EndScope();
			ProfilerScopes.RemoveAt(ProfilerScopes.Num() - 1);
		}
	}

	FORCEINLINE void PopScope(const ANSICHAR* Override)
	{
		if (ProfilerScopes.Num() > 0)
		{
			ProfilerScopes.Last().EndScope(Override);
			ProfilerScopes.RemoveAt(ProfilerScopes.Num() - 1);
		}
	}

	FORCEINLINE void PopScope(const TCHAR* Override)
	{
		if (ProfilerScopes.Num() > 0)
		{
			ProfilerScopes.Last().EndScope(Override);
			ProfilerScopes.RemoveAt(ProfilerScopes.Num() - 1);
		}
	}
};

static void SendCPUStats()
{
	FRAMEPRO_NAMED_SCOPE("FramePro_SendCPUStats");

#if PLATFORM_ANDROID

	int32 NumCores = FMath::Min(FAndroidMisc::NumberOfCores(), 8);
	for (int32 CoreIdx = 0; CoreIdx < NumCores; CoreIdx++)
	{
		float Freq = (float)FAndroidMisc::GetCoreFrequency(CoreIdx, FAndroidMisc::ECoreFrequencyProperty::CurrentFrequency) / 1000000.f;

		switch (CoreIdx)
		{
		case 0:
			FRAMEPRO_CUSTOM_STAT("Core0Frequency", Freq, "CPUFreq", "GHz");
			break;
		case 1:
			FRAMEPRO_CUSTOM_STAT("Core1Frequency", Freq, "CPUFreq", "GHz");
			break;
		case 2:
			FRAMEPRO_CUSTOM_STAT("Core2Frequency", Freq, "CPUFreq", "GHz");
			break;
		case 3:
			FRAMEPRO_CUSTOM_STAT("Core3Frequency", Freq, "CPUFreq", "GHz");
			break;
		case 4:
			FRAMEPRO_CUSTOM_STAT("Core4Frequency", Freq, "CPUFreq", "GHz");
			break;
		case 5:
			FRAMEPRO_CUSTOM_STAT("Core5Frequency", Freq, "CPUFreq", "GHz");
			break;
		case 6:
			FRAMEPRO_CUSTOM_STAT("Core6Frequency", Freq, "CPUFreq", "GHz");
			break;
		case 7:
			FRAMEPRO_CUSTOM_STAT("Core7Frequency", Freq, "CPUFreq", "GHz");
			break;
		}
	}
#endif // PLATFORM_ANDROID
}

float GFrameProCPUStatsUpdateRate = 0.00100; // default to 1000Hz (ie every frame)
static FAutoConsoleVariableRef CVarFrameProCPUStatsUpdateRate(
	TEXT("framepro.CPUStatsUpdateRate"),
	GFrameProCPUStatsUpdateRate,
	TEXT("Update rate in seconds for collecting CPU Stats (Default: 0.001)\n")
	TEXT("0 to disable."),
	ECVF_Default);

void FFrameProProfiler::FrameStart()
{
	static bool bFirstFrame = true;
#if PLATFORM_SWITCH
	if (bFirstFrame && GFrameProEnabled)
#else
	if (bFirstFrame)
#endif
	{
		UE_LOG(LogFramePro, Log, TEXT("FramePro Support Available"));

		FramePro::SendSessionInfo(WTEXT(""), *FString::Printf(TEXT("%d"), FEngineVersion::Current().GetChangelist()));

		FRAMEPRO_THREAD_ORDER(WTEXT("GameThread"));
		FRAMEPRO_THREAD_ORDER(WTEXT("RenderThread"));
		FRAMEPRO_THREAD_ORDER(WTEXT("RenderThread 1"));
		FRAMEPRO_THREAD_ORDER(WTEXT("RenderThread 2"));
		FRAMEPRO_THREAD_ORDER(WTEXT("RenderThread 3"));
		FRAMEPRO_THREAD_ORDER(WTEXT("RenderThread 4"));
		FRAMEPRO_THREAD_ORDER(WTEXT("RenderThread 5"));
		FRAMEPRO_THREAD_ORDER(WTEXT("RenderThread 6"));
		FRAMEPRO_THREAD_ORDER(WTEXT("RenderThread 7"));
		FRAMEPRO_THREAD_ORDER(WTEXT("RHIThread"));
		FRAMEPRO_THREAD_ORDER(WTEXT("TaskGraphThreadNP 0"));
		FRAMEPRO_THREAD_ORDER(WTEXT("TaskGraphThreadNP 1"));
		FRAMEPRO_THREAD_ORDER(WTEXT("TaskGraphThreadNP 2"));
		FRAMEPRO_THREAD_ORDER(WTEXT("TaskGraphThreadNP 3"));
		FRAMEPRO_THREAD_ORDER(WTEXT("AudioThread"));

		bFirstFrame = false;
	}

	if (GFrameProEnabled)
	{
		FramePro::FrameStart();

		static uint64 LastCollectionTime = FPlatformTime::Cycles64();
		uint64 CurrentTime = FPlatformTime::Cycles64();
		if (GFrameProCPUStatsUpdateRate > 0.0f)
		{
			bool bUpdateStats = ((FPlatformTime::ToSeconds(CurrentTime - LastCollectionTime) >= GFrameProCPUStatsUpdateRate));
			if (bUpdateStats)
			{
				LastCollectionTime = CurrentTime;
				SendCPUStats();
			}
		}
	}
}

void FFrameProProfiler::PushEvent()
{
	if (GFrameProEnabled)
	{
		FFrameProProfilerContext::Get().PushScope();
	}
}

void FFrameProProfiler::PushEvent(const ANSICHAR* Text)
{
	if (GFrameProEnabled)
	{
		FFrameProProfilerContext::Get().PushScope(Text);
	}
}

void FFrameProProfiler::PushEvent(const TCHAR* Text)
{
	if (GFrameProEnabled)
	{
		FFrameProProfilerContext::Get().PushScope(Text);
	}
}

void FFrameProProfiler::PopEvent()
{
	if (GFrameProEnabled)
	{
		FFrameProProfilerContext::Get().PopScope();
	}
}

void FFrameProProfiler::PopEvent(const TCHAR* Override)
{
	if (GFrameProEnabled)
	{
		FFrameProProfilerContext::Get().PopScope(Override);
	}
}

void FFrameProProfiler::PopEvent(const ANSICHAR* Override)
{
	if (GFrameProEnabled)
	{
		FFrameProProfilerContext::Get().PopScope(Override);
	}
}

static void StartFrameProRecording(const TArray< FString >& Args)
{
	FString RelPathName = FPaths::ProfilingDir() + TEXT("FramePro/");
	bool bSuccess = IFileManager::Get().MakeDirectory(*RelPathName, true); // ensure folder exists

	FString FilenameRoot = TEXT("Profile");	
	if (Args.Num() > 0 && Args[0].Len() > 0)
	{
		FilenameRoot = Args[0];
	}

	FString Filename = FString::Printf(TEXT("%s(%s).framepro_recording"), *FilenameRoot, *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	FString OutputFilename = RelPathName + Filename;

	UE_LOG(LogFramePro, Log, TEXT("--- Start Recording To File: %s"), *OutputFilename);
	
	FramePro::StartRecording(OutputFilename, false, 100 * 1024 * 1024); // 100 MB file
	FramePro::SetConditionalScopeMinTimeInMicroseconds(25);

	// Force this on, no events to record without it
	GFrameProEnabled = true;

	// Enable named events as well
	GCycleStatsShouldEmitNamedEvents = true;
}

static FAutoConsoleCommand StartFrameProRecordCommand(
	TEXT("framepro.startrec"),
	TEXT("Start FramePro recording"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&StartFrameProRecording)
);

static void StopFrameProRecording()
{
	FramePro::StopRecording();

	// Disable named events
	GCycleStatsShouldEmitNamedEvents = false;

	UE_LOG(LogFramePro, Log, TEXT("--- Stop Recording"));
}

static FAutoConsoleCommand StopFrameProRecordCommand(
	TEXT("framepro.stoprec"),
	TEXT("Stop FramePro recording"),
	FConsoleCommandDelegate::CreateStatic(&StopFrameProRecording)
);


#endif // FRAMEPRO_ENABLED
