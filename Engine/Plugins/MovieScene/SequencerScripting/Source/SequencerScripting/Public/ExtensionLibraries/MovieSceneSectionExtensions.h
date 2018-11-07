// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/SubclassOf.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SequencerScriptingRange.h"
#include "MovieSceneScriptingChannel.h"
#include "MovieSceneSectionExtensions.generated.h"

class UMovieSceneSection;

/**
 * Function library containing methods that should be hoisted onto UMovieSceneSections for scripting
 */
UCLASS()
class UMovieSceneSectionExtensions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
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

	/**
	* Find all channels that belong to the specified UMovieSceneSection. Some sections have many channels (such as
	* Transforms containing 9 float channels to represent Translation/Rotation/Scale), and a section may have mixed
	* channel types.
	*
	* @param Section       The section to use.
	* @return An array containing any key channels that match the type specified
	*/
	UFUNCTION(BlueprintCallable, Category = Section, meta=(ScriptMethod))
	static TArray<UMovieSceneScriptingChannel*> GetChannels(UMovieSceneSection* Section);

	/**
	* Find all channels that belong to the specified UMovieSceneSection that match the specific type. This will filter out any children who do not inherit
	* from the specified type for you.
	*
	* @param Section        The section to use.
	* @param ChannelType	The class type to look for.
	* @return An array containing any key channels that match the type specified
	*/
	UFUNCTION(BlueprintCallable, Category = Section, meta = (ScriptMethod))
	static TArray<UMovieSceneScriptingChannel*> FindChannelsByType(UMovieSceneSection* Section, TSubclassOf<UMovieSceneScriptingChannel> ChannelType);

};