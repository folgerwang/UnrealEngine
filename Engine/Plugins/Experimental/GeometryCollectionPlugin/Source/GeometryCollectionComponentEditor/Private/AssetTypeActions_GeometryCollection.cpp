// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_GeometryCollection.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "GeometryCollection.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_GeometryCollection::GetSupportedClass() const
{
	return 
		UGeometryCollection::StaticClass();
}

UThumbnailInfo* FAssetTypeActions_GeometryCollection::GetThumbnailInfo(UObject* Asset) const
{
	UGeometryCollection* GeometryCollection = CastChecked<UGeometryCollection>(Asset);
	UThumbnailInfo* ThumbnailInfo = GeometryCollection->ThumbnailInfo;
	if (ThumbnailInfo == NULL)
	{
		ThumbnailInfo = NewObject<USceneThumbnailInfo>(GeometryCollection, NAME_None, RF_Transactional);
		GeometryCollection->ThumbnailInfo = ThumbnailInfo;
	}
	return ThumbnailInfo;
}

void FAssetTypeActions_GeometryCollection::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	FAssetTypeActions_Base::GetActions(InObjects, MenuBuilder);
	//Set("ClassIcon.GeometryCollection", new IMAGE_BRUSH("Icons/AssetIcons/GeometryCollectionComponentAtlasGroup_16x", Icon16x16));
	//Set("ClassThumbnail.GeometryCollection", new IMAGE_BRUSH("Icons/AssetIcons/GeometryCollectionComponentAtlasGroup_64x", Icon64x64));
	// IconPath = Plugin->GetBaseDir() / TEXT("Resources/Icon128.png");
}

void FAssetTypeActions_GeometryCollection::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
}

#undef LOCTEXT_NAMESPACE
