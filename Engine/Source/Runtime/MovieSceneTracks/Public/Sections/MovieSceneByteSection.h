// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneSection.h"
#include "Channels/MovieSceneByteChannel.h"
#include "MovieSceneByteSection.generated.h"

/**
 * A single byte section.
 */
UCLASS(MinimalAPI)
class UMovieSceneByteSection 
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:

	/** Ordered curve data */
	UPROPERTY()
	FMovieSceneByteChannel ByteCurve;
};
