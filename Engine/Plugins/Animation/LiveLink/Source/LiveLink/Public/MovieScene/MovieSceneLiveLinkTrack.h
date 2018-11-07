// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tracks/MovieScenePropertyTrack.h"
#include "MovieSceneLiveLinkTrack.generated.h"

/**
* A track for animating FMoveSceneLiveLinkTrack properties.
*/
UCLASS(MinimalAPI)
class UMovieSceneLiveLinkTrack : public UMovieScenePropertyTrack
{
	GENERATED_BODY()

public:

	UMovieSceneLiveLinkTrack(const FObjectInitializer& ObjectInitializer);

	// UMovieSceneTrack interface

	virtual UMovieSceneSection* CreateNewSection() override;
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;
};