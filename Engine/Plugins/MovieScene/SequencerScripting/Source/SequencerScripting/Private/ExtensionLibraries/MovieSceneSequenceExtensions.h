// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"
#include "Containers/ArrayView.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SequencerBindingProxy.h"
#include "SequencerScriptingRange.h"
#include "Templates/SubclassOf.h"
#include "MovieSceneTrack.h"

#include "MovieSceneSequenceExtensions.generated.h"

/**
 * Function library containing methods that should be hoisted onto UMovieSceneSequences for scripting purposes
 */
UCLASS()
class UMovieSceneSequenceExtensions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	/**
	 * Get this sequence's movie scene data
	 *
	 * @param Sequence        The sequence to use
	 * @return This sequence's movie scene data object
	 */
	UFUNCTION(BlueprintCallable, Category="Sequence", meta=(ScriptMethod))
	static UMovieScene* GetMovieScene(UMovieSceneSequence* Sequence);

	/**
	 * Get all master tracks
	 *
	 * @param Sequence        The sequence to use
	 * @return An array containing all master tracks in this sequence
	 */
	UFUNCTION(BlueprintCallable, Category="Sequence", meta=(ScriptMethod))
	static TArray<UMovieSceneTrack*> GetMasterTracks(UMovieSceneSequence* Sequence);

	/**
	 * Find all master tracks of the specified type
	 *
	 * @param Sequence        The sequence to use
	 * @param TrackType     A UMovieSceneTrack class type specifying which types of track to return
	 * @return An array containing any tracks that match the type specified
	 */
	UFUNCTION(BlueprintCallable, Category="Sequence", meta=(ScriptMethod))
	static TArray<UMovieSceneTrack*> FindMasterTracksByType(UMovieSceneSequence* Sequence, TSubclassOf<UMovieSceneTrack> TrackType);

	/**
	 * Find all master tracks of the specified type, not allowing sub-classed types
	 *
	 * @param Sequence        The sequence to use
	 * @param TrackType     A UMovieSceneTrack class type specifying the exact types of track to return
	 * @return An array containing any tracks that are exactly the same as the type specified
	 */
	UFUNCTION(BlueprintCallable, Category="Sequence", meta=(ScriptMethod))
	static TArray<UMovieSceneTrack*> FindMasterTracksByExactType(UMovieSceneSequence* Sequence, TSubclassOf<UMovieSceneTrack> TrackType);

	/**
	 * Add a new master track of the specified type
	 *
	 * @param Sequence        The sequence to use
	 * @param TrackType     A UMovieSceneTrack class type to create
	 * @return The newly created track, if successful
	 */
	UFUNCTION(BlueprintCallable, Category="Sequence", meta=(ScriptMethod))
	static UMovieSceneTrack* AddMasterTrack(UMovieSceneSequence* Sequence, TSubclassOf<UMovieSceneTrack> TrackType);

	/**
	 * Get's this sequence's display rate
	 *
	 * @param Sequence        The sequence to use
	 * @return The display rate that this sequence is displayed as
	 */
	UFUNCTION(BlueprintCallable, Category="Sequence", meta=(ScriptMethod))
	static FFrameRate GetDisplayRate(UMovieSceneSequence* Sequence);

	/**
	 * Get's this sequence's tick resolution
	 *
	 * @param Sequence        The sequence to use
	 * @return The tick resolution of the sequence, defining the smallest unit of time representable on this sequence
	 */
	UFUNCTION(BlueprintCallable, Category="Sequence", meta=(ScriptMethod))
	static FFrameRate GetTickResolution(UMovieSceneSequence* Sequence);

	/**
	 * Make a new range for this sequence in its display rate
	 *
	 * @param Sequence        The sequence within which to find the binding
	 * @param StartFrame      The frame at which to start the range
	 * @param Duration        The length of the range
	 * @return The display rate that this sequence is displayed as
	 */
	UFUNCTION(BlueprintCallable, Category="Sequence", meta=(ScriptMethod))
	static FSequencerScriptingRange MakeRange(UMovieSceneSequence* Sequence, int32 StartFrame, int32 Duration);

	/**
	 * Make a new range for this sequence in its display rate
	 *
	 * @param Sequence        The sequence within which to find the binding
	 * @param StartTime       The time in seconds at which to start the range
	 * @param Duration        The length of the range in seconds
	 * @return The display rate that this sequence is displayed as
	 */
	UFUNCTION(BlueprintCallable, Category="Sequence", meta=(ScriptMethod))
	static FSequencerScriptingRange MakeRangeSeconds(UMovieSceneSequence* Sequence, float StartTime, float Duration);

	/**
	 * Attempt to locate a binding in this sequence by its name
	 *
	 * @param Sequence        The sequence within which to find the binding
	 * @param Name            The display name of the binding to look up
	 * @return A unique identifier for the binding, or invalid
	 */
	UFUNCTION(BlueprintCallable, Category="Sequence", meta=(ScriptMethod))
	static FSequencerBindingProxy FindBindingByName(UMovieSceneSequence* Sequence, FString Name);

	/**
	 * Get all the bindings in this sequence
	 *
	 * @param Sequence        The sequence to get bindings for
	 * @return An array of unique identifiers for all the bindings in this sequence
	 */
	UFUNCTION(BlueprintCallable, Category="Sequence", meta=(ScriptMethod))
	static TArray<FSequencerBindingProxy> GetBindings(UMovieSceneSequence* Sequence);

	/**
	 * Add a new binding to this sequence that will possess the specified object
	 *
	 * @param Sequence        The sequence to add a possessable to
	 * @param ObjectToPossess The object that this sequence should possess when evaluating
	 * @return A unique identifier for the new binding
	 */
	UFUNCTION(BlueprintCallable, Category="Sequence", meta=(ScriptMethod))
	static FSequencerBindingProxy AddPossessable(UMovieSceneSequence* Sequence, UObject* ObjectToPossess);

	/**
	 * Add a new binding to this sequence that will spawn the specified object
	 *
	 * @param Sequence        The sequence to add to
	 * @param ObjectToSpawn   An object instance to use as a template for spawning
	 * @return A unique identifier for the new binding
	 */
	UFUNCTION(BlueprintCallable, Category="Sequence", meta=(ScriptMethod))
	static FSequencerBindingProxy AddSpawnableFromInstance(UMovieSceneSequence* Sequence, UObject* ObjectToSpawn);

	/**
	 * Add a new binding to this sequence that will spawn the specified object
	 *
	 * @param Sequence        The sequence to add to
	 * @param ClassToSpawn    A class or blueprint type to spawn for this binding
	 * @return A unique identifier for the new binding
	 */
	UFUNCTION(BlueprintCallable, Category="Sequence", meta=(ScriptMethod))
	static FSequencerBindingProxy AddSpawnableFromClass(UMovieSceneSequence* Sequence, UClass* ClassToSpawn);

public:

	/**
	 * Filter the specified array of tracks by class, optionally only matching exact classes
	 *
	 * @param InTracks       The array of tracks to filter
	 * @param DesiredClass   The class to match against
	 * @param bExactMatch    Whether to match sub classes or not
	 * @return A filtered array of tracks
	 */
	static TArray<UMovieSceneTrack*> FilterTracks(TArrayView<UMovieSceneTrack* const> InTracks, UClass* DesiredClass, bool bExactMatch);
};