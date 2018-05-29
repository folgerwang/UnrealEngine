// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MovieScene/MovieSceneNiagaraTrack.h"

void UMovieSceneNiagaraTrack::AddSection(UMovieSceneSection& Section)
{
	Sections.Add(&Section);
}

const TArray<UMovieSceneSection*>& UMovieSceneNiagaraTrack::GetAllSections() const
{
	return Sections;
}

bool UMovieSceneNiagaraTrack::HasSection(const UMovieSceneSection& Section) const
{
	return Sections.Contains(&Section);
}

bool UMovieSceneNiagaraTrack::IsEmpty() const
{
	return Sections.Num() != 0;
}

void UMovieSceneNiagaraTrack::RemoveAllAnimationData()
{
	Sections.Empty();
}

void UMovieSceneNiagaraTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
}
