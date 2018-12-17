// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "IShotgunModule.h"
#include "ShotgunSettings.h"
#include "ShotgunUIManager.h"

#include "ISettingsModule.h"
#include "ISettingsSection.h"

#define LOCTEXT_NAMESPACE "Shotgun"

class FShotgunModule : public IShotgunModule
{
public:
	virtual void StartupModule() override
	{
		if (GIsEditor && !IsRunningCommandlet())
		{
			RegisterSettings();

			FShotgunUIManager::Initialize();
		}
	}

	virtual void ShutdownModule() override
	{
		if ((GIsEditor && !IsRunningCommandlet()))
		{
			FShotgunUIManager::Shutdown();

			UnregisterSettings();
		}
	}

protected:
	void RegisterSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule != nullptr)
		{
			ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project", "Plugins", "Shotgun",
				LOCTEXT("ShotgunSettingsName", "Shotgun"),
				LOCTEXT("ShotgunSettingsDescription", "Configure the Shotgun plug-in."),
				GetMutableDefault<UShotgunSettings>()
			);
		}
	}

	void UnregisterSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "Shotgun");
		}
	}
};

IMPLEMENT_MODULE(FShotgunModule, Shotgun);

#undef LOCTEXT_NAMESPACE
