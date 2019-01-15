// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TakesCoreFwd.h"
#include "TakeMetaData.h"
#include "TakesCoreBlueprintLibrary.h"

#include "LevelSequence.h"
#include "MovieSceneToolsModule.h"
#include "MovieSceneSequence.h"
#include "Sections/MovieSceneSubSection.h"

#include "AssetRegistryModule.h"

class FTakesCoreTakeData : public IMovieSceneToolsTakeData
{
public:
	virtual ~FTakesCoreTakeData() { }

	virtual bool GatherTakes(const UMovieSceneSection* Section, TArray<uint32>& TakeNumbers, uint32& CurrentTakeNumber)
	{
		const UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section);
		if (!SubSection)
		{
			return false;
		}

		ULevelSequence* Sequence = Cast<ULevelSequence>(SubSection->GetSequence());
		if (!Sequence)
		{
			return false;
		}

		UTakeMetaData* TakeMetaData = Sequence->FindMetaData<UTakeMetaData>();
		if (!TakeMetaData)
		{
			return false;
		}

		TArray<FAssetData> TakeDatas = UTakesCoreBlueprintLibrary::FindTakes(TakeMetaData->GetSlate());
		for (FAssetData TakeData : TakeDatas)
		{
			FAssetDataTagMapSharedView::FFindTagResult TakeNumberTag = TakeData.TagsAndValues.FindTag(UTakeMetaData::AssetRegistryTag_TakeNumber);

			int32 ThisTakeNumber = 0;
			if (TakeNumberTag.IsSet() && LexTryParseString(ThisTakeNumber, *TakeNumberTag.GetValue()))
			{
				TakeNumbers.Add(ThisTakeNumber);
			}
		}

		CurrentTakeNumber = TakeMetaData->GetTakeNumber();

		return true;
	}

	virtual UObject* GetTake(const UMovieSceneSection* Section, uint32 TakeNumber)
	{
		const UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section);
		if (!SubSection)
		{
			return nullptr;
		}

		ULevelSequence* Sequence = Cast<ULevelSequence>(SubSection->GetSequence());
		if (!Sequence)
		{
			return nullptr;
		}

		UTakeMetaData* TakeMetaData = Sequence->FindMetaData<UTakeMetaData>();
		if (!TakeMetaData)
		{
			return nullptr;
		}

		TArray<FAssetData> TakeDatas = UTakesCoreBlueprintLibrary::FindTakes(TakeMetaData->GetSlate());
		for (FAssetData TakeData : TakeDatas)
		{
			FAssetDataTagMapSharedView::FFindTagResult TakeNumberTag = TakeData.TagsAndValues.FindTag(UTakeMetaData::AssetRegistryTag_TakeNumber);

			int32 ThisTakeNumber = 0;
			if (TakeNumberTag.IsSet() && LexTryParseString(ThisTakeNumber, *TakeNumberTag.GetValue()))
			{
				if (ThisTakeNumber == TakeNumber)
				{
					return TakeData.GetAsset();
				}
			}
		}

		return nullptr;
	}
};