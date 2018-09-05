// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneIntegerTrack.h"
#include "Sections/MovieSceneIntegerSection.h"
#include "Evaluation/MovieScenePropertyTemplates.h"

UMovieSceneIntegerTrack::UMovieSceneIntegerTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	SupportedBlendTypes = FMovieSceneBlendTypeField::All();
}


UMovieSceneSection* UMovieSceneIntegerTrack::CreateNewSection()
{
	return NewObject<UMovieSceneIntegerSection>(this, NAME_None, RF_Transactional);
}

FMovieSceneEvalTemplatePtr UMovieSceneIntegerTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneIntegerPropertySectionTemplate(*CastChecked<UMovieSceneIntegerSection>(&InSection), *this);
}
