// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "MovieSceneEvent.generated.h"

class UEdGraph;
class UBlueprint;
class UK2Node_FunctionEntry;
class UMovieSceneEventSectionBase;


/**
 * Struct type that is bound to a blueprint function entry node, and resolved to a cached UFunction when the blueprint/track is recompiled.
 * Events can be bound to either of the following function signatures:
 *
 *   1. A function with no parameters (and no return value).
 *     - Compatible with master tracks or object bindings
 *     - No context passed through to event

 *   2. A function with a single object or interface parameter (and no return value).
 *     - Compatible with master tracks or object bindings
 *     - Will be triggered with objects in the following order:
 *         - Objects bound to the track's object binding, or:
 *         - Objects specified on the track's event receivers array, or:
 *         - Objects provided by the playback context.
 *     - Will only trigger if the specified object is of the same type as the parameter (or interface)
 */
USTRUCT()
struct MOVIESCENETRACKS_API FMovieSceneEvent
{
	GENERATED_BODY()


	/**
	 * Called after this event has been serialized in order to cache the function pointer if necessary
	 */
	void PostSerialize(const FArchive& Ar);

	/**
	 * Called to perform custom serialization logic for this struct.
	 */
	bool Serialize(FArchive& Ar);

	/**
	 * Check whether the specified function is valid for a movie scene event
	 * Functions must have either no parameters, or a single, pass-by-value object/interface parameter, with no return parameter.
	 */
	static bool IsValidFunction(UFunction* Function);


	/**
	 * The function that should be called to invoke this event.
	 * Functions must have either no parameters, or a single, pass-by-value object/interface parameter, with no return parameter.
	 */
	UPROPERTY()
	FName FunctionName;

#if WITH_EDITORONLY_DATA

public:


	/**
	 * Cache the function name to call from the blueprint function entry node. Will only cache the function if it has a valid signature.
	 */
	void CacheFunctionName();


	/**
	 * Check whether this event is bound to a valid blueprint entry node
	 *
	 * @return true if this event is bound to a function entry node with a valid signature, false otherwise.
	 */
	bool IsBoundToBlueprint() const;


	/**
	 * Helper function to determine whether the specified function entry is valid for this event
	 *
	 * @param Node         The node to test
	 * @return true if the function entry node is compatible with a moviescene event, false otherwise
	 */
	static bool IsValidFunction(UK2Node_FunctionEntry* Node);


	/**
	 * Retrieve the function entry node this event is bound to
	 *
	 * @note Events may be bound to invalid function entries if they have been changed since they were assigned.
	 * @see SetFunctionEntry, IsValidFunction
	 * @return The function entry node if still available, nullptr if it has been destroyed, or was never assigned.
	 */
	UK2Node_FunctionEntry* GetFunctionEntry() const;


	/**
	 * Set the function entry that this event should trigger
	 *
	 * @param Entry        The graph node to bind to
	 */
	void SetFunctionEntry(UK2Node_FunctionEntry* Entry);

private:

	/** Serialized soft pointer to the blueprint that contains the function graph endpoint for this event. Stored as a soft path so that renames of the blueprint don't break this event binding. */
	UPROPERTY()
	TSoftObjectPtr<UBlueprint> SoftBlueprintPath;

	/** The UEdGraph::GraphGuid property that relates to the function entry to call. */
	UPROPERTY()
	FGuid GraphGuid;

	/** Non-serialized weak pointer to the function entry within the blueprint graph for this event. Stored as an editor-only UObject so UHT can parse it when building for non-editor. */
	UPROPERTY(transient)
	mutable TWeakObjectPtr<UObject> CachedFunctionEntry;

	/** Deprecated weak pointer to the function entry to call - no longer serialized but cached on load. */
	UPROPERTY()
	TWeakObjectPtr<UObject> FunctionEntry_DEPRECATED;

#endif // WITH_EDITORONLY_DATA
};

template<>
struct TStructOpsTypeTraits<FMovieSceneEvent> : TStructOpsTypeTraitsBase2<FMovieSceneEvent>
{
	enum { WithSerializer = true, WithPostSerialize = true };
};

