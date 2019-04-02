// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSection.h"
#include "Channels/MovieSceneObjectPathChannel.h"
#include "MovieSceneObjectPropertySection.generated.h"

UCLASS(MinimalAPI)
class UMovieSceneObjectPropertySection : public UMovieSceneSection
{
public:

	GENERATED_BODY()

	UMovieSceneObjectPropertySection(const FObjectInitializer& ObjInit);

	UPROPERTY()
	FMovieSceneObjectPathChannel ObjectChannel;
};
