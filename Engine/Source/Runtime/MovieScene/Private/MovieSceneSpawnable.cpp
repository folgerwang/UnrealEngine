// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneSpawnable.h"
#include "UObject/UObjectAnnotation.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"

struct FIsSpawnable
{
	FIsSpawnable() : bIsSpawnable(false) {}
	explicit FIsSpawnable(bool bInIsSpawnable) : bIsSpawnable(bInIsSpawnable) {}

	bool IsDefault() const { return !bIsSpawnable; }

	bool bIsSpawnable;
};

static FUObjectAnnotationSparse<FIsSpawnable,true> SpawnablesAnnotation;

bool FMovieSceneSpawnable::IsSpawnableTemplate(const UObject& InObject)
{
	return !SpawnablesAnnotation.GetAnnotation(&InObject).IsDefault();
}

void FMovieSceneSpawnable::MarkSpawnableTemplate(const UObject& InObject)
{
	SpawnablesAnnotation.AddAnnotation(&InObject, FIsSpawnable(true));
}

void FMovieSceneSpawnable::CopyObjectTemplate(UObject& InSourceObject, UMovieSceneSequence& MovieSceneSequence)
{
	const FName ObjectName = ObjectTemplate ? ObjectTemplate->GetFName() : InSourceObject.GetFName();

	if (ObjectTemplate)
	{
		ObjectTemplate->Rename(*MakeUniqueObjectName(MovieSceneSequence.GetMovieScene(), ObjectTemplate->GetClass(), "ExpiredSpawnable").ToString());
		ObjectTemplate->MarkPendingKill();
		ObjectTemplate = nullptr;
	}

	ObjectTemplate = MovieSceneSequence.MakeSpawnableTemplateFromInstance(InSourceObject, ObjectName);

	check(ObjectTemplate);

	MarkSpawnableTemplate(*ObjectTemplate);
	MovieSceneSequence.MarkPackageDirty();
}
