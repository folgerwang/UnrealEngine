// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ExtensionLibraries/MovieSceneTrackExtensions.h"
#include "MovieSceneTrack.h"

FText UMovieSceneTrackExtensions::GetDisplayName(UMovieSceneTrack* Track)
{
	return Track->GetDisplayName();
}

UMovieSceneSection* UMovieSceneTrackExtensions::AddSection(UMovieSceneTrack* Track)
{
	UMovieSceneSection* NewSection = Track->CreateNewSection();

	if (NewSection)
	{
		Track->AddSection(*NewSection);
	}

	return NewSection;
}

TArray<UMovieSceneSection*> UMovieSceneTrackExtensions::GetSections(UMovieSceneTrack* Track)
{
	return Track->GetAllSections();
}

void UMovieSceneTrackExtensions::RemoveSection(UMovieSceneTrack* Track, UMovieSceneSection* Section)
{
	if (Section)
	{
		Track->RemoveSection(*Section);
	}
}