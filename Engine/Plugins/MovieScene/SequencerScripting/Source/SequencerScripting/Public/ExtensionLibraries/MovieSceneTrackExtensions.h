// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "MovieSceneTrackExtensions.generated.h"

class FText;

class UMovieSceneTrack;
class UMovieSceneSection;

/**
 * Function library containing methods that should be hoisted onto UMovieSceneTracks for scripting
 */
UCLASS()
class UMovieSceneTrackExtensions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Get this track's display name
	 *
	 * @param Track        The track to use
	 * @return This track's display name
	 */
	UFUNCTION(BlueprintCallable, Category="Track", meta=(ScriptMethod))
	static FText GetDisplayName(UMovieSceneTrack* Track);

	/**
	 * Add a new section to this track
	 *
	 * @param Track        The track to use
	 * @return The newly create section if successful
	 */
	UFUNCTION(BlueprintCallable, Category="Track", meta=(ScriptMethod))
	static UMovieSceneSection* AddSection(UMovieSceneTrack* Track);

	/**
	 * Access all this track's sections
	 *
	 * @param Track        The track to use
	 * @return An array of this track's sections
	 */
	UFUNCTION(BlueprintCallable, Category="Track", meta=(ScriptMethod))
	static TArray<UMovieSceneSection*> GetSections(UMovieSceneTrack* Track);

	/**
	 * Remove the specified section
	 *
	 * @param Track        The track to remove the section from, if present
	 * @param Section      The section to remove
	 */
	UFUNCTION(BlueprintCallable, Category="Track", meta=(ScriptMethod))
	static void RemoveSection(UMovieSceneTrack* Track, UMovieSceneSection* Section);
};