// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "MovieSceneVectorTrackExtensions.generated.h"


class UMovieSceneVectorTrack;

/**
 * Function library containing methods that should be hoisted onto UMovieSceneVectorTrack for scripting
 */
UCLASS()
class UMovieSceneVectorTrackExtensions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Set the number of channels used for this track
	 *
	 * @param Track        The track to set
	 * @param InNumChannelsUsed The number of channels to use for this track
	 */
	UFUNCTION(BlueprintCallable, Category = "Track", meta = (ScriptMethod))
	static void SetNumChannelsUsed(UMovieSceneVectorTrack* Track, int32 InNumChannelsUsed);

	/**
	 * Get the number of channels used for this track
	 *
	 * @param Track        The track to query for the number of channels used
	 * @return The number of channels used for this track
	 */
	UFUNCTION(BlueprintCallable, Category = "Track", meta = (ScriptMethod))
	static int32 GetNumChannelsUsed(UMovieSceneVectorTrack* Track);


};
