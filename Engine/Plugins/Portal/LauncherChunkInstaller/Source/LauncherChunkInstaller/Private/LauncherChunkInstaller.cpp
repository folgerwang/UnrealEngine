// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LauncherChunkInstaller.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFilemanager.h"
#include "IPlatformFilePak.h"
#include "Misc/CommandLine.h"
#include "Modules/ModuleManager.h"

/**
 * Module for the Launcher Chunk Installer
 */
class FLauncherChunkInstallerModule : public IPlatformChunkInstallModule
{
public:
	TUniquePtr<IPlatformChunkInstall> ChunkInstaller;

	FLauncherChunkInstallerModule()
		: ChunkInstaller(new FLauncherChunkInstaller())
	{

	}

	virtual IPlatformChunkInstall* GetPlatformChunkInstall()
	{
		return ChunkInstaller.Get();
	}
};

IMPLEMENT_MODULE(FLauncherChunkInstallerModule, LauncherChunkInstaller);

EChunkLocation::Type FLauncherChunkInstaller::GetChunkLocation(uint32 ChunkID)
{
	// get the platform pak file management API.
	FPakPlatformFile* PakPlatformFile = (FPakPlatformFile*)(FPlatformFileManager::Get().FindPlatformFile(FPakPlatformFile::GetTypeName()));
	if (PakPlatformFile)
	{
		// Initially get all pak folders.
		// Go through each folder to find all pak files in the corresponding folder.
		// Go through all pak files and find the first one that starts with the prefix 
		// for the chunk we're interested in.

		TArray<FString> PakFolders;
		TArray<FString> AllPakFiles;
		PakPlatformFile->GetPakFolders(FCommandLine::Get(), PakFolders);
		for (const FString& PakFolder : PakFolders)
		{
			TArray<FString> PakFiles;
			IFileManager::Get().FindFiles(PakFiles, *PakFolder, TEXT(".pak"));
			for (const FString& PakFile : PakFiles)
			{
				AllPakFiles.Add(PakFile);
			}
		}

		TArray<FStringFormatArg> FormatArgs = { ChunkID };
		FString ChunkPrefix = FString::Format(TEXT("pakchunk{0}"), FormatArgs);

		for (FString PakFile : AllPakFiles)
		{
			FString PakFileClean = FPaths::GetCleanFilename(PakFile);
			if (PakFileClean.StartsWith(ChunkPrefix))
			{
				return EChunkLocation::LocalFast;
			}
		}
	}

	// This is a fall back for builds to fail on shipping or testing.
	// This code will also force available to trigger when we're running editor

#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	return EChunkLocation::NotAvailable;
#else 
	return EChunkLocation::LocalFast;
#endif
}
