// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/NameTypes.h"

struct FMovieSceneEvent;

class UBlueprint;
class UMovieSceneTrack;
class UMovieSceneSequence;
class UMovieSceneEventTrack;
class UK2Node_FunctionEntry;
class UMovieSceneEventSectionBase;

struct SEQUENCER_API FMovieSceneSequenceEditor
{
	virtual ~FMovieSceneSequenceEditor(){}


	/**
	 * The name of the target pin for event function entry nodes
	 */
	static FName TargetPinName;


	/**
	 * Attempt to find a sequence editor for the specified sequence
	 *
	 * @param Sequence        The sequence to get an editor for
	 * @return The sequence editor ptr, or null if one is not available for the specified type of sequence
	 */
	static FMovieSceneSequenceEditor* Find(UMovieSceneSequence* InSequence);


	/**
	 * Bind an event to an endpoint. Ensures the necessary pointers are set up to enable automatic re-compilation on evaluation.
	 *
	 * @param EventSection    The section that the event belongs to
	 * @param Event           The event to bind to the endpoint
	 * @param Endpoint        The blueprint node to bind to
	 */
	static void BindEventToEndpoint(UMovieSceneEventSectionBase* EventSection, FMovieSceneEvent* Event, UK2Node_FunctionEntry* Endpoint);

	/**
	 * Check whether the specified sequence supports events
	 *
	 * @param Sequence        The sequence to test
	 */
	bool SupportsEvents(UMovieSceneSequence* InSequence) const;


	/**
	 * Access the director blueprint for the specified sequence
	 *
	 * @param Sequence        The sequence to access the director blueprint for
	 * @return The sequence's director blueprint or nullptr if it does not have one (or cannot)
	 */
	UBlueprint* GetDirectorBlueprint(UMovieSceneSequence* Sequence) const;


	/**
	 * Create a new event endpoint for the specified sequence
	 *
	 * @param Sequence        The sequence to create an endpoint in
	 * @param DesiredName     (Optional) A user-specified name to use for the function
	 * @return The blueprint node for the event endpoint, or nullptr if one does not exist
	 */
	UK2Node_FunctionEntry* CreateEventEndpoint(UMovieSceneSequence* Sequence, const FString& DesiredName = FString()) const;


	/**
	 * Initializes the specified endpoint node by creating an appropriate input pin for the track's object binding, if necessary
	 *
	 * @param EventTrack      The event track that owns the event
	 * @param Endpoint        The event endpoint to initialize
	 */
	void InitializeEndpointForTrack(UMovieSceneEventTrack* EventTrack, UK2Node_FunctionEntry* Endpoint) const;

protected:


	/**
	 * Find the class of the object binding the specified track is on, or nullptr
	 */
	static UClass* FindTrackObjectBindingClass(UMovieSceneTrack* Track);


private:

	/**
	 * Access the director blueprint for the specified sequence
	 *
	 * @param Sequence        The sequence to access the director blueprint for
	 * @return The sequence's director blueprint or nullptr if it does not have one (or cannot)
	 */
	UBlueprint* AccessDirectorBlueprint(UMovieSceneSequence* Sequence) const;

private:

	virtual bool CanCreateEvents(UMovieSceneSequence* InSequence) const { return false; }
	virtual UBlueprint* GetBlueprintForSequence(UMovieSceneSequence* InSequence) const { return nullptr; }
	virtual UBlueprint* CreateBlueprintForSequence(UMovieSceneSequence* InSequence) const { return nullptr; }
	virtual void SetupDefaultPinForEndpoint(UMovieSceneEventTrack* EventTrack, UK2Node_FunctionEntry* Endpoint) const {}
};

