// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Animation/MovieSceneMarginTrack.h"
#include "Animation/MovieSceneMarginSection.h"
#include "Animation/MovieSceneMarginTemplate.h"


UMovieSceneMarginTrack::UMovieSceneMarginTrack(const FObjectInitializer& Init)
	: Super(Init)
{
	EvalOptions.bEvaluateNearestSection_DEPRECATED = EvalOptions.bCanEvaluateNearestSection = true;
	SupportedBlendTypes = FMovieSceneBlendTypeField::All();
}

bool UMovieSceneMarginTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneMarginSection::StaticClass();
}

UMovieSceneSection* UMovieSceneMarginTrack::CreateNewSection()
{
	return NewObject<UMovieSceneMarginSection>(this, NAME_None, RF_Transactional);
}


FMovieSceneEvalTemplatePtr UMovieSceneMarginTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneMarginSectionTemplate(*CastChecked<UMovieSceneMarginSection>(&InSection), *this);
}
