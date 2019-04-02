// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Lumin/LuminLifecycle.h"
#include "EngineDefines.h"
#include "Misc/CallbackDevice.h"
#include "Async/TaskGraphInterfaces.h"
#include "Misc/CommandLine.h"

DEFINE_LOG_CATEGORY(LogLifecycle);

bool FLuminLifecycle::bIsEngineLoopInitComplete = false;
bool FLuminLifecycle::bIsAppPaused = false;
MLResult FLuminLifecycle::LifecycleState = MLResult_UnspecifiedFailure;
MLLifecycleCallbacks FLuminLifecycle::LifecycleCallbacks = { nullptr, nullptr, nullptr, nullptr, nullptr };
TArray<FString> FLuminLifecycle::PendingStartupArgs;

void FLuminLifecycle::Initialize()
{
	if (IsLifecycleInitialized())
	{
		return;
	}

	LifecycleCallbacks.on_stop = Stop_Handler;
	LifecycleCallbacks.on_pause = Pause_Handler;
	LifecycleCallbacks.on_resume = Resume_Handler;
	LifecycleCallbacks.on_unload_resources = UnloadResources_Handler;
	LifecycleCallbacks.on_new_initarg = OnNewInitArgs_Handler;

	LifecycleState = MLLifecycleInit(&LifecycleCallbacks, nullptr);

	FCoreDelegates::OnFEngineLoopInitComplete.AddStatic(FLuminLifecycle::OnFEngineLoopInitComplete_Handler);

	// TODO: confirm this comment for ml_lifecycle.
	// There's a known issue where ck_lifecycle_init will fail to initialize if the debugger is attached.
	// Ideally, this should assert since the app won't be able to react to events correctly.
	if (LifecycleState != MLResult_Ok)
	{
		FPlatformMisc::LowLevelOutputDebugString(TEXT("Lifecycle system failed to initialize! App may not suspend, resume, or teriminate correctly."));
	}
	else
	{
		// It is possible that FLuminLifecycle::Initialize() is called before LaunchLumin::InitCommandLine().
		// So initialize command line here to take in args passed via mldb launch.
		if (!FCommandLine::IsInitialized())
		{
			// initialize the command line to an empty string
			FCommandLine::Set(TEXT(""));
		}
		OnNewInitArgs_Handler(nullptr);
	}
}

bool FLuminLifecycle::IsLifecycleInitialized()
{
	return (LifecycleState == MLResult_Ok);
}

void FLuminLifecycle::Stop_Handler(void* ApplicationContext)
{
	UE_LOG(LogLifecycle, Log, TEXT("FLuminLifecycle : The application is being stopped by the system."));

	if (FTaskGraphInterface::IsRunning())
	{
		FGraphEventRef WillTerminateTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
		{
			FCoreDelegates::ApplicationWillTerminateDelegate.Broadcast();
		}, TStatId(), nullptr, ENamedThreads::GameThread);
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(WillTerminateTask);
	}

	FPlatformMisc::RequestExit(false);
}

void FLuminLifecycle::Pause_Handler(void* ApplicationContext)
{
	UE_LOG(LogLifecycle, Log, TEXT("FLuminLifecycle : The application is being paused / suspended by the system."));

	// TODO: confirm this comment for ml_lifecycle.
	// Currently, the lifecycle service can invoke "pause" multiple times, so we need to guard against it.
	if (!bIsAppPaused)
	{
		if (FTaskGraphInterface::IsRunning())
		{
			FGraphEventRef DeactivateTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
			{
				FCoreDelegates::ApplicationWillDeactivateDelegate.Broadcast();
			}, TStatId(), nullptr, ENamedThreads::GameThread);
			FGraphEventRef EnterBackgroundTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
			{
				FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
			}, TStatId(), DeactivateTask, ENamedThreads::GameThread);
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(EnterBackgroundTask);
		}

		bIsAppPaused = true;
	}
}

void FLuminLifecycle::Resume_Handler(void* ApplicationContext)
{
	UE_LOG(LogLifecycle, Log, TEXT("FLuminLifecycle : The application is being resumed after being suspended."));

	// TODO: confirm this comment for ml_lifecycle.
	// Currently, the lifecycle service can invoke "resume" multiple times, so we need to guard against it.
	if (bIsAppPaused)
	{
		if (FTaskGraphInterface::IsRunning())
		{
			FGraphEventRef EnterForegroundTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
			{
				FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
			}, TStatId(), nullptr, ENamedThreads::GameThread);

			FGraphEventRef ReactivateTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
			{
				FCoreDelegates::ApplicationHasReactivatedDelegate.Broadcast();
			}, TStatId(), EnterForegroundTask, ENamedThreads::GameThread);
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(ReactivateTask);
		}

		bIsAppPaused = false;
	}
}

void FLuminLifecycle::UnloadResources_Handler(void* ApplicationContext)
{
	UE_LOG(LogLifecycle, Log, TEXT("FLuminLifecycle : The application is being asked to free up cached resources by the system."));

	if (FTaskGraphInterface::IsRunning())
	{
		FGraphEventRef UnloadResourcesTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
		{
			FCoreDelegates::ApplicationShouldUnloadResourcesDelegate.Broadcast();
		}, TStatId(), nullptr, ENamedThreads::GameThread);
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(UnloadResourcesTask);
	}
}

void FLuminLifecycle::OnNewInitArgs_Handler(void* ApplicationContext)
{
	MLLifecycleInitArgList* InitArgList = nullptr;
	MLResult Result = MLLifecycleGetInitArgList(&InitArgList);
	if (Result == MLResult_Ok && InitArgList != nullptr)
	{
		int64_t InitArgCount = 0;
		Result = MLLifecycleGetInitArgListLength(InitArgList, &InitArgCount);
		if (Result == MLResult_Ok && InitArgCount > 0)
		{
			TArray<FString> InitArgsArray;
			for (int64 i = 0; i < InitArgCount; ++i)
			{
				const MLLifecycleInitArg* InitArg = nullptr;
				Result = MLLifecycleGetInitArgByIndex(InitArgList, i, &InitArg);
				if (Result == MLResult_Ok && InitArg != nullptr)
				{
					const char* Arg = nullptr;
					Result = MLLifecycleGetInitArgUri(InitArg, &Arg);
					if (Result == MLResult_Ok && Arg != nullptr)
					{
						// Start with a space because the command line already in place may not have any trailing spaces.
						FString ArgStr = TEXT(" ");
						ArgStr.Append(UTF8_TO_TCHAR(Arg));
						ArgStr.TrimEndInline();
						FCommandLine::Append(*ArgStr);
					}
					InitArgsArray.Add(UTF8_TO_TCHAR(Arg));
				}
			}

			if (FTaskGraphInterface::IsRunning() && bIsEngineLoopInitComplete)
			{
				FGraphEventRef StartupArgumentsTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
				{
					FCoreDelegates::ApplicationReceivedStartupArgumentsDelegate.Broadcast(InitArgsArray);
				}, TStatId(), nullptr, ENamedThreads::GameThread);
				FTaskGraphInterface::Get().WaitUntilTaskCompletes(StartupArgumentsTask);
			}
			else
			{
				PendingStartupArgs = InitArgsArray;
			}
		}
	}
}

void FLuminLifecycle::OnFEngineLoopInitComplete_Handler()
{
	bIsEngineLoopInitComplete = true;

	if (PendingStartupArgs.Num() > 0 && FTaskGraphInterface::IsRunning())
	{
		FPlatformMisc::LowLevelOutputDebugString(TEXT("FLuminLifecycle :: Firing startup args..."));
		FGraphEventRef StartupArgumentsTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
		{
			FCoreDelegates::ApplicationReceivedStartupArgumentsDelegate.Broadcast(PendingStartupArgs);
		}, TStatId(), nullptr, ENamedThreads::GameThread);
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(StartupArgumentsTask);
		PendingStartupArgs.Empty();
	}
}
