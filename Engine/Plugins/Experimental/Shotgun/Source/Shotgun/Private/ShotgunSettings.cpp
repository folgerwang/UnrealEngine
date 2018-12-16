// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ShotgunSettings.h"

#if WITH_EDITOR

DEFINE_LOG_CATEGORY_STATIC(LogShotgun, Log, All);

void UShotgunSettings::PostInitProperties()
{
	Super::PostInitProperties();

	ApplyMetaDataTagsSettings();
}

void UShotgunSettings::ApplyMetaDataTagsSettings()
{
	TSet<FName>& GlobalTagsForAssetRegistry = UObject::GetMetaDataTagsForAssetRegistry();
	for (FName Tag : MetaDataTagsForAssetRegistry)
	{
		if (!Tag.IsNone())
		{
			if (!GlobalTagsForAssetRegistry.Contains(Tag))
			{
				GlobalTagsForAssetRegistry.Add(Tag);
			}
			else
			{
				// To catch the case where the same tag is used by different users and their settings are synced after edition
				UE_LOG(LogShotgun, Warning, TEXT("Cannot use duplicate metadata tag '%s' for Asset Registry"), *Tag.ToString());
			}
		}
	}
}

void UShotgunSettings::ClearMetaDataTagsSettings()
{
	TSet<FName>& GlobalTagsForAssetRegistry = UObject::GetMetaDataTagsForAssetRegistry();
	for (FName Tag : MetaDataTagsForAssetRegistry)
	{
		if (!Tag.IsNone())
		{
			GlobalTagsForAssetRegistry.Remove(Tag);
		}
	}
}

void UShotgunSettings::PreEditChange(UProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	if (PropertyAboutToChange != nullptr)
	{
		FName PropertyName = PropertyAboutToChange->GetFName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UShotgunSettings, MetaDataTagsForAssetRegistry))
		{
			ClearMetaDataTagsSettings();
		}
	}
}

void UShotgunSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName = (PropertyChangedEvent.MemberProperty != NULL) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UShotgunSettings, MetaDataTagsForAssetRegistry))
	{
		if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
		{
			// Check if the new value already exists in the global tags list
			int32 Index = PropertyChangedEvent.GetArrayIndex(PropertyName.ToString());
			if (Index > 0)
			{
				TSet<FName>::TIterator It = MetaDataTagsForAssetRegistry.CreateIterator();
				for (int32 i = 0; i < Index; ++i)
				{
					++It;
				}
				FName NewValue = It ? *It : FName();
				if (UObject::GetMetaDataTagsForAssetRegistry().Contains(NewValue))
				{
					*It = FName();
					UE_LOG(LogShotgun, Warning, TEXT("Cannot use duplicate metadata tag '%s' for Asset Registry"), *NewValue.ToString());
				}
			}
		}
		ApplyMetaDataTagsSettings();
	}
}
#endif