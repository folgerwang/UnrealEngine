// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSection.h"
#include "Channels/MovieSceneObjectPathChannel.h"
#include "MovieScenePrimitiveMaterialSection.generated.h"

UCLASS(MinimalAPI)
class UMovieScenePrimitiveMaterialSection : public UMovieSceneSection
{
public:

	GENERATED_BODY()

	UMovieScenePrimitiveMaterialSection(const FObjectInitializer& ObjInit);

	UPROPERTY()
	FMovieSceneObjectPathChannel MaterialChannel;
};
