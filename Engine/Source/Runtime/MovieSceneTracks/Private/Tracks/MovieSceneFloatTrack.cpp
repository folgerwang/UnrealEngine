// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneFloatTrack.h"
#include "MovieSceneCommonHelpers.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Evaluation/MovieScenePropertyTemplates.h"


UMovieSceneFloatTrack::UMovieSceneFloatTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	SupportedBlendTypes = FMovieSceneBlendTypeField::All();
}


UMovieSceneSection* UMovieSceneFloatTrack::CreateNewSection()
{
	return NewObject<UMovieSceneFloatSection>(this, NAME_None, RF_Transactional);
}

FMovieSceneEvalTemplatePtr UMovieSceneFloatTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneFloatPropertySectionTemplate(*CastChecked<const UMovieSceneFloatSection>(&InSection), *this);
}