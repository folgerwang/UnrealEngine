// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/CString.h"

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

		FString MLSDKEnvironmentVariableName = TEXT("MLSDK");
		FString MLSDK = FPlatformMisc::GetEnvironmentVariable(*MLSDKEnvironmentVariableName);


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

			// We also search in the MLSDK VDZI paths for libraries if we have them.
			if (!MLSDK.IsEmpty())
			{
				TArray<FString> ZIShimPath = GetZIShimPath(MLSDK);
				if (ZIShimPath.Num() > 0)
				{
					DllSearchPaths.Append(GetZIShimPath(MLSDK));
				}
				else
				{
					// Fallback to adding fixed known paths if we fail to get anything from
					// the configuration data.
					// The default VDZI dir.
					DllSearchPaths.Add(FPaths::Combine(*MLSDK, TEXT("VirtualDevice"), TEXT("lib")));
					// We also need to add the default bin dir as dependent libs are placed there instead
					// of in the lib directory.
					DllSearchPaths.Add(FPaths::Combine(*MLSDK, TEXT("VirtualDevice"), TEXT("bin")));
				}
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

	/** Fills the Variables with the evaluated contents of the SDK Shim discovery data. */
	bool GetZIShimVariables(const FString & MLSDK, TMap<FString, FString> & ResultVariables) const
	{
		// The known path to the paths file.
		FString SDKShimDiscoveryFile = FPaths::Combine(MLSDK, TEXT(".metadata"), TEXT("sdk_shim_discovery.txt"));
		if (!FPaths::FileExists(SDKShimDiscoveryFile))
		{
			return false;
		}
		// Map of variable to value for evaluating the content of the file.
		// On successful parsing and evaluation we copy the data to the result.
		TMap<FString, FString> Variables;
		Variables.Add(TEXT("$(MLSDK)"), MLSDK);
		// TODO: Determine MLSDK version and set MLSDK_VERSION variable.
#if PLATFORM_WINDOWS
		Variables.Add(TEXT("$(HOST)"), TEXT("win64"));
#elif PLATFORM_LINUX
		Variables.Add(TEXT("$(HOST)"), TEXT("linux64"));
#elif PLATFORM_MAC
		Variables.Add(TEXT("$(HOST)"), TEXT("osx"));
#endif // PLATFORM_WINDOWS
		// Single pass algo for evaluating the file:
		// 1. for each line:
		// a. for each occurance of $(NAME):
		// i. replace with Variables[NAME], if no NAME var found ignore replacement.
		// b. split into var=value, and add to Variables.
		IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();
		TUniquePtr<IFileHandle> File(PlatformFile.OpenRead(*SDKShimDiscoveryFile));
		if (File)
		{
			TArray<uint8> Data;
			Data.SetNumZeroed(File->Size() + 1);
			File->Read(Data.GetData(), Data.Num() - 1);
			FUTF8ToTCHAR TextConvert(reinterpret_cast<ANSICHAR*>(Data.GetData()));
			const TCHAR * Text = TextConvert.Get();
			while (Text && *Text)
			{
				Text += FCString::Strspn(Text, TEXT("\t "));
				if (Text[0] == '#' || Text[0] == '\r' || Text[0] == '\n' || !*Text)
				{
					// Skip comment or empty lines.
					Text += FCString::Strcspn(Text, TEXT("\r\n"));
				}
				else
				{
					// Parse variable=value line.
					FString Variable(FCString::Strcspn(Text, TEXT("\t= ")), Text);
					Text += Variable.Len();
					Text += FCString::Strspn(Text, TEXT("\t= "));
					FString Value(FCString::Strcspn(Text, TEXT("\r\n")), Text);
					Text += Value.Len();
					Value.TrimEndInline();
					// Eval any var references in both variable and value.
					int32 EvalCount = 0;
					do
					{
						EvalCount = 0;
						for (auto VarEntry : Variables)
						{
							EvalCount += Variable.ReplaceInline(*VarEntry.Key, *VarEntry.Value);
							EvalCount += Value.ReplaceInline(*VarEntry.Key, *VarEntry.Value);
						}
					} while (EvalCount != 0 && (Variable.Contains(TEXT("$(")) || Value.Contains(TEXT("$("))));
					// Intern the new variable.
					Variable = TEXT("$(") + Variable + TEXT(")");
					Variables.Add(Variable, Value);
				}
				// Skip over EOL to get to the next line.
				if (Text[0] == '\r' && Text[1] == '\n')
				{
					Text += 2;
				}
				else
				{
					Text += 1;
				}
			}
		}
		// We now need to copy the evaled variables to the result and un-munge them for
		// plain access. We use them munged for the eval for easier eval replacement above.
		ResultVariables.Empty(Variables.Num());
		for (auto VarEntry : Variables)
		{
			ResultVariables.Add(VarEntry.Key.Mid(2, VarEntry.Key.Len() - 3), VarEntry.Value);
		}
		return true;
	}

private:
	TArray<FString> DllSearchPaths;
	TArray<void*> DllHandles;

	TArray<FString> GetZIShimPath(const FString & MLSDK) const
	{
#if PLATFORM_WINDOWS
		const FString ZIShimPathVar = "ZI_SHIM_PATH_win64";
#elif PLATFORM_LINUX
		const FString ZIShimPathVar = "ZI_SHIM_PATH_linux64";
#elif PLATFORM_MAC
		const FString ZIShimPathVar = "ZI_SHIM_PATH_osx";
#else
		const FString ZIShimPathVar = "";
#endif // PLATFORM_WINDOWS

		TMap<FString, FString> Variables;
		if (GetZIShimVariables(MLSDK, Variables) && Variables.Contains(ZIShimPathVar))
		{
			// The shim path var we are looking for.
			FString Value = Variables[ZIShimPathVar];
			// Since it's a path variable it can have multiple components. Hence
			// split those out into our result;
			TArray<FString> Result;
			for (FString Path; Value.Split(TEXT(";"), &Path, &Value); )
			{
				Result.Add(Path);
			}
			Result.Add(Value);
			return Result;
		}
		return TArray<FString>();
	}
};
