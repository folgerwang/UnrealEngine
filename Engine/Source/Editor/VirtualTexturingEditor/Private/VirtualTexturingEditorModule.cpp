// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsModule.h"

#include "PropertyEditorModule.h"
#include "RuntimeVirtualTextureAssetTypeActions.h"
#include "RuntimeVirtualTextureDetailsCustomization.h"

#define LOCTEXT_NAMESPACE "VirtualTexturingEditorModule"

class FVirtualTexturingEditorModule
	: public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_RuntimeVirtualTexture));
	
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout("RuntimeVirtualTexture", FOnGetDetailCustomizationInstance::CreateStatic(&FRuntimeVirtualTextureDetailsCustomization::MakeInstance));
		PropertyModule.RegisterCustomClassLayout("RuntimeVirtualTextureComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FRuntimeVirtualTextureComponentDetailsCustomization::MakeInstance));
	}

	virtual void ShutdownModule() override
	{
	}

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}
};

IMPLEMENT_MODULE(FVirtualTexturingEditorModule, VirtualTexturingEditor);

#undef LOCTEXT_NAMESPACE
