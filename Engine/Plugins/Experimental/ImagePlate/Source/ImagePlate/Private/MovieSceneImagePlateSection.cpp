// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneImagePlateSection.h"
#include "MovieScene.h"

#define LOCTEXT_NAMESPACE "MovieSceneImagePlateSection"

UMovieSceneImagePlateSection::UMovieSceneImagePlateSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	ThumbnailReferenceOffset = 0.f;
#endif

	EvalOptions.CompletionMode = EMovieSceneCompletionMode::RestoreState;
	bReuseExistingTexture = false;

	UMovieScene* Outer = GetTypedOuter<UMovieScene>();
	FFrameRate FrameRate = Outer ? Outer->GetTickResolution() : FFrameRate(24, 1);

	// media tracks have some preroll by default to precache frames
	SetPreRollFrames( (0.5 * FrameRate).RoundToFrame().Value );
}

#undef LOCTEXT_NAMESPACE