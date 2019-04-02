// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MovieScene/Parameters/MovieSceneNiagaraColorParameterTrack.h"
#include "MovieScene/Parameters/MovieSceneNiagaraColorParameterSectionTemplate.h"
#include "Sections/MovieSceneColorSection.h"
#include "Evaluation/MovieSceneEvalTemplate.h"

bool UMovieSceneNiagaraColorParameterTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneColorSection::StaticClass();
}

UMovieSceneSection* UMovieSceneNiagaraColorParameterTrack::CreateNewSection()
{
	return NewObject<UMovieSceneColorSection>(this, NAME_None, RF_Transactional);
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