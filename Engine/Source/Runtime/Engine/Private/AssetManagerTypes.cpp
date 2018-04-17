// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Engine/AssetManagerTypes.h"
#include "Engine/AssetManager.h"
#include "Engine/AssetManagerSettings.h"

bool FPrimaryAssetTypeInfo::FillRuntimeData()
{
	if (PrimaryAssetType == NAME_None)
	{
		// Invalid type
		return false;
	}

	if (!ensureMsgf(!AssetBaseClass.IsNull(), TEXT("Primary Asset Type %s must have a class set!"), *PrimaryAssetType.ToString()))
	{
		return false;
	}

	// Hot reload may have messed up asset pointer
	AssetBaseClass.ResetWeakPtr();
	AssetBaseClassLoaded = AssetBaseClass.LoadSynchronous();

	if (!ensureMsgf(AssetBaseClassLoaded, TEXT("Failed to load class %s for Primary Asset Type %s!"), *AssetBaseClass.ToString(), *PrimaryAssetType.ToString()))
	{
		return false;
	}

	for (const FSoftObjectPath& AssetRef : SpecificAssets)
	{
		if (!AssetRef.IsNull())
		{
			AssetScanPaths.AddUnique(AssetRef.ToString());
		}
	}

	for (const FDirectoryPath& PathRef : Directories)
	{
		if (!PathRef.Path.IsEmpty())
		{
			AssetScanPaths.AddUnique(PathRef.Path);
		}
	}

	if (AssetScanPaths.Num() == 0)
	{
		// No scan locations picked out
		return false;
	}

	

	return true;
}

bool FPrimaryAssetRules::IsDefault() const
{
	return *this == FPrimaryAssetRules();
}

void FPrimaryAssetRules::OverrideRules(const FPrimaryAssetRules& OverrideRules)
{
	static FPrimaryAssetRules DefaultRules;

	if (OverrideRules.Priority != DefaultRules.Priority)
	{
		Priority = OverrideRules.Priority;
	}

	if (OverrideRules.bApplyRecursively != DefaultRules.bApplyRecursively)
	{
		bApplyRecursively = OverrideRules.bApplyRecursively;
	}

	if (OverrideRules.ChunkId != DefaultRules.ChunkId)
	{
		ChunkId = OverrideRules.ChunkId;
	}

	if (OverrideRules.CookRule != DefaultRules.CookRule)
	{
		CookRule = OverrideRules.CookRule;
	}
}

void FPrimaryAssetRules::PropagateCookRules(const FPrimaryAssetRules& ParentRules)
{
	static FPrimaryAssetRules DefaultRules;

	if (ParentRules.ChunkId != DefaultRules.ChunkId && ChunkId == DefaultRules.ChunkId)
	{
		ChunkId = ParentRules.ChunkId;
	}

	if (ParentRules.CookRule != DefaultRules.CookRule && CookRule == DefaultRules.CookRule)
	{
		CookRule = ParentRules.CookRule;
	}
}

#if WITH_EDITOR
void UAssetManagerSettings::PostInitProperties()
{
	Super::PostInitProperties();

	ApplyMetaDataTagsSettings();
}

void UAssetManagerSettings::ApplyMetaDataTagsSettings()
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
				UE_LOG(LogAssetManager, Warning, TEXT("Cannot use duplicate metadata tag '%s' for Asset Registry"), *Tag.ToString());
			}
		}
	}
}

void UAssetManagerSettings::ClearMetaDataTagsSettings()
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

void UAssetManagerSettings::PreEditChange(UProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	if (PropertyAboutToChange != nullptr)
	{
		FName PropertyName = PropertyAboutToChange->GetFName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UAssetManagerSettings, MetaDataTagsForAssetRegistry))
		{
			ClearMetaDataTagsSettings();
		}
	}
}

void UAssetManagerSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName = (PropertyChangedEvent.MemberProperty != NULL) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UAssetManagerSettings, MetaDataTagsForAssetRegistry))
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
					UE_LOG(LogAssetManager, Warning, TEXT("Cannot use duplicate metadata tag '%s' for Asset Registry"), *NewValue.ToString());
				}
			}
		}
		ApplyMetaDataTagsSettings();
	}
	else if (PropertyChangedEvent.Property && UAssetManager::IsValid())
	{
		UAssetManager::Get().ReinitializeFromConfig();
	}
}
#endif