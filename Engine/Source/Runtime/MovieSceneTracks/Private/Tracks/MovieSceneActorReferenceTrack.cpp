// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneActorReferenceTrack.h"
#include "Sections/MovieSceneActorReferenceSection.h"
#include "Evaluation/MovieSceneActorReferenceTemplate.h"


UMovieSceneActorReferenceTrack::UMovieSceneActorReferenceTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ }

bool UMovieSceneActorReferenceTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneActorReferenceSection::StaticClass();
}

UMovieSceneSection* UMovieSceneActorReferenceTrack::CreateNewSection()
{
	return NewObject<UMovieSceneActorReferenceSection>(this, NAME_None, RF_Transactional);
}

FMovieSceneEvalTemplatePtr UMovieSceneActorReferenceTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneActorReferenceSectionTemplate(*CastChecked<UMovieSceneActorReferenceSection>(&InSection), *this);
}

