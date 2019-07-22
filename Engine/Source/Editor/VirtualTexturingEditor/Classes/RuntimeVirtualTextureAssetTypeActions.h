// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

/** Asset actions setup for URuntimeVirtualTexture */
class FAssetTypeActions_RuntimeVirtualTexture : public FAssetTypeActions_Base
{
public:
	FAssetTypeActions_RuntimeVirtualTexture() {}

protected:
	//~ Begin FAssetTypeActions_Base Interface.
	virtual UClass* GetSupportedClass() const override;
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual uint32 GetCategories() override;
	//~ End FAssetTypeActions_Base Interface.
};
