// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CurveKeyEditors/SBoolCurveKeyEditor.h"
#include "Widgets/Input/SCheckBox.h"
#include "ISequencer.h"
#include "ScopedTransaction.h"
#include "Channels/MovieSceneBoolChannel.h"

#define LOCTEXT_NAMESPACE "BoolCurveKeyEditor"

void SBoolCurveKeyEditor::Construct(const FArguments& InArgs, const TSequencerKeyEditor<FMovieSceneBoolChannel, bool>& InKeyEditor)
{
	KeyEditor = InKeyEditor;

	ChildSlot
	[
		SNew(SCheckBox)
		.IsChecked(this, &SBoolCurveKeyEditor::IsChecked)
		.OnCheckStateChanged(this, &SBoolCurveKeyEditor::OnCheckStateChanged)
	];
}

ECheckBoxState SBoolCurveKeyEditor::IsChecked() const
{
	return KeyEditor.GetCurrentValue() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SBoolCurveKeyEditor::OnCheckStateChanged(ECheckBoxState NewCheckboxState)
{
	FScopedTransaction Transaction(LOCTEXT("SetBoolKey", "Set Bool Key Value"));

	const bool bNewValue = NewCheckboxState == ECheckBoxState::Checked;
	KeyEditor.SetValueWithNotify(bNewValue, EMovieSceneDataChangeType::TrackValueChangedRefreshImmediately);
}

#undef LOCTEXT_NAMESPACE
