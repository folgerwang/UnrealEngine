// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#include "HAL/ThreadHeartBeat.h"
#include "HAL/PlatformStackWalk.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "Logging/LogMacros.h"
#include "CoreGlobals.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadManager.h"
#include "Misc/ScopeLock.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "HAL/ExceptionHandling.h"
#include "Stats/Stats.h"
#include "Async/TaskGraphInterfaces.h"

// When enabled, the heart beat thread will call abort() when a hang
// is detected, rather than performing stack back-traces and logging.
#define MINIMAL_FATAL_HANG_DETECTION	((PLATFORM_PS4 || PLATFORM_XBOXONE) && 1)

// When enabled, the heart beat thread will call abort() when a hang
// is detected, rather than performing stack back-traces and logging.
#define MINIMAL_FATAL_HANG_DETECTION	((PLATFORM_PS4 || PLATFORM_XBOXONE) && 1)

#ifndef UE_ASSERT_ON_HANG
#define UE_ASSERT_ON_HANG 0
#endif

#ifndef WALK_STACK_ON_HITCH_DETECTED
	#define WALK_STACK_ON_HITCH_DETECTED 0
#endif

FThreadHeartBeat::FThreadHeartBeat()
	: Thread(nullptr)
	, bReadyToCheckHeartbeat(false)
	, ConfigHangDuration(0)
	, CurrentHangDuration(0)
	, HangDurationMultiplier(1.0)
	, LastHangCallstackCRC(0)
	, LastHungThreadId(0)
{
	InitSettings();

	const bool bAllowThreadHeartBeat = FPlatformMisc::AllowThreadHeartBeat() && ConfigHangDuration > 0.0;

	// We don't care about programs for now so no point in spawning the extra thread
#if USE_HANG_DETECTION
	if (bAllowThreadHeartBeat && FPlatformProcess::SupportsMultithreading())
	{
		Thread = FRunnableThread::Create(this, TEXT("FHeartBeatThread"), 0, TPri_AboveNormal);

		FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(this, &FThreadHeartBeat::OnApplicationWillEnterBackground);
		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FThreadHeartBeat::OnApplicationEnteredForeground);
	}
#endif

	if (!bAllowThreadHeartBeat)
	{
		// Disable the check
		ConfigHangDuration = 0.0;
	}
}

FThreadHeartBeat::~FThreadHeartBeat()
{
#if USE_HANG_DETECTION
	// Intentionally not unbinding these delegates because this object is a static singleton and the delegates may be destructed before this object is
	// This is fine, since both the delegate and this object are both destroyed at static destruction time, so there is no need to unregister
	//FCoreDelegates::ApplicationHasEnteredForegroundDelegate.RemoveAll(this);
	//FCoreDelegates::ApplicationWillEnterBackgroundDelegate.RemoveAll(this);
#endif

	delete Thread;
	Thread = nullptr;
}

FThreadHeartBeat* FThreadHeartBeat::Singleton = nullptr;

FThreadHeartBeat& FThreadHeartBeat::Get()
{
	struct FInitHelper
	{
		FThreadHeartBeat* Instance;

		FInitHelper()
		{
			check(!Singleton);
			Instance = new FThreadHeartBeat();
			Singleton = Instance;
		}

		~FInitHelper()
		{
			Singleton = nullptr;

			delete Instance;
			Instance = nullptr;
		}
	};

	// Use a function static helper to ensure creation
	// of the FThreadHeartBeat instance is thread safe.
	static FInitHelper Helper;
	return *Helper.Instance;
}

FThreadHeartBeat* FThreadHeartBeat::GetNoInit()
{
	return Singleton;
}

	//~ Begin FRunnable Interface.
bool FThreadHeartBeat::Init()
{
	return true;
}

uint32 FThreadHeartBeat::Run()
{
	bool InHungState = false;

#if USE_HANG_DETECTION
	while (StopTaskCounter.GetValue() == 0 && !GIsRequestingExit)
	{
		double HangDuration;
		uint32 ThreadThatHung = CheckHeartBeat(HangDuration);

		if (ThreadThatHung == FThreadHeartBeat::InvalidThreadId)
		{
			InHungState = false;
		}
		else if (InHungState == false)
		{
#if MINIMAL_FATAL_HANG_DETECTION

			InHungState = true;
			LastHungThreadId = ThreadThatHung;

			// We want to avoid all memory allocations if a hang is detected.
			// Force a crash in a way that will generate a crash report.
			FPlatformMisc::RaiseException(0xe0000001);

#else // MINIMAL_FATAL_HANG_DETECTION == 0

			// Only want to call this once per hang (particularly if we're just ensuring).
			InHungState = true;

			const SIZE_T StackTraceSize = 65535;
			ANSICHAR* StackTrace = (ANSICHAR*)GMalloc->Malloc(StackTraceSize);
			StackTrace[0] = 0;
			// Walk the stack and dump it to the allocated memory. This process usually allocates a lot of memory.
			FPlatformStackWalk::ThreadStackWalkAndDump(StackTrace, StackTraceSize, 0, ThreadThatHung);

			// First verify we're not reporting the same hang over and over again
			uint32 CallstackCRC = FCrc::StrCrc32(StackTrace);
			if (CallstackCRC != LastHangCallstackCRC || ThreadThatHung != LastHungThreadId)
			{
				LastHangCallstackCRC = CallstackCRC;
				LastHungThreadId = ThreadThatHung;

				FString StackTraceText(StackTrace);
				TArray<FString> StackLines;
				StackTraceText.ParseIntoArrayLines(StackLines);

				// Dump the callstack and the thread name to log
				FString ThreadName(ThreadThatHung == GGameThreadId ? TEXT("GameThread") : FThreadManager::Get().GetThreadName(ThreadThatHung));
				if (ThreadName.IsEmpty())
				{
					ThreadName = FString::Printf(TEXT("unknown thread (%u)"), ThreadThatHung);
				}
				UE_LOG(LogCore, Error, TEXT("Hang detected on %s (thread hasn't sent a heartbeat for %.2llf seconds):"), *ThreadName,  HangDuration);
				for (FString& StackLine : StackLines)
				{
					UE_LOG(LogCore, Error, TEXT("  %s"), *StackLine);
				}

				// Assert (on the current thread unfortunately) with a trimmed stack.
				FString StackTrimmed;
				for (int32 LineIndex = 0; LineIndex < StackLines.Num() && StackTrimmed.Len() < 512; ++LineIndex)
				{
					StackTrimmed += TEXT("  ");
					StackTrimmed += StackLines[LineIndex];
					StackTrimmed += LINE_TERMINATOR;
				}

				const FString ErrorMessage = FString::Printf(TEXT("Hang detected on %s:%s%s%sCheck log for full callstack."), *ThreadName, LINE_TERMINATOR, *StackTrimmed, LINE_TERMINATOR);
#if UE_ASSERT_ON_HANG
				UE_LOG(LogCore, Fatal, TEXT("%s"), *ErrorMessage);
#else
				UE_LOG(LogCore, Error, TEXT("%s"), *ErrorMessage);

#if PLATFORM_DESKTOP
				GLog->PanicFlushThreadedLogs();
				// GErrorMessage here is very unfortunate but it's used internally by the crash context code.
				FCString::Strcpy(GErrorMessage, ARRAY_COUNT(GErrorMessage), *ErrorMessage);
				// Skip macros and FDebug, we always want this to fire
				NewReportEnsure(*ErrorMessage);
				GErrorMessage[0] = '\0';
#endif // PLATFORM_DESKTOP

#endif // UE_ASSERT_ON_HANG == 0
			}

			GMalloc->Free(StackTrace);
#endif // MINIMAL_FATAL_HANG_DETECTION == 0
		}
		if (StopTaskCounter.GetValue() == 0 && !GIsRequestingExit)
		{
			FPlatformProcess::SleepNoStats(0.5f);
		}
	}
#endif // USE_HANG_DETECTION

	return 0;
}

void FThreadHeartBeat::Stop()
{
	bReadyToCheckHeartbeat = false;
	StopTaskCounter.Increment();
}

void FThreadHeartBeat::Start()
{
	bReadyToCheckHeartbeat = true;
}

void FThreadHeartBeat::InitSettings()
{
	// Default to 25 seconds if not overridden in config.
	double NewHangDuration = 25.0;

	if (GConfig)
	{
		GConfig->GetDouble(TEXT("Core.System"), TEXT("HangDuration"), NewHangDuration, GEngineIni);

		const double MinHangDuration = 5.0;
		if (NewHangDuration > 0.0 && NewHangDuration < 5.0)
		{
			UE_LOG(LogCore, Warning, TEXT("HangDuration is set to %.4llfs which is a very short time for hang detection. Changing to %.2llfs."), NewHangDuration, MinHangDuration);
			NewHangDuration = MinHangDuration;
		}
	}

	ConfigHangDuration = NewHangDuration;
	CurrentHangDuration = ConfigHangDuration * HangDurationMultiplier;
}

void FThreadHeartBeat::OnApplicationWillEnterBackground()
{
	// Suspend all threads
#if USE_HANG_DETECTION
	FScopeLock HeartBeatLock(&HeartBeatCritical);
	for (TPair<uint32, FHeartBeatInfo>& LastHeartBeat : ThreadHeartBeat)
	{
		FHeartBeatInfo& HeartBeatInfo = LastHeartBeat.Value;
		HeartBeatInfo.SuspendedCount++;
	}
#endif
}

void FThreadHeartBeat::OnApplicationEnteredForeground()
{
	// Resume all threads
#if USE_HANG_DETECTION
	FScopeLock HeartBeatLock(&HeartBeatCritical);
	for (TPair<uint32, FHeartBeatInfo>& LastHeartBeat : ThreadHeartBeat)
	{
		FHeartBeatInfo& HeartBeatInfo = LastHeartBeat.Value;
		check(HeartBeatInfo.SuspendedCount > 0);
		if (--HeartBeatInfo.SuspendedCount == 0)
		{
			HeartBeatInfo.LastHeartBeatTime = FPlatformTime::Seconds();
		}
	}
#endif
}

void FThreadHeartBeat::HeartBeat(bool bReadConfig)
{
#if USE_HANG_DETECTION
	// disable on platforms that don't start the thread
	if (FPlatformMisc::AllowThreadHeartBeat() == false)
	{
		return;
	}

	uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();
	FScopeLock HeartBeatLock(&HeartBeatCritical);
	if (bReadConfig && ThreadId == GGameThreadId && GConfig)
	{
		InitSettings();
	}
	FHeartBeatInfo& HeartBeatInfo = ThreadHeartBeat.FindOrAdd(ThreadId);
	HeartBeatInfo.LastHeartBeatTime = FPlatformTime::Seconds();
	HeartBeatInfo.HangDuration = CurrentHangDuration;
#endif
}

uint32 FThreadHeartBeat::CheckHeartBeat(double& OutHangDuration)
{
	// Editor and debug builds run too slow to measure them correctly
#if USE_HANG_DETECTION
	static bool bDisabled = FParse::Param(FCommandLine::Get(), TEXT("nothreadtimeout"));

	bool CheckBeats = ConfigHangDuration > 0.0
		&& bReadyToCheckHeartbeat
		&& !GIsRequestingExit
		&& !FPlatformMisc::IsDebuggerPresent()
		&& !bDisabled;

	if (CheckBeats)
	{
		// Check heartbeat for all threads and return thread ID of the thread that hung.
		const double CurrentTime = FPlatformTime::Seconds();
		FScopeLock HeartBeatLock(&HeartBeatCritical);
		for (TPair<uint32, FHeartBeatInfo>& LastHeartBeat : ThreadHeartBeat)
		{
			FHeartBeatInfo& HeartBeatInfo =  LastHeartBeat.Value;
			if (HeartBeatInfo.SuspendedCount == 0 && (CurrentTime - HeartBeatInfo.LastHeartBeatTime) > HeartBeatInfo.HangDuration)
			{
				HeartBeatInfo.LastHeartBeatTime = CurrentTime;
				OutHangDuration = HeartBeatInfo.HangDuration;
				return LastHeartBeat.Key;
			}
		}
	}
#endif
	return InvalidThreadId;
}

void FThreadHeartBeat::KillHeartBeat()
{
#if USE_HANG_DETECTION
	uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();
	FScopeLock HeartBeatLock(&HeartBeatCritical);
	ThreadHeartBeat.Remove(ThreadId);
#endif
}

void FThreadHeartBeat::SuspendHeartBeat()
{
#if USE_HANG_DETECTION
	uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();
	FScopeLock HeartBeatLock(&HeartBeatCritical);
	FHeartBeatInfo* HeartBeatInfo = ThreadHeartBeat.Find(ThreadId);
	if (HeartBeatInfo)
	{
		HeartBeatInfo->SuspendedCount++;
	}
#endif
}
void FThreadHeartBeat::ResumeHeartBeat()
{
#if USE_HANG_DETECTION
	uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();
	FScopeLock HeartBeatLock(&HeartBeatCritical);
	FHeartBeatInfo* HeartBeatInfo = ThreadHeartBeat.Find(ThreadId);
	if (HeartBeatInfo)
	{
		check(HeartBeatInfo->SuspendedCount > 0);
		if (--HeartBeatInfo->SuspendedCount == 0)
		{
			HeartBeatInfo->LastHeartBeatTime = FPlatformTime::Seconds();
		}
	}
#endif
}

bool FThreadHeartBeat::IsBeating()
{
	uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();
	FScopeLock HeartBeatLock(&HeartBeatCritical);
	FHeartBeatInfo* HeartBeatInfo = ThreadHeartBeat.Find(ThreadId);
	if (HeartBeatInfo && HeartBeatInfo->SuspendedCount == 0)
	{
		return true;
	}

	return false;
}

void FThreadHeartBeat::SetDurationMultiplier(double NewMultiplier)
{
	check(IsInGameThread());

#if USE_HANG_DETECTION
	if (NewMultiplier < 1.0)
	{
		UE_LOG(LogCore, Warning, TEXT("Cannot set the hang duration multiplier to less than 1.0. Specified value was %.4llfs."), NewMultiplier);
		NewMultiplier = 1.0;
	}

	FScopeLock HeartBeatLock(&HeartBeatCritical);

	HangDurationMultiplier = NewMultiplier;
	InitSettings();

	UE_LOG(LogCore, Display, TEXT("Setting hang detector multiplier to %.4llfs. New hang duration: %.4llfs."), NewMultiplier, CurrentHangDuration);

	// Update the existing thread's hang durations.
	for (TPair<uint32, FHeartBeatInfo>& Pair : ThreadHeartBeat)
	{
		// Only increase existing thread's heartbeats.
		// We don't want to decrease here, otherwise reducing the multiplier could cause a false detection.
		// Threads will pick up a smaller hang duration the next time they call HeartBeat().
		if (Pair.Value.HangDuration < CurrentHangDuration)
		{
			Pair.Value.HangDuration = CurrentHangDuration;
		}
	}
#endif
}

FGameThreadHitchHeartBeat::FGameThreadHitchHeartBeat()
	: Thread(nullptr)
	, HangDuration(-1.f)
	, bWalkStackOnHitch(false)
	, FirstStartTime(0.0)
	, FrameStartTime(0.0)
	, LastReportTime(0.0)
	, SuspendedCount(0)
{
	// We don't care about programs for now so no point in spawning the extra thread
#if USE_HITCH_DETECTION
	InitSettings();

	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(this, &FGameThreadHitchHeartBeat::OnApplicationWillEnterBackground);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FGameThreadHitchHeartBeat::OnApplicationEnteredForeground);
#endif
}

FGameThreadHitchHeartBeat::~FGameThreadHitchHeartBeat()
{
#if USE_HITCH_DETECTION
	// Intentionally not unbinding these delegates because this object is a static singleton and the delegates may be destructed before this object is
	// This is fine, since both the delegate and this object are both destroyed at static destruction time, so there is no need to unregister
	//FCoreDelegates::ApplicationHasEnteredForegroundDelegate.RemoveAll(this);
	//FCoreDelegates::ApplicationWillEnterBackgroundDelegate.RemoveAll(this);
#endif

	delete Thread;
	Thread = nullptr;
}

FGameThreadHitchHeartBeat& FGameThreadHitchHeartBeat::Get()
{
	static FGameThreadHitchHeartBeat Singleton;
	return Singleton;
}

//~ Begin FRunnable Interface.
bool FGameThreadHitchHeartBeat::Init()
{
	return true;
}

void FGameThreadHitchHeartBeat::InitSettings()
{
#if USE_HITCH_DETECTION
	static bool bFirst = true;
	static bool bHasCmdLine = false;
	static float CmdLine_HangDuration = 0.0f;
	static bool CmdLine_StackWalk = false;

	if (bFirst)
	{
		bHasCmdLine = FParse::Value(FCommandLine::Get(), TEXT("hitchdetection="), CmdLine_HangDuration);
		CmdLine_StackWalk = FParse::Param(FCommandLine::Get(), TEXT("hitchdetectionstackwalk"));
		bFirst = false;
	}

	if (bHasCmdLine)
	{
		// Command line takes priority over config
		HangDuration = CmdLine_HangDuration;
		bWalkStackOnHitch = CmdLine_StackWalk;
	}
	else
	{
		float Config_Duration = -1.0f;
		bool Config_StackWalk = false;

		// Read from config files
		bool bReadFromConfig = false;
		if (GConfig)
		{
			bReadFromConfig |= GConfig->GetFloat(TEXT("Core.System"), TEXT("GameThreadHeartBeatHitchDuration"), Config_Duration, GEngineIni);
			bReadFromConfig |= GConfig->GetBool(TEXT("Core.System"), TEXT("GameThreadHeartBeatStackWalk"), Config_StackWalk, GEngineIni);
		}

		if (bReadFromConfig)
		{
			HangDuration = Config_Duration;
			bWalkStackOnHitch = Config_StackWalk;
		}
		else
		{
			// No config provided. Use defaults to disable.
			HangDuration = -1.0f;
			bWalkStackOnHitch = false;
		}
	}

	// Start the heart beat thread if it hasn't already been started.
	if (Thread == nullptr && FPlatformProcess::SupportsMultithreading() && HangDuration > 0)
	{
		Thread = FRunnableThread::Create(this, TEXT("FGameThreadHitchHeartBeat"), 0, TPri_AboveNormal);
	}
#endif
}

void FGameThreadHitchHeartBeat::OnApplicationWillEnterBackground()
{
	SuspendHeartBeat();
}

void FGameThreadHitchHeartBeat::OnApplicationEnteredForeground()
{
	ResumeHeartBeat();
}

uint32 FGameThreadHitchHeartBeat::Run()
{
#if USE_HITCH_DETECTION
#if WALK_STACK_ON_HITCH_DETECTED
	if (bWalkStackOnHitch)
	{
		// Perform a stack trace immediately, so we pay the first time setup cost
		// during engine boot, rather than during game play. The results are discarded.
#if LOOKUP_SYMBOLS_IN_HITCH_STACK_WALK
		FPlatformStackWalk::ThreadStackWalkAndDump(StackTrace, StackTraceSize, 0, GGameThreadId);
#else
		FPlatformStackWalk::CaptureThreadStackBackTrace(GGameThreadId, StackTrace, MaxStackDepth);
#endif
	}
#endif

	while (StopTaskCounter.GetValue() == 0 && !GIsRequestingExit)
	{
		if (!GIsRequestingExit && !GHitchDetected && UE_LOG_ACTIVE(LogCore, Error)) // && !FPlatformMisc::IsDebuggerPresent())
		{
			double LocalFrameStartTime;
			float LocalHangDuration;
			{
				FScopeLock HeartBeatLock(&HeartBeatCritical);
				LocalFrameStartTime = FrameStartTime;
				LocalHangDuration = HangDuration;
			}
			if (LocalFrameStartTime > 0.0 && LocalHangDuration > 0.0f && SuspendedCount == 0)
			{
				const double CurrentTime = FPlatformTime::Seconds();
				if (CurrentTime - LastReportTime > 60.0 && float(CurrentTime - LocalFrameStartTime) > LocalHangDuration)
				{
					if (StopTaskCounter.GetValue() == 0)
					{
						GHitchDetected = true;
						UE_LOG(LogCore, Error, TEXT("Hitch detected on gamethread (frame hasn't finished for %8.2fms):"), float(CurrentTime - LocalFrameStartTime) * 1000.0f);
						//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Hitch detected on GameThread (frame hasn't finished for %8.2fms):"), float(CurrentTime - LocalFrameStartTime) * 1000.0f);

#if WALK_STACK_ON_HITCH_DETECTED
						if (bWalkStackOnHitch)
						{
							double StartTime = FPlatformTime::Seconds();

#if LOOKUP_SYMBOLS_IN_HITCH_STACK_WALK
							// Walk the stack and dump it to the temp buffer. This process usually allocates a lot of memory.
							StackTrace[0] = 0;
							FPlatformStackWalk::ThreadStackWalkAndDump(StackTrace, StackTraceSize, 0, GGameThreadId);
							FString StackTraceText(StackTrace);
							TArray<FString> StackLines;
							StackTraceText.ParseIntoArrayLines(StackLines);

							UE_LOG(LogCore, Error, TEXT("------Stack start"));
							for (FString& StackLine : StackLines)
							{
								UE_LOG(LogCore, Error, TEXT("  %s"), *StackLine);
							}
							UE_LOG(LogCore, Error, TEXT("------Stack end"));

#else // LOOKUP_SYMBOLS_IN_HITCH_STACK_WALK == 0

							// Only do a thread stack back trace and print the raw addresses to the log.
							uint32 Depth = FPlatformStackWalk::CaptureThreadStackBackTrace(GGameThreadId, StackTrace, MaxStackDepth);

							UE_LOG(LogCore, Error, TEXT("------Stack start"));
							for (uint32 Index = 0; Index < Depth; ++Index)
							{
								UE_LOG(LogCore, Error, TEXT("  0x%016llx"), StackTrace[Index]);
							}
							UE_LOG(LogCore, Error, TEXT("------Stack end"));

#endif // LOOKUP_SYMBOLS_IN_HITCH_STACK_WALK == 0


							double EndTime = FPlatformTime::Seconds();
							double Duration = EndTime - StartTime;
							UE_LOG(LogCore, Error, TEXT(" ## Stack tracing took %f seconds."), Duration);
						}
#endif
						UE_LOG(LogCore, Error, TEXT("Leaving hitch detector (+%8.2fms)"), float(FPlatformTime::Seconds() - LocalFrameStartTime) * 1000.0f);
					}
				}
			}
		}
		if (StopTaskCounter.GetValue() == 0 && !GIsRequestingExit)
		{
			FPlatformProcess::SleepNoStats(0.008f); // check every 8ms
		}
	}
#endif
	return 0;
}

void FGameThreadHitchHeartBeat::Stop()
{
	StopTaskCounter.Increment();
}

void FGameThreadHitchHeartBeat::FrameStart(bool bSkipThisFrame)
{
#if USE_HITCH_DETECTION
	check(IsInGameThread());
	FScopeLock HeartBeatLock(&HeartBeatCritical);
	// Grab this everytime to handle hotfixes
	if (!bSkipThisFrame)
	{
		InitSettings();
	}
	double Now = FPlatformTime::Seconds();
	if (FirstStartTime == 0.0)
	{
		FirstStartTime = Now;
	}
	//if (Now - FirstStartTime > 60.0)
	{
		FrameStartTime = bSkipThisFrame ? 0.0 : Now;
	}
#if !ENABLE_STATNAMEDEVENTS && defined(USE_LIGHTWEIGHT_STATS_FOR_HITCH_DETECTION) && USE_LIGHTWEIGHT_STATS_FOR_HITCH_DETECTION && USE_HITCH_DETECTION
	if (GHitchDetected)
	{
		TFunction<void(ENamedThreads::Type CurrentThread)> Broadcast =
			[this](ENamedThreads::Type MyThread)
		{
			FString ThreadString(FPlatformTLS::GetCurrentThreadId() == GGameThreadId ? TEXT("GameThread") : FThreadManager::Get().GetThreadName(FPlatformTLS::GetCurrentThreadId()));
			UE_LOG(LogCore, Error, TEXT("FGameThreadHitchHeartBeat Flushed Thread [%s]"), *ThreadString);
		};
		// Skip task threads we will catch the wait for them
		FTaskGraphInterface::BroadcastSlow_OnlyUseForSpecialPurposes(false, false, Broadcast);
	}
#endif
	GHitchDetected = false;
#endif
}

void FGameThreadHitchHeartBeat::SuspendHeartBeat()
{
#if USE_HITCH_DETECTION
	FPlatformAtomics::InterlockedIncrement(&SuspendedCount);
#endif
}
void FGameThreadHitchHeartBeat::ResumeHeartBeat()
{
#if USE_HITCH_DETECTION
	check(SuspendedCount > 0);
	if (FPlatformAtomics::InterlockedDecrement(&SuspendedCount) == 0)
	{
		FrameStart(true);
	}
#endif
}

double FGameThreadHitchHeartBeat::GetFrameStartTime()
{
	return FrameStartTime;
}