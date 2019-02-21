// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#if WITH_EDITOR || PLATFORM_ANDROID || PLATFORM_IOS

#include "UObject/SoftObjectPtr.h"
#include "Materials/Material.h"
#include "WebBrowserAssetManager.generated.h"

class UMaterial;
/**
 * 
 */
UCLASS()
class WEBBROWSERWIDGET_API UWebBrowserAssetManager : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	void LoadDefaultMaterials();

	UMaterial* GetDefaultMaterial(); 
	UMaterial* GetDefaultTranslucentMaterial(); 

protected:
	UPROPERTY()
	TSoftObjectPtr<UMaterial> DefaultMaterial;
	TSoftObjectPtr<UMaterial> DefaultTranslucentMaterial;
};

#endif