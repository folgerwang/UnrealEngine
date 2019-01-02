// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ShotgunSettings.generated.h"

UCLASS(config = Game)
class UShotgunSettings : public UObject
{
	GENERATED_BODY()

public:
	/** The metadata tags to be transferred to the Asset Registry. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Asset Registry", DisplayName = "Metadata Tags For Asset Registry")
	TSet<FName> MetaDataTagsForAssetRegistry;

#if WITH_EDITOR
	virtual void PostInitProperties() override;
	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

private:
	void ApplyMetaDataTagsSettings();
	void ClearMetaDataTagsSettings();
#endif
};
