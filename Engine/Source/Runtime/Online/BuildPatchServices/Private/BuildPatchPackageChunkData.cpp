// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "BuildPatchPackageChunkData.h"
#include "BuildPatchManifest.h"
#include "Async/Future.h"
#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "Serialization/MemoryWriter.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/FileHelper.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"
#include "Core/Platform.h"
#include "Common/HttpManager.h"
#include "Common/FileSystem.h"
#include "Common/SpeedRecorder.h"
#include "Common/ChunkDataSizeProvider.h"
#include "Generation/ChunkDatabaseWriter.h"
#include "Installer/ChunkReferenceTracker.h"
#include "Installer/ChunkEvictionPolicy.h"
#include "Installer/MemoryChunkStore.h"
#include "Installer/CloudChunkSource.h"
#include "Installer/DownloadService.h"
#include "Installer/MessagePump.h"
#include "Installer/InstallerError.h"
#include "Installer/InstallerAnalytics.h"
#include "Installer/Statistics/FileOperationTracker.h"
#include "Installer/Statistics/MemoryChunkStoreStatistics.h"
#include "Installer/Statistics/DownloadServiceStatistics.h"
#include "Installer/Statistics/CloudChunkSourceStatistics.h"
#include "BuildPatchProgress.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPackageChunkData, Log, All);
DEFINE_LOG_CATEGORY(LogPackageChunkData);

// For the output file we'll use pretty json in debug, otherwise condensed.
#if UE_BUILD_DEBUG
typedef  TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FPackageChunksJsonWriter;
typedef  TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FPackageChunksJsonWriterFactory;
#else
typedef  TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>> FPackageChunksJsonWriter;
typedef  TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>> FPackageChunksJsonWriterFactory;
#endif //UE_BUILD_DEBUG

namespace PackageChunksHelpers
{
	int32 GetNumDigitsRequiredForInteger(int32 Integer)
	{
		// Technically, there are mathematical solutions to this, however there can be floating point errors in log that cause edge cases there.
		// We'll just use the obvious simple method.
		return FString::Printf(TEXT("%d"), Integer).Len();
	}

	TFuture<FBuildPatchAppManifestPtr> AsyncLoadManifestFile(const FString& ManifestFilePath)
	{
		if (ManifestFilePath.IsEmpty())
		{
			TPromise<FBuildPatchAppManifestPtr> BrokenPromise;
			BrokenPromise.SetValue(nullptr);
			return BrokenPromise.GetFuture();
		}
		else
		{
			return Async<FBuildPatchAppManifestPtr>(EAsyncExecution::ThreadPool, [ManifestFilePath]() -> FBuildPatchAppManifestPtr
			{
				FBuildPatchAppManifestRef BuildManifest = MakeShareable(new FBuildPatchAppManifest());
				if (BuildManifest->LoadFromFile(ManifestFilePath))
				{
					return BuildManifest;
				}
				return nullptr;
			});
		}
	}

	TArray<FGuid> GetCustomChunkReferences(const TArray<TSet<FString>>& TagSetArray, const FBuildPatchAppManifestRef& NewManifest, const FBuildPatchAppManifestRef& PrevManifest)
	{
		using namespace BuildPatchServices;
		TSet<FGuid> VisitedChunks;
		TArray<FGuid> UniqueChunkReferences;
		for (const TSet<FString>& TagSet : TagSetArray)
		{
			for (const FGuid& ChunkReference : CustomChunkReferencesHelpers::OrderedUniquePatchReferencesTagged(NewManifest, PrevManifest, TagSet))
			{
				bool bIsAlreadyInSet = false;
				VisitedChunks.Add(ChunkReference, &bIsAlreadyInSet);
				if (!bIsAlreadyInSet)
				{
					UniqueChunkReferences.Add(ChunkReference);
				}
			}
		}
		return UniqueChunkReferences;
	}

	TArray<FGuid> GetCustomChunkReferences(const TArray<TSet<FString>>& TagSetArray, const FBuildPatchAppManifestRef& NewManifest)
	{
		using namespace BuildPatchServices;
		TSet<FGuid> VisitedChunks;
		TArray<FGuid> UniqueChunkReferences;
		for (const TSet<FString>& TagSet : TagSetArray)
		{
			for (const FGuid& ChunkReference : CustomChunkReferencesHelpers::OrderedUniqueReferencesTagged(NewManifest, TagSet))
			{
				bool bIsAlreadyInSet = false;
				VisitedChunks.Add(ChunkReference, &bIsAlreadyInSet);
				if (!bIsAlreadyInSet)
				{
					UniqueChunkReferences.Add(ChunkReference);
				}
			}
		}
		return UniqueChunkReferences;
	}
}

bool FBuildPackageChunkData::PackageChunkData(const FString& ManifestFilePath, const FString& PrevManifestFilePath, const TArray<TSet<FString>>& InTagSetArray, const FString& OutputFile, const FString& CloudDir, uint64 MaxOutputFileSize, const FString& ResultDataFilePath)
{
	const TCHAR* ChunkDbExtension = TEXT(".chunkdb");
	using namespace BuildPatchServices;
	bool bSuccess = true;
	TFuture<FBuildPatchAppManifestPtr> ManifestFuture = PackageChunksHelpers::AsyncLoadManifestFile(ManifestFilePath);
	TFuture<FBuildPatchAppManifestPtr> PrevManifestFuture = PackageChunksHelpers::AsyncLoadManifestFile(PrevManifestFilePath);
	FBuildPatchAppManifestPtr Manifest = ManifestFuture.Get();
	FBuildPatchAppManifestPtr PrevManifest = PrevManifestFuture.Get();
	TArray<FChunkDatabaseFile> ChunkDbFiles;
	TArray<TSet<FString>> TagSetArray;
	TArray<TArray<int32>> TagSetLookupTable;
	// Check required manifest was loaded ok.
	if (!Manifest.IsValid())
	{
		UE_LOG(LogPackageChunkData, Error, TEXT("Failed to load manifest %s"), *ManifestFilePath);
		bSuccess = false;
	}
	// Check previous manifest was loaded ok if it was provided.
	if (!PrevManifest.IsValid() && !PrevManifestFilePath.IsEmpty())
	{
		UE_LOG(LogPackageChunkData, Error, TEXT("Failed to load manifest %s"), *PrevManifestFilePath);
		bSuccess = false;
	}
	if (bSuccess)
	{
		// If TagSetArray was not provided, we need to adjust it to contain an entry that uses all tags.
		if (InTagSetArray.Num() == 0)
		{
			TagSetArray.AddDefaulted();
			Manifest->GetFileTagList(TagSetArray.Last());
		}
		else
		{
			TagSetArray = InTagSetArray;
		}
		TagSetLookupTable.AddDefaulted(TagSetArray.Num());
		// Construct the chunk reference tracker, building our list of ordered unique chunk references.
		TUniquePtr<IChunkReferenceTracker> ChunkReferenceTracker;
		if (PrevManifest.IsValid())
		{
			ChunkReferenceTracker.Reset(FChunkReferenceTrackerFactory::Create(PackageChunksHelpers::GetCustomChunkReferences(TagSetArray, Manifest.ToSharedRef(), PrevManifest.ToSharedRef())));
		}
		else
		{
			ChunkReferenceTracker.Reset(FChunkReferenceTrackerFactory::Create(PackageChunksHelpers::GetCustomChunkReferences(TagSetArray, Manifest.ToSharedRef())));
		}

		// Programmatically calculate header file size effects, so that we automatically handle any changes to the header spec.
		TArray<uint8> HeaderData;
		FMemoryWriter HeaderWriter(HeaderData);
		FChunkDatabaseHeader ChunkDbHeader;
		HeaderWriter << ChunkDbHeader;
		const uint64 ChunkDbHeaderSize = HeaderData.Num();
		HeaderWriter.Seek(0);
		HeaderData.Reset();
		ChunkDbHeader.Contents.Add({FGuid::NewGuid(), 0, 0});
		HeaderWriter << ChunkDbHeader;
		const uint64 PerEntryHeaderSize = HeaderData.Num() - ChunkDbHeaderSize;

		// Enumerate the chunks, allocating them to chunk db files.
		TSet<FGuid> FullDataSet = ChunkReferenceTracker->GetReferencedChunks();
		if (FullDataSet.Num() > 0)
		{
			// Create the data set for each tagset.
			int32 NumSetsWithData = 0;
			TArray<TSet<FGuid>> TaggedDataSets;
			TSet<FGuid> VisitedChunks;
			for (const TSet<FString>& TagSet : TagSetArray)
			{
				TSet<FGuid> TaggedChunks;
				TSet<FString> TaggedFiles;
				Manifest->GetTaggedFileList(TagSet, TaggedFiles);
				Manifest->GetChunksRequiredForFiles(TaggedFiles, TaggedChunks);
				TaggedChunks = TaggedChunks.Intersect(FullDataSet).Difference(VisitedChunks);
				if (TaggedChunks.Num() > 0)
				{
					++NumSetsWithData;
					VisitedChunks.Append(TaggedChunks);
				}
				TaggedDataSets.Add(MoveTemp(TaggedChunks));
			}
			const int32 NumDigitsForTagSets = NumSetsWithData > 1 ? PackageChunksHelpers::GetNumDigitsRequiredForInteger(TaggedDataSets.Num()) : 0;

			for (int32 ChunkDbTagSetIdx = 0; ChunkDbTagSetIdx < TaggedDataSets.Num(); ++ChunkDbTagSetIdx)
			{
				if (TaggedDataSets[ChunkDbTagSetIdx].Num() > 0)
				{
					const int32 FirstChunkDbFileIdx = ChunkDbFiles.Num();
					int32 ChunkDbPartCount = 0;
					int32 CurrentChunkDbFileIdx = FirstChunkDbFileIdx-1;
					
					// Figure out the chunks to write per chunkdb file.
					uint64 AvailableFileSize = 0;
					for (const FGuid& DataId : TaggedDataSets[ChunkDbTagSetIdx])
					{
						const uint64 DataSize = Manifest->GetDataSize(DataId) + PerEntryHeaderSize;
						if (AvailableFileSize < DataSize && 
							((CurrentChunkDbFileIdx < FirstChunkDbFileIdx) || (ChunkDbFiles[CurrentChunkDbFileIdx].DataList.Num() > 0)))
						{
							ChunkDbFiles.AddDefaulted();
							++ChunkDbPartCount;
							++CurrentChunkDbFileIdx;
							AvailableFileSize = MaxOutputFileSize - ChunkDbHeaderSize;
							TagSetLookupTable[ChunkDbTagSetIdx].Add(CurrentChunkDbFileIdx);
						}

						ChunkDbFiles[CurrentChunkDbFileIdx].DataList.Add(DataId);
						if (AvailableFileSize > DataSize)
						{
							AvailableFileSize -= DataSize;
						}
						else
						{
							AvailableFileSize = 0;
						}
					}

					// Figure out the filenames of each chunkdb.
					if (ChunkDbPartCount > 1)
					{
						// Figure out the per file filename;
						FString FilenameFmt = OutputFile;
						FilenameFmt.RemoveFromEnd(ChunkDbExtension);
						if (NumDigitsForTagSets > 0)
						{
							FilenameFmt += FString::Printf(TEXT(".tagset%0*d"), NumDigitsForTagSets, ChunkDbTagSetIdx + 1);
						}
						// Technically, there are mathematical solutions to this, however there can be floating point errors in log that cause edge cases there.
						// We'll just use the obvious simple method.
						const int32 NumDigitsForParts = PackageChunksHelpers::GetNumDigitsRequiredForInteger(ChunkDbPartCount);
						for (int32 ChunkDbFileIdx = FirstChunkDbFileIdx; ChunkDbFileIdx < ChunkDbFiles.Num(); ++ChunkDbFileIdx)
						{
							ChunkDbFiles[ChunkDbFileIdx].DatabaseFilename = FString::Printf(TEXT("%s.part%0*d.chunkdb"), *FilenameFmt, NumDigitsForParts, (ChunkDbFileIdx - FirstChunkDbFileIdx) + 1);
						}
					}
					else if (ChunkDbPartCount == 1)
					{
						// Figure out the per file filename;
						FString FilenameFmt = OutputFile;
						FilenameFmt.RemoveFromEnd(ChunkDbExtension);
						// Make sure we don't confuse any existing % characters when formatting (yes, % is a valid filename character).
						FilenameFmt.ReplaceInline(TEXT("%"), TEXT("%%"));
						if (NumDigitsForTagSets > 0)
						{
							FilenameFmt += FString::Printf(TEXT(".tagset%0*d"), NumDigitsForTagSets, ChunkDbTagSetIdx + 1);
						}
						FilenameFmt += ChunkDbExtension;
						ChunkDbFiles[CurrentChunkDbFileIdx].DatabaseFilename = FilenameFmt;
					}
				}
			}

			// Cloud config.
			FCloudSourceConfig CloudSourceConfig({CloudDir});
			CloudSourceConfig.bBeginDownloadsOnFirstGet = false;
			CloudSourceConfig.MaxRetryCount = 30;

			// Create systems.
			const int32 CloudStoreId = 0;
			FBuildPatchProgress BuildProgress;
			TUniquePtr<IHttpManager> HttpManager(FHttpManagerFactory::Create());
			TUniquePtr<IFileSystem> FileSystem(FFileSystemFactory::Create());
			TUniquePtr<IPlatform> Platform(FPlatformFactory::Create());
			TUniquePtr<IMessagePump> MessagePump(FMessagePumpFactory::Create());
			TUniquePtr<IInstallerError> InstallerError(FInstallerErrorFactory::Create());
			TUniquePtr<IInstallerAnalytics> InstallerAnalytics(FInstallerAnalyticsFactory::Create(
				nullptr,
				nullptr));
			TUniquePtr<IFileOperationTracker> FileOperationTracker(FFileOperationTrackerFactory::Create(
				FTicker::GetCoreTicker(),
				Manifest.Get()));
			TUniquePtr<IMemoryChunkStoreAggregateStatistics> MemoryChunkStoreAggregateStatistics(FMemoryChunkStoreAggregateStatisticsFactory::Create(
				TSet<FGuid>(),
				FileOperationTracker.Get()));
			TUniquePtr<ISpeedRecorder> DownloadSpeedRecorder(FSpeedRecorderFactory::Create());
			TUniquePtr<IChunkDataSizeProvider> ChunkDataSizeProvider(FChunkDataSizeProviderFactory::Create());
			ChunkDataSizeProvider->AddManifestData(Manifest.Get());
			TUniquePtr<IDownloadServiceStatistics> DownloadServiceStatistics(FDownloadServiceStatisticsFactory::Create(
				DownloadSpeedRecorder.Get(),
				ChunkDataSizeProvider.Get(),
				InstallerAnalytics.Get()));
			TUniquePtr<ICloudChunkSourceStatistics> CloudChunkSourceStatistics(FCloudChunkSourceStatisticsFactory::Create(
				InstallerAnalytics.Get(),
				&BuildProgress,
				FileOperationTracker.Get()));
			TUniquePtr<IChunkDataSerialization> ChunkDataSerialization(FChunkDataSerializationFactory::Create(
				FileSystem.Get()));
			TUniquePtr<IChunkEvictionPolicy> MemoryEvictionPolicy(FChunkEvictionPolicyFactory::Create(
				ChunkReferenceTracker.Get()));
			TUniquePtr<IMemoryChunkStore> CloudChunkStore(FMemoryChunkStoreFactory::Create(
				512,
				MemoryEvictionPolicy.Get(),
				nullptr,
				MemoryChunkStoreAggregateStatistics->Expose(CloudStoreId)));
			TUniquePtr<IDownloadService> DownloadService(FDownloadServiceFactory::Create(
				FTicker::GetCoreTicker(),
				HttpManager.Get(),
				FileSystem.Get(),
				DownloadServiceStatistics.Get(),
				InstallerAnalytics.Get()));
			TUniquePtr<ICloudChunkSource> CloudChunkSource(FCloudChunkSourceFactory::Create(
				CloudSourceConfig,
				Platform.Get(),
				CloudChunkStore.Get(),
				DownloadService.Get(),
				ChunkReferenceTracker.Get(),
				ChunkDataSerialization.Get(),
				MessagePump.Get(),
				InstallerError.Get(),
				CloudChunkSourceStatistics.Get(),
				Manifest.ToSharedRef(),
				FullDataSet));

			// Start an IO output thread which saves all the chunks to the chunkdbs.
			TUniquePtr<IChunkDatabaseWriter> ChunkDatabaseWriter(FChunkDatabaseWriterFactory::Create(
				CloudChunkSource.Get(),
				FileSystem.Get(),
				InstallerError.Get(),
				ChunkReferenceTracker.Get(),
				ChunkDataSerialization.Get(),
				ChunkDbFiles,
				[](bool){ GIsRequestingExit = true; }));

			// Main loop.
			double DeltaTime = 0.0;
			double LastTime = FPlatformTime::Seconds();

			// Setup desired frame times.
			float MainsFramerate = 30.0f;
			const float MainsFrameTime = 1.0f / MainsFramerate;

			// Run a main tick loop, exiting when complete.
			while (!GIsRequestingExit)
			{
				// Increment global frame counter once for each app tick.
				GFrameCounter++;

				// Update sub-systems.
				FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
				FTicker::GetCoreTicker().Tick(DeltaTime);

				// Flush threaded logs.
				GLog->FlushThreadedLogs();

				// Throttle frame rate.
				FPlatformProcess::Sleep(FMath::Max<float>(0.0f, MainsFrameTime - (FPlatformTime::Seconds() - LastTime)));

				// Calculate deltas.
				const double AppTime = FPlatformTime::Seconds();
				DeltaTime = AppTime - LastTime;
				LastTime = AppTime;
			}

			// Do any success state checks?
			bSuccess = !InstallerError->HasError();
			if (!bSuccess)
			{
				UE_LOG(LogPackageChunkData, Error, TEXT("%s: %s"), *InstallerError->GetErrorCode(), *InstallerError->GetErrorText().BuildSourceString());
			}
		}
		else
		{
			UE_LOG(LogPackageChunkData, Error, TEXT("Manifest has no data"));
		}
	}

	// Save the output.
	if (bSuccess && ResultDataFilePath.IsEmpty() == false)
	{
		FString JsonOutput;
		TSharedRef<FPackageChunksJsonWriter> Writer = FPackageChunksJsonWriterFactory::Create(&JsonOutput);
		Writer->WriteObjectStart();
		{
			Writer->WriteArrayStart(TEXT("ChunkDbFilePaths"));
			for (const FChunkDatabaseFile& ChunkDbFile : ChunkDbFiles)
			{
				Writer->WriteValue(ChunkDbFile.DatabaseFilename);
			}
			Writer->WriteArrayEnd();
			if (InTagSetArray.Num() > 0)
			{
				Writer->WriteArrayStart(TEXT("TagSetLookupTable"));
				for (const TArray<int32>& TagSetLookup : TagSetLookupTable)
				{
					Writer->WriteArrayStart();
					for (const int32& ChunkDbFileIdx : TagSetLookup)
					{
						Writer->WriteValue(ChunkDbFileIdx);
					}
					Writer->WriteArrayEnd();
				}
				Writer->WriteArrayEnd();
			}
		}
		Writer->WriteObjectEnd();
		Writer->Close();
		bSuccess = FFileHelper::SaveStringToFile(JsonOutput, *ResultDataFilePath);
		if (!bSuccess)
		{
			UE_LOG(LogPackageChunkData, Error, TEXT("Could not save output to %s"), *ResultDataFilePath);
		}
	}

	return bSuccess;
}
