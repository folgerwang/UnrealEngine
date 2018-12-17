// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneMediaSection.h"
#include "MovieScene.h"

#define LOCTEXT_NAMESPACE "MovieSceneMediaSection"


/* UMovieSceneMediaSection interface
 *****************************************************************************/

UMovieSceneMediaSection::UMovieSceneMediaSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	ThumbnailReferenceOffset = 0.f;
#endif

	EvalOptions.CompletionMode = EMovieSceneCompletionMode::RestoreState;
}

void UMovieSceneMediaSection::PostInitProperties()
{
	Super::PostInitProperties();

	UMovieScene* Outer = GetTypedOuter<UMovieScene>();
	FFrameRate TickResolution = Outer ? Outer->GetTickResolution() : FFrameRate(24, 1);

	// media tracks have some preroll by default to precache frames
	SetPreRollFrames( (0.5 * TickResolution).RoundToFrame().Value );
}

#undef LOCTEXT_NAMESPACE
