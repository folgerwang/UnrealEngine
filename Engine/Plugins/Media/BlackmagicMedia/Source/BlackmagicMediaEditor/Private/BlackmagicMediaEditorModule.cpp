// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "BlackmagicMediaFinder.h"

#include "BlackmagicMediaSettings.h"
#include "BlackmagicMediaFinder.h"

#include "Customizations/BlackmagicMediaPortCustomization.h"
#include "Customizations/BlackmagicMediaModeCustomization.h"

#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Templates/UniquePtr.h"

#define LOCTEXT_NAMESPACE "BlackmagicMediaEditor"

/**
 * Implements the MediaEditor module.
 */
class FBlackmagicMediaEditorModule : public IModuleInterface
{
public:

	//~ IModuleInterface interface

	virtual void StartupModule() override
	{
		RegisterCustomizations();
		RegisterSettings();
		RegisterStyle();
	}

	virtual void ShutdownModule() override
	{
		if (!UObjectInitialized() && !GIsRequestingExit)
		{
			UnregisterStyle();
			UnregisterSettings();
			UnregisterCustomizations();
		}
	}

private:
	TUniquePtr<FSlateStyleSet> StyleInstance;

private:

	/** Register details view customizations. */
	void RegisterCustomizations()
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomPropertyTypeLayout(FBlackmagicMediaPort::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FBlackmagicMediaPortCustomization::MakeInstance));

		PropertyModule.RegisterCustomPropertyTypeLayout(FBlackmagicMediaModeInput::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FBlackmagicMediaModeCustomization::MakeInputInstance));
		PropertyModule.RegisterCustomPropertyTypeLayout(FBlackmagicMediaModeOutput::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FBlackmagicMediaModeCustomization::MakeOutputInstance));
	}

	/** Unregister details view customizations. */
	void UnregisterCustomizations()
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout(FBlackmagicMediaPort::StaticStruct()->GetFName());

		PropertyModule.UnregisterCustomPropertyTypeLayout(FBlackmagicMediaModeInput::StaticStruct()->GetFName());
		PropertyModule.UnregisterCustomPropertyTypeLayout(FBlackmagicMediaModeOutput::StaticStruct()->GetFName());
	}

	void RegisterSettings()
	{
		// register settings
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule != nullptr)
		{
			ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project", "Plugins", "BlackmagicMedia",
				LOCTEXT("BlackmagicMediaSettingsName", "Blackmagic Media"),
				LOCTEXT("BlackmagicMediaSettingsDescription", "Configure the Blackmagic Media plug-in."),
				GetMutableDefault<UBlackmagicMediaSettings>()
			);
		}
	}

	void UnregisterSettings()
	{
		// unregister settings
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "BlackmagicMedia");
		}
	}

	void RegisterStyle()
	{
#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(StyleInstance->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)

		StyleInstance = MakeUnique<FSlateStyleSet>("BlackmagicStyle");

		TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BlackmagicMedia"));
		if (Plugin.IsValid())
		{
			StyleInstance->SetContentRoot(FPaths::Combine(Plugin->GetContentDir(), TEXT("Editor/Icons")));
		}

		const FVector2D Icon20x20(20.0f, 20.0f);
		const FVector2D Icon64x64(64.0f, 64.0f);

		StyleInstance->Set("ClassThumbnail.BlackmagicMediaSource", new IMAGE_BRUSH("BlackmagicMediaSource_64x", Icon64x64));
		StyleInstance->Set("ClassIcon.BlackmagicMediaSource", new IMAGE_BRUSH("BlackmagicMediaSource_20x", Icon20x20));
		StyleInstance->Set("ClassThumbnail.BlackmagicMediaOutput", new IMAGE_BRUSH("BlackmagicMediaOutput_64x", Icon64x64));
		StyleInstance->Set("ClassIcon.BlackmagicMediaOutput", new IMAGE_BRUSH("BlackmagicMediaOutput_20x", Icon20x20));

		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance.Get());

#undef IMAGE_BRUSH
	}

	void UnregisterStyle()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance.Get());
		StyleInstance.Reset();
	}
};


IMPLEMENT_MODULE(FBlackmagicMediaEditorModule, IModuleInterface);

#undef LOCTEXT_NAMESPACE

