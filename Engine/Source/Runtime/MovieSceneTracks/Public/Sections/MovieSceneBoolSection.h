// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneSection.h"
#include "Channels/MovieSceneBoolChannel.h"
#include "MovieSceneBoolSection.generated.h"

/**
 * A single bool section.
 */
UCLASS(MinimalAPI)
class UMovieSceneBoolSection 
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:

	/** The default value to use when no keys are present - use GetCurve().SetDefaultValue() */
	UPROPERTY()
	bool DefaultValue_DEPRECATED;

public:

	FMovieSceneBoolChannel& GetChannel() { return BoolCurve; }
	const FMovieSceneBoolChannel& GetChannel() const { return BoolCurve; }

public:

	//~ UObject interface

	virtual void PostLoad() override;

protected:

	/** Ordered curve data */
	UPROPERTY()
	FMovieSceneBoolChannel BoolCurve;
};
