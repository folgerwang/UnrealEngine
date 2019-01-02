// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

class FOculusAssetDirectory
{
public: 
#if WITH_EDITORONLY_DATA
	OCULUSHMD_API static void LoadForCook();
	OCULUSHMD_API static void ReleaseAll();
#endif

	static FSoftObjectPath AssetListing[];
};
