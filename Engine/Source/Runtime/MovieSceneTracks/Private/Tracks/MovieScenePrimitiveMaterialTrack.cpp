// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieScenePrimitiveMaterialTrack.h"
#include "Sections/MovieScenePrimitiveMaterialSection.h"
#include "Evaluation/MovieScenePrimitiveMaterialTemplate.h"


UMovieScenePrimitiveMaterialTrack::UMovieScenePrimitiveMaterialTrack(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	MaterialIndex = 0;
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(64,192,64,75);
#endif
}

UMovieSceneSection* UMovieScenePrimitiveMaterialTrack::CreateNewSection()
{
	return NewObject<UMovieScenePrimitiveMaterialSection>(this);
}

FMovieSceneEvalTemplatePtr UMovieScenePrimitiveMaterialTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieScenePrimitiveMaterialTemplate(*CastChecked<UMovieScenePrimitiveMaterialSection>(&InSection), *this);
}
