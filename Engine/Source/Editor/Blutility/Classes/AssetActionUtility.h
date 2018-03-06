// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GlobalEditorUtilityBase.h"
#include "AssetActionUtility.generated.h"

/** 
 * Base class for all asset action-related utilities
 * Any functions/events that are exposed on derived classes that have the correct signature will be
 * included as menu options when right-clicking on a group of assets int he content browser.
 */
UCLASS(Abstract, hideCategories=(Object), Blueprintable)
class BLUTILITY_API UAssetActionUtility : public UGlobalEditorUtilityBase
{
	GENERATED_BODY()

public:
	/** Return the class that this asset action supports */
	UFUNCTION(BlueprintPure, BlueprintImplementableEvent, Category="Assets")
	UClass* GetSupportedClass() const;
};