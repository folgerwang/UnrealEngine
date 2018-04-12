// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sequencer/ControlRigObjectSpawner.h"

class FControlRigEditorObjectSpawner : public FControlRigObjectSpawner
{
public:

	FControlRigEditorObjectSpawner();
	~FControlRigEditorObjectSpawner();

	static TSharedRef<IMovieSceneObjectSpawner> CreateObjectSpawner();

	// IMovieSceneObjectSpawner interface
	virtual bool IsEditor() const override { return true; }
	virtual UObject* SpawnObject(FMovieSceneSpawnable& Spawnable, FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player) override;

#if WITH_EDITOR
	virtual TValueOrError<FNewSpawnable, FText> CreateNewSpawnableType(UObject& SourceObject, UMovieScene& OwnerMovieScene, UActorFactory* ActorFactory = nullptr) override;
	virtual void SetupDefaultsForSpawnable(UObject* SpawnedObject, const FGuid& Guid, const TOptional<FTransformData>& TransformData, TSharedRef<ISequencer> Sequencer, USequencerSettings* Settings) override;
	virtual bool CanConvertSpawnableToPossessable(FMovieSceneSpawnable& Spawnable) const override { return false; }
	/** Called from the editor when a blueprint object replacement has occurred */
	void OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewInstanceMap);
#endif
};
