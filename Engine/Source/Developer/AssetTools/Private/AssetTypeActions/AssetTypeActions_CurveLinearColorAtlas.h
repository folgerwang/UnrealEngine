// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions/AssetTypeActions_Texture.h"
#include "Curves/CurveLinearColorAtlas.h"

class FMenuBuilder;

class FAssetTypeActions_CurveLinearColorAtlas : public FAssetTypeActions_Texture
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_CurveLinearColorAtlas", "Curve Atlas"); }
	virtual FColor GetTypeColor() const override { return FColor(128,64,64); }
	virtual UClass* GetSupportedClass() const override { return UCurveLinearColorAtlas::StaticClass(); }
	virtual bool HasActions ( const TArray<UObject*>& InObjects ) const override { return true; }
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	virtual bool IsImportedAsset() const override { return false; }
	virtual bool CanFilter() override { return true; }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
};
