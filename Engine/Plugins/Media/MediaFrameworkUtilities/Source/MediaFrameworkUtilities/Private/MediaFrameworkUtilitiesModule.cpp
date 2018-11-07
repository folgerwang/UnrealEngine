// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MediaFrameworkUtilitiesModule.h"

#include "Engine/Engine.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Profile/MediaProfile.h"
#include "Profile/MediaProfileManager.h"
#include "Profile/MediaProfileSettings.h"

#if WITH_EDITOR
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#endif //WITH_EDITOR



DEFINE_LOG_CATEGORY(LogMediaFrameworkUtilities);

#define LOCTEXT_NAMESPACE "MediaFrameworkUtilities"

/**
 * Implements the MediaFrameworkUtilitiesModule module.
 */
class FMediaFrameworkUtilitiesModule : public IMediaFrameworkUtilitiesModule
{
	FMediaProfileManager MediaProfileManager;
	FDelegateHandle PostEngineInitHandle;

	virtual void StartupModule() override
	{
		RegisterSettings();
		ApplyStartupMediaProfile();
	}

	virtual void ShutdownModule() override
	{
		RemoveStartupMediaProfile();
		UnregisterSettings();
	}

	virtual IMediaProfileManager& GetProfileManager() override
	{
		return MediaProfileManager;
	}

	void RegisterSettings()
	{
#if WITH_EDITOR
		if (GIsEditor)
		{
			// register settings
			ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
			if (SettingsModule != nullptr)
			{
				SettingsModule->RegisterSettings("Project", "Plugins", "MediaProfile",
					LOCTEXT("MediaProfilesSettingsName", "Media Profile"),
					LOCTEXT("MediaProfilesDescription", "Configure the Media Profile."),
					GetMutableDefault<UMediaProfileSettings>()
				);

				SettingsModule->RegisterSettings("Editor", "General", "MediaProfile",
					LOCTEXT("MediaProfilesSettingsName", "Media Profile"),
					LOCTEXT("MediaProfilesDescription", "Configure the Media Profile."),
					GetMutableDefault<UMediaProfileEditorSettings>()
				);
			}
		}
#endif //WITH_EDITOR
	}

	void UnregisterSettings()
	{
#if WITH_EDITOR
		if (GIsEditor)
		{
			// unregister settings
			ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
			if (SettingsModule != nullptr)
			{
				SettingsModule->UnregisterSettings("Project", "Media", "MediaProfile");
				SettingsModule->UnregisterSettings("Editor", "Media", "MediaProfile");
			}
		}
#endif //WITH_EDITOR
	}

	void ApplyStartupMediaProfile()
	{
		auto ApplyMediaProfile = [this]()
		{
			UMediaProfile* MediaProfile = nullptr;

#if WITH_EDITOR
			MediaProfile = GetDefault<UMediaProfileEditorSettings>()->GetUserMediaProfile();
#endif

			if (MediaProfile == nullptr)
			{
				MediaProfile = GetDefault<UMediaProfileSettings>()->GetStartupMediaProfile();
			}

			MediaProfileManager.SetCurrentMediaProfile(MediaProfile);
		};

		if (FApp::CanEverRender() || GetDefault<UMediaProfileSettings>()->bApplyInCommandlet)
		{
			if (GEngine && GEngine->IsInitialized())
			{
				ApplyMediaProfile();
			}
			else
			{
				PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddLambda(ApplyMediaProfile);
			}
		}
	}

	void RemoveStartupMediaProfile()
	{
		if (PostEngineInitHandle.IsValid())
		{
			FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
		}

		if (!GIsRequestingExit)
		{
			MediaProfileManager.SetCurrentMediaProfile(nullptr);
		}
	}
};

IMPLEMENT_MODULE(FMediaFrameworkUtilitiesModule, MediaFrameworkUtilities);

#undef LOCTEXT_NAMESPACE
