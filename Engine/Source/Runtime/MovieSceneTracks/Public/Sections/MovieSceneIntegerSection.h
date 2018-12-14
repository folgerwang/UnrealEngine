// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/KeyHandle.h"
#include "Curves/IntegralCurve.h"
#include "MovieSceneSection.h"
#include "Channels/MovieSceneIntegerChannel.h"
#include "MovieSceneIntegerSection.generated.h"


/**
 * A single integer section.
 */
UCLASS(MinimalAPI)
class UMovieSceneIntegerSection 
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * Access this section's underlying raw data
	 */
	const FMovieSceneIntegerChannel& GetChannel() const { return IntegerCurve; }

private:

	/** Ordered curve data */
	UPROPERTY()
	FMovieSceneIntegerChannel IntegerCurve;
};
