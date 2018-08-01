// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MovieScene/Parameters/MovieSceneNiagaraBoolParameterTrack.h"
#include "MovieScene/Parameters/MovieSceneNiagaraBoolParameterSectionTemplate.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Templates/Casts.h"

UMovieSceneSection* UMovieSceneNiagaraBoolParameterTrack::CreateNewSection()
{
	return NewObject<UMovieSceneBoolSection>(this);
}

FMovieSceneEvalTemplatePtr UMovieSceneNiagaraBoolParameterTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	const UMovieSceneBoolSection* BoolSection = Cast<UMovieSceneBoolSection>(&InSection);
	if (BoolSection != nullptr)
	{
		return FMovieSceneNiagaraBoolParameterSectionTemplate(GetParameter(), BoolSection->GetChannel());
	}
	return FMovieSceneEvalTemplatePtr();
}