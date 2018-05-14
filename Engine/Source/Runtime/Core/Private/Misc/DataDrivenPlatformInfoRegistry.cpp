// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Misc/DataDrivenPlatformInfoRegistry.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"


const TMap<FString, FDataDrivenPlatformInfoRegistry::FPlatformInfo>& FDataDrivenPlatformInfoRegistry::GetAllPlatformInfos()
{
	static bool bHasSearchedForPlatforms = false;
	static TMap<FString, FPlatformInfo> DataDrivenPlatforms;

	// look on disk for special files
	if (bHasSearchedForPlatforms == false)
	{
		TArray<FString> FoundFiles;

		// look for the special files in any congfig subdirectories
		IFileManager::Get().FindFilesRecursive(FoundFiles, *FPaths::EngineConfigDir(), TEXT("DataDrivenPlatformInfo.ini"), true, false);

		for (int32 PlatformIndex = 0; PlatformIndex < FoundFiles.Num(); PlatformIndex++)
		{
			// load the .ini file
			FConfigFile PlatformIni;
			FConfigCacheIni::LoadExternalIniFile(PlatformIni, *FPaths::GetBaseFilename(FoundFiles[PlatformIndex]), nullptr, *FPaths::GetPath(FoundFiles[PlatformIndex]), false);

			// cache info
			FString PlatformName = FPaths::GetCleanFilename(FPaths::GetPath(FoundFiles[PlatformIndex]));
			FPlatformInfo& Info = DataDrivenPlatforms.Add(PlatformName, FPlatformInfo());
			PlatformIni.GetBool(TEXT("DataDrivenPlatformInfo"), TEXT("bIsConfidential"), Info.bIsConfidential);
			PlatformIni.GetString(TEXT("DataDrivenPlatformInfo"), TEXT("IniParent"), Info.IniParent);
		}

		bHasSearchedForPlatforms = true;
	}

	return DataDrivenPlatforms;
}


const FDataDrivenPlatformInfoRegistry::FPlatformInfo& FDataDrivenPlatformInfoRegistry::GetPlatformInfo(const FString& PlatformName)
{
	const FPlatformInfo* Info = GetAllPlatformInfos().Find(PlatformName);
	static FPlatformInfo Empty;
	return Info ? *Info : Empty;
}


const TArray<FString>& FDataDrivenPlatformInfoRegistry::GetConfidentialPlatforms()
{
	static bool bHasSearchedForPlatforms = false;
	static TArray<FString> FoundPlatforms;

	// look on disk for special files
	if (bHasSearchedForPlatforms == false)
	{
		for (auto It : GetAllPlatformInfos())
		{
			if (It.Value.bIsConfidential)
			{
				FoundPlatforms.Add(It.Key);
			}
		}

		bHasSearchedForPlatforms = true;
	}

	// return whatever we have already found
	return FoundPlatforms;
}