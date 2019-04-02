// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MovieSceneTrack.h"
#include "Templates/SubclassOf.h"
#include "SequencerBindingProxy.h"

#include "MovieSceneBindingExtensions.generated.h"


/**
 * Function library containing methods that should be hoisted onto FMovieSceneBindingProxies for scripting
 */
UCLASS()
class UMovieSceneBindingExtensions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/**
	 * Check whether the specified binding is valid
	 */
	UFUNCTION(BlueprintPure, Category=Sequence, meta=(ScriptMethod))
	static bool IsValid(const FSequencerBindingProxy& InBinding);

	/**
	 * Get this binding's ID
	 *
	 * @param InBinding     The binding to get the ID of
	 * @return The guid that uniquely represents this binding
	 */
	UFUNCTION(BlueprintCallable, Category=Sequence, meta=(ScriptMethod))
	static FGuid GetId(const FSequencerBindingProxy& InBinding);

	/**
	 * Get this binding's name
	 *
	 * @param InBinding     The binding to get the name of
	 * @return The display name of the binding
	 */
	UFUNCTION(BlueprintCallable, Category=Sequence, meta=(ScriptMethod))
	static FText GetDisplayName(const FSequencerBindingProxy& InBinding);

	/**
	 * Get this binding's object non-display name
	 *
	 * @param InBinding     The binding to get the name of
	 * @return The name of the binding
	 */
	UFUNCTION(BlueprintCallable, Category=Sequence, meta=(ScriptMethod))
	static FString GetName(const FSequencerBindingProxy& InBinding);

	/**
	 * Get all the tracks stored within this binding
	 *
	 * @param InBinding     The binding to find tracks in
	 * @return An array containing all the binding's tracks
	 */
	UFUNCTION(BlueprintCallable, Category=Sequence, meta=(ScriptMethod))
	static TArray<UMovieSceneTrack*> GetTracks(const FSequencerBindingProxy& InBinding);

	/**
	 * Find all tracks within a given binding of the specified type
	 *
	 * @param InBinding     The binding to find tracks in
	 * @param TrackType     A UMovieSceneTrack class type specifying which types of track to return
	 * @return An array containing any tracks that match the type specified
	 */
	UFUNCTION(BlueprintCallable, Category=Sequence, meta=(ScriptMethod))
	static TArray<UMovieSceneTrack*> FindTracksByType(const FSequencerBindingProxy& InBinding, TSubclassOf<UMovieSceneTrack> TrackType);

	/**
	 * Find all tracks within a given binding of the specified type, not allowing sub-classed types
	 *
	 * @param InBinding     The binding to find tracks in
	 * @param TrackType     A UMovieSceneTrack class type specifying the exact types of track to return
	 * @return An array containing any tracks that are exactly the same as the type specified
	 */
	UFUNCTION(BlueprintCallable, Category=Sequence, meta=(ScriptMethod))
	static TArray<UMovieSceneTrack*> FindTracksByExactType(const FSequencerBindingProxy& InBinding, TSubclassOf<UMovieSceneTrack> TrackType);

	/**
	 * Remove the specified track from this binding
	 *
	 * @param InBinding     The binding to remove the track from
	 * @param TrackToRemove The track to remove
	 */
	UFUNCTION(BlueprintCallable, Category=Sequence, meta=(ScriptMethod))
	static void RemoveTrack(const FSequencerBindingProxy& InBinding, UMovieSceneTrack* TrackToRemove);

	/**
	 * Remove the specified binding
	 *
	 * @param InBinding     The binding to remove the track from
	 */
	UFUNCTION(BlueprintCallable, Category=Sequence, meta=(ScriptMethod))
	static void Remove(const FSequencerBindingProxy& InBinding);

	/**
	 * Add a new track to the specified binding
	 *
	 * @param InBinding     The binding to add tracks to
	 * @param TrackType     A UMovieSceneTrack class type specifying the type of track to create
	 * @return The newly created track, if successful
	 */
	UFUNCTION(BlueprintCallable, Category=Sequence, meta=(ScriptMethod))
	static UMovieSceneTrack* AddTrack(const FSequencerBindingProxy& InBinding, TSubclassOf<UMovieSceneTrack> TrackType);

	/**
	* Get all the children of this binding
	*
	* @param InBinding     The binding to to get children of
	* @return An array containing all the binding's children
	*/
	UFUNCTION(BlueprintCallable, Category=Sequence, meta=(ScriptMethod))
	static TArray<FSequencerBindingProxy> GetChildPossessables(const FSequencerBindingProxy& InBinding);

	/**
	* Get this binding's object template
	*
	* @param InBinding     The binding to get the object template of
	* @return The object template of the binding
	*/
	UFUNCTION(BlueprintCallable, Category=Sequence, meta=(ScriptMethod))
	static UObject* GetObjectTemplate(const FSequencerBindingProxy& InBinding);

	/**
	* Get this binding's possessed object class
	*
	* @param InBinding     The binding to get the possessed object class of
	* @return The possessed object class of the binding
	*/
	UFUNCTION(BlueprintCallable, Category=Sequence, meta=(ScriptMethod))
	static UClass* GetPossessedObjectClass(const FSequencerBindingProxy& InBinding);

	/**
	* Get the parent of this binding
	*
	* @param InBinding     The binding to get the parent of
	* @return The binding's parent
	*/
	UFUNCTION(BlueprintCallable, Category=Sequence, meta=(ScriptMethod))
	static FSequencerBindingProxy GetParent(const FSequencerBindingProxy& InBinding);

	/**
	 * Set the parent to this binding
	 *
	 * @param InBinding     The binding to set 
	 * @param InParentBinding     The parent to set the InBinding to
	 */
	UFUNCTION(BlueprintCallable, Category = Sequence, meta = (ScriptMethod))
	static void SetParent(const FSequencerBindingProxy& InBinding, const FSequencerBindingProxy& InParentBinding);
};