// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Recorder/TakeRecorderPanel.h"

class UTakePreset;
class UTakeMetaData;
class ULevelSequence;
class STakeRecorderPanel;
class FTakePresetToolkit;
class UTakeRecorderSources;
class STakePresetAssetEditor;

class STakeRecorderTabContent : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(STakeRecorderTabContent){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	FText GetTitle() const;

	const FSlateBrush* GetIcon() const;

	ETakeRecorderPanelMode GetMode() const;

	void SetupForRecording(ULevelSequence* LevelSequenceAsset);

	void SetupForRecording(UTakePreset* BasePreset);

	void SetupForEditing(TSharedPtr<FTakePresetToolkit> InToolkit);

	void SetupForViewing(ULevelSequence* LevelSequence);

	/*~ UTakeRecorderPanel exposure */

	ULevelSequence* GetLevelSequence() const;

	UTakeMetaData* GetTakeMetaData() const;

	UTakeRecorderSources* GetSources() const;

	void StartRecording() const;

	void StopRecording() const;

	void NewTake();

private:

	EActiveTimerReturnType OnActiveTimer(double InCurrentTime, float InDeltaTime);

private:

	TOptional<ETakeRecorderPanelMode> CurrentMode;
	TAttribute<FText> TitleAttribute;
	TAttribute<const FSlateBrush*> IconAttribute;
	TWeakPtr<STakeRecorderPanel> WeakPanel;
	TWeakPtr<STakePresetAssetEditor> WeakAssetEditor;
};
