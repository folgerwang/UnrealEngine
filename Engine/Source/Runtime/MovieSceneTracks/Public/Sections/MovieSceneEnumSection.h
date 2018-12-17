// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Curves/IntegralCurve.h"
#include "MovieSceneSection.h"
#include "Channels/MovieSceneByteChannel.h"
#include "MovieSceneEnumSection.generated.h"


/**
 * A single enum section.
 */
UCLASS(MinimalAPI)
class UMovieSceneEnumSection
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:

	/** Ordered curve data */
	UPROPERTY()
	FMovieSceneByteChannel EnumCurve;
};
