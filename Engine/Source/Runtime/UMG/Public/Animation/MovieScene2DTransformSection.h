// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "MovieSceneSection.h"
#include "MovieScene2DTransformSection.generated.h"


/**
 * A transform section
 */
UCLASS(MinimalAPI)
class UMovieScene2DTransformSection
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:

	/** Translation curves*/
	UPROPERTY()
	FMovieSceneFloatChannel Translation[2];
	
	/** Rotation curve */
	UPROPERTY()
	FMovieSceneFloatChannel Rotation;

	/** Scale curves */
	UPROPERTY()
	FMovieSceneFloatChannel Scale[2];

	/** Shear curve */
	UPROPERTY()
	FMovieSceneFloatChannel Shear[2];
};
