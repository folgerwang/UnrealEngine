// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MovieScene/MovieSceneLiveLinkTrack.h"
#include "MovieScene/MovieSceneLiveLinkSection.h"
#include "MovieSceneLiveLinkSectionTemplate.h"
#include "Evaluation/MovieSceneEvalTemplate.h"

UMovieSceneLiveLinkTrack::UMovieSceneLiveLinkTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(48, 227, 255, 65);
#endif
	SupportedBlendTypes = FMovieSceneBlendTypeField::All();
}

UMovieSceneSection* UMovieSceneLiveLinkTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneLiveLinkSection::StaticClass(), NAME_None, RF_Transactional);
}

FMovieSceneEvalTemplatePtr UMovieSceneLiveLinkTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneLiveLinkSectionTemplate(*CastChecked<const UMovieSceneLiveLinkSection>(&InSection), *this);
}
