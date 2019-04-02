// Copyright (c) Microsoft Corporation. All rights reserved.

#include "WindowsMixedRealityRuntimeSettings.h"
#include "Modules/ModuleInterface.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "WindowsMixedRealityDetails.h"

#define LOCTEXT_NAMESPACE "FWindowsMixedRealityPlatformEditorModule"


/**
 * Module for WindowsMR platform editor utilities
 */
class FWindowsMixedRealityPlatformEditorModule
	: public IModuleInterface
{
	virtual void StartupModule() override
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(FName("WindowsMixedRealityRuntimeSettings"), FOnGetDetailCustomizationInstance::CreateStatic(&FWindowsMixedRealityDetails::MakeInstance));
		PropertyModule.NotifyCustomizationModuleChanged();

		// register settings
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			{
				SettingsModule->RegisterSettings("Project", "Platforms", "WindowsMixedReality",
					LOCTEXT("RuntimeSettingsName", "Windows Mixed Reality"),
					LOCTEXT("RuntimeSettingsDescription", "Project settings for Windows Mixed Reality"),
					GetMutableDefault<UWindowsMixedRealityRuntimeSettings>()
				);
			}
		}
	}

	virtual void ShutdownModule() override
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Platforms", "WindowsMixedReality");
		}
	}
};


IMPLEMENT_MODULE(FWindowsMixedRealityPlatformEditorModule, WindowsMixedRealityPlatformEditor);

#undef LOCTEXT_NAMESPACE
