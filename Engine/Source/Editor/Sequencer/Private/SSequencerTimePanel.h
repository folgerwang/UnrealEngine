// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "Styling/SlateTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "MovieSceneFwd.h"
#include "FrameNumberDisplayFormat.h"

class FSequencer;
class UMovieScene;
class UMovieSceneTrack;
class UMovieSceneSection;
class UMovieSceneSequence;

class SSequencerTimePanel : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SSequencerTimePanel){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TWeakPtr<FSequencer> Sequencer);

private:

	FReply OnBorderFadeClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	FReply Close();
	FReply Apply();

	EVisibility GetWarningVisibility() const;

	FFrameRate OnGetFrameResolution() const;
	bool IsRecommendedResolution(FFrameRate InFrameRate) const;

	FText GetSupportedTimeRange() const;

	void OnSetFrameResolution(FFrameRate InFrameResolution);

	void UpdateCommonFrameRates();

	UMovieSceneSequence* GetFocusedSequence() const;

	static void MigrateFrameTimes(FFrameRate SourceRate, FFrameRate DestinationRate, UMovieScene* MovieScene);
	static void MigrateFrameTimes(FFrameRate SourceRate, FFrameRate DestinationRate, UMovieSceneTrack* Track);
	static void MigrateFrameTimes(FFrameRate SourceRate, FFrameRate DestinationRate, UMovieSceneSection* Section);

	FFrameRate CurrentFrameResolution;
	TSharedPtr<SVerticalBox> CommonFrameRates;
	TWeakPtr<FSequencer> WeakSequencer;
};