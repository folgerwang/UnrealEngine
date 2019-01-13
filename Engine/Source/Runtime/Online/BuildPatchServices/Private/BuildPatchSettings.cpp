// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "BuildPatchSettings.h"

#include "Misc/App.h"
#include "HAL/PlatformProcess.h"

namespace BuildPatchServices
{
	FBuildPatchServicesInitSettings::FBuildPatchServicesInitSettings()
		: ApplicationSettingsDir(FPlatformProcess::ApplicationSettingsDir())
		, ProjectName(FApp::GetProjectName())
		, LocalMachineConfigFileName(TEXT("BuildPatchServicesLocal.ini"))
	{
	}

	FInstallerConfiguration::FInstallerConfiguration(const IBuildManifestRef& InInstallManifest)
		: CurrentManifest(nullptr)
		, InstallManifest(InInstallManifest)
		, InstallDirectory()
		, StagingDirectory()
		, BackupDirectory()
		, ChunkDatabaseFiles()
		, CloudDirectories()
		, InstallTags()
		, InstallMode(EInstallMode::NonDestructiveInstall)
		, VerifyMode(EVerifyMode::ShaVerifyAllFiles)
		, DeltaPolicy(EDeltaPolicy::Skip)
		, bIsRepair(false)
		, bRunRequiredPrereqs(true)
		, bAllowConcurrentExecution(false)
	{
	}

	FInstallerConfiguration::FInstallerConfiguration(FInstallerConfiguration&& MoveFrom)
		: CurrentManifest(MoveTemp(MoveFrom.CurrentManifest))
		, InstallManifest(MoveTemp(MoveFrom.InstallManifest))
		, InstallDirectory(MoveTemp(MoveFrom.InstallDirectory))
		, StagingDirectory(MoveTemp(MoveFrom.StagingDirectory))
		, BackupDirectory(MoveTemp(MoveFrom.BackupDirectory))
		, ChunkDatabaseFiles(MoveTemp(MoveFrom.ChunkDatabaseFiles))
		, CloudDirectories(MoveTemp(MoveFrom.CloudDirectories))
		, InstallTags(MoveTemp(MoveFrom.InstallTags))
		, InstallMode(MoveFrom.InstallMode)
		, VerifyMode(MoveFrom.VerifyMode)
		, DeltaPolicy(MoveFrom.DeltaPolicy)
		, bIsRepair(MoveFrom.bIsRepair)
		, bRunRequiredPrereqs(MoveFrom.bRunRequiredPrereqs)
		, bAllowConcurrentExecution(MoveFrom.bAllowConcurrentExecution)
	{
	}

	FInstallerConfiguration::FInstallerConfiguration(const FInstallerConfiguration& CopyFrom)
		: CurrentManifest(CopyFrom.CurrentManifest)
		, InstallManifest(CopyFrom.InstallManifest)
		, InstallDirectory(CopyFrom.InstallDirectory)
		, StagingDirectory(CopyFrom.StagingDirectory)
		, BackupDirectory(CopyFrom.BackupDirectory)
		, ChunkDatabaseFiles(CopyFrom.ChunkDatabaseFiles)
		, CloudDirectories(CopyFrom.CloudDirectories)
		, InstallTags(CopyFrom.InstallTags)
		, InstallMode(CopyFrom.InstallMode)
		, VerifyMode(CopyFrom.VerifyMode)
		, DeltaPolicy(CopyFrom.DeltaPolicy)
		, bIsRepair(CopyFrom.bIsRepair)
		, bRunRequiredPrereqs(CopyFrom.bRunRequiredPrereqs)
		, bAllowConcurrentExecution(CopyFrom.bAllowConcurrentExecution)
	{
	}

	FChunkBuildConfiguration::FChunkBuildConfiguration()
		: FeatureLevel(EFeatureLevel::Latest)
		, OutputChunkWindowSize(1024 * 1024)
		, bShouldMatchAnyWindowSize(true)
	{
	}

	FChunkDeltaOptimiserConfiguration::FChunkDeltaOptimiserConfiguration()
		: ScanWindowSize(8191)
		, OutputChunkSize(1024 * 1024)
	{
	}

	FPatchDataEnumerationConfiguration::FPatchDataEnumerationConfiguration()
		: bIncludeSizes(false)
	{
	}

	FDiffManifestsConfiguration::FDiffManifestsConfiguration()
	{
	}

	FCompactifyConfiguration::FCompactifyConfiguration()
		: DataAgeThreshold(7.0f)
		, bRunPreview(true)
	{
	}

	FPackageChunksConfiguration::FPackageChunksConfiguration()
		: FeatureLevel(EFeatureLevel::Latest)
		, MaxOutputFileSize(TNumericLimits<uint64>::Max())
	{
	}
}
