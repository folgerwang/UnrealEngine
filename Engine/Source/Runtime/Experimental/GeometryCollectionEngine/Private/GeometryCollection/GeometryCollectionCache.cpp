// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionCache.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "HAL/IConsoleManager.h"
#include "Serialization/ArchiveCountMem.h"
#include "Features/IModularFeatures.h"

DEFINE_LOG_CATEGORY(LogGeometryCollectionCache);

FName UGeometryCollectionCache::TagName_Name = FName("CollectionName");
FName UGeometryCollectionCache::TagName_IdGuid = FName("CollectionIdGuid");
FName UGeometryCollectionCache::TagName_StateGuid = FName("CollectionStateGuid");

void UGeometryCollectionCache::SetFromRawTrack(const FRecordedTransformTrack& InTrack)
{
	ProcessRawRecordedDataInternal(InTrack);
	CompatibleCollectionState = SupportedCollection ? SupportedCollection->GetStateGuid() : FGuid();
}

void UGeometryCollectionCache::SetFromTrack(const FRecordedTransformTrack& InTrack)
{
	RecordedData = InTrack;
	CompatibleCollectionState = SupportedCollection ? SupportedCollection->GetStateGuid() : FGuid();
}

void UGeometryCollectionCache::SetSupportedCollection(UGeometryCollection* InCollection)
{
	if(InCollection != SupportedCollection)
	{
		// New collection. Set it and then clear out recorded data
		SupportedCollection = InCollection;
		RecordedData.Records.Reset();
	}
}

void UGeometryCollectionCache::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	OutTags.Add(FAssetRegistryTag(TagName_Name, SupportedCollection ? SupportedCollection->GetName() : FString(TEXT("None")), FAssetRegistryTag::TT_Alphabetical));
	OutTags.Add(FAssetRegistryTag(TagName_IdGuid, SupportedCollection ? SupportedCollection->GetIdGuid().ToString() : FString(TEXT("INVALID")), FAssetRegistryTag::TT_Hidden));
	OutTags.Add(FAssetRegistryTag(TagName_StateGuid, SupportedCollection ? CompatibleCollectionState.ToString() : FString(TEXT("INVALID")), FAssetRegistryTag::TT_Hidden));
}

UGeometryCollectionCache* UGeometryCollectionCache::CreateCacheForCollection(UGeometryCollection* InCollection)
{
	UGeometryCollectionCache* ResultCache = nullptr;

	if(InCollection)
	{
		IModularFeatures& ModularFeatures = IModularFeatures::Get();
		if(ModularFeatures.IsModularFeatureAvailable(ITargetCacheProvider::GetFeatureName()))
		{
			ITargetCacheProvider* Provider = &ModularFeatures.GetModularFeature<ITargetCacheProvider>(ITargetCacheProvider::GetFeatureName());
			check(Provider);

			ResultCache = Provider->GetCacheForCollection(InCollection);

			if(ResultCache)
			{
				ResultCache->SetSupportedCollection(InCollection);
			}
		}
	}

	return ResultCache;
}

void UGeometryCollectionCache::ProcessRawRecordedDataInternal(const FRecordedTransformTrack& InTrack)
{
	RecordedData = FRecordedTransformTrack::ProcessRawRecordedData(InTrack);
}
