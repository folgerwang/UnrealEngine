// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneSpawnableAnnotation.h"

#if WITH_EDITOR

#include "UObject/UObjectAnnotation.h"


static FUObjectAnnotationSparse<FMovieSceneSpawnableAnnotation,true> SpawnedObjectAnnotation;

void FMovieSceneSpawnableAnnotation::Add(UObject* SpawnedObject, const FGuid& ObjectBindingID, UMovieSceneSequence* InOriginatingSequence)
{
	if (SpawnedObject)
	{
		FMovieSceneSpawnableAnnotation Annotation;
		Annotation.ObjectBindingID = ObjectBindingID;
		Annotation.OriginatingSequence = InOriginatingSequence;

		SpawnedObjectAnnotation.AddAnnotation(SpawnedObject, Annotation);
	}
}

TOptional<FMovieSceneSpawnableAnnotation> FMovieSceneSpawnableAnnotation::Find(UObject* SpawnedObject)
{
	const FMovieSceneSpawnableAnnotation& Annotation = SpawnedObjectAnnotation.GetAnnotation(SpawnedObject);

	TOptional<FMovieSceneSpawnableAnnotation> ReturnValue;
	if (!Annotation.IsDefault())
	{
		ReturnValue = Annotation;
	}

	return ReturnValue;
}

#endif // WITH_EDITOR