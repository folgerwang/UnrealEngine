// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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
public:
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
	 * Gets this sequence's display rate
	 *
	 * @param Sequence        The sequence to use
	 * @return The display rate that this sequence is displayed as
	 */
	UFUNCTION(BlueprintCallable, Category="Sequence", meta=(ScriptMethod))
	static FFrameRate GetDisplayRate(UMovieSceneSequence* Sequence);

	/**
	 * Sets this sequence's display rate
	 *
	 * @param Sequence        The sequence to use
	 * @param DisplayRate The display rate that this sequence is displayed as
	 */
	UFUNCTION(BlueprintCallable, Category="Sequence", meta=(ScriptMethod))
	static void SetDisplayRate(UMovieSceneSequence* Sequence, FFrameRate DisplayRate);

	/**
	 * Gets this sequence's tick resolution
	 *
	 * @param Sequence        The sequence to use
	 * @return The tick resolution of the sequence, defining the smallest unit of time representable on this sequence
	 */
	UFUNCTION(BlueprintCallable, Category="Sequence", meta=(ScriptMethod))
	static FFrameRate GetTickResolution(UMovieSceneSequence* Sequence);

	/**
	 * Sets this sequence's tick resolution
	 *
	 * @param Sequence        The sequence to use
	 * @param TickResolution The tick resolution of the sequence, defining the smallest unit of time representable on this sequence
	 */
	UFUNCTION(BlueprintCallable, Category="Sequence", meta=(ScriptMethod))
	static void SetTickResolution(UMovieSceneSequence* Sequence, FFrameRate TickResolution);

	/**
	 * Make a new range for this sequence in its display rate
	 *
	 * @param Sequence        The sequence within which to find the binding
	 * @param StartFrame      The frame at which to start the range
	 * @param Duration        The length of the range
	 * @return Specified sequencer range
	 */
	UFUNCTION(BlueprintCallable, Category="Sequence", meta=(ScriptMethod))
	static FSequencerScriptingRange MakeRange(UMovieSceneSequence* Sequence, int32 StartFrame, int32 Duration);

	/**
	 * Make a new range for this sequence in seconds
	 *
	 * @param Sequence        The sequence within which to find the binding
	 * @param StartTime       The time in seconds at which to start the range
	 * @param Duration        The length of the range in seconds
	 * @return Specified sequencer range
	 */
	UFUNCTION(BlueprintCallable, Category="Sequence", meta=(ScriptMethod))
	static FSequencerScriptingRange MakeRangeSeconds(UMovieSceneSequence* Sequence, float StartTime, float Duration);

	UE_DEPRECATED(4.22, "Please use GetPlaybackStart and GetPlaybackEnd instead.")
	static FSequencerScriptingRange GetPlaybackRange(UMovieSceneSequence* Sequence);

	/**
	 * Get playback start of this sequence
	 *
	 * @param Sequence        The sequence within which to get the playback start
	 * @return Playback start of this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequence", meta = (ScriptMethod))
	static int32 GetPlaybackStart(UMovieSceneSequence* Sequence);

	/**
	 * Get playback start of this sequence in seconds
	 *
	 * @param Sequence        The sequence within which to get the playback start
	 * @return Playback start of this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequence", meta = (ScriptMethod))
	static float GetPlaybackStartSeconds(UMovieSceneSequence* Sequence);

	/**
	 * Get playback end of this sequence
	 *
	 * @param Sequence        The sequence within which to get the playback end
	 * @return Playback end of this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequence", meta = (ScriptMethod))
	static int32 GetPlaybackEnd(UMovieSceneSequence* Sequence);

	/**
	 * Get playback end of this sequence in seconds
	 *
	 * @param Sequence        The sequence within which to get the playback end
	 * @return Playback end of this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequence", meta = (ScriptMethod))
	static float GetPlaybackEndSeconds(UMovieSceneSequence* Sequence);

	/**
	 * Set playback start of this sequence
	 *
	 * @param Sequence        The sequence within which to set the playback start
	 * @param StartFrame      The desired start frame for this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequence", meta = (ScriptMethod))
	static void SetPlaybackStart(UMovieSceneSequence* Sequence, int32 StartFrame);

	/**
	 * Set playback start of this sequence in seconds
	 *
	 * @param Sequence        The sequence within which to set the playback start
	 * @param StartTime       The desired start time in seconds for this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequence", meta = (ScriptMethod))
	static void SetPlaybackStartSeconds(UMovieSceneSequence* Sequence, float StartTime);

	/**
	 * Set playback end of this sequence
	 *
	 * @param Sequence        The sequence within which to set the playback end
	 * @param EndFrame        The desired end frame for this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequence", meta = (ScriptMethod))
	static void SetPlaybackEnd(UMovieSceneSequence* Sequence, int32 EndFrame);

	/**
	 * Set playback end of this sequence in seconds
	 *
	 * @param Sequence        The sequence within which to set the playback end
	 * @param EndTime         The desired end time in seconds for this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequence", meta = (ScriptMethod))
	static void SetPlaybackEndSeconds(UMovieSceneSequence* Sequence, float EndTime);

	/**
	 * Set the sequence view range start in seconds
	 *
	 * @param Sequence The sequence within which to set the view range start
	 * @param StartTimeInSeconds The desired view range start time in seconds for this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequence", meta = (ScriptMethod, DevelopmentOnly))
	static void SetViewRangeStart(UMovieSceneSequence* InSequence, float StartTimeInSeconds);

	/**
	 * Get the sequence view range start in seconds
	 *
	 * @param Sequence The sequence within which to get the view range start
	 * @return The view range start time in seconds for this sequence
	 */
	UFUNCTION(BlueprintPure, Category = "Sequence", meta = (ScriptMethod, DevelopmentOnly))
	static float GetViewRangeStart(UMovieSceneSequence* InSequence);

	/**
	 * Set the sequence view range end in seconds
	 *
	 * @param Sequence The sequence within which to set the view range end
	 * @param StartTimeInSeconds The desired view range end time in seconds for this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequence", meta = (ScriptMethod, DevelopmentOnly))
	static void SetViewRangeEnd(UMovieSceneSequence* InSequence, float EndTimeInSeconds);

	/**
	 * Get the sequence view range end in seconds
	 *
	 * @param Sequence The sequence within which to get the view range end
	 * @return The view range end time in seconds for this sequence
	 */
	UFUNCTION(BlueprintPure, Category = "Sequence", meta = (ScriptMethod, DevelopmentOnly))
	static float GetViewRangeEnd(UMovieSceneSequence* InSequence);

	/**
	 * Set the sequence work range start in seconds
	 *
	 * @param Sequence The sequence within which to set the work range start
	 * @param StartTimeInSeconds The desired work range start time in seconds for this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequence", meta = (ScriptMethod, DevelopmentOnly))
	static void SetWorkRangeStart(UMovieSceneSequence* InSequence, float StartTimeInSeconds);

	/**
	 * Get the sequence work range start in seconds
	 *
	 * @param Sequence The sequence within which to get the work range start
	 * @return The work range start time in seconds for this sequence
	 */
	UFUNCTION(BlueprintPure, Category = "Sequence", meta = (ScriptMethod, DevelopmentOnly))
	static float GetWorkRangeStart(UMovieSceneSequence* InSequence);

	/**
	 * Set the sequence work range end in seconds
	 *
	 * @param Sequence The sequence within which to set the work range end
	 * @param StartTimeInSeconds The desired work range end time in seconds for this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequence", meta = (ScriptMethod, DevelopmentOnly))
	static void SetWorkRangeEnd(UMovieSceneSequence* InSequence, float EndTimeInSeconds);

	/**
	 * Get the sequence work range end in seconds
	 *
	 * @param Sequence The sequence within which to get the work range end
	 * @return The work range end time in seconds for this sequence
	 */
	UFUNCTION(BlueprintPure, Category = "Sequence", meta = (ScriptMethod, DevelopmentOnly))
	static float GetWorkRangeEnd(UMovieSceneSequence* InSequence);

	/**
	 * Get the timecode source of this sequence
	 *
	 * @param Sequence        The sequence within which to get the timecode source
	 * @return Timecode source of this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequence", meta = (ScriptMethod))
	static FTimecode GetTimecodeSource(UMovieSceneSequence* Sequence);

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
	* Get all the spawnables in this sequence
	*
	* @param Sequence        The sequence to get spawnables for
	* @return Spawnables in this sequence
	*/
	UFUNCTION(BlueprintCallable, Category="Sequence", meta=(ScriptMethod))
	static TArray<FSequencerBindingProxy> GetSpawnables(UMovieSceneSequence* Sequence);

	/**
	* Get all the possessables in this sequence
	*
	* @param Sequence        The sequence to get possessables for
	* @return Possessables in this sequence
	*/
	UFUNCTION(BlueprintCallable, Category="Sequence", meta=(ScriptMethod))
	static TArray<FSequencerBindingProxy> GetPossessables(UMovieSceneSequence* Sequence);

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

	/**
	 * Locate all the objects that correspond to the specified object ID, using the specified context
	 *
	 * @param Sequence   The sequence to locate bound objects for
	 * @param InBinding  The object binding
	 * @param Context    Optional context to use to find the required object
	 * @return An array of all bound objects
	 */
	UFUNCTION(BlueprintCallable, Category="Sequence", meta=(ScriptMethod))
	static TArray<UObject*> LocateBoundObjects(UMovieSceneSequence* Sequence, const FSequencerBindingProxy& InBinding, UObject* Context);

	/**
	 * Get the root folders in the provided sequence
	 *
	 * @param Sequence	The sequence to retrieve folders from
	 * @return The folders contained within the given sequence
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Editor Scripting|Sequencer Tools|Folders", meta=(ScriptMethod))
	static TArray<UMovieSceneFolder*> GetRootFoldersInSequence(UMovieSceneSequence* Sequence);

	/**
	 * Add a root folder to the given sequence
	 *
	 * @param Sequence			The sequence to add a folder to
	 * @param NewFolderName		The name to give the added folder
	 * @return The newly created folder
	 */
	UFUNCTION(BlueprintCallable, Category="Editor Scripting|Sequencer Tools|Folders", meta=(ScriptMethod))
	static UMovieSceneFolder* AddRootFolderToSequence(UMovieSceneSequence* Sequence, FString NewFolderName);

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

public:

	/*
	 * @return Return the user marked frames
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequence", meta = (ScriptMethod))
	static TArray<FMovieSceneMarkedFrame> GetMarkedFrames(UMovieSceneSequence* Sequence);

	/*
	 * Add a given user marked frame.
	 * A unique label will be generated if the marked frame label is empty
	 *
	 * @InMarkedFrame The given user marked frame to add
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequence", meta = (ScriptMethod))
	static void AddMarkedFrame(UMovieSceneSequence* Sequence, const FMovieSceneMarkedFrame& InMarkedFrame);

	/*
	 * Remove the user marked frame by index.
	 *
	 * @RemoveIndex The index to the user marked frame to remove
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequence", meta = (ScriptMethod))
	static void RemoveMarkedFrame(UMovieSceneSequence* Sequence, int32 RemoveIndex);

	/*
	 * Clear all user marked frames
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequence", meta = (ScriptMethod))
	static void ClearMarkedFrames(UMovieSceneSequence* Sequence);

	/*
	 * Find the user marked frame by label
	 *
	 * @InLabel The label to the user marked frame to find
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequence", meta = (ScriptMethod))
	static int32 FindMarkedFrameByLabel(UMovieSceneSequence* Sequence, const FString& InLabel);

	/*
	 * Find the user marked frame by frame number
	 *
	 * @InFrameNumber The frame number of the user marked frame to find
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequence", meta = (ScriptMethod))
	static int32 FindMarkedFrameByFrameNumber(UMovieSceneSequence* Sequence, FFrameNumber InFrameNumber);

	/*
	 * Find the next/previous user marked frame from the given frame number
	 *
	 * @InFrameNumber The frame number to find the next/previous user marked frame from
	 * @bForward Find forward from the given frame number.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequence", meta = (ScriptMethod))
	static int32 FindNextMarkedFrame(UMovieSceneSequence* Sequence, FFrameNumber InFrameNumber, bool bForward);
};