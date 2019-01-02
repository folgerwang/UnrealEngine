// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneFadeSection.h"
#include "UObject/SequencerObjectVersion.h"


/* UMovieSceneFadeSection structors
 *****************************************************************************/

UMovieSceneFadeSection::UMovieSceneFadeSection()
	: UMovieSceneFloatSection()
	, FadeColor(FLinearColor::Black)
	, bFadeAudio(false)
{
#if WITH_EDITORONLY_DATA
	bIsInfinite_DEPRECATED = true;
#endif

	SetRange(TRange<FFrameNumber>::All());

	EvalOptions.EnableAndSetCompletionMode
		(GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToProjectDefault ? 
			EMovieSceneCompletionMode::RestoreState : 
			EMovieSceneCompletionMode::ProjectDefault);
}
