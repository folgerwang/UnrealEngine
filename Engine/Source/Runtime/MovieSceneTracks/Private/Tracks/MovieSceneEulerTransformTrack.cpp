// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneEulerTransformTrack.h"
#include "MovieSceneCommonHelpers.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Compilation/MovieSceneSegmentCompiler.h"
#include "Compilation/MovieSceneTemplateInterrogation.h"
#include "Evaluation/MovieScenePropertyTemplates.h"


UMovieSceneEulerTransformTrack::UMovieSceneEulerTransformTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(65, 173, 164, 65);
#endif

	SupportedBlendTypes = FMovieSceneBlendTypeField::All();

	EvalOptions.bEvaluateNearestSection_DEPRECATED = EvalOptions.bCanEvaluateNearestSection = true;
}

UMovieSceneSection* UMovieSceneEulerTransformTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieScene3DTransformSection::StaticClass(), NAME_None, RF_Transactional);
}

FMovieSceneEvalTemplatePtr UMovieSceneEulerTransformTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneEulerTransformPropertySectionTemplate(*CastChecked<UMovieScene3DTransformSection>(&InSection), *this);
}

FMovieSceneInterrogationKey UMovieSceneEulerTransformTrack::GetInterrogationKey()
{
	static FMovieSceneAnimTypeID TypeID = FMovieSceneAnimTypeID::Unique();
	return TypeID;
}
