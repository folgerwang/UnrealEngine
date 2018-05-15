// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/NodeMappingContainer.h"

class FAssetTypeActions_NodeMappingContainer : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_NodeMappingContainer", "Node Mapping Container"); }
	virtual FColor GetTypeColor() const override { return FColor(112,146,190); }
	virtual UClass* GetSupportedClass() const override { return UNodeMappingContainer::StaticClass(); }
	virtual bool CanFilter() override { return true; }
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override { return false; }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Animation; }
};
