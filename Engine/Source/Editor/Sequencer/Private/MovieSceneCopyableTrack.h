// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneCopyableTrack.generated.h"

class UMovieSceneTrack;

UCLASS(Transient)
class UMovieSceneCopyableTrack : public UObject
{
public:
	GENERATED_BODY()

	UPROPERTY()
	UMovieSceneTrack* Track;

	UPROPERTY()
	bool bIsAMasterTrack;
};
