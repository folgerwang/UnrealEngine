// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterEditor.h"
#include "DisplayClusterEditorSettings.h"

#include "Modules/ModuleManager.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/Class.h"
#include "ISettingsModule.h"


#define LOCTEXT_NAMESPACE "DisplayClusterEditor"

void FDisplayClusterEditorModule::StartupModule()
{
	RegisterSettings();
}

void FDisplayClusterEditorModule::ShutdownModule()
{
	UnregisterSettings();
}


void FDisplayClusterEditorModule::RegisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{				
		SettingsModule->RegisterSettings(
			"Project", "Plugins", "nDisplay",
			LOCTEXT("RuntimeSettingsName", "nDisplay"),
			LOCTEXT("RuntimeSettingsDescription", "Configure nDisplay"),
			GetMutableDefault<UDisplayClusterEditorSettings>()
		);
	}
}

void FDisplayClusterEditorModule::UnregisterSettings()
{	
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "nDisplay");
	}
}


IMPLEMENT_MODULE(FDisplayClusterEditorModule, DisplayClusterEditor);

#undef LOCTEXT_NAMESPACE
