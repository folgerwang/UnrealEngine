// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ISequencer.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"

FFrameRate ISequencer::GetRootFrameResolution() const
{
	UMovieSceneSequence* RootSequence = GetRootMovieSceneSequence();
	if (RootSequence)
	{
		return RootSequence->GetMovieScene()->GetFrameResolution();
	}

	ensureMsgf(false, TEXT("No valid sequence found."));
	return FFrameRate();
}

FFrameRate ISequencer::GetRootPlayRate() const
{
	UMovieSceneSequence* RootSequence = GetRootMovieSceneSequence();
	if (RootSequence)
	{
		return RootSequence->GetMovieScene()->GetPlaybackFrameRate();
	}

	ensureMsgf(false, TEXT("No valid sequence found."));
	return FFrameRate();
}

FFrameRate ISequencer::GetFocusedFrameResolution() const
{
	UMovieSceneSequence* FocusedSequence = GetFocusedMovieSceneSequence();
	if (FocusedSequence)
	{
		return FocusedSequence->GetMovieScene()->GetFrameResolution();
	}

	ensureMsgf(false, TEXT("No valid sequence found."));
	return FFrameRate();
}

FFrameRate ISequencer::GetFocusedPlayRate() const
{
	UMovieSceneSequence* FocusedSequence = GetFocusedMovieSceneSequence();
	if (FocusedSequence)
	{
		return FocusedSequence->GetMovieScene()->GetPlaybackFrameRate();
	}

	ensureMsgf(false, TEXT("No valid sequence found."));
	return FFrameRate();
}