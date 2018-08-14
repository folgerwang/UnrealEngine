// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/SubclassOf.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MovieSceneSubSectionExtensions.generated.h"

class UMovieSceneSubSection;
class UMovieSceneSequence;

/**
 * Function library containing methods that should be hoisted onto UMovieSceneSections for scripting
 */
UCLASS()
class UMovieSceneSubSectionExtensions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/**
	 * Get sequence of this section
	 */
	UFUNCTION(BlueprintCallable, Category=Section, meta=(ScriptMethod))
	static UMovieSceneSequence* GetSequence(UMovieSceneSubSection* SubSection);
};
