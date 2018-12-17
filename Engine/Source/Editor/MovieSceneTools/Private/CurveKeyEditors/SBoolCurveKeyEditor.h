// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "SequencerKeyEditor.h"
#include "Channels/MovieSceneBoolChannel.h"

enum class ECheckBoxState : uint8;

/**
 * A widget for editing a curve representing bool keys.
 */
class SBoolCurveKeyEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBoolCurveKeyEditor) {}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, const TSequencerKeyEditor<FMovieSceneBoolChannel, bool>& InKeyEditor);

private:

	ECheckBoxState IsChecked() const;
	void OnCheckStateChanged(ECheckBoxState);

	TSequencerKeyEditor<FMovieSceneBoolChannel, bool> KeyEditor;
};
