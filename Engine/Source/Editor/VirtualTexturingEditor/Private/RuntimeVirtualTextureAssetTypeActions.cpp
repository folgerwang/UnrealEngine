// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RuntimeVirtualTextureAssetTypeActions.h"

#include "Toolkits/IToolkitHost.h"
#include "VT/RuntimeVirtualTexture.h"

#define LOCTEXT_NAMESPACE "VirtualTexturingEditorModule"

UClass* FAssetTypeActions_RuntimeVirtualTexture::GetSupportedClass() const
{
	return URuntimeVirtualTexture::StaticClass();
}

FText FAssetTypeActions_RuntimeVirtualTexture::GetName() const
{
	return LOCTEXT("AssetTypeActions_RuntimeVirtualTexture", "Runtime Virtual Texture"); 
}

FColor FAssetTypeActions_RuntimeVirtualTexture::GetTypeColor() const 
{
	return FColor(128, 128, 128); 
}

uint32 FAssetTypeActions_RuntimeVirtualTexture::GetCategories() 
{
	return EAssetTypeCategories::MaterialsAndTextures; 
}

#undef LOCTEXT_NAMESPACE
