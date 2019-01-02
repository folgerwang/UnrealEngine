// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterAppExit.h"
#include "DisplayClusterLog.h"
#include "Engine/GameEngine.h"

#if WITH_EDITOR
#include "Editor/UnrealEd/Public/UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#endif

FCriticalSection FDisplayClusterAppExit::InternalsSyncScope;

auto FDisplayClusterAppExit::ExitTypeToStr(ExitType type)
{
	switch (type)
	{
	case ExitType::KillImmediately:
		return TEXT("KILL");
	case ExitType::NormalSoft:
		return TEXT("UE4_soft");
	case ExitType::NormalForce:
		return TEXT("UE4_force");
	default:
		return TEXT("unknown");
	}
}

void FDisplayClusterAppExit::ExitApplication(ExitType exitType, const FString& strMsg)
{
	if (GEngine && GEngine->IsEditor())
	{
#if WITH_EDITOR
		UE_LOG(LogDisplayClusterModule, Log, TEXT("PIE STOP: %s application quit requested: %s"), ExitTypeToStr(exitType), *strMsg);
		GUnrealEd->RequestEndPlayMap();
#endif
		return;
	}
	else
	{
		FScopeLock lock(&InternalsSyncScope);

		// We process only first call. Thus we won't have a lot of requests from different socket threads.
		// We also will know the first requester which may be useful in step-by-step problem solving.
		static bool bRequestedBefore = false;
		if (bRequestedBefore == false || exitType == ExitType::KillImmediately)
		{
			bRequestedBefore = true;
			UE_LOG(LogDisplayClusterModule, Log, TEXT("%s application quit requested: %s"), ExitTypeToStr(exitType), *strMsg);

			GLog->Flush();

#if 0
			if (IsInGameThread())
			{
				GLog->FlushThreadedLogs();
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				TGuardValue<bool> GuardMainThreadBlockedOnRenderThread(GMainThreadBlockedOnRenderThread, true);
#endif
				SCOPE_CYCLE_COUNTER(STAT_PumpMessages);
				FPlatformMisc::PumpMessages(false);
			}
#endif

			switch (exitType)
			{
				case ExitType::KillImmediately:
				{
					FProcHandle hProc = FPlatformProcess::OpenProcess(FPlatformProcess::GetCurrentProcessId());
					FPlatformProcess::TerminateProc(hProc, true);
					break;
				}

				case ExitType::NormalSoft:
				{
//@todo: This is workaround for exit issue - crash on exit. Need to be checked on new UE versions.
// <ErrorMessage>Assertion failed: NumRemoved == 1 [File:D:\work\UE4.12.5.build\Engine\Source\Runtime\CoreUObject\Private\UObject\UObjectHash.cpp] [Line: 905] &nl;&nl;</ErrorMessage>
					FProcHandle hProc = FPlatformProcess::OpenProcess(FPlatformProcess::GetCurrentProcessId());
					FPlatformProcess::TerminateProc(hProc, true);
					break;
				}

				case ExitType::NormalForce:
				{
					FPlatformMisc::RequestExit(true);
					break;
				}

				default:
				{
					UE_LOG(LogDisplayClusterModule, Warning, TEXT("Unknown exit type requested"));
					break;
				}
			}
		}
	}
}
