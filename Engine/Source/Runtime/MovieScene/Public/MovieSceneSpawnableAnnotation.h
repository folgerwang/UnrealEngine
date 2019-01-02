// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "Misc/Optional.h"
#include "Misc/Guid.h"
#include "UObject/WeakObjectPtr.h"
#include "MovieSceneSequence.h"

/**
 * An annotation that's added to spawned objects from movie scene spawnables
 */
struct FMovieSceneSpawnableAnnotation
{
	FMovieSceneSpawnableAnnotation()
	{}

	/**
	 * Add the annotation to the specified spawned object, allowing a back-reference to the sequence and binding ID
	 */
	MOVIESCENE_API static void Add(UObject* SpawnedObject, const FGuid& ObjectBindingID, UMovieSceneSequence* InOriginatingSequence);

	/**
	 * Attempt to find an annotation for the specified object
	 */
	MOVIESCENE_API static TOptional<FMovieSceneSpawnableAnnotation> Find(UObject* SpawnedObject);

	bool IsDefault() const
	{
		return !ObjectBindingID.IsValid();
	}

	/** ID of the object binding that spawned the object */
	FGuid ObjectBindingID;

	/** Sequence that contains the object binding that spawned the object */
	TWeakObjectPtr<UMovieSceneSequence> OriginatingSequence;
};


#endif // WITH_EDITOR