// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Compactify/PatchDataCompactifier.h"

#include "Algo/Partition.h"
#include "Algo/Transform.h"
#include "Logging/LogMacros.h"

#include "Common/FileSystem.h"
#include "Enumeration/PatchDataEnumeration.h"

DECLARE_LOG_CATEGORY_CLASS(LogDataCompactifier, Log, All);

namespace BuildPatchServices
{
	class FPatchDataCompactifier
		: public IPatchDataCompactifier
	{
	public:
		FPatchDataCompactifier(const FCompactifyConfiguration& InConfiguration);
		~FPatchDataCompactifier();

		// IPatchDataCompactifier interface begin.
		virtual bool Run() override;
		// IPatchDataCompactifier interface end.

	private:
		void DeleteFile(const FString& FilePath) const;
		bool IsPatchData(const FString& FilePath) const;
		FString BuildSizeString(uint64 Size) const;

	private:
		const FCompactifyConfiguration Configuration;
		FNumberFormattingOptions SizeFormattingOptions;
		TUniquePtr<IFileSystem> FileSystem;
	};

	FPatchDataCompactifier::FPatchDataCompactifier(const FCompactifyConfiguration& InConfiguration)
		: Configuration(InConfiguration)
		, SizeFormattingOptions(FNumberFormattingOptions().SetMaximumFractionalDigits(3).SetMinimumFractionalDigits(3))
		, FileSystem(FFileSystemFactory::Create())
	{
	}

	FPatchDataCompactifier::~FPatchDataCompactifier()
	{
	}

	bool FPatchDataCompactifier::Run()
	{
		bool bSuccess = true;

		// We output filenames deleted if requested.
		TUniquePtr<FArchive> DeletedChunkOutput;
		if (!Configuration.DeletedChunkLogFile.IsEmpty())
		{
			DeletedChunkOutput = FileSystem->CreateFileWriter(*Configuration.DeletedChunkLogFile);
			if (!DeletedChunkOutput.IsValid())
			{
				UE_LOG(LogDataCompactifier, Error, TEXT("Could not open output file for writing %s."), *Configuration.DeletedChunkLogFile);
				bSuccess = false;
			}
		}

		if (bSuccess)
		{
			// Track some statistics.
			uint32 FilesProcessed = 0;
			uint32 FilesSkipped = 0;
			uint32 NonPatchFilesProcessed = 0;
			uint32 FilesDeleted = 0;
			uint64 BytesProcessed = 0;
			uint64 BytesSkipped = 0;
			uint64 NonPatchBytesProcessed = 0;
			uint64 BytesDeleted = 0;

			// We'll work out the date of the oldest unreferenced file we'll keep
			FDateTime Cutoff = FDateTime::UtcNow() - FTimespan::FromDays(Configuration.DataAgeThreshold);

			// List all files first, and then we'll work with the list.
			TArray<FString> AllFiles;
			const bool bFindFiles = true;
			const bool bFindDirectories = false;
			FileSystem->FindFilesRecursively(AllFiles, *Configuration.CloudDirectory);

			// Filter for manifest files.
			int32 FirstNonManifest = Algo::Partition(AllFiles.GetData(), AllFiles.Num(), [&](const FString& Elem) { return Elem.EndsWith(TEXT(".manifest"), ESearchCase::IgnoreCase); });

			// If we don't have any manifest files, notify that we'll continue to delete all mature chunks.
			if (FirstNonManifest == 0)
			{
				UE_LOG(LogDataCompactifier, Log, TEXT("Could not find any manifest files. Proceeding to delete all mature data."));
			}

			// Process all found files.
			TSet<FString> ReferencedFileSet;
			for (int32 FileIdx = 0; FileIdx < AllFiles.Num(); ++FileIdx)
			{
				const FString& Filename = AllFiles[FileIdx];
				int64 CurrentFileSize;
				bool bGotFileSize = FileSystem->GetFileSize(*Filename, CurrentFileSize);
				if (bGotFileSize)
				{
					++FilesProcessed;
					BytesProcessed += CurrentFileSize;
					// For each manifest, enumerate referenced data files.
					const bool bIsManifestFile = FileIdx < FirstNonManifest;
					if (bIsManifestFile)
					{
						BuildPatchServices::FPatchDataEnumerationConfiguration EnumerationConfig;
						EnumerationConfig.InputFile = Filename;
						TUniquePtr<IPatchDataEnumeration> PatchDataEnumeration(FPatchDataEnumerationFactory::Create(EnumerationConfig));
						TArray<FString> ReferencedFileArray;
						PatchDataEnumeration->Run(ReferencedFileArray);
						Algo::Transform(ReferencedFileArray, ReferencedFileSet, [&](const FString& Elem) { return Configuration.CloudDirectory / Elem; });
						UE_LOG(LogDataCompactifier, Display, TEXT("Extracted %d references from %s. Unioning with existing files, new count of %d."), ReferencedFileArray.Num(), *EnumerationConfig.InputFile, ReferencedFileSet.Num());
					}
					// For each other file, check whether it is referenced.
					else if (!ReferencedFileSet.Contains(Filename))
					{
						FDateTime FileTimeStamp;
						const bool bIsOldEnough = FileSystem->GetTimeStamp(*Filename, FileTimeStamp) && FileTimeStamp < Cutoff;
						const bool bIsRecognisedFileType = IsPatchData(Filename);
						if (!bIsOldEnough)
						{
							++FilesSkipped;
							BytesSkipped += CurrentFileSize;
						}
						else if (!bIsRecognisedFileType)
						{
							++NonPatchFilesProcessed;
							NonPatchBytesProcessed += CurrentFileSize;
						}
						else
						{
							DeleteFile(Filename);
							++FilesDeleted;
							BytesDeleted += CurrentFileSize;
							if (DeletedChunkOutput.IsValid())
							{
								FString OutputLine = Filename + TEXT("\r\n");
								FTCHARToUTF8 UTF8String(*OutputLine);
								DeletedChunkOutput->Serialize((UTF8CHAR*)UTF8String.Get(), UTF8String.Length() * sizeof(UTF8CHAR));
							}
						}
					}
				}
				else
				{
					UE_LOG(LogDataCompactifier, Warning, TEXT("Could not determine size of %s. Perhaps it has been removed by another process."), *Filename);
				}
			}

			UE_LOG(LogDataCompactifier, Display, TEXT("Found %u files totalling %s."), FilesProcessed, *BuildSizeString(BytesProcessed));
			UE_LOG(LogDataCompactifier, Display, TEXT("Found %u unknown files totalling %s."), NonPatchFilesProcessed, *BuildSizeString(NonPatchBytesProcessed));
			UE_LOG(LogDataCompactifier, Display, TEXT("Deleted %u files totalling %s."), FilesDeleted, *BuildSizeString(BytesDeleted));
			UE_LOG(LogDataCompactifier, Display, TEXT("Skipped %u young files totalling %s."), FilesSkipped, *BuildSizeString(BytesSkipped));
		}

		return bSuccess;
	}

	void FPatchDataCompactifier::DeleteFile(const FString& FilePath) const
	{
		if (!Configuration.bRunPreview)
		{
			FileSystem->DeleteFile(*FilePath);
		}
		UE_LOG(LogDataCompactifier, Log, TEXT("Deprecated data %s%s."), *FilePath, Configuration.bRunPreview ? TEXT("") : TEXT(" deleted"));
	}

	bool FPatchDataCompactifier::IsPatchData(const FString& FilePath) const
	{
		const TCHAR* ChunkFile = TEXT(".chunk");
		const TCHAR* DeltaFile = TEXT(".delta");
		const TCHAR* LegacyFile = TEXT(".file");
		return FilePath.EndsWith(ChunkFile) || FilePath.EndsWith(DeltaFile) || FilePath.EndsWith(LegacyFile);
	}

	FString FPatchDataCompactifier::BuildSizeString(uint64 Size) const
	{
		return FString::Printf(TEXT("%s bytes (%s, %s)"), *FText::AsNumber(Size).ToString(), *FText::AsMemory(Size, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(Size, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
	}

	IPatchDataCompactifier* FPatchDataCompactifierFactory::Create(const FCompactifyConfiguration& Configuration)
	{
		return new FPatchDataCompactifier(Configuration);
	}
}
