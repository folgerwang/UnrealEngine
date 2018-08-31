// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneSequence.h"
#include "LevelSequenceObject.h"
#include "LevelSequenceBindingReference.h"
#include "LevelSequenceLegacyObjectReference.h"
#include "LevelSequence.generated.h"

class UMovieScene;
class ULevelSequenceDirectorGeneratedClass;

/**
 * Movie scene animation for Actors.
 */
UCLASS(BlueprintType)
class LEVELSEQUENCE_API ULevelSequence
	: public UMovieSceneSequence
{
	GENERATED_UCLASS_BODY()

public:

	/** Pointer to the movie scene that controls this animation. */
	UPROPERTY()
	UMovieScene* MovieScene;

public:

	/** Initialize this level sequence. */
	virtual void Initialize();

	/** Convert old-style lazy object ptrs to new-style references using the specified context */
	void ConvertPersistentBindingsToDefault(UObject* FixupContext);

public:

	// UMovieSceneSequence interface
	virtual void BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UObject* Context) override;
	virtual bool CanPossessObject(UObject& Object, UObject* InPlaybackContext) const override;
	virtual void LocateBoundObjects(const FGuid& ObjectId, UObject* Context, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const override;
	virtual void GatherExpiredObjects(const FMovieSceneObjectCache& InObjectCache, TArray<FGuid>& OutInvalidIDs) const override;
	virtual UMovieScene* GetMovieScene() const override;
	virtual UObject* GetParentObject(UObject* Object) const override;
	virtual void UnbindPossessableObjects(const FGuid& ObjectId) override;
	virtual bool AllowsSpawnableObjects() const override;
	virtual bool CanRebindPossessable(const FMovieScenePossessable& InPossessable) const override;
	virtual UObject* MakeSpawnableTemplateFromInstance(UObject& InSourceObject, FName ObjectName) override;
	virtual bool CanAnimateObject(UObject& InObject) const override;
	virtual UObject* CreateDirectorInstance(IMovieScenePlayer& Player) override;
#if WITH_EDITOR
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
#endif
	virtual void PostLoad() override;

#if WITH_EDITORONLY_DATA

	/** A pointer to the director blueprint that generates this sequence's DirectorClass. */
	UPROPERTY()
	UObject* DirectorBlueprint;

#endif

	/**
	 * The class that is used to spawn this level sequence's director instance.
	 * Director instances are allocated on-demand one per sequence during evaluation and are used by event tracks for triggering events.
	 */
	UPROPERTY()
	ULevelSequenceDirectorGeneratedClass* DirectorClass;

protected:

#if WITH_EDITOR

	virtual FGuid CreatePossessable(UObject* ObjectToPossess) override;
	virtual FGuid CreateSpawnable(UObject* ObjectToSpawn) override;

	FGuid FindOrAddBinding(UObject* ObjectToPossess);

#endif // WITH_EDITOR

protected:

	/** Legacy object references - should be read-only. Not deprecated because they need to still be saved */
	UPROPERTY()
	FLevelSequenceObjectReferenceMap ObjectReferences;

	/** References to bound objects. */
	UPROPERTY()
	FLevelSequenceBindingReferences BindingReferences;

	/** Deprecated property housing old possessed object bindings */
	UPROPERTY()
	TMap<FString, FLevelSequenceObject> PossessedObjects_DEPRECATED;
};
