// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneSection.h"
#include "MovieSceneStringChannel.h"
#include "MovieSceneStringSection.generated.h"

/**
 * A single string section
 */
UCLASS(MinimalAPI)
class UMovieSceneStringSection
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * Public access to this section's internal data function
	 */
	const FMovieSceneStringChannel& GetChannel() const { return StringCurve; }

private:

	/** Curve data */
	UPROPERTY()
	FMovieSceneStringChannel StringCurve;
};
