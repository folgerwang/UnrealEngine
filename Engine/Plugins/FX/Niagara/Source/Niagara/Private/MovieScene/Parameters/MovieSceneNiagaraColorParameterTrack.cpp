// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieScene/Parameters/MovieSceneNiagaraColorParameterTrack.h"
#include "MovieScene/Parameters/MovieSceneNiagaraColorParameterSectionTemplate.h"
#include "Sections/MovieSceneColorSection.h"
#include "Evaluation/MovieSceneEvalTemplate.h"

UMovieSceneSection* UMovieSceneNiagaraColorParameterTrack::CreateNewSection()
{
	return NewObject<UMovieSceneColorSection>(this);
}

FMovieSceneEvalTemplatePtr UMovieSceneNiagaraColorParameterTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	const UMovieSceneColorSection* ColorSection = Cast<UMovieSceneColorSection>(&InSection);
	if (ColorSection != nullptr)
	{
		return FMovieSceneNiagaraColorParameterSectionTemplate(GetParameter(), ColorSection->GetRedChannel(), ColorSection->GetGreenChannel(), ColorSection->GetBlueChannel(), ColorSection->GetAlphaChannel());
	}
	return FMovieSceneEvalTemplatePtr();
}