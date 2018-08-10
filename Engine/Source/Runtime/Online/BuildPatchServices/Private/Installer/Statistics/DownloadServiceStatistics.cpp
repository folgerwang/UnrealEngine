// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Installer/Statistics/DownloadServiceStatistics.h"
#include "Misc/ScopeLock.h"
#include "Misc/Paths.h"
#include "Core/AsyncHelpers.h"
#include "Common/StatsCollector.h"
#include "Installer/InstallerAnalytics.h"
#include "BuildPatchProgress.h"
#include "BuildPatchManifest.h"
#include "BuildPatchUtil.h"

namespace BuildPatchServices
{
	class FDownloadServiceStatistics
		: public IDownloadServiceStatistics
	{
	public:
		FDownloadServiceStatistics(ISpeedRecorder* SpeedRecorder, IInstallerAnalytics* InstallerAnalytics, FBuildPatchAppManifest* Manifest);
		~FDownloadServiceStatistics();

		// IDownloadServiceStat interface start.
		virtual void OnDownloadStarted(int32 RequestId, const FString& Uri) override;
		virtual void OnDownloadProgress(int32 RequestId, int32 BytesReceived) override;
		virtual void OnDownloadComplete(const FDownloadRecord& DownloadRecord) override;
		// IDownloadServiceStat interface end.

		// IDownloadServiceStatistics interface start.
		virtual uint64 GetBytesDownloaded() const override;
		virtual int32 GetNumSuccessfulChunkDownloads() const override;
		virtual int32 GetNumFailedChunkDownloads() const override;
		virtual int32 GetNumCurrentDownloads() const override;
		virtual TArray<FDownload> GetCurrentDownloads() const override;
		// IDownloadServiceStatistics interface end.

	private:
		int64 GetDownloadSize(const FString& Uri) const;

	private:
		ISpeedRecorder* SpeedRecorder;
		IInstallerAnalytics* InstallerAnalytics;
		FBuildPatchAppManifest* Manifest;
		TMap<FString, int64> DownloadSizes;
		FThreadSafeInt64 TotalBytesReceived;
		FThreadSafeInt32 NumSuccessfulDownloads;
		FThreadSafeInt32 NumFailedDownloads;

		typedef TTuple<FString, int32> FDownloadTuple;
		TMap<int32, FDownloadTuple> Downloads;
	};

	FDownloadServiceStatistics::FDownloadServiceStatistics(ISpeedRecorder* InSpeedRecorder, IInstallerAnalytics* InInstallerAnalytics, FBuildPatchAppManifest* InManifest)
		: SpeedRecorder(InSpeedRecorder)
		, InstallerAnalytics(InInstallerAnalytics)
		, Manifest(InManifest)
		, TotalBytesReceived(0)
		, NumSuccessfulDownloads(0)
		, NumFailedDownloads(0)
	{
		// Fill out download size lookup.
		TSet<FGuid> DataList;
		Manifest->GetDataList(DataList);
		for (const FGuid& DataId : DataList)
		{
			FString CleanFilename = FPaths::GetCleanFilename(FBuildPatchUtils::GetDataFilename(*Manifest, TEXT(""), DataId));
			DownloadSizes.Add(MoveTemp(CleanFilename), Manifest->GetDataSize(DataId));
		}
	}

	FDownloadServiceStatistics::~FDownloadServiceStatistics()
	{
	}

	void FDownloadServiceStatistics::OnDownloadStarted(int32 RequestId, const FString& Uri)
	{
		checkSlow(IsInGameThread());
		FDownloadTuple& DownloadTuple = Downloads.FindOrAdd(RequestId);
		DownloadTuple.Get<0>() = Uri;
		DownloadTuple.Get<1>() = 0;
	}

	void FDownloadServiceStatistics::OnDownloadProgress(int32 RequestId, int32 BytesReceived)
	{
		checkSlow(IsInGameThread());
		FDownloadTuple& DownloadTuple = Downloads.FindOrAdd(RequestId);
		DownloadTuple.Get<1>() = BytesReceived;
	}

	void FDownloadServiceStatistics::OnDownloadComplete(const FDownloadRecord& DownloadRecord)
	{
		checkSlow(IsInGameThread());
		Downloads.Remove(DownloadRecord.RequestId);
		TotalBytesReceived.Add(DownloadRecord.SpeedRecord.Size);
		if (DownloadRecord.bSuccess)
		{
			NumSuccessfulDownloads.Increment();
			SpeedRecorder->AddRecord(DownloadRecord.SpeedRecord);
		}
		else
		{
			NumFailedDownloads.Increment();
			InstallerAnalytics->RecordChunkDownloadError(DownloadRecord.Uri, DownloadRecord.ResponseCode, TEXT("DownloadFail"));
		}
	}

	uint64 FDownloadServiceStatistics::GetBytesDownloaded() const
	{
		return TotalBytesReceived.GetValue();
	}

	int32 FDownloadServiceStatistics::GetNumSuccessfulChunkDownloads() const
	{
		return NumSuccessfulDownloads.GetValue();
	}

	int32 FDownloadServiceStatistics::GetNumFailedChunkDownloads() const
	{
		return NumFailedDownloads.GetValue();
	}

	int32 FDownloadServiceStatistics::GetNumCurrentDownloads() const
	{
		return Downloads.Num();
	}

	TArray<FDownload> FDownloadServiceStatistics::GetCurrentDownloads() const
	{
		checkSlow(IsInGameThread());
		TArray<FDownload> Result;
		Result.Empty(Downloads.Num());
		for (const TPair<int32, FDownloadTuple>& Download : Downloads)
		{
			Result.AddDefaulted();
			FDownload& Element = Result.Last();
			Element.Data = FPaths::GetCleanFilename(Download.Value.Get<0>());
			Element.Size = GetDownloadSize(Element.Data);
			Element.Received = Download.Value.Get<1>();
		}
		return Result;
	}

	int64 FDownloadServiceStatistics::GetDownloadSize(const FString& Filename) const
	{
		checkSlow(IsInGameThread());
		int64 DownloadSize = BuildPatchServices::ChunkDataSize;
		const int64* DownloadSizePtr = DownloadSizes.Find(Filename);
		if (DownloadSizePtr != nullptr)
		{
			DownloadSize = *DownloadSizePtr;
		}
		return DownloadSize;
	}

	IDownloadServiceStatistics* FDownloadServiceStatisticsFactory::Create(ISpeedRecorder* SpeedRecorder, IInstallerAnalytics* InstallerAnalytics, FBuildPatchAppManifest* Manifest)
	{
		check(SpeedRecorder != nullptr);
		check(InstallerAnalytics != nullptr);
		check(Manifest != nullptr);
		return new FDownloadServiceStatistics(SpeedRecorder, InstallerAnalytics, Manifest);
	}
};