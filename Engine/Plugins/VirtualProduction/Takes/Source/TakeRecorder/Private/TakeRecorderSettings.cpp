// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TakeRecorderSettings.h"
#include "UObject/Package.h"

UTakeRecorderUserSettings::UTakeRecorderUserSettings()
{
	Settings.bMaximizeViewport  = false;
	Settings.CountdownSeconds   = 3.f;
	Settings.EngineTimeDilation = 1.f;
	Settings.SampleClock        = nullptr;
	Settings.bRemoveRedundantTracks = true;
	Settings.bSaveRecordedAssets = true;
	Settings.bAutoSerialize     = false;
	PresetSaveDir.Path    = TEXT("/Game/Cinematics/Takes/Presets/");
	bIsSequenceOpen       = true;
	bShowUserSettingsOnUI = false;
}

void UTakeRecorderUserSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		// Dragging spinboxes causes this to be called every frame so we wait until they've finished dragging before saving.
		SaveConfig();
	}
}

UTakeRecorderProjectSettings::UTakeRecorderProjectSettings()
{
	Settings.RootTakeSaveDir.Path = TEXT("/Game/Cinematics/Takes");
	Settings.TakeSaveDir = TEXT("{year}-{month}-{day}/{slate}_{take}");
	Settings.DefaultSlate = TEXT("Scene_1");
	Settings.bRecordSourcesIntoSubSequences = true;
	Settings.bRecordToPossessable = false;
}

void UTakeRecorderProjectSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		// Dragging spinboxes causes this to be called every frame so we wait until they've finished dragging before saving.
		SaveConfig();
	}
}