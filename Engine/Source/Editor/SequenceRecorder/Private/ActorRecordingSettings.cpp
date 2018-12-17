// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ActorRecordingSettings.h"
#include "IMovieSceneSectionRecorderFactory.h"
#include "Features/IModularFeatures.h"

FActorRecordingSettings::FActorRecordingSettings()
{
	CreateSettingsObjectsFromFactory();
}

FActorRecordingSettings::FActorRecordingSettings(UObject* InOuter)
{
	Outer = InOuter;
	CreateSettingsObjectsFromFactory();
}

void FActorRecordingSettings::CreateSettingsObjectsFromFactory()
{
	TArray<IMovieSceneSectionRecorderFactory*> ModularFeatures = IModularFeatures::Get().GetModularFeatureImplementations<IMovieSceneSectionRecorderFactory>("MovieSceneSectionRecorderFactory");
	for (IMovieSceneSectionRecorderFactory* Factory : ModularFeatures)
	{
		UObject* SettingsObject = Factory->CreateSettingsObject(Outer.Get() ? Outer.Get() : (UObject*)GetTransientPackage());
		if (SettingsObject)
		{
			Settings.Add(SettingsObject);
		}
	}
}