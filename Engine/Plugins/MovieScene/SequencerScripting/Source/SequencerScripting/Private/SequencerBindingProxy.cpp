// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SequencerBindingProxy.h"
#include "MovieSceneSequence.h"

UMovieScene* FSequencerBindingProxy::GetMovieScene() const
{
	return Sequence ? Sequence->GetMovieScene() : nullptr;
}