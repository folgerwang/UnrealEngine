// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Misc/SecureHash.h"
#include "Common/StatsCollector.h"
#include "BuildPatchFeatureLevel.h"

namespace BuildPatchServices
{
	class ICloudEnumeration
	{
	public:
		virtual bool IsComplete() const = 0;
		virtual const TSet<uint32>& GetChunkWindowSizes() const = 0;
		virtual const TMap<uint64, TSet<FGuid>>& GetChunkInventory() const = 0;
		virtual const TMap<FGuid, int64>& GetChunkFileSizes() const = 0;
		virtual const TMap<FGuid, FSHAHash>& GetChunkShaHashes() const = 0;
		virtual bool IsChunkFeatureLevelMatch(const FGuid& ChunkId) const = 0;
		virtual const uint64& GetChunkHash(const FGuid& ChunkId) const = 0;
		virtual const FSHAHash& GetChunkShaHash(const FGuid& ChunkId) const = 0;
	};

	typedef TSharedRef<ICloudEnumeration, ESPMode::ThreadSafe> ICloudEnumerationRef;
	typedef TSharedPtr<ICloudEnumeration, ESPMode::ThreadSafe> ICloudEnumerationPtr;

	class FCloudEnumerationFactory
	{
	public:
		static ICloudEnumerationRef Create(const FString& CloudDirectory, const FDateTime& ManifestAgeThreshold, const EFeatureLevel& OutputFeatureLevel, const FStatsCollectorRef& StatsCollector);
	};
}
