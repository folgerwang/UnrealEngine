// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Installer/OptimisedDelta.h"

#include "Async/Future.h"
#include "Misc/ConfigCacheIni.h"

#include "Installer/DownloadService.h"
#include "BuildPatchUtil.h"
#include "BuildPatchMergeManifests.h"

DECLARE_LOG_CATEGORY_CLASS(LogOptimisedDelta, Log, All);

namespace ConfigHelpers
{
	int32 LoadDeltaRetries(int32 Min)
	{
		int32 DeltaRetries = 6;
		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("DeltaRetries"), DeltaRetries, GEngineIni);
		DeltaRetries = FMath::Clamp<int32>(DeltaRetries, Min, 1000);
		return DeltaRetries;
	}
}

namespace BuildPatchServices
{
	class FOptimisedDelta
		: public IOptimisedDelta
	{
	public:
		FOptimisedDelta(const FOptimisedDeltaConfiguration& Configuration, const FOptimisedDeltaDependencies& Dependencies);

		// IOptimisedDelta interface begin.
		virtual FBuildPatchAppManifestPtr GetDestinationManifest() override;
		virtual int32 GetMetaDownloadSize() override;
		// IOptimisedDelta interface end.

	private:
		void OnDownloadComplete(int32 RequestId, const FDownloadRef& Download);
		bool ShouldRetry(const FDownloadRef& Download);
		void SetFailedDownload();

	private:
		const FOptimisedDeltaConfiguration Configuration;
		const FOptimisedDeltaDependencies Dependencies;
		const FString RelativeDeltaFilePath;
		const int32 DeltaRetries;
		EDeltaPolicy DeltaPolicy;
		int32 CloudDirIdx;
		int32 RetryCount;
		FDownloadProgressDelegate ChunkDeltaProgress;
		FDownloadCompleteDelegate ChunkDeltaComplete;
		TPromise<FBuildPatchAppManifestPtr> ChunkDeltaPromise;
		TFuture<FBuildPatchAppManifestPtr> ChunkDeltaFuture;
		FThreadSafeCounter DownloadedBytes;
	};

	FOptimisedDelta::FOptimisedDelta(const FOptimisedDeltaConfiguration& InConfiguration, const FOptimisedDeltaDependencies& InDependencies)
		: Configuration(InConfiguration)
		, Dependencies(InDependencies)
		, RelativeDeltaFilePath(Configuration.SourceManifest.IsValid() ? FBuildPatchUtils::GetChunkDeltaFilename(*Configuration.SourceManifest.Get(), Configuration.DestinationManifest.Get()) : TEXT(""))
		, DeltaRetries(ConfigHelpers::LoadDeltaRetries(Configuration.CloudDirectories.Num()))
		, DeltaPolicy(Configuration.DeltaPolicy)
		, CloudDirIdx(0)
		, RetryCount(0)
		, ChunkDeltaComplete(FDownloadCompleteDelegate::CreateRaw(this, &FOptimisedDelta::OnDownloadComplete))
		, ChunkDeltaPromise()
		, ChunkDeltaFuture(ChunkDeltaPromise.GetFuture())
		, DownloadedBytes(0)
	{
		// There are some conditions in which we do not use a delta.
		const bool bNoSourceManifest = Configuration.SourceManifest.IsValid() == false;
		const bool bNotPatching = Configuration.InstallerConfiguration == nullptr ? false : Configuration.InstallerConfiguration->bIsRepair || Configuration.InstallerConfiguration->InstallMode == EInstallMode::PrereqOnly;
		const bool bSameBuild = bNoSourceManifest ? false : Configuration.SourceManifest->GetBuildId() == Configuration.DestinationManifest->GetBuildId();
		if (bNoSourceManifest || bNotPatching || bSameBuild)
		{
			DeltaPolicy = EDeltaPolicy::Skip;
		}
		// Kick off the request if we should be.
		if (DeltaPolicy != EDeltaPolicy::Skip)
		{
			UE_LOG(LogOptimisedDelta, Log, TEXT("Requesting optimised delta file %s"), *RelativeDeltaFilePath);
			Dependencies.DownloadService->RequestFile(Configuration.CloudDirectories[CloudDirIdx] / RelativeDeltaFilePath, ChunkDeltaComplete, ChunkDeltaProgress);
		}
		// Otherwise we provide the standard destination manifest.
		else
		{
			ChunkDeltaPromise.SetValue(Configuration.DestinationManifest);
			Dependencies.OnComplete(Configuration.DestinationManifest);
		}
	}

	FBuildPatchAppManifestPtr FOptimisedDelta::GetDestinationManifest()
	{
		return ChunkDeltaFuture.Get();
	}

	int32 FOptimisedDelta::GetMetaDownloadSize()
	{
		ChunkDeltaFuture.Wait();
		return DownloadedBytes.GetValue();
	}

	void FOptimisedDelta::OnDownloadComplete(int32 RequestId, const FDownloadRef& Download)
	{
		if (Download->WasSuccessful())
		{
			// Perform a merge with current manifest so that the delta can support missing out unnecessary information.
			FBuildPatchAppManifestPtr NewManifest;
			FBuildPatchAppManifestRef DeltaManifest = MakeShareable(new FBuildPatchAppManifest());
			if (DeltaManifest->DeserializeFromData(Download->GetData()))
			{
				NewManifest = FBuildMergeManifests::MergeDeltaManifest(Configuration.DestinationManifest, DeltaManifest);
			}
			if (NewManifest.IsValid())
			{
				UE_LOG(LogOptimisedDelta, Log, TEXT("Received optimised delta file successfully %s"), *RelativeDeltaFilePath);
				DownloadedBytes.Set(Download->GetData().Num());
				ChunkDeltaPromise.SetValue(NewManifest);
				Dependencies.OnComplete(NewManifest);
			}
			else if (ShouldRetry(Download))
			{
				++RetryCount;
				CloudDirIdx = (CloudDirIdx + RetryCount) % Configuration.CloudDirectories.Num();
				Dependencies.DownloadService->RequestFile(Configuration.CloudDirectories[CloudDirIdx] / RelativeDeltaFilePath, ChunkDeltaComplete, ChunkDeltaProgress);
			}
			else
			{
				SetFailedDownload();
			}
		}
		else if (ShouldRetry(Download))
		{
			++RetryCount;
			CloudDirIdx = (CloudDirIdx + RetryCount) % Configuration.CloudDirectories.Num();
			Dependencies.DownloadService->RequestFile(Configuration.CloudDirectories[CloudDirIdx] / RelativeDeltaFilePath, ChunkDeltaComplete, ChunkDeltaProgress);
		}
		else
		{
			SetFailedDownload();
		}
	}

	bool FOptimisedDelta::ShouldRetry(const FDownloadRef& Download)
	{
		// If the response code was in the 'client error' range - interpreted as we asked for something invalid, then we accept that as the
		// 'no delta' response. Any other failure reason is a server or network issue which we should retry.
		const int32 ResponseCode = Download->GetResponseCode();
		const bool bCanRetry = ResponseCode < 400 || ResponseCode >= 500;
		const bool bMayRetry = RetryCount < DeltaRetries;
		return bCanRetry && bMayRetry;
	}

	void FOptimisedDelta::SetFailedDownload()
	{
		if (DeltaPolicy == EDeltaPolicy::TryFetchContinueWithout)
		{
			UE_LOG(LogOptimisedDelta, Log, TEXT("Skipping optimised delta file."));
			ChunkDeltaPromise.SetValue(Configuration.DestinationManifest);
			Dependencies.OnComplete(Configuration.DestinationManifest);
		}
		else
		{
			UE_LOG(LogOptimisedDelta, Log, TEXT("Failed optimised delta file fetch %s"), *RelativeDeltaFilePath);
			ChunkDeltaPromise.SetValue(nullptr);
			Dependencies.OnComplete(nullptr);
		}
	}

	FOptimisedDeltaConfiguration::FOptimisedDeltaConfiguration(FBuildPatchAppManifestRef InDestinationManifest)
		: DestinationManifest(MoveTemp(InDestinationManifest))
		, DeltaPolicy(EDeltaPolicy::TryFetchContinueWithout)
		, InstallerConfiguration(nullptr)
	{
	}

	FOptimisedDeltaDependencies::FOptimisedDeltaDependencies()
		: DownloadService(nullptr)
		, OnComplete([](FBuildPatchAppManifestPtr) {})
	{
	}

	IOptimisedDelta* FOptimisedDeltaFactory::Create(const FOptimisedDeltaConfiguration& Configuration, const FOptimisedDeltaDependencies& Dependencies)
	{
		check(Dependencies.DownloadService != nullptr);
		return new FOptimisedDelta(Configuration, Dependencies);
	}
}
