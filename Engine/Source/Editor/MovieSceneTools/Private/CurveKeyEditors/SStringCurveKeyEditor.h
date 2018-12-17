// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "SequencerKeyEditor.h"
#include "Sections/MovieSceneStringChannel.h"

struct FMovieSceneStringChannel;

/**
 * A widget for editing a curve representing string keys.
 */
class SStringCurveKeyEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SStringCurveKeyEditor) {}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, const TSequencerKeyEditor<FMovieSceneStringChannel, FString>& InKeyEditor);

private:

	FText GetText() const;
	void OnTextCommitted(const FText& InText, ETextCommit::Type InCommitType);

	TSequencerKeyEditor<FMovieSceneStringChannel, FString> KeyEditor;
};
