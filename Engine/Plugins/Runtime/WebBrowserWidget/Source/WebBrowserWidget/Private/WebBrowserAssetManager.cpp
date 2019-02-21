// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR || PLATFORM_ANDROID || PLATFORM_IOS
#include "WebBrowserAssetManager.h"
#include "WebBrowserTexture.h"

/////////////////////////////////////////////////////
// WebBrowserAssetManager

UWebBrowserAssetManager::UWebBrowserAssetManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) ,
	DefaultMaterial(FString(TEXT("/WebBrowserWidget/WebTexture_M.WebTexture_M"))),
	DefaultTranslucentMaterial(FString(TEXT("/WebBrowserWidget/WebTexture_TM.WebTexture_TM")))
{
	// Add a hard reference to UWebBrowserTexture, without this the WebBrowserTexture DLL never gets loaded on Windows.
	UWebBrowserTexture::StaticClass();
};

void UWebBrowserAssetManager::LoadDefaultMaterials()
{
	DefaultMaterial.LoadSynchronous();
	DefaultTranslucentMaterial.LoadSynchronous();
}

UMaterial* UWebBrowserAssetManager::GetDefaultMaterial()
{
	return DefaultMaterial.Get();
}

UMaterial* UWebBrowserAssetManager::GetDefaultTranslucentMaterial()
{
	return DefaultTranslucentMaterial.Get();
}
#endif
