// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/KeyHandle.h"
#include "Curves/RichCurve.h"
#include "MovieSceneSection.h"
#include "Layout/Margin.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "MovieSceneMarginSection.generated.h"

/**
 * A section in a Margin track
 */
UCLASS(MinimalAPI)
class UMovieSceneMarginSection 
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()
public:
	UMG_API const static FMovieSceneInterrogationKey GetMarginInterrogationKey();

public:

	/** Red curve data */
	UPROPERTY()
	FMovieSceneFloatChannel TopCurve;

	/** Green curve data */
	UPROPERTY()
	FMovieSceneFloatChannel LeftCurve;

	/** Blue curve data */
	UPROPERTY()
	FMovieSceneFloatChannel RightCurve;

	/** Alpha curve data */
	UPROPERTY()
	FMovieSceneFloatChannel BottomCurve;
};
