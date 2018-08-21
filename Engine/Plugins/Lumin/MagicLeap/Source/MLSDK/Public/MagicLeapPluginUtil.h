// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

#if PLATFORM_WINDOWS
#define ML_INCLUDES_START THIRD_PARTY_INCLUDES_START \
	__pragma(warning(disable: 4201)) /* warning C4201: nonstandard extension used: nameless struct/union */
#else
#define ML_INCLUDES_START THIRD_PARTY_INCLUDES_START
#endif

#define ML_INCLUDES_END THIRD_PARTY_INCLUDES_END

DEFINE_LOG_CATEGORY_STATIC(LogMagicLeapAPISetup, Display, All);

/** Utility class to load the correct MLSDK libs depedning on the path set to the MLSDK package and whether or not we want to use MLremote / Zero Iteration*/
class FMagicLeapAPISetup
{
public:
	/** Reads the config file and environment variable for the MLSDK package path and sets up the correct environment to load the libraries from. */
	void Startup(bool CheckForVDZILibraries = false)
	{
#if !PLATFORM_LUMIN
		// We search various places for the ML API DLLs to support loading alternate
		// implementations. For example to use VDZI on PC platforms.
		// Public MLSDK path.
		FString MLSDK;

		// Give preference to the config setting.
		FString MLSDKPath;
		FString MLSDKEnvironmentVariableName = TEXT("MLSDK");
		FString OriginalMLSDKEnvironmentVariableName = TEXT("MLSDKOriginal");

		//save off the value of MLSDK, if it hasn't been saved before
		MLSDK = FPlatformMisc::GetEnvironmentVariable(*OriginalMLSDKEnvironmentVariableName);
		MLSDK.TrimToNullTerminator();
		if (MLSDK.IsEmpty())
		{
			//only once, read the starting value
			MLSDK = FPlatformMisc::GetEnvironmentVariable(*MLSDKEnvironmentVariableName);
			FPlatformMisc::SetEnvironmentVar(*OriginalMLSDKEnvironmentVariableName, *MLSDK);
		}


		GConfig->GetString(TEXT("/Script/LuminPlatformEditor.MagicLeapSDKSettings"), TEXT("MLSDKPath"), MLSDKPath, GEngineIni);
		MLSDKPath.TrimToNullTerminator();
		if (MLSDKPath.Len() > 0)
		{
			int32 StartIndex = -1;
			bool bFirstQuote = MLSDKPath.FindChar('"', StartIndex);
			int32 EndIndex = -1;
			bool bLastQuote = MLSDKPath.FindLastChar('"', EndIndex);
			if (StartIndex != 1 && EndIndex != -1 && StartIndex < EndIndex)
			{
				++StartIndex;
				MLSDKPath = MLSDKPath.Mid(StartIndex, EndIndex - StartIndex);
			}
			else
			{
				MLSDKPath.Empty();
			}

			if (MLSDKPath.Len() > 0 && FPaths::DirectoryExists(MLSDKPath))
			{
				MLSDK = MLSDKPath;
			}
		}

		MLSDK.TrimToNullTerminator();
		FPlatformMisc::SetEnvironmentVar(*MLSDKEnvironmentVariableName, *MLSDK);

		if (CheckForVDZILibraries)
		{
			// VDZI search paths: VDZI is only active in PC builds.
			// This allows repointing MLAPI loading to the VDZI DLLs.
			FString VDZILibraryPath = TEXT("");
			GConfig->GetString(TEXT("MLSDK"), TEXT("LibraryPath"), VDZILibraryPath, GEngineIni);
			if (!VDZILibraryPath.IsEmpty())
			{
				DllSearchPaths.Add(VDZILibraryPath);
			}

			if (!MLSDK.IsEmpty())
			{
				// The default VDZI dir.
				DllSearchPaths.Add(FPaths::Combine(*MLSDK, TEXT("VirtualDevice"), TEXT("lib")));
				// We also need to add the default bin dir as dependent libs are placed there instead
				// of in the lib directory.
				DllSearchPaths.Add(FPaths::Combine(*MLSDK, TEXT("VirtualDevice"), TEXT("bin")));
			}
		}
#endif

		// The MLSDK DLLs are platform specific and are segregated in directories for each platform.

#if PLATFORM_LUMIN
		// Lumin uses the system path as we are in device.
		DllSearchPaths.Add(TEXT("/system/lib64"));
#else

		if (!MLSDK.IsEmpty())
		{
#if PLATFORM_WINDOWS
			DllSearchPaths.Add(FPaths::Combine(*MLSDK, TEXT("lib"), TEXT("win64")));
#elif PLATFORM_LINUX
			DllSearchPaths.Add(FPaths::Combine(*MLSDK, TEXT("lib"), TEXT("linux64")));
#elif PLATFORM_MAC
			DllSearchPaths.Add(FPaths::Combine(*MLSDK, TEXT("lib"), TEXT("osx")));
#endif // PLATFORM_WINDOWS
		}

#endif // PLATFORM_LUMIN

		// Add the search paths to where we will load the DLLs from. For all just add to the UE4
		// directory listing. But for Windows we also need to manipulate the PATH for the load
		// to take effect.
#if PLATFORM_WINDOWS
		// Need to adjust PATH with additional MLSDK load path to allow the delay-loaded DLLs
		// to work in the plugin.

		//if we've previously saved the original path off, just use that saved original version
		FString PathVar = FPlatformMisc::GetEnvironmentVariable(TEXT("PATHOriginal"));
		PathVar.TrimToNullTerminator();
		if (PathVar.IsEmpty())
		{
			//save off the path before we add to it
			PathVar = FPlatformMisc::GetEnvironmentVariable(TEXT("PATH"));
			PathVar.TrimToNullTerminator();
			FPlatformMisc::SetEnvironmentVar(TEXT("PATHOriginal"), *PathVar);
		}

		for (const FString& path : DllSearchPaths)
		{
			PathVar.Append(FPlatformMisc::GetPathVarDelimiter());
			PathVar.Append(path);
		}
		FPlatformMisc::SetEnvironmentVar(TEXT("PATH"), *PathVar);
#endif
		// For all we add to the UE4 dir listing which takes care of the first-level
		// loading of DLL modules.
		for (const FString& path : DllSearchPaths)
		{
			FPlatformProcess::AddDllDirectory(*path);
		}
	}

	/**
	  Loads the given library from the correct path.
	  @param Name Name of library to load, without any prefix or extension. e.g."ml_perception_client".
	  @return True if the library was succesfully loaded. A false value generally indicates that the MLSDK path is not set correctly.
	*/
	bool LoadDLL(FString Name)
	{
		Name = FString(FPlatformProcess::GetModulePrefix()) + Name + TEXT(".") + FPlatformProcess::GetModuleExtension();
#if PLATFORM_MAC
		// FPlatformProcess::GetModulePrefix() for Mac is an empty string in Unreal
		// whereas MLSDK uses 'lib' as the prefix for its OSX libs.
		if (FString(FPlatformProcess::GetModulePrefix()).Len() == 0)
		{
			Name = FString(TEXT("lib")) + Name;
		}
#endif
		for (const FString& path : DllSearchPaths)
		{
			void* dll = FPlatformProcess::GetDllHandle(*FPaths::Combine(*path, *Name));
			if (dll != nullptr)
			{
				UE_LOG(LogMagicLeapAPISetup, Display, TEXT("Dll loaded: %s"), *FPaths::Combine(*path, *Name));
				DllHandles.Add(dll);
				return true;
			}
		}

		return false;
	}

	/** Frees all the dll handles. */
	void Shutdown()
	{
		for (void* handle : DllHandles)
		{
			check(handle != nullptr);
			FPlatformProcess::FreeDllHandle(handle);
		}
	}

private:
	TArray<FString> DllSearchPaths;
	TArray<void*> DllHandles;
};
