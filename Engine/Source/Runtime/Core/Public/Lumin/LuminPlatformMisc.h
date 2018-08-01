// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Android/AndroidMisc.h"
#include "Logging/LogVerbosity.h"

struct CORE_API FLuminPlatformMisc : public FAndroidMisc
{
	static void InitLifecycle();

	/** 
	 * Platform overrides 
	 */
	static void PlatformPreInit();
	static bool AllowRenderThread();
	static bool SupportsLocalCaching();
	static bool SupportsMessaging();
	static void GetValidTargetPlatforms(TArray<FString>& TargetPlatformNames);
	static bool GetOverrideResolution(int32 &ResX, int32& ResY);

	static bool GetUseVirtualJoysticks()
	{
		return false;
	}

#if !UE_BUILD_SHIPPING
	static bool IsDebuggerPresent();
#endif

	/** Break into debugger. Returning false allows this function to be used in conditionals. */
	FORCEINLINE static bool DebugBreakReturningFalse()
	{
#if !UE_BUILD_SHIPPING
		UE_DEBUG_BREAK();
#endif
		return false;
	}

	/** Prompts for remote debugging if debugger is not attached. Regardless of result, breaks into debugger afterwards. Returns false for use in conditionals. */
	static FORCEINLINE bool DebugBreakAndPromptForRemoteReturningFalse(bool bIsEnsure = false)
	{
#if !UE_BUILD_SHIPPING
		if (!IsDebuggerPresent())
		{
			PromptForRemoteDebugging(bIsEnsure);
		}
		UE_DEBUG_BREAK();
#endif
		return false;
	}

	FORCEINLINE static void MemoryBarrier()
	{
		__sync_synchronize();
	}

	FORCEINLINE static const TCHAR* GetNullRHIShaderFormat()
	{
		return TEXT("GLSL_ES2");
	}

	static void LowLevelOutputDebugString(const TCHAR *Message);
	static void LocalPrint(const TCHAR* Str);

	static void LowLevelOutputDebugStringWithVerbosity(const TCHAR *Message, ELogVerbosity::Type Verbosity);
	static void VARARGS LowLevelOutputDebugStringfWithVerbosity(ELogVerbosity::Type Verbosity, const TCHAR *Format, ...);
	static void LocalPrintWithVerbosity(const TCHAR *Message, ELogVerbosity::Type Verbosity);

	/** Android overrides. */
	static bool ShouldUseVulkan();
	static bool ShouldUseDesktopVulkan();

	/** Lumin specific. */
	static bool ShouldUseDesktopOpenGL();
	static const FString& GetApplicationWritableDirectoryPath();
	static const FString& GetApplicationPackageDirectoryPath();
	static const FString& GetApplicationApplicationPackageName();
	static const FString& GetApplicationComponentName();

private:
	static void InitApplicationPaths();

private:
	static FString WritableDirPath;
	static FString PackageDirPath;
	static FString PackageName;
	static FString ComponentName;

	static bool ApplicationPathsInitialized;
};

typedef FLuminPlatformMisc FPlatformMisc;
