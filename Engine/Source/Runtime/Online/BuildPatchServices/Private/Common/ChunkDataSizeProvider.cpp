// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Common/ChunkDataSizeProvider.h"
#include "Misc/Paths.h"
#include "BuildPatchUtil.h"

namespace BuildPatchServices
{
	class FChunkDataSizeProvider
		: public IChunkDataSizeProvider
	{
	public:
		// Begin IDataSizeProvider
		virtual int64 GetDownloadSize(const FString& Identifier) const override
		{
			checkSlow(IsInGameThread());
			int64 DownloadSize = INDEX_NONE;
			const int64* DownloadSizePtr = DownloadSizes.Find(Identifier);
			if (DownloadSizePtr != nullptr)
			{
				DownloadSize = *DownloadSizePtr;
			}
			return DownloadSize;
		}
		// End IDataSizeProvider

		// Begin IChunkDataSizeProvider
		virtual void AddManifestData(const FBuildPatchAppManifest* Manifest) override
		{
			check(IsInGameThread());
			if (Manifest != nullptr)
			{
				TSet<FGuid> DataList;
				Manifest->GetDataList(DataList);
				for (const FGuid& DataId : DataList)
				{
					FString CleanFilename = FPaths::GetCleanFilename(FBuildPatchUtils::GetDataFilename(*Manifest, TEXT(""), DataId));
					DownloadSizes.Add(MoveTemp(CleanFilename), Manifest->GetDataSize(DataId));
				}
			}
		}
		// End IChunkDataSizeProvider

	private:
		TMap<FString, int64> DownloadSizes;
	};
	
	IChunkDataSizeProvider* FChunkDataSizeProviderFactory::Create()
	{
		return new FChunkDataSizeProvider();
	}
}
