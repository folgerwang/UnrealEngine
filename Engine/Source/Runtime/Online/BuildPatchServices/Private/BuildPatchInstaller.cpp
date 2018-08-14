// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchInstaller.cpp: Implements the FBuildPatchInstaller class which
	controls the process of installing a build described by a build manifest.
=============================================================================*/

#include "BuildPatchInstaller.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "HAL/RunnableThread.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FeedbackContext.h"
#include "Math/UnitConversion.h"
#include "Containers/Ticker.h"
#include "HttpModule.h"
#include "BuildPatchFileConstructor.h"
#include "BuildPatchServicesModule.h"
#include "BuildPatchUtil.h"
#include "BuildPatchServicesPrivate.h"
#include "Common/HttpManager.h"
#include "Common/FileSystem.h"
#include "Installer/InstallerError.h"
#include "Installer/DiskChunkStore.h"
#include "Installer/CloudChunkSource.h"
#include "Installer/MemoryChunkStore.h"
#include "Installer/ChunkReferenceTracker.h"
#include "Installer/ChunkEvictionPolicy.h"
#include "Installer/DownloadService.h"
#include "Installer/ChunkDbChunkSource.h"
#include "Installer/InstallChunkSource.h"
#include "Installer/ChainedChunkSource.h"
#include "Installer/Verifier.h"
#include "Installer/FileAttribution.h"
#include "Installer/InstallerAnalytics.h"
#include "Installer/Prerequisites.h"
#include "Installer/MachineConfig.h"
#include "Installer/MessagePump.h"
#include "Installer/Statistics/MemoryChunkStoreStatistics.h"
#include "Installer/Statistics/DiskChunkStoreStatistics.h"
#include "Installer/Statistics/DownloadServiceStatistics.h"
#include "Installer/Statistics/ChunkDbChunkSourceStatistics.h"
#include "Installer/Statistics/InstallChunkSourceStatistics.h"
#include "Installer/Statistics/CloudChunkSourceStatistics.h"
#include "Installer/Statistics/FileConstructorStatistics.h"
#include "Installer/Statistics/VerifierStatistics.h"
#include "Installer/Statistics/FileOperationTracker.h"

namespace ConfigHelpers
{
	int32 LoadNumFileMoveRetries()
	{
		int32 MoveRetries = 5;
		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("NumFileMoveRetries"), MoveRetries, GEngineIni);
		return FMath::Clamp<int32>(MoveRetries, 1, 50);
	}

	int32 LoadNumInstallerRetries()
	{
		int32 InstallerRetries = 5;
		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("NumInstallerRetries"), InstallerRetries, GEngineIni);
		return FMath::Clamp<int32>(InstallerRetries, 1, 50);
	}

	float LoadDownloadSpeedAverageTime()
	{
		float AverageTime = 10.0;
		GConfig->GetFloat(TEXT("Portal.BuildPatch"), TEXT("DownloadSpeedAverageTime"), AverageTime, GEngineIni);
		return FMath::Clamp<float>(AverageTime, 1.0f, 30.0f);
	}

	float DownloadSpeedAverageTime()
	{
		static float AverageTime = LoadDownloadSpeedAverageTime();
		return AverageTime;
	}

	int32 NumFileMoveRetries()
	{
		static int32 MoveRetries = LoadNumFileMoveRetries();
		return MoveRetries;
	}

	int32 NumInstallerRetries()
	{
		static int32 InstallerRetries = LoadNumInstallerRetries();
		return InstallerRetries;
	}
}

namespace InstallerHelpers
{
	void LogBuildStatInfo(const FBuildInstallStats& BuildStats)
	{
		using namespace BuildPatchServices;
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: AppName: %s"), *BuildStats.AppName);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: AppInstalledVersion: %s"), *BuildStats.AppInstalledVersion);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: AppPatchVersion: %s"), *BuildStats.AppPatchVersion);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: CloudDirectory: %s"), *BuildStats.CloudDirectory);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumFilesInBuild: %u"), BuildStats.NumFilesInBuild);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumFilesOutdated: %u"), BuildStats.NumFilesOutdated);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumFilesToRemove: %u"), BuildStats.NumFilesToRemove);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumChunksRequired: %u"), BuildStats.NumChunksRequired);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: ChunksQueuedForDownload: %u"), BuildStats.ChunksQueuedForDownload);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: ChunksLocallyAvailable: %u"), BuildStats.ChunksLocallyAvailable);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: ChunksInChunkDbs: %u"), BuildStats.ChunksInChunkDbs);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumChunksDownloaded: %u"), BuildStats.NumChunksDownloaded);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumChunksRecycled: %u"), BuildStats.NumChunksRecycled);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumChunksReadFromChunkDbs: %u"), BuildStats.NumChunksReadFromChunkDbs);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumFailedDownloads: %u"), BuildStats.NumFailedDownloads);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumBadDownloads: %u"), BuildStats.NumBadDownloads);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumAbortedDownloads: %u"), BuildStats.NumAbortedDownloads);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumRecycleFailures: %u"), BuildStats.NumRecycleFailures);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumChunksStoreBooted: %u"), BuildStats.NumChunksStoreBooted);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumDriveStoreChunkLoads: %u"), BuildStats.NumDriveStoreChunkLoads);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumDriveStoreLoadFailures: %u"), BuildStats.NumDriveStoreLoadFailures);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumChunkDbChunksFailed: %u"), BuildStats.NumChunkDbChunksFailed);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: TotalDownloadedData: %llu"), BuildStats.TotalDownloadedData);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: AverageDownloadSpeed: %s bytes (%s, %s) /sec"), *FText::AsNumber(BuildStats.AverageDownloadSpeed).ToString(), *FText::AsMemory(BuildStats.AverageDownloadSpeed, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(BuildStats.AverageDownloadSpeed, EMemoryUnitStandard::IEC).ToString());
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: PeakDownloadSpeed: %s bytes (%s, %s) /sec"), *FText::AsNumber(BuildStats.PeakDownloadSpeed).ToString(), *FText::AsMemory(BuildStats.PeakDownloadSpeed, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(BuildStats.PeakDownloadSpeed, EMemoryUnitStandard::IEC).ToString());
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: TotalReadData: %llu"), BuildStats.TotalReadData);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: AverageDiskReadSpeed: %s bytes (%s, %s) /sec"), *FText::AsNumber(BuildStats.AverageDiskReadSpeed).ToString(), *FText::AsMemory(BuildStats.AverageDiskReadSpeed, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(BuildStats.AverageDiskReadSpeed, EMemoryUnitStandard::IEC).ToString());
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: PeakDiskReadSpeed: %s bytes (%s, %s) /sec"), *FText::AsNumber(BuildStats.PeakDiskReadSpeed).ToString(), *FText::AsMemory(BuildStats.PeakDiskReadSpeed, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(BuildStats.PeakDiskReadSpeed, EMemoryUnitStandard::IEC).ToString());
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: TotalWrittenData: %llu"), BuildStats.TotalWrittenData);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: AverageDiskWriteSpeed: %s bytes (%s, %s) /sec"), *FText::AsNumber(BuildStats.AverageDiskWriteSpeed).ToString(), *FText::AsMemory(BuildStats.AverageDiskWriteSpeed, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(BuildStats.AverageDiskWriteSpeed, EMemoryUnitStandard::IEC).ToString());
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: PeakDiskWriteSpeed: %s bytes (%s, %s) /sec"), *FText::AsNumber(BuildStats.PeakDiskWriteSpeed).ToString(), *FText::AsMemory(BuildStats.PeakDiskWriteSpeed, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(BuildStats.PeakDiskWriteSpeed, EMemoryUnitStandard::IEC).ToString());
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumFilesConstructed: %u"), BuildStats.NumFilesConstructed);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: InitializeTime: %s"), *FPlatformTime::PrettyTime(BuildStats.InitializeTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: TheoreticalDownloadTime: %s"), *FPlatformTime::PrettyTime(BuildStats.TheoreticalDownloadTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: ConstructTime: %s"), *FPlatformTime::PrettyTime(BuildStats.ConstructTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: MoveFromStageTime: %s"), *FPlatformTime::PrettyTime(BuildStats.MoveFromStageTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: FileAttributesTime: %s"), *FPlatformTime::PrettyTime(BuildStats.FileAttributesTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: VerifyTime: %s"), *FPlatformTime::PrettyTime(BuildStats.VerifyTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: CleanUpTime: %s"), *FPlatformTime::PrettyTime(BuildStats.CleanUpTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: PrereqTime: %s"), *FPlatformTime::PrettyTime(BuildStats.PrereqTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: ProcessPausedTime: %s"), *FPlatformTime::PrettyTime(BuildStats.ProcessPausedTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: ProcessActiveTime: %s"), *FPlatformTime::PrettyTime(BuildStats.ProcessActiveTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: ProcessExecuteTime: %s"), *FPlatformTime::PrettyTime(BuildStats.ProcessExecuteTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: ProcessSuccess: %s"), BuildStats.ProcessSuccess ? TEXT("TRUE") : TEXT("FALSE"));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: ErrorCode: %s"), *BuildStats.ErrorCode);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: FailureReasonText: %s"), *BuildStats.FailureReasonText.BuildSourceString());
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: FailureType: %s"), *EnumToString(BuildStats.FailureType));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumInstallRetries: %u"), BuildStats.NumInstallRetries);
		check(BuildStats.NumInstallRetries == BuildStats.RetryFailureTypes.Num() && BuildStats.NumInstallRetries == BuildStats.RetryErrorCodes.Num());
		for (uint32 RetryIdx = 0; RetryIdx < BuildStats.NumInstallRetries; ++RetryIdx)
		{
			UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: RetryFailureType %u: %s"), RetryIdx, *EnumToString(BuildStats.RetryFailureTypes[RetryIdx]));
			UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: RetryErrorCodes %u: %s"), RetryIdx, *BuildStats.RetryErrorCodes[RetryIdx]);
		}
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: FinalProgressValue: %f"), BuildStats.FinalProgress);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: OverallRequestSuccessRate: %f"), BuildStats.OverallRequestSuccessRate);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: ExcellentDownloadHealthTime: %s"), *FPlatformTime::PrettyTime(BuildStats.ExcellentDownloadHealthTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: GoodDownloadHealthTime: %s"), *FPlatformTime::PrettyTime(BuildStats.GoodDownloadHealthTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: OkDownloadHealthTime: %s"), *FPlatformTime::PrettyTime(BuildStats.OkDownloadHealthTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: PoorDownloadHealthTime: %s"), *FPlatformTime::PrettyTime(BuildStats.PoorDownloadHealthTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: DisconnectedDownloadHealthTime: %s"), *FPlatformTime::PrettyTime(BuildStats.DisconnectedDownloadHealthTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: AverageMemoryStoreUse: %.1f"), BuildStats.AverageMemoryStoreUse);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: PeakMemoryStoreUse: %u"), BuildStats.PeakMemoryStoreUse);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: AverageMemoryStoreRetained: %.1f"), BuildStats.AverageMemoryStoreRetained);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: PeakMemoryStoreRetained: %u"), BuildStats.PeakMemoryStoreRetained);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: MemoryStoreSize: %u"), BuildStats.MemoryStoreSize);
	}

	void LogBuildConfiguration(const BuildPatchServices::FInstallerConfiguration& InstallerConfiguration)
	{
		using namespace BuildPatchServices;
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Config: InstallManifest: 0x%p"), &InstallerConfiguration.InstallManifest.Get());
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Config: InstallManifest Version: %s"), *InstallerConfiguration.InstallManifest->GetVersionString());
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Config: CurrentManifest: 0x%p"), InstallerConfiguration.CurrentManifest.Get());
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Config: CurrentManifest Version: %s"), InstallerConfiguration.CurrentManifest.IsValid() ? *InstallerConfiguration.CurrentManifest->GetVersionString() : TEXT("NULL"));

		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Config: InstallDirectory: %s"), *InstallerConfiguration.InstallDirectory);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Config: StagingDirectory: %s"), *InstallerConfiguration.StagingDirectory);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Config: BackupDirectory: %s"), *InstallerConfiguration.BackupDirectory);

		for (const FString& DatabaseFile : InstallerConfiguration.ChunkDatabaseFiles)
		{
			UE_LOG(LogBuildPatchServices, Log, TEXT("Build Config: ChunkDatabaseFile: %s"), *DatabaseFile);
		}
		
		for (const FString& CloudDirectory : InstallerConfiguration.CloudDirectories)
		{
			UE_LOG(LogBuildPatchServices, Log, TEXT("Build Config: CloudDirectories: %s"), *CloudDirectory);
		}

		for (const FString& Tag : InstallerConfiguration.InstallTags)
		{
			UE_LOG(LogBuildPatchServices, Log, TEXT("Build Config: InstallTags: %s"), *Tag);
		}

		TSet<FString> ValidTags;
		InstallerConfiguration.InstallManifest->GetFileTagList(ValidTags);
		for (const FString& Tag : ValidTags)
		{
			UE_LOG(LogBuildPatchServices, Log, TEXT("Build Config: ValidTags: %s"), *Tag);
		}

		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Config: InstallMode: %s"), *EnumToString(InstallerConfiguration.InstallMode));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Config: VerifyMode: %s"), *EnumToString(InstallerConfiguration.VerifyMode));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Config: bIsRepair: %s"), (InstallerConfiguration.bIsRepair) ? TEXT("true") : TEXT("false"));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Config: bRunRequiredPrereqs: %s"), (InstallerConfiguration.bRunRequiredPrereqs) ? TEXT("true") : TEXT("false"));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Config: bAllowConcurrentExecution: %s"), (InstallerConfiguration.bAllowConcurrentExecution) ? TEXT("true") : TEXT("false"));
	}

	TSet<FGuid> GetMultipleReferencedChunks(const FBuildPatchAppManifestRef& Manifest)
	{
		TSet<FGuid> MultipleReferencedChunks;
		TSet<FGuid> AllReferencedChunks;
		TArray<FString> AllFiles;
		Manifest->GetFileList(AllFiles);
		for (const FString& File : AllFiles)
		{
			const FFileManifest* NewFileManifest = Manifest->GetFileManifest(File);
			if (NewFileManifest != nullptr)
			{
				for (const FChunkPart& ChunkPart : NewFileManifest->FileChunkParts)
				{
					if (AllReferencedChunks.Contains(ChunkPart.Guid))
					{
						MultipleReferencedChunks.Add(ChunkPart.Guid);
					}
					else
					{
						AllReferencedChunks.Add(ChunkPart.Guid);
					}
				}
			}
		}
		return MultipleReferencedChunks;
	}

	const TCHAR* GetVerifyErrorCode(const BuildPatchServices::EVerifyResult& VerifyResult)
	{
		using namespace BuildPatchServices;
		switch (VerifyResult)
		{
			case EVerifyResult::FileMissing: return VerifyErrorCodes::FileMissing;
			case EVerifyResult::OpenFileFailed: return VerifyErrorCodes::OpenFileFailed;
			case EVerifyResult::HashCheckFailed: return VerifyErrorCodes::HashCheckFailed;
			case EVerifyResult::FileSizeFailed: return VerifyErrorCodes::FileSizeFailed;
		}

		return VerifyErrorCodes::UnknownFail;
	}

	void LogAdditionalVerifyErrors(BuildPatchServices::EVerifyError Error, int32 Count)
	{
		using namespace BuildPatchServices;
		EVerifyResult VerifyResult;
		if (TryConvertToVerifyResult(Error, VerifyResult))
		{
			FString Prefix = InstallErrorPrefixes::ErrorTypeStrings[static_cast<int32>(EBuildPatchInstallError::BuildVerifyFail)];
			FString Suffix = InstallerHelpers::GetVerifyErrorCode(VerifyResult);
			UE_LOG(LogBuildPatchServices, Log, TEXT("Build verification error encountered: %s: %d"), *(Prefix + Suffix), Count);
		}
	}
}

namespace BuildPatchServices
{
	struct FScopedControllables
	{
	public:
		FScopedControllables(FCriticalSection* InSyncObject, TArray<IControllable*>& InRegistrationArray, bool& bInIsPaused, bool& bInShouldAbort)
			: SyncObject(InSyncObject)
			, RegistrationArray(InRegistrationArray)
			, bIsPaused(bInIsPaused)
			, bShouldAbort(bInShouldAbort)
		{
		}

		~FScopedControllables()
		{
			FScopeLock ScopeLock(SyncObject);
			for (IControllable* Controllable : RegisteredArray)
			{
				RegistrationArray.Remove(Controllable);
			}
		}

		void Register(IControllable* Controllable)
		{
			FScopeLock ScopeLock(SyncObject);
			RegistrationArray.Add(Controllable);
			RegisteredArray.Add(Controllable);
			if (bShouldAbort)
			{
				Controllable->Abort();
			}
			else
			{
				Controllable->SetPaused(bIsPaused);
			}
		}

	private:
		FCriticalSection* SyncObject;
		TArray<IControllable*>& RegistrationArray;
		TArray<IControllable*> RegisteredArray;
		bool& bIsPaused;
		bool& bShouldAbort;
	};

	struct FBuildPatchDownloadRecord
	{
		double StartTime;
		double EndTime;
		int64 DownloadSize;

		FBuildPatchDownloadRecord()
			: StartTime(0)
			, EndTime(0)
			, DownloadSize(0)
		{}

		friend bool operator<(const FBuildPatchDownloadRecord& Lhs, const FBuildPatchDownloadRecord& Rhs)
		{
			return Lhs.StartTime < Rhs.StartTime;
		}
	};

	enum class EMemoryStore : int32
	{
		Install = 0,
		Cloud
	};

	/* FBuildPatchInstaller implementation
	*****************************************************************************/
	FBuildPatchInstaller::FBuildPatchInstaller(FInstallerConfiguration InConfiguration, TMap<FString, FBuildPatchAppManifestRef> InInstallationInfo, const FString& InLocalMachineConfigFile, TSharedPtr<IAnalyticsProvider> InAnalytics, TSharedPtr<FHttpServiceTracker> InHttpTracker, FBuildPatchBoolManifestDelegate InOnCompleteDelegate)
		: Thread(nullptr)
		, OnCompleteDelegate(InOnCompleteDelegate)
		, Configuration(MoveTemp(InConfiguration))
		, CurrentBuildManifest(StaticCastSharedPtr<FBuildPatchAppManifest>(Configuration.CurrentManifest))
		, NewBuildManifest(StaticCastSharedRef<FBuildPatchAppManifest>(Configuration.InstallManifest))
		, DataStagingDir(Configuration.StagingDirectory / TEXT("PatchData"))
		, InstallStagingDir(Configuration.StagingDirectory / TEXT("Install"))
		, PreviousMoveMarker(Configuration.InstallDirectory / TEXT("$movedMarker"))
		, ThreadLock()
		, bIsFileData(NewBuildManifest->IsFileDataManifest())
		, bIsChunkData(!bIsFileData)
		, bSuccess(false)
		, bIsRunning(false)
		, bIsInited(false)
		, bFirstInstallIteration(true)
		, PreviousTotalDownloadRequired(0)
		, BuildStats()
		, BuildProgress()
		, bIsPaused(false)
		, bShouldAbort(false)
		, FilesInstalled()
		, TaggedFiles()
		, FilesToConstruct()
		, InstallationInfo(MoveTemp(InInstallationInfo))
		, LocalMachineConfigFile(InLocalMachineConfigFile)
		, HttpManager(FHttpManagerFactory::Create())
		, FileSystem(FFileSystemFactory::Create())
		, Platform(FPlatformFactory::Create())
		, InstallerError(FInstallerErrorFactory::Create())
		, Analytics(MoveTemp(InAnalytics))
		, HttpTracker(MoveTemp(InHttpTracker))
		, InstallerAnalytics(FInstallerAnalyticsFactory::Create(Analytics.Get(), HttpTracker.Get()))
		, FileOperationTracker(FFileOperationTrackerFactory::Create(FTicker::GetCoreTicker(), &NewBuildManifest.Get()))
		, MemoryChunkStoreAggregateStatistics(FMemoryChunkStoreAggregateStatisticsFactory::Create(InstallerHelpers::GetMultipleReferencedChunks(NewBuildManifest), FileOperationTracker.Get()))
		, DiskChunkStoreStatistics(FDiskChunkStoreStatisticsFactory::Create(InstallerAnalytics.Get(), FileOperationTracker.Get()))
		, DownloadSpeedRecorder(FSpeedRecorderFactory::Create())
		, DiskReadSpeedRecorder(FSpeedRecorderFactory::Create())
		, DiskWriteSpeedRecorder(FSpeedRecorderFactory::Create())
		, ChunkDbReadSpeedRecorder(FSpeedRecorderFactory::Create())
		, DownloadServiceStatistics(FDownloadServiceStatisticsFactory::Create(DownloadSpeedRecorder.Get(), InstallerAnalytics.Get(), &NewBuildManifest.Get()))
		, ChunkDbChunkSourceStatistics(FChunkDbChunkSourceStatisticsFactory::Create(ChunkDbReadSpeedRecorder.Get(), FileOperationTracker.Get()))
		, InstallChunkSourceStatistics(FInstallChunkSourceStatisticsFactory::Create(DiskReadSpeedRecorder.Get(), InstallerAnalytics.Get(), FileOperationTracker.Get()))
		, CloudChunkSourceStatistics(FCloudChunkSourceStatisticsFactory::Create(InstallerAnalytics.Get(), &BuildProgress, FileOperationTracker.Get()))
		, FileConstructorStatistics(FFileConstructorStatisticsFactory::Create(DiskReadSpeedRecorder.Get(), DiskWriteSpeedRecorder.Get(), &BuildProgress, FileOperationTracker.Get()))
		, VerifierStatistics(FVerifierStatisticsFactory::Create(DiskReadSpeedRecorder.Get(), &BuildProgress, FileOperationTracker.Get()))
		, DownloadService(FDownloadServiceFactory::Create(FTicker::GetCoreTicker(), HttpManager.Get(), FileSystem.Get(), DownloadServiceStatistics.Get(), InstallerAnalytics.Get()))
		, MessagePump(FMessagePumpFactory::Create())
		, Controllables()
	{
		UE_LOG(LogBuildPatchServices, Log, TEXT("FBuildPatchInstaller CTOR Configuration.InstallManifest Check:   0x%p"), &Configuration.InstallManifest.Get());
		UE_LOG(LogBuildPatchServices, Log, TEXT("FBuildPatchInstaller CTOR Configuration.CurrentManifest Check:   0x%p"), Configuration.CurrentManifest.Get());
		UE_LOG(LogBuildPatchServices, Log, TEXT("FBuildPatchInstaller CTOR CurrentBuildManifest Check:   0x%p"), CurrentBuildManifest.Get());
		UE_LOG(LogBuildPatchServices, Log, TEXT("FBuildPatchInstaller CTOR NewBuildManifest Check:   0x%p"), &NewBuildManifest.Get());

		FPaths::NormalizeDirectoryName(Configuration.InstallDirectory);
		FPaths::CollapseRelativeDirectories(Configuration.InstallDirectory);
		if (InstallationInfo.Contains(Configuration.InstallDirectory) == false && CurrentBuildManifest.IsValid())
		{
			InstallationInfo.Add(Configuration.InstallDirectory, CurrentBuildManifest.ToSharedRef());
		}
		InstallerError->RegisterForErrors([this]() { CancelInstall(); });
		Controllables.Add(&BuildProgress);
	}

	FBuildPatchInstaller::~FBuildPatchInstaller()
	{
		PreExit();
	}

	void FBuildPatchInstaller::PreExit()
	{
		// Set shutdown error so any running threads will exit if no error has already been set.
		if (bIsRunning)
		{
			InstallerError->SetError(EBuildPatchInstallError::ApplicationClosing, ApplicationClosedErrorCodes::ApplicationClosed);
		}
		if (Thread != nullptr)
		{
			Thread->WaitForCompletion();
			delete Thread;
			Thread = nullptr;
		}
		if (InstallerAnalytics.IsValid())
		{
			InstallerAnalytics->Flush();
		}
	}

	const IFileOperationTracker* FBuildPatchInstaller::GetFileOperationTracker() const
	{
		return FileOperationTracker.Get();
	}

	const ISpeedRecorder* FBuildPatchInstaller::GetDownloadSpeedRecorder() const
	{
		return DownloadSpeedRecorder.Get();
	}

	const ISpeedRecorder* FBuildPatchInstaller::GetDiskReadSpeedRecorder() const
	{
		return DiskReadSpeedRecorder.Get();
	}

	const ISpeedRecorder* FBuildPatchInstaller::GetChunkDbReadSpeedRecorder() const
	{
		return ChunkDbReadSpeedRecorder.Get();
	}

	const ISpeedRecorder* FBuildPatchInstaller::GetDiskWriteSpeedRecorder() const
	{
		return DiskWriteSpeedRecorder.Get();
	}

	const IDownloadServiceStatistics* FBuildPatchInstaller::GetDownloadServiceStatistics() const
	{
		return DownloadServiceStatistics.Get();
	}

	const IInstallChunkSourceStatistics* FBuildPatchInstaller::GetInstallChunkSourceStatistics() const
	{
		return InstallChunkSourceStatistics.Get();
	}

	const ICloudChunkSourceStatistics* FBuildPatchInstaller::GetCloudChunkSourceStatistics() const
	{
		return CloudChunkSourceStatistics.Get();
	}

	const IFileConstructorStatistics* FBuildPatchInstaller::GetFileConstructorStatistics() const
	{
		return FileConstructorStatistics.Get();
	}

	const IVerifierStatistics* FBuildPatchInstaller::GetVerifierStatistics() const
	{
		return VerifierStatistics.Get();
	}

	const IMemoryChunkStoreStatistics* FBuildPatchInstaller::GetCloudMemoryChunkStoreStatistics() const
	{
		return MemoryChunkStoreAggregateStatistics->Expose(static_cast<int32>(EMemoryStore::Cloud));
	}

	const IMemoryChunkStoreStatistics* FBuildPatchInstaller::GetInstallMemoryChunkStoreStatistics() const
	{
		return MemoryChunkStoreAggregateStatistics->Expose(static_cast<int32>(EMemoryStore::Install));
	}

	const IDiskChunkStoreStatistics* FBuildPatchInstaller::GetDiskChunkStoreStatistics() const
	{
		return DiskChunkStoreStatistics.Get();
	}

	const FInstallerConfiguration& FBuildPatchInstaller::GetConfiguration() const
	{
		return Configuration;
	}

	bool FBuildPatchInstaller::StartInstallation()
	{
		if (Thread == nullptr)
		{
			// Pre-process install tags. Doing this logic here means it doesn't need repeating around lower level code
			// No tags means full installation
			if (Configuration.InstallTags.Num() == 0)
			{
				NewBuildManifest->GetFileTagList(Configuration.InstallTags);
			}

			// Always require the empty tag
			Configuration.InstallTags.Add(TEXT(""));

			// Start thread!
			const TCHAR* ThreadName = TEXT("BuildPatchInstallerThread");
			Thread = FRunnableThread::Create(this, ThreadName);
		}
		return Thread != nullptr;
	}

	bool FBuildPatchInstaller::Initialize()
	{
		bool bInstallerInitSuccess = true;
		InstallerHelpers::LogBuildConfiguration(Configuration);

		// Check provided tags are all valid.
		TSet<FString> ValidTags;
		Configuration.InstallManifest->GetFileTagList(ValidTags);
		if (Configuration.InstallTags.Difference(ValidTags).Num() > 0)
		{
			UE_LOG(LogBuildPatchServices, Error, TEXT("Installer configuration: Invalid InstallTags provided."));
			InstallerError->SetError(EBuildPatchInstallError::InitializationError, InitializationErrorCodes::InvalidInstallTags, 0,
				NSLOCTEXT("BuildPatchInstallError", "InvalidInstallTags", "This installation could not continue due to a configuration issue. Please contact support."));
			bInstallerInitSuccess = false;
		}

		// Check that we were provided with a bound delegate.
		if (!OnCompleteDelegate.IsBound())
		{
			UE_LOG(LogBuildPatchServices, Error, TEXT("Installer configuration: Completion delegate not provided."));
			InstallerError->SetError(EBuildPatchInstallError::InitializationError, InitializationErrorCodes::MissingCompleteDelegate);
			bInstallerInitSuccess = false;
		}

		// Make sure we have install directory access.
		IFileManager::Get().MakeDirectory(*Configuration.InstallDirectory, true);
		if (!IFileManager::Get().DirectoryExists(*Configuration.InstallDirectory))
		{
			UE_LOG(LogBuildPatchServices, Error, TEXT("Installer setup: Inability to create InstallDirectory %s."), *Configuration.InstallDirectory);
			InstallerError->SetError(EBuildPatchInstallError::InitializationError, InitializationErrorCodes::MissingInstallDirectory, 0,
				FText::Format(NSLOCTEXT("BuildPatchInstallError", "MissingInstallDirectory", "The installation directory could not be created.\n{0}"), FText::FromString(Configuration.InstallDirectory)));
			bInstallerInitSuccess = false;
		}

		// Make sure we have staging directory access.
		IFileManager::Get().MakeDirectory(*Configuration.StagingDirectory, true);
		if (!IFileManager::Get().DirectoryExists(*Configuration.StagingDirectory))
		{
			UE_LOG(LogBuildPatchServices, Error, TEXT("Installer setup: Inability to create StagingDirectory %s."), *Configuration.StagingDirectory);
			InstallerError->SetError(EBuildPatchInstallError::InitializationError, InitializationErrorCodes::MissingStageDirectory, 0,
				FText::Format(NSLOCTEXT("BuildPatchInstallError", "MissingStageDirectory", "The following directory could not be created.\n{0}"), FText::FromString(Configuration.StagingDirectory)));
			bInstallerInitSuccess = false;
		}

		// Make sure that we have a prereq if we've specified a prereq only install.
		if (Configuration.InstallMode == EInstallMode::PrereqOnly && NewBuildManifest->GetPrereqPath().IsEmpty())
		{
			UE_LOG(LogBuildPatchServices, Error, TEXT("Installer setup: PrereqOnly install selected for manifest with no prereq."));
			InstallerError->SetError(EBuildPatchInstallError::InitializationError, InitializationErrorCodes::MissingPrereqForPrereqOnlyInstall, 0,
				NSLOCTEXT("BuildPatchInstallError", "MissingPrereqForPrereqOnlyInstall", "This installation could not continue due to a prerequisite configuration issue. Please contact support."));
			bInstallerInitSuccess = false;
		}

		// Init build statistics that are known.
		{
			FScopeLock Lock(&ThreadLock);
			BuildStats.AppName = NewBuildManifest->GetAppName();
			BuildStats.AppPatchVersion = NewBuildManifest->GetVersionString();
			BuildStats.AppInstalledVersion = CurrentBuildManifest.IsValid() ? CurrentBuildManifest->GetVersionString() : TEXT("NONE");
			BuildStats.CloudDirectory = Configuration.CloudDirectories[0];
			BuildStats.NumFilesInBuild = NewBuildManifest->GetNumFiles();
			BuildStats.ProcessSuccess = bInstallerInitSuccess;
			BuildStats.ErrorCode = InstallerError->GetErrorCode();
			BuildStats.FailureReasonText = InstallerError->GetErrorText();
			BuildStats.FailureType = InstallerError->GetErrorType();
		}

		bIsInited = true;
		return bInstallerInitSuccess;
	}

	uint32 FBuildPatchInstaller::Run()
	{
		// Make sure this function can never be parallelized
		static FCriticalSection SingletonFunctionLockCS;
		const bool bShouldLock = !Configuration.bAllowConcurrentExecution;
		if (bShouldLock)
		{
			SingletonFunctionLockCS.Lock();
		}
		bIsRunning = true;
		ProcessExecuteTimer.Start();
		ProcessActiveTimer.Start();

		// Init prereqs progress value
		const bool bInstallPrereqs = Configuration.bRunRequiredPrereqs && !NewBuildManifest->GetPrereqPath().IsEmpty();

		// Initialization
		InitializeTimer.Start();
		bool bProcessSuccess = Initialize();

		// Run if successful init
		if (bProcessSuccess)
		{
			// Keep track of files that failed verify
			TArray<FString> CorruptFiles;

			// Keep retrying the install while it is not canceled, or caused by download error
			bProcessSuccess = false;
			bool bCanRetry = true;
			int32 InstallRetries = ConfigHelpers::NumInstallerRetries();
			while (!bProcessSuccess && bCanRetry)
			{
				// No longer queued
				BuildProgress.SetStateProgress(EBuildPatchState::Queued, 1.0f);

				// Run the install
				bool bInstallSuccess = RunInstallation(CorruptFiles);
				InitializeTimer.Stop();
				BuildProgress.SetStateProgress(EBuildPatchState::PrerequisitesInstall, bInstallPrereqs ? 0.0f : 1.0f);
				if (bInstallSuccess)
				{
					BuildProgress.SetStateProgress(EBuildPatchState::Downloading, 1.0f);
					BuildProgress.SetStateProgress(EBuildPatchState::Installing, 1.0f);
				}

				// Backup local changes then move generated files
				bInstallSuccess = bInstallSuccess && RunBackupAndMove();

				// There is no more potential for initializing
				BuildProgress.SetStateProgress(EBuildPatchState::Initializing, 1.0f);

				// Setup file attributes
				bInstallSuccess = bInstallSuccess && RunFileAttributes(Configuration.bIsRepair);

				// Run Verification
				CorruptFiles.Empty();
				bProcessSuccess = bInstallSuccess && RunVerification(CorruptFiles);

				// Clean staging if INSTALL success
				BuildProgress.SetStateProgress(EBuildPatchState::CleanUp, 0.0f);
				if (bInstallSuccess)
				{
					CleanUpTimer.Start();
					if (Configuration.InstallMode == EInstallMode::StageFiles)
					{
						UE_LOG(LogBuildPatchServices, Log, TEXT("Deleting litter from staging area."));
						IFileManager::Get().DeleteDirectory(*DataStagingDir, false, true);
						IFileManager::Get().Delete(*(InstallStagingDir / TEXT("$resumeData")), false, true);
					}
					else
					{
						UE_LOG(LogBuildPatchServices, Log, TEXT("Deleting staging area."));
						IFileManager::Get().DeleteDirectory(*Configuration.StagingDirectory, false, true);
					}
					CleanUpTimer.Stop();
				}
				BuildProgress.SetStateProgress(EBuildPatchState::CleanUp, 1.0f);

				// Set if we can retry
				--InstallRetries;
				bCanRetry = InstallRetries > 0 && !InstallerError->IsCancelled() && InstallerError->CanRetry();

				// If successful or we will retry, remove the moved files marker
				if (bProcessSuccess || bCanRetry)
				{
					UE_LOG(LogBuildPatchServices, Log, TEXT("Reset MM."));
					IFileManager::Get().Delete(*PreviousMoveMarker, false, true);
				}

				// Setup end of attempt stats
				bFirstInstallIteration = false;
				float TempFinalProgress = BuildProgress.GetProgressNoMarquee();
				{
					FScopeLock Lock(&ThreadLock);
					BuildStats.NumInstallRetries = ConfigHelpers::NumInstallerRetries() - (InstallRetries + 1);
					BuildStats.FinalProgress = TempFinalProgress;
					// If we failed, and will retry, record this failure type and reset the abort flag
					if (!bProcessSuccess && bCanRetry)
					{
						BuildStats.RetryFailureTypes.Add(InstallerError->GetErrorType());
						BuildStats.RetryErrorCodes.Add(InstallerError->GetErrorCode());
						bShouldAbort = false;
					}
				}
			}
		}

		if (bProcessSuccess)
		{
			// Run the prerequisites installer if this is our first install and the manifest has prerequisites info
			if (bInstallPrereqs)
			{
				PrereqTimer.Start();
				bProcessSuccess &= RunPrerequisites();
				PrereqTimer.Stop();
			}
		}

		// Make sure all timers are stopped
		InitializeTimer.Stop();
		ConstructTimer.Stop();
		MoveFromStageTimer.Stop();
		FileAttributesTimer.Stop();
		VerifyTimer.Stop();
		CleanUpTimer.Stop();
		PrereqTimer.Stop();
		ProcessPausedTimer.Stop();
		ProcessActiveTimer.Stop();
		ProcessExecuteTimer.Stop();

		// Set final stat values and log out results
		bSuccess = bProcessSuccess;
		{
			FScopeLock Lock(&ThreadLock);
			BuildStats.InitializeTime = InitializeTimer.GetSeconds();
			BuildStats.ConstructTime = ConstructTimer.GetSeconds();
			BuildStats.MoveFromStageTime = MoveFromStageTimer.GetSeconds();
			BuildStats.FileAttributesTime = FileAttributesTimer.GetSeconds();
			BuildStats.VerifyTime = VerifyTimer.GetSeconds();
			BuildStats.CleanUpTime = CleanUpTimer.GetSeconds();
			BuildStats.PrereqTime = PrereqTimer.GetSeconds();
			BuildStats.ProcessPausedTime = ProcessPausedTimer.GetSeconds();
			BuildStats.ProcessActiveTime = ProcessActiveTimer.GetSeconds();
			BuildStats.ProcessExecuteTime = ProcessExecuteTimer.GetSeconds();
			BuildStats.ProcessSuccess = bProcessSuccess;
			BuildStats.ErrorCode = InstallerError->GetErrorCode();
			BuildStats.FailureReasonText = InstallerError->GetErrorText();
			BuildStats.FailureType = InstallerError->GetErrorType();
		}

		// Mark that we are done
		bIsRunning = false;
		if (bShouldLock)
		{
			SingletonFunctionLockCS.Unlock();
		}
		return bSuccess ? 0 : 1;
	}

	bool FBuildPatchInstaller::CheckForExternallyInstalledFiles()
	{
		// Check the marker file for a previous installation unfinished
		if (IPlatformFile::GetPlatformPhysical().FileExists(*PreviousMoveMarker))
		{
			return true;
		}

		// If we are patching, but without the marker, we should not return true, the existing files will be old installation
		if (CurrentBuildManifest.IsValid())
		{
			return false;
		}

		// Check if any required file is potentially already in place, by comparing file size as a quick 'same file' check
		TArray<FString> BuildFiles;
		NewBuildManifest->GetFileList(BuildFiles);
		for (const FString& BuildFile : BuildFiles)
		{
			if (NewBuildManifest->GetFileSize(BuildFile) == IFileManager::Get().FileSize(*(Configuration.InstallDirectory / BuildFile)))
			{
				return true;
			}
		}
		return false;
	}

	FChunkDbSourceConfig FBuildPatchInstaller::BuildChunkDbSourceConfig()
	{
		FChunkDbSourceConfig ChunkDbSourceConfig(Configuration.ChunkDatabaseFiles);

		// Load batch fetch config.
		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("ChunkDbSourcePreFetchMinimum"), ChunkDbSourceConfig.PreFetchMinimum, GEngineIni);
		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("ChunkDbSourcePreFetchMaximum"), ChunkDbSourceConfig.PreFetchMaximum, GEngineIni);
		ChunkDbSourceConfig.PreFetchMinimum = FMath::Clamp<int32>(ChunkDbSourceConfig.PreFetchMinimum, 1, 1000);
		ChunkDbSourceConfig.PreFetchMaximum = FMath::Clamp<int32>(ChunkDbSourceConfig.PreFetchMaximum, ChunkDbSourceConfig.PreFetchMinimum, 1000);

		// Load reopen retry time.
		GConfig->GetFloat(TEXT("Portal.BuildPatch"), TEXT("ChunkDbSourceChunkDbOpenRetryTime"), ChunkDbSourceConfig.ChunkDbOpenRetryTime, GEngineIni);
		ChunkDbSourceConfig.ChunkDbOpenRetryTime = FMath::Clamp<float>(ChunkDbSourceConfig.ChunkDbOpenRetryTime, 0.5f, 60.0f);

		return ChunkDbSourceConfig;
	}

	FInstallSourceConfig FBuildPatchInstaller::BuildInstallSourceConfig(TSet<FGuid> ChunkIgnoreSet)
	{
		FInstallSourceConfig InstallSourceConfig;
		InstallSourceConfig.ChunkIgnoreSet = MoveTemp(ChunkIgnoreSet);

		// Load batch fetch config
		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("InstallSourceBatchFetchMinimum"), InstallSourceConfig.BatchFetchMinimum, GEngineIni);
		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("InstallSourceBatchFetchMaximum"), InstallSourceConfig.BatchFetchMaximum, GEngineIni);
		InstallSourceConfig.BatchFetchMinimum = FMath::Clamp<int32>(InstallSourceConfig.BatchFetchMinimum, 1, 1000);
		InstallSourceConfig.BatchFetchMaximum = FMath::Clamp<int32>(InstallSourceConfig.BatchFetchMaximum, InstallSourceConfig.BatchFetchMinimum, 1000);

		return InstallSourceConfig;
	}

	FCloudSourceConfig FBuildPatchInstaller::BuildCloudSourceConfig()
	{
		FCloudSourceConfig CloudSourceConfig(Configuration.CloudDirectories);

		// Load simultaneous downloads from engine config.
		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("ChunkDownloads"), CloudSourceConfig.NumSimultaneousDownloads, GEngineIni);
		CloudSourceConfig.NumSimultaneousDownloads = FMath::Clamp<int32>(CloudSourceConfig.NumSimultaneousDownloads, 1, 100);

		// Load max download retry count from engine config.
		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("ChunkRetries"), CloudSourceConfig.MaxRetryCount, GEngineIni);
		CloudSourceConfig.MaxRetryCount = FMath::Clamp<int32>(CloudSourceConfig.MaxRetryCount, -1, 1000);

		// Load prefetch config
		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("CloudSourcePreFetchMinimum"), CloudSourceConfig.PreFetchMinimum, GEngineIni);
		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("CloudSourcePreFetchMaximum"), CloudSourceConfig.PreFetchMaximum, GEngineIni);
		CloudSourceConfig.PreFetchMinimum = FMath::Clamp<int32>(CloudSourceConfig.PreFetchMinimum, 1, 1000);
		CloudSourceConfig.PreFetchMaximum = FMath::Clamp<int32>(CloudSourceConfig.PreFetchMaximum, CloudSourceConfig.PreFetchMinimum, 1000);

		// Load retry times from engine config.
		TArray<FString> ConfigStrings;
		GConfig->GetArray(TEXT("Portal.BuildPatch"), TEXT("RetryTimes"), ConfigStrings, GEngineIni);
		bool bReadArraySuccess = ConfigStrings.Num() > 0;
		TArray<float> RetryDelayTimes;
		RetryDelayTimes.AddZeroed(ConfigStrings.Num());
		for (int32 TimeIdx = 0; TimeIdx < ConfigStrings.Num() && bReadArraySuccess; ++TimeIdx)
		{
			float TimeValue = FPlatformString::Atof(*ConfigStrings[TimeIdx]);
			// Atof will return 0.0 if failed to parse, and we don't expect a time of 0.0 so presume error
			if (TimeValue > 0.0f)
			{
				RetryDelayTimes[TimeIdx] = FMath::Clamp<float>(TimeValue, 0.5f, 300.0f);
			}
			else
			{
				bReadArraySuccess = false;
			}
		}
		// If the retry array was parsed successfully, set on config.
		if (bReadArraySuccess)
		{
			CloudSourceConfig.RetryDelayTimes = MoveTemp(RetryDelayTimes);
		}

		// Load percentiles for download health groupings from engine config.
		// If the enum was changed since writing, the config here needs updating.
		check((int32)EBuildPatchDownloadHealth::NUM_Values == 5);
		TArray<float> HealthPercentages;
		HealthPercentages.AddZeroed((int32)EBuildPatchDownloadHealth::NUM_Values);
		if (GConfig->GetFloat(TEXT("Portal.BuildPatch"), TEXT("OKHealth"), HealthPercentages[(int32)EBuildPatchDownloadHealth::OK], GEngineIni)
		 && GConfig->GetFloat(TEXT("Portal.BuildPatch"), TEXT("GoodHealth"), HealthPercentages[(int32)EBuildPatchDownloadHealth::Good], GEngineIni)
		 && GConfig->GetFloat(TEXT("Portal.BuildPatch"), TEXT("ExcellentHealth"), HealthPercentages[(int32)EBuildPatchDownloadHealth::Excellent], GEngineIni))
		{
			CloudSourceConfig.HealthPercentages = MoveTemp(HealthPercentages);
		}

		// Load the delay for how long we get no data for until determining the health as disconnected.
		GConfig->GetFloat(TEXT("Portal.BuildPatch"), TEXT("DisconnectedDelay"), CloudSourceConfig.DisconnectedDelay, GEngineIni);
		CloudSourceConfig.DisconnectedDelay = FMath::Clamp<float>(CloudSourceConfig.DisconnectedDelay, 1.0f, 30.0f);

		// We tell the cloud source to only start downloads once it receives the first get call.
		CloudSourceConfig.bBeginDownloadsOnFirstGet = true;

		return CloudSourceConfig;
	}

	bool FBuildPatchInstaller::RunInstallation(TArray<FString>& CorruptFiles)
	{
		UE_LOG(LogBuildPatchServices, Log, TEXT("Starting Installation"));
		// Save the staging directories
		FPaths::NormalizeDirectoryName(DataStagingDir);
		FPaths::NormalizeDirectoryName(InstallStagingDir);

		// Make sure staging directories exist
		IFileManager::Get().MakeDirectory(*DataStagingDir, true);
		IFileManager::Get().MakeDirectory(*InstallStagingDir, true);

		// Reset our error and build progress
		InstallerError.Reset(FInstallerErrorFactory::Create());
		InstallerError->RegisterForErrors([this]() { CancelInstall(); });
		BuildProgress.Reset();
		BuildProgress.SetStateProgress(EBuildPatchState::Queued, 1.0f);
		BuildProgress.SetStateProgress(EBuildPatchState::Initializing, 0.01f);
		BuildProgress.SetStateProgress(EBuildPatchState::CleanUp, 0.0f);

		// Store some totals
		const uint32 NumFilesInBuild = NewBuildManifest->GetNumFiles();

		// Get the list of required files, by the tags
		TaggedFiles.Empty();
		NewBuildManifest->GetTaggedFileList(Configuration.InstallTags, TaggedFiles);

		const bool bIsPrereqOnly = Configuration.InstallMode == EInstallMode::PrereqOnly;
		const bool bHasCorruptFiles = CorruptFiles.Num() > 0;

		// Get the list of files actually needing construction
		FilesToConstruct.Empty();
		if (bHasCorruptFiles)
		{
			FilesToConstruct.Append(CorruptFiles);
		}
		else if (bIsPrereqOnly)
		{
			FilesToConstruct.Add(NewBuildManifest->GetPrereqPath());
		}
		else
		{
			TSet<FString> OutdatedFiles;
			NewBuildManifest->GetOutdatedFiles(CurrentBuildManifest, Configuration.InstallDirectory, OutdatedFiles);
			FilesToConstruct = OutdatedFiles.Intersect(TaggedFiles);
		}
		FilesToConstruct.Sort(TLess<FString>());
		UE_LOG(LogBuildPatchServices, Log, TEXT("Requiring %d files"), FilesToConstruct.Num());

		// Check if we should skip out of this process due to existing installation,
		// that will mean we start with the verification stage
		if (!bHasCorruptFiles && (bIsPrereqOnly || CheckForExternallyInstalledFiles()))
		{
			UE_LOG(LogBuildPatchServices, Log, TEXT("Detected previous staging completed, or existing files in target directory"));
			// Set weights for verify only
			BuildProgress.SetStateWeight(EBuildPatchState::Downloading, 0.0f);
			BuildProgress.SetStateWeight(EBuildPatchState::Installing, 0.0f);
			BuildProgress.SetStateWeight(EBuildPatchState::MovingToInstall, 0.0f);
			BuildProgress.SetStateWeight(EBuildPatchState::SettingAttributes, 0.2f);
			BuildProgress.SetStateWeight(EBuildPatchState::BuildVerification, 1.0f);
			// Mark all installation steps complete
			BuildProgress.SetStateProgress(EBuildPatchState::Initializing, 1.0f);
			BuildProgress.SetStateProgress(EBuildPatchState::Resuming, 1.0f);
			BuildProgress.SetStateProgress(EBuildPatchState::Downloading, 1.0f);
			BuildProgress.SetStateProgress(EBuildPatchState::Installing, 1.0f);
			BuildProgress.SetStateProgress(EBuildPatchState::MovingToInstall, 1.0f);
			return true;
		}

		if (!bHasCorruptFiles)
		{
			FScopeLock Lock(&ThreadLock);
			BuildStats.NumFilesOutdated = FilesToConstruct.Num();
		}

		// Make sure all the files won't exceed the maximum path length
		for (const FString& FileToConstruct : FilesToConstruct)
		{
			const FString InstallConstructionFile = Configuration.InstallDirectory / FileToConstruct;
			const FString StagedConstructionFile = InstallStagingDir / FileToConstruct;
			if (InstallConstructionFile.Len() >= PLATFORM_MAX_FILEPATH_LENGTH)
			{
				UE_LOG(LogBuildPatchServices, Error, TEXT("Could not create new file due to exceeding maximum path length %s"), *InstallConstructionFile);
				InstallerError->SetError(EBuildPatchInstallError::PathLengthExceeded, PathLengthErrorCodes::InstallDirectory);
				return false;
			}
			if (StagedConstructionFile.Len() >= PLATFORM_MAX_FILEPATH_LENGTH)
			{
				UE_LOG(LogBuildPatchServices, Error, TEXT("Could not create new file due to exceeding maximum path length %s"), *StagedConstructionFile);
				InstallerError->SetError(EBuildPatchInstallError::PathLengthExceeded, PathLengthErrorCodes::StagingDirectory);
				return false;
			}
		}

		// Set initial states on IO state tracker.
		const bool bVerifyAllFiles = Configuration.VerifyMode == EVerifyMode::ShaVerifyAllFiles || Configuration.VerifyMode == EVerifyMode::FileSizeCheckAllFiles;
		const EFileOperationState UntouchedFileState = (Configuration.bIsRepair || bVerifyAllFiles) ? EFileOperationState::Installed : EFileOperationState::Complete;
		for (const FString& TaggedFile : TaggedFiles)
		{
			if (!FilesToConstruct.Contains(TaggedFile))
			{
				FileOperationTracker->OnFileStateUpdate(TaggedFile, UntouchedFileState);
			}
		}

		// Default chunk store sizes to tie in with the default prefetch maxes for source configs.
		// Cloud chunk source will share store with chunkdb source, since chunkdb is designed for standing in place of the need to download.
		const int32 DefaultChunkDbMaxRead = FChunkDbSourceConfig({}).PreFetchMaximum;
		const int32 DefaultInstallMaxRead = FInstallSourceConfig().BatchFetchMaximum;
		const int32 DefaultCloudMaxRead = FCloudSourceConfig({}).PreFetchMaximum;
		int32 CloudChunkStoreMemorySize = DefaultCloudMaxRead + DefaultChunkDbMaxRead;
		int32 InstallChunkStoreMemorySize = DefaultInstallMaxRead;
		// Load overridden sizes from config.
		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("CloudChunkStoreMemorySize"), CloudChunkStoreMemorySize, GEngineIni);
		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("InstallChunkStoreMemorySize"), InstallChunkStoreMemorySize, GEngineIni);
		// Clamp to sensible limits.
		CloudChunkStoreMemorySize = FMath::Clamp<int32>(CloudChunkStoreMemorySize, 32, 2048);
		InstallChunkStoreMemorySize = FMath::Clamp<int32>(InstallChunkStoreMemorySize, 32, 2048);
		// Cache the last download requirement incase we are running a retry.
		PreviousTotalDownloadRequired.Add(CloudChunkSourceStatistics->GetRequiredDownloadSize());
		// Reset so that we don't double count data.
		CloudChunkSourceStatistics->OnRequiredDataUpdated(0);
		CloudChunkSourceStatistics->OnReceivedDataUpdated(0);

		// Scoped systems composition and execution.
		{
			TUniquePtr<IChunkDataSerialization> ChunkDataSerialization(FChunkDataSerializationFactory::Create(
				FileSystem.Get()));
			TUniquePtr<IChunkReferenceTracker> ChunkReferenceTracker(FChunkReferenceTrackerFactory::Create(
				NewBuildManifest,
				FilesToConstruct));
			TSet<FGuid> ReferencedChunks = ChunkReferenceTracker->GetReferencedChunks();
			TUniquePtr<IChunkEvictionPolicy> MemoryEvictionPolicy(FChunkEvictionPolicyFactory::Create(
				ChunkReferenceTracker.Get()));
			TUniquePtr<IDiskChunkStore> DiskOverflowStore(FDiskChunkStoreFactory::Create(
				FileSystem.Get(),
				ChunkDataSerialization.Get(),
				DiskChunkStoreStatistics.Get(),
				FDiskChunkStoreConfig(DataStagingDir)));
			TUniquePtr<IMemoryChunkStore> InstallChunkStore(FMemoryChunkStoreFactory::Create(
				InstallChunkStoreMemorySize,
				MemoryEvictionPolicy.Get(),
				DiskOverflowStore.Get(),
				MemoryChunkStoreAggregateStatistics->Expose(static_cast<int32>(EMemoryStore::Install))));
			TUniquePtr<IMemoryChunkStore> CloudChunkStore(FMemoryChunkStoreFactory::Create(
				CloudChunkStoreMemorySize,
				MemoryEvictionPolicy.Get(),
				DiskOverflowStore.Get(),
				MemoryChunkStoreAggregateStatistics->Expose(static_cast<int32>(EMemoryStore::Cloud))));
			TUniquePtr<IChunkDbChunkSource> ChunkDbChunkSource(FChunkDbChunkSourceFactory::Create(
				BuildChunkDbSourceConfig(),
				Platform.Get(),
				FileSystem.Get(),
				CloudChunkStore.Get(),
				ChunkReferenceTracker.Get(),
				ChunkDataSerialization.Get(),
				MessagePump.Get(),
				InstallerError.Get(),
				ChunkDbChunkSourceStatistics.Get()));
			TUniquePtr<IInstallChunkSource> InstallChunkSource(FInstallChunkSourceFactory::Create(
				BuildInstallSourceConfig(ChunkDbChunkSource->GetAvailableChunks()),
				FileSystem.Get(),
				InstallChunkStore.Get(),
				ChunkReferenceTracker.Get(),
				InstallerError.Get(),
				InstallChunkSourceStatistics.Get(),
				InstallationInfo,
				NewBuildManifest));
			const TSet<FGuid> InitialDownloadChunks = ReferencedChunks.Difference(InstallChunkSource->GetAvailableChunks()).Difference(ChunkDbChunkSource->GetAvailableChunks());
			FileOperationTracker->OnDataStateUpdate(ReferencedChunks.Intersect(ChunkDbChunkSource->GetAvailableChunks()), EFileOperationState::PendingLocalChunkDbData);
			FileOperationTracker->OnDataStateUpdate(ReferencedChunks.Intersect(InstallChunkSource->GetAvailableChunks()).Difference(ChunkDbChunkSource->GetAvailableChunks()), EFileOperationState::PendingLocalInstallData);
			FileOperationTracker->OnDataStateUpdate(InitialDownloadChunks, EFileOperationState::PendingRemoteCloudData);
			TUniquePtr<ICloudChunkSource> CloudChunkSource(FCloudChunkSourceFactory::Create(
				BuildCloudSourceConfig(),
				Platform.Get(),
				CloudChunkStore.Get(),
				DownloadService.Get(),
				ChunkReferenceTracker.Get(),
				ChunkDataSerialization.Get(),
				MessagePump.Get(),
				InstallerError.Get(),
				CloudChunkSourceStatistics.Get(),
				NewBuildManifest,
				InitialDownloadChunks));
			TArray<IChunkSource*> ChunkSources;
			ChunkSources.Add(ChunkDbChunkSource.Get());
			ChunkSources.Add(InstallChunkSource.Get());
			ChunkSources.Add(CloudChunkSource.Get());
			TUniquePtr<IChainedChunkSource> ChainedChunkSource(FChainedChunkSourceFactory::Create(
				ChunkSources));
			TUniquePtr<FBuildPatchFileConstructor> FileConstructor(new FBuildPatchFileConstructor(
				FFileConstructorConfig({
					NewBuildManifest,
					CurrentBuildManifest,
					Configuration.InstallDirectory,
					InstallStagingDir,
					FilesToConstruct.Array(),
					Configuration.InstallMode}),
				FileSystem.Get(),
				ChainedChunkSource.Get(),
				ChunkReferenceTracker.Get(),
				InstallerError.Get(),
				InstallerAnalytics.Get(),
				FileConstructorStatistics.Get()));
			FDelegateHandle OnBeforeDeleteFileHandle = FileConstructor->OnBeforeDeleteFile().AddLambda([this, &InstallChunkSource](const FString& FilePath)
			{
				FString BuildRelativeFilename = FilePath;
				BuildRelativeFilename.RemoveFromStart(Configuration.InstallDirectory);
				BuildRelativeFilename.RemoveFromStart(TEXT("/"));
				OldFilesRemovedBySystem.Add(BuildRelativeFilename);
				InstallChunkSource->HarvestRemainingChunksFromFile(FilePath);
			});
			TFunction<void(const FGuid&)> LostChunkCallback = [&ChainedChunkSource](const FGuid& LostChunk)
			{
				ChainedChunkSource->AddRepeatRequirement(LostChunk);
			};
			DiskOverflowStore->SetLostChunkCallback(LostChunkCallback);
			InstallChunkStore->SetLostChunkCallback(LostChunkCallback);
			CloudChunkStore->SetLostChunkCallback(LostChunkCallback);


			// Register controllables.
			FScopedControllables ScopedControllables(&ThreadLock, Controllables, bIsPaused, bShouldAbort);
			ScopedControllables.Register(InstallChunkSource.Get());
			ScopedControllables.Register(CloudChunkSource.Get());
			ScopedControllables.Register(FileConstructor.Get());

			// Set chunk counter stats.
			if(!bHasCorruptFiles)
			{
				FScopeLock Lock(&ThreadLock);
				BuildStats.NumChunksRequired = ReferencedChunks.Num();
				BuildStats.ChunksQueuedForDownload = InitialDownloadChunks.Num();
				BuildStats.ChunksLocallyAvailable = ReferencedChunks.Intersect(InstallChunkSource->GetAvailableChunks()).Num();
				BuildStats.ChunksInChunkDbs = ReferencedChunks.Intersect(ChunkDbChunkSource->GetAvailableChunks()).Num();
			}

			// Setup some weightings for the progress tracking
			const float NumRequiredChunksFloat = ReferencedChunks.Num();
			const bool bHasFileAttributes = NewBuildManifest->HasFileAttributes();
			const float AttributesWeight = bHasFileAttributes ? Configuration.bIsRepair ? 1.0f / 50.0f : 1.0f / 20.0f : 0.0f;
			const float VerifyWeight = Configuration.VerifyMode == EVerifyMode::ShaVerifyAllFiles || Configuration.VerifyMode == EVerifyMode::ShaVerifyTouchedFiles ? 1.1f / 9.0f : 0.3f / 9.0f;
			BuildProgress.SetStateWeight(EBuildPatchState::Downloading, 0.0f);
			BuildProgress.SetStateWeight(EBuildPatchState::Installing, FilesToConstruct.Num() > 0 ? 1.0f : 0.0f);
			BuildProgress.SetStateWeight(EBuildPatchState::MovingToInstall, FilesToConstruct.Num() > 0 ? 0.05f : 0.0f);
			BuildProgress.SetStateWeight(EBuildPatchState::SettingAttributes, AttributesWeight);
			BuildProgress.SetStateWeight(EBuildPatchState::BuildVerification, VerifyWeight);

			// If this is a repair operation, start off with install and download complete
			if (Configuration.bIsRepair)
			{
				UE_LOG(LogBuildPatchServices, Log, TEXT("Performing a repair operation"));
				BuildProgress.SetStateProgress(EBuildPatchState::Downloading, 1.0f);
				BuildProgress.SetStateProgress(EBuildPatchState::Installing, 1.0f);
				BuildProgress.SetStateProgress(EBuildPatchState::MovingToInstall, 1.0f);
			}

			// Initializing is now complete if we are constructing files
			BuildProgress.SetStateProgress(EBuildPatchState::Initializing, FilesToConstruct.Num() > 0 ? 1.0f : 0.0f);
			InitializeTimer.Stop();

			// Wait for the file constructor to complete
			ConstructTimer.Start();
			FileConstructor->Wait();
			ConstructTimer.Stop();
			FileConstructor->OnBeforeDeleteFile().Remove(OnBeforeDeleteFileHandle);
			UE_LOG(LogBuildPatchServices, Log, TEXT("File construction complete"));
		}

		// Process some final stats.
		{
			FScopeLock Lock(&ThreadLock);
			BuildStats.NumChunksDownloaded = DownloadServiceStatistics->GetNumSuccessfulChunkDownloads();
			BuildStats.NumFailedDownloads = DownloadServiceStatistics->GetNumFailedChunkDownloads();
			BuildStats.NumBadDownloads = CloudChunkSourceStatistics->GetNumCorruptChunkDownloads();
			BuildStats.NumAbortedDownloads = CloudChunkSourceStatistics->GetNumAbortedChunkDownloads();
			BuildStats.OverallRequestSuccessRate = CloudChunkSourceStatistics->GetDownloadSuccessRate();
			BuildStats.NumChunksRecycled = InstallChunkSourceStatistics->GetNumSuccessfulChunkRecycles();
			BuildStats.NumChunksReadFromChunkDbs = ChunkDbChunkSourceStatistics->GetNumSuccessfulLoads();
			BuildStats.NumRecycleFailures = InstallChunkSourceStatistics->GetNumFailedChunkRecycles();
			BuildStats.NumChunksStoreBooted = MemoryChunkStoreAggregateStatistics->GetTotalNumBooted();
			BuildStats.NumDriveStoreChunkLoads = DiskChunkStoreStatistics->GetNumSuccessfulLoads();
			BuildStats.NumDriveStoreLoadFailures = DiskChunkStoreStatistics->GetNumFailedLoads();
			BuildStats.NumChunkDbChunksFailed = ChunkDbChunkSourceStatistics->GetNumFailedLoads();
			TArray<float> HealthTimers = CloudChunkSourceStatistics->GetDownloadHealthTimers();
			BuildStats.ExcellentDownloadHealthTime = HealthTimers[(int32)EBuildPatchDownloadHealth::Excellent];
			BuildStats.GoodDownloadHealthTime = HealthTimers[(int32)EBuildPatchDownloadHealth::Good];
			BuildStats.OkDownloadHealthTime = HealthTimers[(int32)EBuildPatchDownloadHealth::OK];
			BuildStats.PoorDownloadHealthTime = HealthTimers[(int32)EBuildPatchDownloadHealth::Poor];
			BuildStats.DisconnectedDownloadHealthTime = HealthTimers[(int32)EBuildPatchDownloadHealth::Disconnected];
		}

		UE_LOG(LogBuildPatchServices, Log, TEXT("Staged install complete"));

		return !InstallerError->HasError();
	}

	bool FBuildPatchInstaller::RunPrerequisites()
	{
		TUniquePtr<IMachineConfig> MachineConfig(FMachineConfigFactory::Create(LocalMachineConfigFile, true));
		TUniquePtr<IPrerequisites> Prerequisites(FPrerequisitesFactory::Create(
			MachineConfig.Get(),
			InstallerAnalytics.Get(),
			InstallerError.Get(),
			FileSystem.Get(),
			Platform.Get()));

		return Prerequisites->RunPrereqs(NewBuildManifest, Configuration, InstallStagingDir, BuildProgress);
	}

	void FBuildPatchInstaller::CleanupEmptyDirectories(const FString& RootDirectory)
	{
		TArray<FString> SubDirNames;
		IFileManager::Get().FindFiles(SubDirNames, *(RootDirectory / TEXT("*")), false, true);
		for(auto DirName : SubDirNames)
		{
			CleanupEmptyDirectories(*(RootDirectory / DirName));
		}

		TArray<FString> SubFileNames;
		IFileManager::Get().FindFilesRecursive(SubFileNames, *RootDirectory, TEXT("*.*"), true, false);
		if (SubFileNames.Num() == 0)
		{
	#if PLATFORM_MAC
			// On Mac we need to delete the .DS_Store file, but FindFiles() skips .DS_Store files.
			IFileManager::Get().Delete(*(RootDirectory / TEXT(".DS_Store")), false, true);
	#endif

			bool bDeleteSuccess = IFileManager::Get().DeleteDirectory(*RootDirectory, false, true);
			const uint32 LastError = FPlatformMisc::GetLastError();
			UE_LOG(LogBuildPatchServices, Log, TEXT("Deleted Empty Folder (%u,%u) %s"), bDeleteSuccess ? 1 : 0, LastError, *RootDirectory);
		}
	}

	bool FBuildPatchInstaller::RunBackupAndMove()
	{
		// We skip this step if performing stage only
		bool bMoveSuccess = true;
		if (Configuration.InstallMode == EInstallMode::StageFiles || (bFirstInstallIteration && Configuration.InstallMode == EInstallMode::PrereqOnly))
		{
			UE_LOG(LogBuildPatchServices, Log, TEXT("Skipping backup and stage relocation"));
			BuildProgress.SetStateProgress(EBuildPatchState::MovingToInstall, 1.0f);
		}
		else
		{
			MoveFromStageTimer.Start();
			UE_LOG(LogBuildPatchServices, Log, TEXT("Running backup and stage relocation"));
			// If there's no error, move all complete files
			bMoveSuccess = InstallerError->HasError() == false;
			if (bMoveSuccess)
			{
				// First handle files that should be removed for patching
				TArray< FString > FilesToRemove;
				if (CurrentBuildManifest.IsValid())
				{
					NewBuildManifest->GetRemovableFiles(CurrentBuildManifest.ToSharedRef(), FilesToRemove);
				}
				// And also files that may no longer be required (removal of tags)
				TArray<FString> NewBuildFiles;
				NewBuildManifest->GetFileList(NewBuildFiles);
				TSet<FString> NewBuildFilesSet(NewBuildFiles);
				TSet<FString> RemovableBuildFiles = NewBuildFilesSet.Difference(TaggedFiles);
				FilesToRemove.Append(RemovableBuildFiles.Array());
				// Add to build stats
				ThreadLock.Lock();
				BuildStats.NumFilesToRemove = FilesToRemove.Num();
				ThreadLock.Unlock();
				for (const FString& OldFilename : FilesToRemove)
				{
					const FString FullFilename = Configuration.InstallDirectory / OldFilename;
					const bool bFileExists = FileSystem->FileExists(*FullFilename);
					bool bWasDeleted = OldFilesRemovedBySystem.Contains(OldFilename);
					if (bFileExists)
					{
						BackupFileIfNecessary(OldFilename);
						const bool bDeleteSuccess = FileSystem->DeleteFile(*FullFilename);
						const uint32 LastError = FPlatformMisc::GetLastError();
						UE_LOG(LogBuildPatchServices, Log, TEXT("Removed (%u,%u) %s"), bDeleteSuccess ? 1 : 0, LastError, *OldFilename);
						if (bDeleteSuccess)
						{
							bWasDeleted = true;
						}
					}
					if (bWasDeleted)
					{
						MessagePump->SendMessage(FInstallationFileAction{FInstallationFileAction::EType::Removed, OldFilename});
					}
				}

				// Now handle files that have been constructed
				bool bSavedMoveMarkerFile = false;
				TArray< FString > ConstructionFiles;
				NewBuildManifest->GetFileList(ConstructionFiles);
				BuildProgress.SetStateProgress(EBuildPatchState::MovingToInstall, 0.0f);
				const float NumConstructionFilesFloat = ConstructionFiles.Num();
				for (auto ConstructionFilesIt = ConstructionFiles.CreateConstIterator(); ConstructionFilesIt && bMoveSuccess && !InstallerError->HasError(); ++ConstructionFilesIt)
				{
					const FString& ConstructionFile = *ConstructionFilesIt;
					const FString SrcFilename = InstallStagingDir / ConstructionFile;
					const FString DestFilename = Configuration.InstallDirectory / ConstructionFile;
					const float FileIndexFloat = ConstructionFilesIt.GetIndex();
					// Skip files not constructed
					if (!FileSystem->FileExists(*SrcFilename))
					{
						BuildProgress.SetStateProgress(EBuildPatchState::MovingToInstall, FileIndexFloat / NumConstructionFilesFloat);
						continue;
					}
					// Create the move marker file
					if (!bSavedMoveMarkerFile)
					{
						bSavedMoveMarkerFile = true;
						UE_LOG(LogBuildPatchServices, Log, TEXT("Create MM"));
						FArchive* MoveMarkerFile = IFileManager::Get().CreateFileWriter(*PreviousMoveMarker, FILEWRITE_EvenIfReadOnly);
						if (MoveMarkerFile != nullptr)
						{
							MoveMarkerFile->Close();
							delete MoveMarkerFile;
						}
						// Make sure we have some progress if we do some work
						if (BuildProgress.GetStateWeight(EBuildPatchState::MovingToInstall) <= 0.0f)
						{
							BuildProgress.SetStateWeight(EBuildPatchState::MovingToInstall, 0.1f);
						}
					}
					// Backup file if need be
					BackupFileIfNecessary(ConstructionFile);

					// First delete the destination file to make room for moving.
					const bool bOldFileExisted = OldFilesRemovedBySystem.Contains(ConstructionFile) || FileSystem->FileExists(*DestFilename);
					bool bDeleteSuccess = false;
					uint32 ErrorCode = 0;
					int32 DeleteRetries = ConfigHelpers::NumFileMoveRetries();
					do
					{
						bDeleteSuccess = FileSystem->DeleteFile(*DestFilename);
						ErrorCode = FPlatformMisc::GetLastError();
						if (!bDeleteSuccess)
						{
							UE_LOG(LogBuildPatchServices, Error, TEXT("Failed to delete destination file %s (%d), retying after 0.5 sec"), *ConstructionFile, ErrorCode);
							FPlatformProcess::Sleep(0.5f);
						}

						--DeleteRetries;
					} while (DeleteRetries > 0 && !bDeleteSuccess);
					if (bDeleteSuccess)
					{
						uint32 CopyErrorCode = 0;
						uint32 MoveErrorCode = 0;
						int32 MoveRetries = ConfigHelpers::NumFileMoveRetries();
						// Move the file to the installation directory
						do
						{
							bMoveSuccess = FileSystem->MoveFile(*DestFilename, *SrcFilename);
							ErrorCode = FPlatformMisc::GetLastError();
							if (MoveErrorCode == 0)
							{
								MoveErrorCode = ErrorCode;
							}

							if (!bMoveSuccess)
							{
								InstallerAnalytics->RecordConstructionError(ConstructionFile, ErrorCode, TEXT("Failed To Move"));
								UE_LOG(LogBuildPatchServices, Error, TEXT("Failed to move file %s (%d), trying copy"), *ConstructionFile, ErrorCode);
								bMoveSuccess = FileSystem->CopyFile(*DestFilename, *SrcFilename);
								ErrorCode = FPlatformMisc::GetLastError();
								if (CopyErrorCode == 0)
								{
									CopyErrorCode = ErrorCode;
								}

								if (bMoveSuccess)
								{
									FileSystem->DeleteFile(*SrcFilename);
								}
								else
								{
									UE_LOG(LogBuildPatchServices, Error, TEXT("Failed to copy file %s (%d), retying after 0.5 sec"), *ConstructionFile, ErrorCode);
									FPlatformProcess::Sleep(0.5f);
								}
							}

							--MoveRetries;
						} while (!bMoveSuccess && MoveRetries > 0);
						if (!bMoveSuccess)
						{
							UE_LOG(LogBuildPatchServices, Error, TEXT("Failed to move file %s"), *FPaths::GetCleanFilename(ConstructionFile));
							if (MoveErrorCode != 0 || CopyErrorCode != 0)
							{
								InstallerError->SetError(EBuildPatchInstallError::MoveFileToInstall, *FString::Printf(TEXT("%s-%u-%u"), MoveErrorCodes::StageToInstall, MoveErrorCode, CopyErrorCode));
							}
							else
							{
								InstallerError->SetError(EBuildPatchInstallError::MoveFileToInstall, MoveErrorCodes::StageToInstall);
							}
							
						}
						else
						{
							FilesInstalled.Add(ConstructionFile);
							BuildProgress.SetStateProgress(EBuildPatchState::MovingToInstall, FileIndexFloat / NumConstructionFilesFloat);
							FileOperationTracker->OnFileStateUpdate(ConstructionFile, EFileOperationState::Installed);
							FInstallationFileAction::EType Action = bOldFileExisted ? FInstallationFileAction::EType::Updated : FInstallationFileAction::EType::Added;
							MessagePump->SendMessage(FInstallationFileAction{Action, ConstructionFile});
						}
					}
					else
					{
						UE_LOG(LogBuildPatchServices, Error, TEXT("Failed to delete file %s"), *FPaths::GetCleanFilename(ConstructionFile));
						InstallerError->SetError(EBuildPatchInstallError::MoveFileToInstall, MoveErrorCodes::DeleteFileFailed, ErrorCode);
					}
				}

				// After we've completed deleting/moving patch files to the install directory, clean up any empty directories left over
				CleanupEmptyDirectories(Configuration.InstallDirectory);

				bMoveSuccess = bMoveSuccess && (InstallerError->HasError() == false);
				if (bMoveSuccess)
				{
					BuildProgress.SetStateProgress(EBuildPatchState::MovingToInstall, 1.0f);
				}
			}
			UE_LOG(LogBuildPatchServices, Log, TEXT("Relocation complete %d"), bMoveSuccess ? 1 : 0);
			MoveFromStageTimer.Stop();
		}
		return bMoveSuccess;
	}

	bool FBuildPatchInstaller::RunFileAttributes(bool bForce)
	{
		// Only provide stage directory if stage-only mode
		FString EmptyString;
		FString& OptionalStageDirectory = Configuration.InstallMode == EInstallMode::StageFiles ? InstallStagingDir : EmptyString;

		// Construct the attributes class
		FileAttributesTimer.Start();
		TUniquePtr<IFileAttribution> Attributes(FFileAttributionFactory::Create(FileSystem.Get(), NewBuildManifest, CurrentBuildManifest, FilesToConstruct, Configuration.InstallDirectory, OptionalStageDirectory, &BuildProgress));
		FScopedControllables ScopedControllables(&ThreadLock, Controllables, bIsPaused, bShouldAbort);
		ScopedControllables.Register(Attributes.Get());
		Attributes->ApplyAttributes(bForce);
		FileAttributesTimer.Stop();

		// We don't fail on this step currently
		return true;
	}

	bool FBuildPatchInstaller::RunVerification(TArray< FString >& CorruptFiles)
	{
		// Make sure this function can never be parallelized
		static FCriticalSection SingletonFunctionLockCS;
		const bool bShouldLock = !Configuration.bAllowConcurrentExecution;
		if (bShouldLock)
		{
			SingletonFunctionLockCS.Lock();
		}

		VerifyTimer.Start();
		BuildProgress.SetStateProgress(EBuildPatchState::BuildVerification, 0.0f);

		// Verify the installation
		UE_LOG(LogBuildPatchServices, Log, TEXT("Verifying install"));
		CorruptFiles.Empty();

		// Only provide stage directory if stage-only mode
		FString EmptyString;
		FString& OptionalStageDirectory = Configuration.InstallMode == EInstallMode::StageFiles ? InstallStagingDir : EmptyString;

		// For a repair operation, the first verify must be ShaVerifyAllFiles.
		EVerifyMode ModeToRun = Configuration.bIsRepair && bFirstInstallIteration ? EVerifyMode::ShaVerifyAllFiles : Configuration.VerifyMode;

		// Construct the verifier
		TUniquePtr<IVerifier> Verifier(FVerifierFactory::Create(FileSystem.Get(), VerifierStatistics.Get(), ModeToRun, FilesToConstruct, Configuration.InstallTags, NewBuildManifest, Configuration.InstallDirectory, OptionalStageDirectory));
		FScopedControllables ScopedControllables(&ThreadLock, Controllables, bIsPaused, bShouldAbort);
		ScopedControllables.Register(Verifier.Get());

		// Verify the build
		EVerifyResult VerifyResult = Verifier->Verify(CorruptFiles);
		const bool bVerifySuccessful = VerifyResult == EVerifyResult::Success;
		if (!bVerifySuccessful)
		{
			UE_LOG(LogBuildPatchServices, Error, TEXT("Build verification failed on %u file(s)"), CorruptFiles.Num());
			InstallerError->SetError(EBuildPatchInstallError::BuildVerifyFail, InstallerHelpers::GetVerifyErrorCode(VerifyResult));
		}
		TMap<EVerifyError, int32> VerifyErrorCounts = VerifierStatistics->GetVerifyErrorCounts();
		for (const TPair<EVerifyError, int32>& VerifyErrorCount : VerifyErrorCounts)
		{
			const int32 CachedCount = CachedVerifyErrorCounts.FindRef(VerifyErrorCount.Key);
			if (CachedCount < VerifyErrorCount.Value)
			{
				InstallerHelpers::LogAdditionalVerifyErrors(VerifyErrorCount.Key, VerifyErrorCount.Value - CachedCount);
			}
		}
		CachedVerifyErrorCounts = MoveTemp(VerifyErrorCounts);

		BuildProgress.SetStateProgress(EBuildPatchState::BuildVerification, 1.0f);

		// Delete/Backup any incorrect files if failure was not cancellation
		if (!InstallerError->IsCancelled())
		{
			for (const FString& CorruptFile : CorruptFiles)
			{
				BackupFileIfNecessary(CorruptFile, true);
				FString StagedFile = InstallStagingDir / CorruptFile;
				if (FileSystem->FileExists(*StagedFile))
				{
					FileSystem->DeleteFile(*StagedFile);
				}
				if (Configuration.InstallMode != EInstallMode::StageFiles)
				{
					FString InstalledFile = Configuration.InstallDirectory / CorruptFile;
					if (FileSystem->FileExists(*InstalledFile))
					{
						FileSystem->DeleteFile(*InstalledFile);
						OldFilesRemovedBySystem.Add(CorruptFile);
					}
				}
			}
		}

		UE_LOG(LogBuildPatchServices, Log, TEXT("Verify stage complete %d"), bVerifySuccessful ? 1 : 0);

		VerifyTimer.Stop();
		if (bShouldLock)
		{
			SingletonFunctionLockCS.Unlock();
		}
		return bVerifySuccessful;
	}

	bool FBuildPatchInstaller::BackupFileIfNecessary(const FString& Filename, bool bDiscoveredByVerification /*= false */)
	{
		const FString InstalledFilename = Configuration.InstallDirectory / Filename;
		const FString BackupFilename = Configuration.BackupDirectory / Filename;
		const bool bBackupOriginals = !Configuration.BackupDirectory.IsEmpty();
		// Skip if not doing backups
		if (!bBackupOriginals)
		{
			return true;
		}
		// Skip if no file to backup
		const bool bInstalledFileExists = FPlatformFileManager::Get().GetPlatformFile().FileExists(*InstalledFilename);
		if (!bInstalledFileExists)
		{
			return true;
		}
		// Skip if already backed up
		const bool bAlreadyBackedUp = FPlatformFileManager::Get().GetPlatformFile().FileExists(*BackupFilename);
		if (bAlreadyBackedUp)
		{
			return true;
		}
		// Skip if the target file was already copied to the installation
		const bool bAlreadyInstalled = FilesInstalled.Contains(Filename);
		if (bAlreadyInstalled)
		{
			return true;
		}
		// If discovered by verification, but the patching system did not touch the file, we know it must be backed up.
		// If patching system touched the file it would already have been backed up
		if (bDiscoveredByVerification && CurrentBuildManifest.IsValid() && !NewBuildManifest->IsFileOutdated(CurrentBuildManifest.ToSharedRef(), Filename))
		{
			return IFileManager::Get().Move(*BackupFilename, *InstalledFilename, true, true, true);
		}
		bool bUserEditedFile = bDiscoveredByVerification;
		const bool bCheckFileChanges = !bDiscoveredByVerification;
		if (bCheckFileChanges)
		{
			const FFileManifest* OldFileManifest = CurrentBuildManifest.IsValid() ? CurrentBuildManifest->GetFileManifest(Filename) : nullptr;
			const FFileManifest* NewFileManifest = NewBuildManifest->GetFileManifest(Filename);
			const int64 InstalledFilesize = IFileManager::Get().FileSize(*InstalledFilename);
			const int64 OriginalFileSize = OldFileManifest ? OldFileManifest->GetFileSize() : INDEX_NONE;
			const int64 NewFileSize = NewFileManifest ? NewFileManifest->GetFileSize() : INDEX_NONE;
			const FSHAHash HashZero;
			const FSHAHash& HashOld = OldFileManifest ? OldFileManifest->FileHash : HashZero;
			const FSHAHash& HashNew = NewFileManifest ? NewFileManifest->FileHash : HashZero;
			const bool bFileSizeDiffers = OriginalFileSize != InstalledFilesize && NewFileSize != InstalledFilesize;
			bUserEditedFile = bFileSizeDiffers || FBuildPatchUtils::VerifyFile(FileSystem.Get(), InstalledFilename, HashOld, HashNew) == 0;
		}
		// Finally, use the above logic to determine if we must do the backup
		const bool bNeedBackup = bUserEditedFile;
		bool bBackupSuccess = true;
		if (bNeedBackup)
		{
			UE_LOG(LogBuildPatchServices, Log, TEXT("Backing up %s"), *Filename);
			bBackupSuccess = IFileManager::Get().Move(*BackupFilename, *InstalledFilename, true, true, true);
		}
		return bBackupSuccess;
	}

	double FBuildPatchInstaller::GetDownloadSpeed() const
	{
		return DownloadSpeedRecorder->GetAverageSpeed(ConfigHelpers::DownloadSpeedAverageTime());
	}

	int64 FBuildPatchInstaller::GetTotalDownloadRequired() const
	{
		return CloudChunkSourceStatistics->GetRequiredDownloadSize() + PreviousTotalDownloadRequired.GetValue();
	}

	int64 FBuildPatchInstaller::GetTotalDownloaded() const
	{
		return DownloadServiceStatistics->GetBytesDownloaded();
	}

	bool FBuildPatchInstaller::IsComplete() const
	{
		return !bIsRunning && bIsInited;
	}

	bool FBuildPatchInstaller::IsCanceled() const
	{
		FScopeLock Lock(&ThreadLock);
		return BuildStats.FailureType == EBuildPatchInstallError::UserCanceled;
	}

	bool FBuildPatchInstaller::IsPaused() const
	{
		FScopeLock Lock(&ThreadLock);
		return bIsPaused;
	}

	bool FBuildPatchInstaller::IsResumable() const
	{
		FScopeLock Lock(&ThreadLock);
		if (BuildStats.FailureType == EBuildPatchInstallError::PathLengthExceeded)
		{
			return false;
		}
		return !BuildStats.ProcessSuccess;
	}

	bool FBuildPatchInstaller::IsUpdate() const
	{
		return CurrentBuildManifest.IsValid() && CurrentBuildManifest->GetVersionString() != NewBuildManifest->GetVersionString();
	}

	bool FBuildPatchInstaller::HasError() const
	{
		FScopeLock Lock(&ThreadLock);
		if (BuildStats.FailureType == EBuildPatchInstallError::UserCanceled)
		{
			return false;
		}
		return !BuildStats.ProcessSuccess;
	}

	EBuildPatchInstallError FBuildPatchInstaller::GetErrorType() const
	{
		FScopeLock Lock(&ThreadLock);
		return BuildStats.FailureType;
	}

	FString FBuildPatchInstaller::GetErrorCode() const
	{
		FScopeLock Lock(&ThreadLock);
		return BuildStats.ErrorCode;
	}

	//@todo this is deprecated and shouldn't be used anymore [6/4/2014 justin.sargent]
	FText FBuildPatchInstaller::GetPercentageText() const
	{
		static const FText PleaseWait = NSLOCTEXT("BuildPatchInstaller", "BuildPatchInstaller_GenericProgress", "Please Wait");

		FScopeLock Lock(&ThreadLock);

		float Progress = GetUpdateProgress() * 100.0f;
		if (Progress <= 0.0f)
		{
			return PleaseWait;
		}

		FNumberFormattingOptions PercentFormattingOptions;
		PercentFormattingOptions.MaximumFractionalDigits = 0;
		PercentFormattingOptions.MinimumFractionalDigits = 0;

		return FText::AsPercent(GetUpdateProgress(), &PercentFormattingOptions);
	}

	//@todo this is deprecated and shouldn't be used anymore [6/4/2014 justin.sargent]
	FText FBuildPatchInstaller::GetDownloadSpeedText() const
	{
		static const FText DownloadSpeedFormat = NSLOCTEXT("BuildPatchInstaller", "BuildPatchInstaller_DownloadSpeedFormat", "{Current} / {Total} ({Speed}/sec)");

		FScopeLock Lock(&ThreadLock);
		FText SpeedDisplayedText;
		double DownloadSpeed = GetDownloadSpeed();
		double InitialDownloadSize = GetTotalDownloadRequired();
		double TotalDownloaded = GetTotalDownloaded();
		if (DownloadSpeed >= 0)
		{
			FNumberFormattingOptions FormattingOptions;
			FormattingOptions.MaximumFractionalDigits = 1;
			FormattingOptions.MinimumFractionalDigits = 1;

			FFormatNamedArguments Args;
			Args.Add(TEXT("Speed"), FText::AsMemory(DownloadSpeed, &FormattingOptions));
			Args.Add(TEXT("Total"), FText::AsMemory(InitialDownloadSize, &FormattingOptions));
			Args.Add(TEXT("Current"), FText::AsMemory(TotalDownloaded, &FormattingOptions));

			return FText::Format(DownloadSpeedFormat, Args);
		}

		return FText();
	}

	EBuildPatchState FBuildPatchInstaller::GetState() const
	{
		return BuildProgress.GetState();
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FText FBuildPatchInstaller::GetStatusText() const
	{
		return BuildPatchServices::StateToText(GetState());
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	float FBuildPatchInstaller::GetUpdateProgress() const
	{
		return BuildProgress.GetProgress();
	}

	FBuildInstallStats FBuildPatchInstaller::GetBuildStatistics() const
	{
		FScopeLock Lock(&ThreadLock);
		return BuildStats;
	}

	EBuildPatchDownloadHealth FBuildPatchInstaller::GetDownloadHealth() const
	{
		return CloudChunkSourceStatistics->GetDownloadHealth();
	}

	FText FBuildPatchInstaller::GetErrorText() const
	{
		return InstallerError->GetErrorText();
	}

	void FBuildPatchInstaller::CancelInstall()
	{
		InstallerError->SetError(EBuildPatchInstallError::UserCanceled, UserCancelErrorCodes::UserRequested);

		// Make sure we are not paused
		if (IsPaused())
		{
			TogglePauseInstall();
		}

		// Abort all controllable classes
		ThreadLock.Lock();
		bShouldAbort = true;
		for (IControllable* Controllable : Controllables)
		{
			Controllable->Abort();
		}
		ThreadLock.Unlock();
	}

	bool FBuildPatchInstaller::TogglePauseInstall()
	{
		FScopeLock Lock(&ThreadLock);
		// If there is an error, we don't allow pausing.
		const bool bShouldBePaused = !bIsPaused && !InstallerError->HasError();
		if (bIsPaused)
		{
			// Stop pause timer.
			ProcessPausedTimer.Stop();
		}
		else if (bShouldBePaused)
		{
			// Start pause timer.
			ProcessPausedTimer.Start();
		}
		bIsPaused = bShouldBePaused;
		// Set pause state on all controllable classes
		for (IControllable* Controllable : Controllables)
		{
			Controllable->SetPaused(bShouldBePaused);
		}
		// Set pause state on pausable process timers.
		ConstructTimer.SetPause(bIsPaused);
		MoveFromStageTimer.SetPause(bIsPaused);
		FileAttributesTimer.SetPause(bIsPaused);
		VerifyTimer.SetPause(bIsPaused);
		CleanUpTimer.SetPause(bIsPaused);
		ProcessActiveTimer.SetPause(bIsPaused);
		return bShouldBePaused;
	}

	void FBuildPatchInstaller::RegisterMessageHandler(FMessageHandler* MessageHandler)
	{
		check(IsInGameThread());
		check(MessageHandler != nullptr);
		MessageHandlers.AddUnique(MessageHandler);
	}

	void FBuildPatchInstaller::UnregisterMessageHandler(FMessageHandler* MessageHandler)
	{
		check(IsInGameThread());
		MessageHandlers.Remove(MessageHandler);
	}

	void FBuildPatchInstaller::ExecuteCompleteDelegate()
	{
		// Should be executed in main thread, and already be complete.
		check(IsInGameThread());
		check(IsComplete());
		// Finish applying build statistics.
		{
			FScopeLock Lock(&ThreadLock);
			BuildStats.FinalDownloadSpeed = GetDownloadSpeed();
			BuildStats.AverageDownloadSpeed = DownloadSpeedRecorder->GetAverageSpeed(TNumericLimits<float>::Max());
			BuildStats.PeakDownloadSpeed = DownloadSpeedRecorder->GetPeakSpeed();
			BuildStats.AverageDiskReadSpeed = DiskReadSpeedRecorder->GetAverageSpeed(TNumericLimits<float>::Max());
			BuildStats.PeakDiskReadSpeed = DiskReadSpeedRecorder->GetPeakSpeed();
			BuildStats.AverageDiskWriteSpeed = DiskWriteSpeedRecorder->GetAverageSpeed(TNumericLimits<float>::Max());
			BuildStats.PeakDiskWriteSpeed = DiskWriteSpeedRecorder->GetPeakSpeed();
			BuildStats.TotalDownloadedData = DownloadServiceStatistics->GetBytesDownloaded();
			BuildStats.TotalReadData = InstallChunkSourceStatistics->GetBytesRead();
			BuildStats.TotalReadData += VerifierStatistics->GetBytesVerified();
			BuildStats.TotalWrittenData = FileConstructorStatistics->GetBytesConstructed();
			BuildStats.NumFilesConstructed = FileConstructorStatistics->GetFilesConstructed();
			BuildStats.TheoreticalDownloadTime = BuildStats.AverageDownloadSpeed > 0 ? BuildStats.TotalDownloadedData / BuildStats.AverageDownloadSpeed : 0;
			BuildStats.AverageMemoryStoreUse = MemoryChunkStoreAggregateStatistics->GetAverageStoreUse();
			BuildStats.PeakMemoryStoreUse = MemoryChunkStoreAggregateStatistics->GetPeakStoreUse();
			BuildStats.AverageMemoryStoreRetained = MemoryChunkStoreAggregateStatistics->GetAverageStoreRetained();
			BuildStats.PeakMemoryStoreRetained = MemoryChunkStoreAggregateStatistics->GetPeakStoreRetained();
			BuildStats.MemoryStoreSize = MemoryChunkStoreAggregateStatistics->GetTotalStoreSize();
			InstallerHelpers::LogBuildStatInfo(BuildStats);
		}
		// Call the complete delegate.
		OnCompleteDelegate.ExecuteIfBound(bSuccess, NewBuildManifest);
	}

	void FBuildPatchInstaller::PumpMessages()
	{
		check(IsInGameThread());
		MessagePump->PumpMessages(MessageHandlers);
	}

	void FBuildPatchInstaller::WaitForThread() const
	{
		if (Thread != nullptr)
		{
			Thread->WaitForCompletion();
		}
	}
}