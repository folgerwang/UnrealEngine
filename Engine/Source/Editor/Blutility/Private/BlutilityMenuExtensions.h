// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FAssetData;
class FMenuBuilder;
class UGlobalEditorUtilityBase;

// Blutility Menu extension helpers
class FBlutilityMenuExtensions
{
public:
	/** Helper function to get all Blutility classes derived from the specified class name */
	static void GetBlutilityClasses(TArray<FAssetData>& OutAssets, const FName& InClassName);

	/** Helper function that populates a menu based on the exposed functions in a set of Blutility objects */
	static void CreateBlutilityActionsMenu(FMenuBuilder& MenuBuilder, TArray<UGlobalEditorUtilityBase*> Utils);
};
