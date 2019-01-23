// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MediaIOCoreFileWriter.h"

#include "Containers/UnrealString.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "Templates/UniquePtr.h"

namespace MediaIOCoreFileWriter
{
	void WriteRawFile(const FString& InFilename, uint8* InBuffer, uint32 InSize)
	{
#if ALLOW_DEBUG_FILES
		if (InFilename != "")
		{
			FString OutputDirectory = FPaths::Combine(*FPaths::ProjectDir(), TEXT("Saved"), TEXT("Media"));
			FPaths::NormalizeDirectoryName(OutputDirectory);

			if (!FPaths::DirectoryExists(OutputDirectory))
			{
				if (!IFileManager::Get().MakeDirectory(*OutputDirectory, true))
				{
					return;
				}
			}

			FString OutputFilename = FPaths::Combine(OutputDirectory, InFilename);

			// Append current date and time
			FDateTime CurrentDateAndTime = FDateTime::Now();
			OutputFilename += FString("_") + CurrentDateAndTime.ToString() + FString(".raw");

			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			if (!PlatformFile.FileExists(*OutputFilename))
			{
				TUniquePtr<IFileHandle> FileHandle;
				FileHandle.Reset(PlatformFile.OpenWrite(*OutputFilename));
				if (FileHandle)
				{
					FileHandle->Write(InBuffer, InSize);
				}
			}
		}
#endif
	}
}
