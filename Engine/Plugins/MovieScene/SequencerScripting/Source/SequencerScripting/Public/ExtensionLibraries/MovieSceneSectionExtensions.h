// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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

	UE_DEPRECATED(4.22, "Please use GetStartFrame and GetEndFrame instead.")
	static FSequencerScriptingRange GetRange(UMovieSceneSection* Section);

	/**
	 * Get start frame
	 *
	 * @param Section        The section within which to get the start frame
	 * @return Start frame of this section
	 */
	UFUNCTION(BlueprintCallable, Category = "Section", meta = (ScriptMethod))
	static int32 GetStartFrame(UMovieSceneSection* Section);

	/**
	 * Get start time in seconds
	 *
	 * @param Section        The section within which to get the start time
	 * @return Start time of this section
	 */
	UFUNCTION(BlueprintCallable, Category = "Section", meta = (ScriptMethod))
	static float GetStartFrameSeconds(UMovieSceneSection* Section);

	/**
	 * Get end frame
	 *
	 * @param Section        The section within which to get the end frame
	 * @return End frame of this section
	 */
	UFUNCTION(BlueprintCallable, Category = "Section", meta = (ScriptMethod))
	static int32 GetEndFrame(UMovieSceneSection* Section);

	/**
	 * Get end time in seconds
	 *
	 * @param Section        The section within which to get the end time
	 * @return End time of this section
	 */
	UFUNCTION(BlueprintCallable, Category = "Section", meta = (ScriptMethod))
	static float GetEndFrameSeconds(UMovieSceneSection* Section);

	UE_DEPRECATED(4.22, "Please use SetStartFrame and SetEndFrame instead.")
	static void SetRange(UMovieSceneSection* Section, const FSequencerScriptingRange& Range);

	/**
	 * Set start frame
	 *
	 * @param Section        The section within which to set the start frame
	 * @param StartFrame The desired start frame for this section
	 */
	UFUNCTION(BlueprintCallable, Category = "Section", meta = (ScriptMethod))
	static void SetStartFrame(UMovieSceneSection* Section, int32 StartFrame);

	/**
	 * Set start time in seconds
	 *
	 * @param Section        The section within which to set the start time
	 * @param StartTime The desired start time for this section
	 */
	UFUNCTION(BlueprintCallable, Category = "Section", meta = (ScriptMethod))
	static void SetStartFrameSeconds(UMovieSceneSection* Section, float StartTime);

	/**
	 * Set end frame
	 *
	 * @param Section        The section within which to set the end frame
	 * @param EndFrame The desired start frame for this section
	 */
	UFUNCTION(BlueprintCallable, Category = "Section", meta = (ScriptMethod))
	static void SetEndFrame(UMovieSceneSection* Section, int32 EndFrame);

	/**
	 * Set end time in seconds
	 *
	 * @param Section        The section within which to set the end time
	 * @param EndTime The desired end time for this section
	 */
	UFUNCTION(BlueprintCallable, Category = "Section", meta = (ScriptMethod))
	static void SetEndFrameSeconds(UMovieSceneSection* Section, float EndTime);

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