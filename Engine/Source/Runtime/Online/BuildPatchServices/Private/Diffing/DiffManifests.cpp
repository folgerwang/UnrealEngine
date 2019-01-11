// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Diffing/DiffManifests.h"

#include "Async/Async.h"
#include "HAL/ThreadSafeBool.h"
#include "Logging/LogMacros.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"
#include "HttpModule.h"

#include "Common/ChunkDataSizeProvider.h"
#include "Common/FileSystem.h"
#include "Common/HttpManager.h"
#include "Common/SpeedRecorder.h"
#include "Common/StatsCollector.h"
#include "Installer/Statistics/DownloadServiceStatistics.h"
#include "Installer/DownloadService.h"
#include "Installer/InstallerAnalytics.h"
#include "Installer/OptimisedDelta.h"
#include "BuildPatchManifest.h"
#include "BuildPatchUtil.h"

DECLARE_LOG_CATEGORY_CLASS(LogDiffManifests, Log, All);

// For the output file we'll use pretty json in debug, otherwise condensed.
#if UE_BUILD_DEBUG
typedef  TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FDiffJsonWriter;
typedef  TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FDiffJsonWriterFactory;
#else
typedef  TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>> FDiffJsonWriter;
typedef  TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>> FDiffJsonWriterFactory;
#endif //UE_BUILD_DEBUG

namespace BuildPatchServices
{
	class FDiffManifests
		: public IDiffManifests
	{
	public:
		FDiffManifests(const FDiffManifestsConfiguration& InConfiguration);
		~FDiffManifests();

		// IChunkDeltaOptimiser interface begin.
		virtual	bool Run() override;
		// IChunkDeltaOptimiser interface end.

	private:
		bool AsyncRun();
		void HandleDownloadComplete(int32 RequestId, const FDownloadRef& Download);

	private:
		const FDiffManifestsConfiguration Configuration;
		FTicker& CoreTicker;
		FDownloadCompleteDelegate DownloadCompleteDelegate;
		FDownloadProgressDelegate DownloadProgressDelegate;
		TUniquePtr<IFileSystem> FileSystem;
		TUniquePtr<IHttpManager> HttpManager;
		TUniquePtr<IChunkDataSizeProvider> ChunkDataSizeProvider;
		TUniquePtr<ISpeedRecorder> DownloadSpeedRecorder;
		TUniquePtr<IInstallerAnalytics> InstallerAnalytics;
		TUniquePtr<IDownloadServiceStatistics> DownloadServiceStatistics;
		TUniquePtr<IDownloadService> DownloadService;
		TUniquePtr<FStatsCollector> StatsCollector;
		FThreadSafeBool bShouldRun;

		// Manifest downloading
		int32 RequestIdManifestA;
		int32 RequestIdManifestB;
		TPromise<FBuildPatchAppManifestPtr> PromiseManifestA;
		TPromise<FBuildPatchAppManifestPtr> PromiseManifestB;
		TFuture<FBuildPatchAppManifestPtr> FutureManifestA;
		TFuture<FBuildPatchAppManifestPtr> FutureManifestB;
	};

	FDiffManifests::FDiffManifests(const FDiffManifestsConfiguration& InConfiguration)
		: Configuration(InConfiguration)
		, CoreTicker(FTicker::GetCoreTicker())
		, DownloadCompleteDelegate(FDownloadCompleteDelegate::CreateRaw(this, &FDiffManifests::HandleDownloadComplete))
		, DownloadProgressDelegate()
		, FileSystem(FFileSystemFactory::Create())
		, HttpManager(FHttpManagerFactory::Create())
		, ChunkDataSizeProvider(FChunkDataSizeProviderFactory::Create())
		, DownloadSpeedRecorder(FSpeedRecorderFactory::Create())
		, InstallerAnalytics(FInstallerAnalyticsFactory::Create(nullptr, nullptr))
		, DownloadServiceStatistics(FDownloadServiceStatisticsFactory::Create(DownloadSpeedRecorder.Get(), ChunkDataSizeProvider.Get(), InstallerAnalytics.Get()))
		, DownloadService(FDownloadServiceFactory::Create(CoreTicker, HttpManager.Get(), FileSystem.Get(), DownloadServiceStatistics.Get(), InstallerAnalytics.Get()))
		, StatsCollector(FStatsCollectorFactory::Create())
		, bShouldRun(true)
		, RequestIdManifestA(INDEX_NONE)
		, RequestIdManifestB(INDEX_NONE)
		, PromiseManifestA()
		, PromiseManifestB()
		, FutureManifestA(PromiseManifestA.GetFuture())
		, FutureManifestB(PromiseManifestB.GetFuture())
	{
	}

	FDiffManifests::~FDiffManifests()
	{
	}

	bool FDiffManifests::Run()
	{
		// Run any core initialisation required.
		FHttpModule::Get();

		// Kick off Manifest downloads.
		RequestIdManifestA = DownloadService->RequestFile(Configuration.ManifestAUri, DownloadCompleteDelegate, DownloadProgressDelegate);
		RequestIdManifestB = DownloadService->RequestFile(Configuration.ManifestBUri, DownloadCompleteDelegate, DownloadProgressDelegate);

		// Start the generation thread.
		TFuture<bool> Thread = Async<bool>(EAsyncExecution::Thread, [this]() { return AsyncRun(); });

		// Main timers.
		double DeltaTime = 0.0;
		double LastTime = FPlatformTime::Seconds();

		// Setup desired frame times.
		float MainsFramerate = 100.0f;
		const float MainsFrameTime = 1.0f / MainsFramerate;

		// Run the main loop.
		while (bShouldRun)
		{
			// Increment global frame counter once for each app tick.
			GFrameCounter++;

			// Application tick.
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
			FTicker::GetCoreTicker().Tick(DeltaTime);
			GLog->FlushThreadedLogs();

			// Control frame rate.
			FPlatformProcess::Sleep(FMath::Max<float>(0.0f, MainsFrameTime - (FPlatformTime::Seconds() - LastTime)));

			// Calculate deltas.
			const double AppTime = FPlatformTime::Seconds();
			DeltaTime = AppTime - LastTime;
			LastTime = AppTime;
		}
		GLog->FlushThreadedLogs();

		// Return thread success.
		return Thread.Get();
	}

	bool FDiffManifests::AsyncRun()
	{
		FBuildPatchAppManifestPtr ManifestA = FutureManifestA.Get();
		FBuildPatchAppManifestPtr ManifestB = FutureManifestB.Get();
		bool bSuccess = true;
		if (ManifestA.IsValid() == false)
		{
			UE_LOG(LogDiffManifests, Error, TEXT("Could not download ManifestA from %s."), *Configuration.ManifestAUri);
			bSuccess = false;
		}
		if (ManifestB.IsValid() == false)
		{
			UE_LOG(LogDiffManifests, Error, TEXT("Could not download ManifestB from %s."), *Configuration.ManifestBUri);
			bSuccess = false;
		}
		if (bSuccess)
		{
			// Check for delta file, replacing ManifestB if we find one
			FOptimisedDeltaConfiguration OptimisedDeltaConfiguration(ManifestB.ToSharedRef());
			OptimisedDeltaConfiguration.SourceManifest = ManifestA;
			OptimisedDeltaConfiguration.DeltaPolicy = EDeltaPolicy::TryFetchContinueWithout;
			OptimisedDeltaConfiguration.CloudDirectories = { FPaths::GetPath(Configuration.ManifestBUri) };
			FOptimisedDeltaDependencies OptimisedDeltaDependencies;
			OptimisedDeltaDependencies.DownloadService = DownloadService.Get();
			TUniquePtr<IOptimisedDelta> OptimisedDelta(FOptimisedDeltaFactory::Create(OptimisedDeltaConfiguration, OptimisedDeltaDependencies));
			ManifestB = OptimisedDelta->GetDestinationManifest();
			const int32 MetaDownloadBytes = OptimisedDelta->GetMetaDownloadSize();

			TSet<FString> TagsA, TagsB;
			ManifestA->GetFileTagList(TagsA);
			if (Configuration.TagSetA.Num() > 0)
			{
				TagsA = TagsA.Intersect(Configuration.TagSetA);
			}
			ManifestB->GetFileTagList(TagsB);
			if (Configuration.TagSetB.Num() > 0)
			{
				TagsB = TagsB.Intersect(Configuration.TagSetB);
			}

			int64 NewChunksCount = 0;
			int64 TotalChunkSize = 0;
			TSet<FString> TaggedFileSetA;
			TSet<FString> TaggedFileSetB;
			TSet<FGuid> ChunkSetA;
			TSet<FGuid> ChunkSetB;
			ManifestA->GetTaggedFileList(TagsA, TaggedFileSetA);
			ManifestA->GetChunksRequiredForFiles(TaggedFileSetA, ChunkSetA);
			ManifestB->GetTaggedFileList(TagsB, TaggedFileSetB);
			ManifestB->GetChunksRequiredForFiles(TaggedFileSetB, ChunkSetB);
			TArray<FString> NewChunkPaths;
			for (FGuid& ChunkB : ChunkSetB)
			{
				if (ChunkSetA.Contains(ChunkB) == false)
				{
					++NewChunksCount;
					int32 ChunkFileSize = ManifestB->GetDataSize(ChunkB);
					TotalChunkSize += ChunkFileSize;
					NewChunkPaths.Add(FBuildPatchUtils::GetDataFilename(ManifestB.ToSharedRef(), TEXT("."), ChunkB));
					UE_LOG(LogDiffManifests, Verbose, TEXT("New chunk discovered: Size: %10lld, Path: %s"), ChunkFileSize, *NewChunkPaths.Last());
				}
			}

			UE_LOG(LogDiffManifests, Display, TEXT("New chunks:  %lld"), NewChunksCount);
			UE_LOG(LogDiffManifests, Display, TEXT("Total bytes: %lld"), TotalChunkSize);

			TSet<FString> NewFilePaths = TaggedFileSetB.Difference(TaggedFileSetA);
			TSet<FString> RemovedFilePaths = TaggedFileSetA.Difference(TaggedFileSetB);
			TSet<FString> ChangedFilePaths;
			TSet<FString> UnchangedFilePaths;

			const TSet<FString>& SetToIterate = TaggedFileSetB.Num() > TaggedFileSetA.Num() ? TaggedFileSetA : TaggedFileSetB;
			for (const FString& TaggedFile : SetToIterate)
			{
				FSHAHash FileHashA;
				FSHAHash FileHashB;
				if (ManifestA->GetFileHash(TaggedFile, FileHashA) && ManifestB->GetFileHash(TaggedFile, FileHashB))
				{
					if (FileHashA == FileHashB)
					{
						UnchangedFilePaths.Add(TaggedFile);
					}
					else
					{
						ChangedFilePaths.Add(TaggedFile);
					}
				}
			}

			// Log download details.
			FNumberFormattingOptions SizeFormattingOptions;
			SizeFormattingOptions.MaximumFractionalDigits = 3;
			SizeFormattingOptions.MinimumFractionalDigits = 3;

			int64 DownloadSizeA = ManifestA->GetDownloadSize(TagsA);
			int64 BuildSizeA = ManifestA->GetBuildSize(TagsA);
			int64 DownloadSizeB = ManifestB->GetDownloadSize(TagsB);
			int64 BuildSizeB = ManifestB->GetBuildSize(TagsB);
			int64 DeltaDownloadSize = ManifestB->GetDeltaDownloadSize(TagsB, ManifestA.ToSharedRef(), TagsA) + MetaDownloadBytes;

			// Break down the sizes and delta into new chunks per tag.
			TMap<FString, int64> TagDownloadImpactA;
			TMap<FString, int64> TagBuildImpactA;
			TMap<FString, int64> TagDownloadImpactB;
			TMap<FString, int64> TagBuildImpactB;
			TMap<FString, int64> TagDeltaImpact;
			for (const FString& Tag : TagsA)
			{
				TSet<FString> TagSet;
				TagSet.Add(Tag);
				TagDownloadImpactA.Add(Tag, ManifestA->GetDownloadSize(TagSet));
				TagBuildImpactA.Add(Tag, ManifestA->GetBuildSize(TagSet));
			}
			for (const FString& Tag : TagsB)
			{
				TSet<FString> TagSet;
				TagSet.Add(Tag);
				TagDownloadImpactB.Add(Tag, ManifestB->GetDownloadSize(TagSet));
				TagBuildImpactB.Add(Tag, ManifestB->GetBuildSize(TagSet));
				TagDeltaImpact.Add(Tag, ManifestB->GetDeltaDownloadSize(TagSet, ManifestA.ToSharedRef(), TagsA));
			}
			if (MetaDownloadBytes > 0)
			{
				TagDeltaImpact.FindOrAdd(TEXT("")) += MetaDownloadBytes;
			}

			// Compare tag sets
			TMap<FString, int64> CompareTagSetDeltaImpact;
			TMap<FString, int64> CompareTagSetBuildImpactA;
			TMap<FString, int64> CompareTagSetDownloadSizeA;
			TMap<FString, int64> CompareTagSetBuildImpactB;
			TMap<FString, int64> CompareTagSetDownloadSizeB;
			TSet<FString> CompareTagSetKeys;
			for (const TSet<FString>& TagSet : Configuration.CompareTagSets)
			{
				TArray<FString> TagArrayCompare = TagSet.Array();
				Algo::Sort(TagArrayCompare);
				FString TagSetString = FString::Join(TagArrayCompare, TEXT(", "));
				CompareTagSetKeys.Add(TagSetString);
				CompareTagSetDeltaImpact.Add(TagSetString, ManifestB->GetDeltaDownloadSize(TagSet, ManifestA.ToSharedRef(), TagSet) + MetaDownloadBytes);
				CompareTagSetBuildImpactB.Add(TagSetString, ManifestB->GetBuildSize(TagSet));
				CompareTagSetDownloadSizeB.Add(TagSetString, ManifestB->GetDownloadSize(TagSet));
				CompareTagSetBuildImpactA.Add(TagSetString, ManifestA->GetBuildSize(TagSet));
				CompareTagSetDownloadSizeA.Add(TagSetString, ManifestA->GetDownloadSize(TagSet));
			}

			// Log the information.
			TArray<FString> TagArrayB = TagsB.Array();
			Algo::Sort(TagArrayB);
			FString UntaggedLog(TEXT("(untagged)"));
			FString TagLogList = FString::Join(TagArrayB, TEXT(", "));
			if (TagLogList.IsEmpty() || TagLogList.StartsWith(TEXT(", ")))
			{
				TagLogList.InsertAt(0, UntaggedLog);
			}
			UE_LOG(LogDiffManifests, Display, TEXT("TagSet: %s"), *TagLogList);
			UE_LOG(LogDiffManifests, Display, TEXT("%s %s:"), *ManifestA->GetAppName(), *ManifestA->GetVersionString());
			UE_LOG(LogDiffManifests, Display, TEXT("    Download Size:  %20s bytes (%10s, %11s)"), *FText::AsNumber(DownloadSizeA).ToString(), *FText::AsMemory(DownloadSizeA, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(DownloadSizeA, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
			UE_LOG(LogDiffManifests, Display, TEXT("    Build Size:     %20s bytes (%10s, %11s)"), *FText::AsNumber(BuildSizeA).ToString(), *FText::AsMemory(BuildSizeA, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(BuildSizeA, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
			UE_LOG(LogDiffManifests, Display, TEXT("%s %s:"), *ManifestB->GetAppName(), *ManifestB->GetVersionString());
			UE_LOG(LogDiffManifests, Display, TEXT("    Download Size:  %20s bytes (%10s, %11s)"), *FText::AsNumber(DownloadSizeB).ToString(), *FText::AsMemory(DownloadSizeB, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(DownloadSizeB, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
			UE_LOG(LogDiffManifests, Display, TEXT("    Build Size:     %20s bytes (%10s, %11s)"), *FText::AsNumber(BuildSizeB).ToString(), *FText::AsMemory(BuildSizeB, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(BuildSizeB, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
			UE_LOG(LogDiffManifests, Display, TEXT("%s %s -> %s %s:"), *ManifestA->GetAppName(), *ManifestA->GetVersionString(), *ManifestB->GetAppName(), *ManifestB->GetVersionString());
			UE_LOG(LogDiffManifests, Display, TEXT("    Delta Size:     %20s bytes (%10s, %11s)"), *FText::AsNumber(DeltaDownloadSize).ToString(), *FText::AsMemory(DeltaDownloadSize, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(DeltaDownloadSize, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
			UE_LOG(LogDiffManifests, Display, TEXT(""));

			for (const FString& Tag : TagArrayB)
			{
				UE_LOG(LogDiffManifests, Display, TEXT("%s Impact:"), *(Tag.IsEmpty() ? UntaggedLog : Tag));
				UE_LOG(LogDiffManifests, Display, TEXT("    Individual Download Size:  %20s bytes (%10s, %11s)"), *FText::AsNumber(TagDownloadImpactB[Tag]).ToString(), *FText::AsMemory(TagDownloadImpactB[Tag], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(TagDownloadImpactB[Tag], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
				UE_LOG(LogDiffManifests, Display, TEXT("    Individual Build Size:     %20s bytes (%10s, %11s)"), *FText::AsNumber(TagBuildImpactB[Tag]).ToString(), *FText::AsMemory(TagBuildImpactB[Tag], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(TagBuildImpactB[Tag], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
				UE_LOG(LogDiffManifests, Display, TEXT("    Individual Delta Size:     %20s bytes (%10s, %11s)"), *FText::AsNumber(TagDeltaImpact[Tag]).ToString(), *FText::AsMemory(TagDeltaImpact[Tag], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(TagDeltaImpact[Tag], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
			}

			for (const FString& TagSet : CompareTagSetKeys)
			{
				const FString& TagSetDisplay = TagSet.StartsWith(TEXT(",")) ? UntaggedLog + TagSet : TagSet;
				UE_LOG(LogDiffManifests, Display, TEXT("Impact of TagSet: %s"), *TagSetDisplay);
				UE_LOG(LogDiffManifests, Display, TEXT("    Download Size:  %20s bytes (%10s, %11s)"), *FText::AsNumber(CompareTagSetDownloadSizeB[TagSet]).ToString(), *FText::AsMemory(CompareTagSetDownloadSizeB[TagSet], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(CompareTagSetDownloadSizeB[TagSet], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
				UE_LOG(LogDiffManifests, Display, TEXT("    Build Size:     %20s bytes (%10s, %11s)"), *FText::AsNumber(CompareTagSetBuildImpactB[TagSet]).ToString(), *FText::AsMemory(CompareTagSetBuildImpactB[TagSet], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(CompareTagSetBuildImpactB[TagSet], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
				UE_LOG(LogDiffManifests, Display, TEXT("    Delta Size:     %20s bytes (%10s, %11s)"), *FText::AsNumber(CompareTagSetDeltaImpact[TagSet]).ToString(), *FText::AsMemory(CompareTagSetDeltaImpact[TagSet], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(CompareTagSetDeltaImpact[TagSet], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
			}

			// Save the output.
			if (bSuccess && Configuration.OutputFilePath.IsEmpty() == false)
			{
				FString JsonOutput;
				TSharedRef<FDiffJsonWriter> Writer = FDiffJsonWriterFactory::Create(&JsonOutput);
				Writer->WriteObjectStart();
				{
					Writer->WriteObjectStart(TEXT("ManifestA"));
					{
						Writer->WriteValue(TEXT("AppName"), ManifestA->GetAppName());
						Writer->WriteValue(TEXT("AppId"), static_cast<int32>(ManifestA->GetAppID()));
						Writer->WriteValue(TEXT("VersionString"), ManifestA->GetVersionString());
						Writer->WriteValue(TEXT("DownloadSize"), DownloadSizeA);
						Writer->WriteValue(TEXT("BuildSize"), BuildSizeA);
						Writer->WriteObjectStart(TEXT("IndividualTagDownloadSizes"));
						for (const TPair<FString, int64>& Pair : TagDownloadImpactA)
						{
							Writer->WriteValue(Pair.Key, Pair.Value);
						}
						Writer->WriteObjectEnd();
						Writer->WriteObjectStart("CompareTagSetDownloadSizes");
						for (const TPair<FString, int64>& Pair : CompareTagSetDownloadSizeA)
						{
							Writer->WriteValue(Pair.Key, Pair.Value);
						}
						Writer->WriteObjectEnd();
						Writer->WriteObjectStart(TEXT("IndividualTagBuildSizes"));
						for (const TPair<FString, int64>& Pair : TagBuildImpactA)
						{
							Writer->WriteValue(Pair.Key, Pair.Value);
						}
						Writer->WriteObjectEnd();
						Writer->WriteObjectStart("CompareTagSetBuildSizes");
						for (const TPair<FString, int64>& Pair : CompareTagSetBuildImpactA)
						{
							Writer->WriteValue(Pair.Key, Pair.Value);
						}
						Writer->WriteObjectEnd();
					}
					Writer->WriteObjectEnd();
					Writer->WriteObjectStart(TEXT("ManifestB"));
					{
						Writer->WriteValue(TEXT("AppName"), ManifestB->GetAppName());
						Writer->WriteValue(TEXT("AppId"), static_cast<int32>(ManifestB->GetAppID()));
						Writer->WriteValue(TEXT("VersionString"), ManifestB->GetVersionString());
						Writer->WriteValue(TEXT("DownloadSize"), DownloadSizeB);
						Writer->WriteValue(TEXT("BuildSize"), BuildSizeB);
						Writer->WriteObjectStart(TEXT("IndividualTagDownloadSizes"));
						for (const TPair<FString, int64>& Pair : TagDownloadImpactB)
						{
							Writer->WriteValue(Pair.Key, Pair.Value);
						}
						Writer->WriteObjectEnd();
						Writer->WriteObjectStart("CompareTagSetDownloadSizes");
						for (const TPair<FString, int64>& Pair : CompareTagSetDownloadSizeB)
						{
							Writer->WriteValue(Pair.Key, Pair.Value);
						}
						Writer->WriteObjectEnd();
						Writer->WriteObjectStart(TEXT("IndividualTagBuildSizes"));
						for (const TPair<FString, int64>& Pair : TagBuildImpactB)
						{
							Writer->WriteValue(Pair.Key, Pair.Value);
						}
						Writer->WriteObjectEnd();
						Writer->WriteObjectStart("CompareTagSetBuildSizes");
						for (const TPair<FString, int64>& Pair : CompareTagSetBuildImpactB)
						{
							Writer->WriteValue(Pair.Key, Pair.Value);
						}
						Writer->WriteObjectEnd();
					}
					Writer->WriteObjectEnd();
					Writer->WriteObjectStart(TEXT("Differential"));
					{
						Writer->WriteArrayStart(TEXT("NewFilePaths"));
						for (const FString& NewFilePath : NewFilePaths)
						{
							Writer->WriteValue(NewFilePath);
						}
						Writer->WriteArrayEnd();
						Writer->WriteArrayStart(TEXT("RemovedFilePaths"));
						for (const FString& RemovedFilePath : RemovedFilePaths)
						{
							Writer->WriteValue(RemovedFilePath);
						}
						Writer->WriteArrayEnd();
						Writer->WriteArrayStart(TEXT("ChangedFilePaths"));
						for (const FString& ChangedFilePath : ChangedFilePaths)
						{
							Writer->WriteValue(ChangedFilePath);
						}
						Writer->WriteArrayEnd();
						Writer->WriteArrayStart(TEXT("UnchangedFilePaths"));
						for (const FString& UnchangedFilePath : UnchangedFilePaths)
						{
							Writer->WriteValue(UnchangedFilePath);
						}
						Writer->WriteArrayEnd();
						Writer->WriteArrayStart(TEXT("NewChunkPaths"));
						for (const FString& NewChunkPath : NewChunkPaths)
						{
							Writer->WriteValue(NewChunkPath);
						}
						Writer->WriteArrayEnd();
						Writer->WriteValue(TEXT("TotalChunkSize"), TotalChunkSize);
						Writer->WriteValue(TEXT("DeltaDownloadSize"), DeltaDownloadSize);
						Writer->WriteObjectStart(TEXT("IndividualTagDeltaSizes"));
						for (const TPair<FString, int64>& Pair : TagDeltaImpact)
						{
							Writer->WriteValue(Pair.Key, Pair.Value);
						}
						Writer->WriteObjectEnd();
						Writer->WriteObjectStart(TEXT("CompareTagSetDeltaSizes"));
						for (const TPair<FString, int64>& Pair : CompareTagSetDeltaImpact)
						{
							Writer->WriteValue(Pair.Key, Pair.Value);
						}
						Writer->WriteObjectEnd();
					}
					Writer->WriteObjectEnd();
				}
				Writer->WriteObjectEnd();
				Writer->Close();
				bSuccess = FFileHelper::SaveStringToFile(JsonOutput, *Configuration.OutputFilePath);
				if (!bSuccess)
				{
					UE_LOG(LogDiffManifests, Error, TEXT("Could not save output to %s"), *Configuration.OutputFilePath);
				}
			}
		}
		bShouldRun = false;
		return bSuccess;
	}

	void FDiffManifests::HandleDownloadComplete(int32 RequestId, const FDownloadRef& Download)
	{
		TPromise<FBuildPatchAppManifestPtr>* RelevantPromisePtr = RequestId == RequestIdManifestA ? &PromiseManifestA : RequestId == RequestIdManifestB ? &PromiseManifestB : nullptr;
		if (RelevantPromisePtr != nullptr)
		{
			if (Download->WasSuccessful())
			{
				Async<void>(EAsyncExecution::ThreadPool, [Download, RelevantPromisePtr]()
				{
					FBuildPatchAppManifestPtr Manifest = MakeShareable(new FBuildPatchAppManifest());
					if (!Manifest->DeserializeFromData(Download->GetData()))
					{
						Manifest.Reset();
					}
					RelevantPromisePtr->SetValue(Manifest);
				});
			}
			else
			{
				RelevantPromisePtr->SetValue(FBuildPatchAppManifestPtr());
			}
		}
	}

	IDiffManifests* FDiffManifestsFactory::Create(const FDiffManifestsConfiguration& Configuration)
	{
		return new FDiffManifests(Configuration);
	}
}