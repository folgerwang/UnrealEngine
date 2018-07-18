// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "MovieSceneEulerTransformTrack.generated.h"

struct FMovieSceneInterrogationKey;

/**
 * Handles manipulation of 3D euler transform properties in a movie scene
 */
UCLASS(MinimalAPI)
class UMovieSceneEulerTransformTrack
	: public UMovieScenePropertyTrack
{
	GENERATED_UCLASS_BODY()

public:

	// UMovieSceneTrack interface

	virtual UMovieSceneSection* CreateNewSection() override;
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;

	/**
	 * Access the interrogation key for transform data - any interrogation data stored with this key is guaranteed to be of type 'FEulerTransform'
	 */
	MOVIESCENETRACKS_API static FMovieSceneInterrogationKey GetInterrogationKey();
};

