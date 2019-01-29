// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

// Copied from AndroidDeviceDetectionModule.cpp
// TODO: would be nice if Unreal make that function public so we don't need to make a duplicate.
inline void GetAdbPath(FString& OutAdbPath)
{
	FString AndroidDirectory = FPlatformMisc::GetEnvironmentVariable(TEXT("ANDROID_HOME"));

#if PLATFORM_MAC
	if (AndroidDirectory.Len() == 0)
	{
		// didn't find ANDROID_HOME, so parse the .bash_profile file on MAC
		FArchive* FileReader = IFileManager::Get().CreateFileReader(*FString([@"~/.bash_profile" stringByExpandingTildeInPath]));
		if (FileReader)
		{
			const int64 FileSize = FileReader->TotalSize();
			ANSICHAR* AnsiContents = (ANSICHAR*)FMemory::Malloc(FileSize + 1);
			FileReader->Serialize(AnsiContents, FileSize);
			FileReader->Close();
			delete FileReader;

			AnsiContents[FileSize] = 0;
			TArray<FString> Lines;
			FString(ANSI_TO_TCHAR(AnsiContents)).ParseIntoArrayLines(Lines);
			FMemory::Free(AnsiContents);

			for (int32 Index = Lines.Num() - 1; Index >= 0; Index--)
			{
				if (AndroidDirectory.Len() == 0 && Lines[Index].StartsWith(TEXT("export ANDROID_HOME=")))
				{
					FString Directory;
					Lines[Index].Split(TEXT("="), NULL, &Directory);
					Directory = Directory.Replace(TEXT("\""), TEXT(""));
					AndroidDirectory = Directory;
					setenv("ANDROID_HOME", TCHAR_TO_ANSI(*AndroidDirectory), 1);
				}
			}
		}
	}
#endif

	if (AndroidDirectory.Len() != 0)
	{
#if PLATFORM_WINDOWS
		OutAdbPath = FString::Printf(TEXT("%s\\platform-tools\\adb.exe"), *AndroidDirectory);
#else
		OutAdbPath = FString::Printf(TEXT("%s/platform-tools/adb"), *AndroidDirectory);
#endif

		// if it doesn't exist then just clear the path as we might set it later
		if (!FPaths::FileExists(*OutAdbPath))
		{
			OutAdbPath.Empty();
		}
	}
}
