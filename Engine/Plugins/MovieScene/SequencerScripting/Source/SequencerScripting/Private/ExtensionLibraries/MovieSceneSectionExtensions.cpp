// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ExtensionLibraries/MovieSceneSectionExtensions.h"

#include "SequencerScriptingRange.h"
#include "MovieSceneSection.h"
#include "MovieScene.h"


FSequencerScriptingRange UMovieSceneSectionExtensions::GetRange(UMovieSceneSection* Section)
{
	UMovieScene* MovieScene = Section->GetTypedOuter<UMovieScene>();
	return FSequencerScriptingRange::FromNative(Section->GetRange(), MovieScene->GetTickResolution());
}

void UMovieSceneSectionExtensions::SetRange(UMovieSceneSection* Section, const FSequencerScriptingRange& InRange)
{
	UMovieScene* MovieScene = Section->GetTypedOuter<UMovieScene>();
	TRange<FFrameNumber> NewRange = InRange.ToNative(MovieScene->GetTickResolution());

	if (NewRange.GetLowerBound().IsOpen() || NewRange.GetUpperBound().IsOpen() || NewRange.GetLowerBoundValue() <= NewRange.GetUpperBoundValue())
	{
		Section->SetRange(NewRange);
	}
	else
	{
		UE_LOG(LogMovieScene, Error, TEXT("Invalid range specified"));
	}
}
