// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SequencerScriptingRange.h"

#include "MovieSceneSectionExtensions.generated.h"

class UMovieSceneSection;

/**
 * Function library containing methods that should be hoisted onto UMovieSceneSections for scripting
 */
UCLASS()
class UMovieSceneSectionExtensions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	/**
	 * Get the range of this section
	 */
	UFUNCTION(BlueprintCallable, Category=Section, meta=(ScriptMethod))
	static FSequencerScriptingRange GetRange(UMovieSceneSection* Section);

	/**
	 * Set the range of this section
	 *
	 * @param Range         The desired range for this section
	 */
	UFUNCTION(BlueprintCallable, Category=Section, meta=(ScriptMethod))
	static void SetRange(UMovieSceneSection* Section, const FSequencerScriptingRange& Range);
};