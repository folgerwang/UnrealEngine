// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ExtensionLibraries/MovieSceneSubSectionExtensions.h"
#include "Sections/MovieSceneSubSection.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"

UMovieSceneSequence* UMovieSceneSubSectionExtensions::GetSequence(UMovieSceneSubSection* SubSection)
{
	return SubSection->GetSequence();
}
