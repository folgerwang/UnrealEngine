// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Recorder/TakeRecorderPanel.h"
#include "Widgets/STakeRecorderTabContent.h"
#include "TakePresetToolkit.h"

bool UTakeRecorderPanel::IsPanelOpen() const
{
	return WeakTabContent.Pin().IsValid();
}

void UTakeRecorderPanel::InitializePanel(TWeakPtr<STakeRecorderTabContent> InTabContent)
{
	WeakTabContent = InTabContent;
}

void UTakeRecorderPanel::ClosePanel()
{
	WeakTabContent = nullptr;
}

bool UTakeRecorderPanel::ValidateTabContent() const
{
	if (!IsPanelOpen())
	{
		FFrame::KismetExecutionMessage(TEXT("This take recorder panel is not open. Either re-call OpenTakeRecorderPanel or GetTakeRecorderPanel to get the current UI panel."), ELogVerbosity::Error);
		return false;
	}
	return true;
}

ETakeRecorderPanelMode UTakeRecorderPanel::GetMode() const
{
	if (!ValidateTabContent())
	{
		return ETakeRecorderPanelMode::NewRecording;
	}

	return WeakTabContent.Pin()->GetMode();
}

void UTakeRecorderPanel::SetupForRecording_TakePreset(UTakePreset* TakePresetAsset)
{
	if (ValidateTabContent())
	{
		WeakTabContent.Pin()->SetupForRecording(TakePresetAsset);
	}
}

void UTakeRecorderPanel::SetupForRecording_LevelSequence(ULevelSequence* LevelSequenceAsset)
{
	if (ValidateTabContent())
	{
		WeakTabContent.Pin()->SetupForRecording(LevelSequenceAsset);
	}
}

void UTakeRecorderPanel::SetupForEditing(UTakePreset* TakePreset)
{
	if (ValidateTabContent())
	{
		TSharedPtr<FTakePresetToolkit> Toolkit = MakeShared<FTakePresetToolkit>();
		Toolkit->Initialize(EToolkitMode::WorldCentric, nullptr, TakePreset);
		WeakTabContent.Pin()->SetupForEditing(Toolkit);
	}
}

void UTakeRecorderPanel::SetupForViewing(ULevelSequence* LevelSequenceAsset)
{
	if (ValidateTabContent())
	{
		WeakTabContent.Pin()->SetupForViewing(LevelSequenceAsset);
	}
}

void UTakeRecorderPanel::NewTake()
{
	if (ValidateTabContent())
	{
		WeakTabContent.Pin()->NewTake();
	}
}

ULevelSequence* UTakeRecorderPanel::GetLevelSequence() const
{
	if (!ValidateTabContent())
	{
		return nullptr;
	}

	return WeakTabContent.Pin()->GetLevelSequence();

}

UTakeMetaData* UTakeRecorderPanel::GetTakeMetaData() const
{
	if (!ValidateTabContent())
	{
		return nullptr;
	}

	return WeakTabContent.Pin()->GetTakeMetaData();
}

UTakeRecorderSources* UTakeRecorderPanel::GetSources() const
{
	if (!ValidateTabContent())
	{
		return nullptr;
	}

	return WeakTabContent.Pin()->GetSources();
}

void UTakeRecorderPanel::StartRecording() const
{
	if (ValidateTabContent())
	{
		return WeakTabContent.Pin()->StartRecording();
	}
}

void UTakeRecorderPanel::StopRecording() const
{
	if (ValidateTabContent())
	{
		return WeakTabContent.Pin()->StopRecording();
	}
}

