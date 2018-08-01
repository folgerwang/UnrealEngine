// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
#include <ml_lifecycle.h>
#include "Logging/LogMacros.h"

class CORE_API FLuminLifecycle
{
public:
	static void Initialize();
	static bool IsLifecycleInitialized();
private:
	static void Stop_Handler(void* ApplicationContext);
	static void Pause_Handler(void* ApplicationContext);
	static void Resume_Handler(void* ApplicationContext);
	static void UnloadResources_Handler(void* ApplicationContext);
	// To use lifecycle init args, launch the app using -
	// mldb launch -i "-arg1=value1 -arg2=value2" <package_name>
	static void OnNewInitArgs_Handler(void* ApplicationContext);

	static void OnFEngineLoopInitComplete_Handler();

private:
	static bool bIsEngineLoopInitComplete;
	static bool bIsAppPaused;
	static MLLifecycleErrorCode LifecycleState;
	static MLLifecycleCallbacks LifecycleCallbacks;
	static TArray<FString> PendingStartupArgs;
};

DECLARE_LOG_CATEGORY_EXTERN(LogLifecycle, Log, All);
