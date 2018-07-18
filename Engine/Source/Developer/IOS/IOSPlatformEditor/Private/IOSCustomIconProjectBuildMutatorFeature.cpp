// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#include "IOSCustomIconProjectBuildMutatorFeature.h"
#include "CoreMinimal.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "PlatformInfo.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/CoreMisc.h"

static bool RequiresBuild()
{
	// determine if there are any project icons
	FString IconDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Build/IOS/Resources/Graphics"));
	struct FDirectoryVisitor : public IPlatformFile::FDirectoryVisitor
	{
		TArray<FString>& FileNames;

		FDirectoryVisitor(TArray<FString>& InFileNames)
			: FileNames(InFileNames)
		{
		}

		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
		{
			FString FileName(FilenameOrDirectory);
			if (FileName.EndsWith(TEXT(".png")) && FileName.Contains(TEXT("Icon")))
			{
				FileNames.Add(FileName);
			}
			return true;
		}
	};

	// Enumerate the contents of the current directory
	TArray<FString> FileNames;
	FDirectoryVisitor Visitor(FileNames);
	FPlatformFileManager::Get().GetPlatformFile().IterateDirectory(*IconDir, Visitor);

	if (FileNames.Num() > 0)
	{
		return true;
	}
	return false;
}

bool FIOSCustomIconProjectBuildMutatorFeature ::RequiresProjectBuild(FName InPlatformInfoName) const
{
	const PlatformInfo::FPlatformInfo* const PlatInfo = PlatformInfo::FindPlatformInfo(InPlatformInfoName);
	check(PlatInfo);

	if (PlatInfo->SDKStatus == PlatformInfo::EPlatformSDKStatus::Installed)
	{
		const ITargetPlatform* const Platform = GetTargetPlatformManager()->FindTargetPlatform(PlatInfo->TargetPlatformName.ToString());
		if (Platform)
		{
			if (InPlatformInfoName.ToString() == TEXT("IOS"))
			{
				return RequiresBuild();
			}
		}
	}
	return false;
}