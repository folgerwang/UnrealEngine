// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MovieScene/Parameters/MovieSceneNiagaraIntegerParameterTrack.h"
#include "MovieScene/Parameters/MovieSceneNiagaraIntegerParameterSectionTemplate.h"
#include "Sections/MovieSceneIntegerSection.h"
#include "Evaluation/MovieSceneEvalTemplate.h"

bool UMovieSceneNiagaraIntegerParameterTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneIntegerSection::StaticClass();
}

UMovieSceneSection* UMovieSceneNiagaraIntegerParameterTrack::CreateNewSection()
{
	return NewObject<UMovieSceneIntegerSection>(this, NAME_None, RF_Transactional);
}

FMovieSceneEvalTemplatePtr UMovieSceneNiagaraIntegerParameterTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	const UMovieSceneIntegerSection* IntegerSection = Cast<UMovieSceneIntegerSection>(&InSection);
	if (IntegerSection != nullptr)
	{
		return FMovieSceneNiagaraIntegerParameterSectionTemplate(GetParameter(), IntegerSection->GetChannel());
	}
	return FMovieSceneEvalTemplatePtr();
}