// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieScenePrimitiveMaterialTrack.h"
#include "Tracks/MovieSceneMaterialTrack.h"
#include "Sections/MovieScenePrimitiveMaterialSection.h"
#include "Evaluation/MovieScenePrimitiveMaterialTemplate.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"


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
	return NewObject<UMovieScenePrimitiveMaterialSection>(this, NAME_None, RF_Transactional);
}

FMovieSceneEvalTemplatePtr UMovieScenePrimitiveMaterialTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieScenePrimitiveMaterialTemplate(*CastChecked<UMovieScenePrimitiveMaterialSection>(&InSection), *this);
}

void UMovieScenePrimitiveMaterialTrack::PostCompile(FMovieSceneEvaluationTrack& OutTrack, const FMovieSceneTrackCompilerArgs& Args) const
{
	// Must evaluate before material parameter tracks
	OutTrack.SetEvaluationPriority(UMovieSceneComponentMaterialTrack::EvaluationPriority + 1);
}