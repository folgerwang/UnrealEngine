// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AdvancedCopyCustomization.h"
#include "Engine/AssetUserData.h"
#include "AssetToolsSettings.generated.h"

USTRUCT()
struct FAdvancedCopyMap
{
	GENERATED_BODY()

public:
	/** When copying this class, use a particular set of dependency and destination rules */
	UPROPERTY(EditAnywhere, Category = "Asset Tools", meta = (MetaClass = "Object"))
	FSoftClassPath ClassToCopy;

	/** The set of dependency and destination rules to use for advanced copy */
	UPROPERTY(EditAnywhere, Category = "Asset Tools", meta = (MetaClass = "AdvancedCopyCustomization"))
	FSoftClassPath AdvancedCopyCustomization;
};

UCLASS(config = Game, defaultconfig, notplaceable, meta = (DisplayName = "Asset Tools"))
class ASSETTOOLS_API UAssetToolsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAssetToolsSettings() {};

	/** List of rules to use when advanced copying assets */
	UPROPERTY(config, EditAnywhere, Category = "Asset Tools", Meta = (TitleProperty = "ClassToCopy"))
	TArray<FAdvancedCopyMap> AdvancedCopyCustomizations;
};
