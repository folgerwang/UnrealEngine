// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "MovieScenePropertyTrackExtensions.generated.h"


class UMovieScenePropertyTrack;

/**
 * Function library containing methods that should be hoisted onto UMovieScenePropertyTrack for scripting
 */
UCLASS()
class UMovieScenePropertyTrackExtensions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Set this track's property name and path
	 *
	 * @param Track        The track to use
	 * @param InPropertyName The property name
	 * @param InPropertyPath The property path
	 */
	UFUNCTION(BlueprintCallable, Category = "Track", meta = (ScriptMethod))
	static void SetPropertyNameAndPath(UMovieScenePropertyTrack* Track, const FName& InPropertyName, const FString& InPropertyPath);

	/**
	 * Get this track's property name
	 *
	 * @param Track        The track to use
	 * @return This track's property name
	 */
	UFUNCTION(BlueprintCallable, Category="Track", meta=(ScriptMethod))
	static FName GetPropertyName(UMovieScenePropertyTrack* Track);

	/**
	 * Get this track's property path
	 *
	 * @param Track        The track to use
	 * @return This track's property path
	 */
	UFUNCTION(BlueprintCallable, Category="Track", meta=(ScriptMethod))
	static FString GetPropertyPath(UMovieScenePropertyTrack* Track);

	/**
	 * Get this track's unique name
	 *
	 * @param Track        The track to use
	 * @return This track's unique name
	 */
	UFUNCTION(BlueprintCallable, Category="Track", meta=(ScriptMethod))
	static FName GetUniqueTrackName(UMovieScenePropertyTrack* Track);
};
