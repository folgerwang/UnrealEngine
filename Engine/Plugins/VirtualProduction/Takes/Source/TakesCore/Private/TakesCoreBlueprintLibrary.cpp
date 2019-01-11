// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TakesCoreBlueprintLibrary.h"
#include "TakeMetaData.h"
#include "LevelSequence.h"
#include "AssetRegistryModule.h"

int32 UTakesCoreBlueprintLibrary::ComputeNextTakeNumber(const FString& Slate)
{
	int32 MaxTake = 0;

	for (const FAssetData& Asset : FindTakes(Slate))
	{
		FAssetDataTagMapSharedView::FFindTagResult TakeNumberTag = Asset.TagsAndValues.FindTag(UTakeMetaData::AssetRegistryTag_TakeNumber);

		int32 ThisTakeNumber = 0;
		if (TakeNumberTag.IsSet() && LexTryParseString(ThisTakeNumber, *TakeNumberTag.GetValue()))
		{
			MaxTake = FMath::Max(MaxTake, ThisTakeNumber);
		}
	}

	return MaxTake + 1;
}

TArray<FAssetData> UTakesCoreBlueprintLibrary::FindTakes(const FString& Slate, int32 TakeNumber)
{
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName).Get();

	FARFilter Filter;
	Filter.ClassNames.Add(ULevelSequence::StaticClass()->GetFName());
	Filter.TagsAndValues.Add(UTakeMetaData::AssetRegistryTag_Slate, Slate);

	TArray<FAssetData> AllAssets;
	AssetRegistry.GetAssets(Filter, AllAssets);

	// Filter.TagsAndValues matches *any* tag/value pair, rather than *all*, so we have to run the filter twice
	if (TakeNumber > 0 && AllAssets.Num() > 0)
	{
		FARFilter TakeFilter;
		TakeFilter.TagsAndValues.Add(UTakeMetaData::AssetRegistryTag_TakeNumber, LexToString(TakeNumber));
		AssetRegistry.RunAssetsThroughFilter(AllAssets, TakeFilter);
	}
	
	return AllAssets;
}