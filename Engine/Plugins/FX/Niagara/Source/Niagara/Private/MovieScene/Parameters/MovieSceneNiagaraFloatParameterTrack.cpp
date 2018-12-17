// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MovieScene/Parameters/MovieSceneNiagaraFloatParameterTrack.h"
#include "MovieScene/Parameters/MovieSceneNiagaraFloatParameterSectionTemplate.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Templates/Casts.h"

bool UMovieSceneNiagaraFloatParameterTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneFloatSection::StaticClass();
}

UMovieSceneSection* UMovieSceneNiagaraFloatParameterTrack::CreateNewSection()
{
	return NewObject<UMovieSceneFloatSection>(this, NAME_None, RF_Transactional);
}

FMovieSceneEvalTemplatePtr UMovieSceneNiagaraFloatParameterTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	const UMovieSceneFloatSection* FloatSection = Cast<UMovieSceneFloatSection>(&InSection);
	if (FloatSection != nullptr)
	{
		return FMovieSceneNiagaraFloatParameterSectionTemplate(GetParameter(), FloatSection->GetChannel());
	}
	return FMovieSceneEvalTemplatePtr();
}