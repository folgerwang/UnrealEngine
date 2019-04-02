// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneSequence.h"
#include "LevelSequenceObject.h"
#include "LevelSequenceBindingReference.h"
#include "LevelSequenceLegacyObjectReference.h"
#include "Templates/SubclassOf.h"
#include "LevelSequence.generated.h"

class UBlueprint;
class UMovieScene;
class UBlueprintGeneratedClass;

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
	virtual void UnbindObjects(const FGuid& ObjectId, const TArray<UObject*>& InObjects, UObject* InContext) override;
	virtual void UnbindInvalidObjects(const FGuid& ObjectId, UObject* InContext) override;
	virtual bool AllowsSpawnableObjects() const override;
	virtual bool CanRebindPossessable(const FMovieScenePossessable& InPossessable) const override;
	virtual UObject* MakeSpawnableTemplateFromInstance(UObject& InSourceObject, FName ObjectName) override;
	virtual bool CanAnimateObject(UObject& InObject) const override;
	virtual UObject* CreateDirectorInstance(IMovieScenePlayer& Player) override;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void GetAssetRegistryTagMetadata(TMap<FName, FAssetRegistryTagMetadata>& OutMetadata) const override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
#endif
	virtual void PostDuplicate(bool bDuplicateForPIE) override;

	void LocateBoundObjects(const FGuid& ObjectId, UObject* Context, FName StreamedLevelAssetPath, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const;
#if WITH_EDITOR


public:

	/**
	 * Assign a new director blueprint to this level sequence. The specified blueprint *must* be contained within this object.?	 */
	void SetDirectorBlueprint(UBlueprint* NewDirectorBlueprint);

	/**
	 * Retrieve the currently assigned director blueprint for this level sequence
	 */
	UBlueprint* GetDirectorBlueprint() const;

protected:

	virtual FGuid CreatePossessable(UObject* ObjectToPossess) override;
	virtual FGuid CreateSpawnable(UObject* ObjectToSpawn) override;

	FGuid FindOrAddBinding(UObject* ObjectToPossess);

	/**
	 * Invoked when this level sequence's director blueprint has been recompiled
	 */
	void OnDirectorRecompiled(UBlueprint*);

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

#if WITH_EDITORONLY_DATA

	/** A pointer to the director blueprint that generates this sequence's DirectorClass. */
	UPROPERTY()
	UBlueprint* DirectorBlueprint;

#endif

	/**
	 * The class that is used to spawn this level sequence's director instance.
	 * Director instances are allocated on-demand one per sequence during evaluation and are used by event tracks for triggering events.
	 */
	UPROPERTY()
	UClass* DirectorClass;

public:
	/**
	* Find meta-data of a particular type for this level sequence instance.
	* @param InClass - Class that you wish to find the metadata object for.
	* @return An instance of this class if it already exists as metadata on this Level Sequence, otherwise null.
	*/
	UFUNCTION(BlueprintPure, Category = "Level Sequence", meta=(DevelopmentOnly))
	UObject* FindMetaDataByClass(TSubclassOf<UObject> InClass) const
	{
#if WITH_EDITORONLY_DATA
		UObject* const* Found = MetaDataObjects.FindByPredicate([InClass](UObject* In) { return In && In->GetClass() == InClass; });
		return Found ? CastChecked<UObject>(*Found) : nullptr;
#endif
		return nullptr;
	}

	/**
	* Find meta-data of a particular type for this level sequence instance, adding it if it doesn't already exist.
	* @param InClass - Class that you wish to find or create the metadata object for.
	* @return An instance of this class as metadata on this Level Sequence.
	*/
	UFUNCTION(BlueprintCallable, Category = "Level Sequence", meta=(DevelopmentOnly))
	UObject* FindOrAddMetaDataByClass(TSubclassOf<UObject> InClass)
	{
#if WITH_EDITORONLY_DATA
		UObject* Found = FindMetaDataByClass(InClass);
		if (!Found)
		{
			Found = NewObject<UObject>(this, InClass);
			MetaDataObjects.Add(Found);
		}
		return Found;
#endif
		return nullptr;
	}

	/**
	* Copy the specified meta data into this level sequence, overwriting any existing meta-data of the same type
	* Meta-data may implement the ILevelSequenceMetaData interface in order to hook into default ULevelSequence functionality.
	* @param InMetaData - Existing Metadata Object that you wish to copy into this Level Sequence.
	* @return The newly copied instance of the Metadata that now exists on this sequence.
	*/
	UFUNCTION(BlueprintCallable, Category = "Level Sequence", meta=(DevelopmentOnly))
	UObject* CopyMetaData(UObject* InMetaData)
	{
#if WITH_EDITORONLY_DATA
		if (!InMetaData)
		{
			return nullptr;
		}

		RemoveMetaDataByClass(InMetaData->GetClass());

		UObject* NewMetaData = DuplicateObject(InMetaData, this);
		MetaDataObjects.Add(NewMetaData);

		return NewMetaData;
#endif	
		return nullptr;
	}

	/**
	* Remove meta-data of a particular type for this level sequence instance, if it exists
	* @param InClass - The class type that you wish to remove the metadata for
	*/
	UFUNCTION(BlueprintCallable, Category = "Level Sequence", meta=(DevelopmentOnly))
	void RemoveMetaDataByClass(TSubclassOf<UObject> InClass)
	{
#if WITH_EDITORONLY_DATA
		MetaDataObjects.RemoveAll([InClass](UObject* In) { return In && In->GetClass() == InClass; });
#endif
	}

#if WITH_EDITORONLY_DATA

public:

	/**
	 * Find meta-data of a particular type for this level sequence instance
	 */
	template<typename MetaDataType>
	MetaDataType* FindMetaData() const
	{
		UClass* PredicateClass = MetaDataType::StaticClass();
		UObject* const* Found = MetaDataObjects.FindByPredicate([PredicateClass](UObject* In){ return In && In->GetClass() == PredicateClass; });
		return Found ? CastChecked<MetaDataType>(*Found) : nullptr;
	}

	/**
	 * Find meta-data of a particular type for this level sequence instance, adding one if it was not found.
	 * Meta-data may implement the ILevelSequenceMetaData interface in order to hook into default ULevelSequence functionality.
	 */
	template<typename MetaDataType>
	MetaDataType* FindOrAddMetaData()
	{
		MetaDataType* Found = FindMetaData<MetaDataType>();
		if (!Found)
		{
			Found = NewObject<MetaDataType>(this);
			MetaDataObjects.Add(Found);
		}
		return Found;
	}

	/**
	 * Copy the specified meta data into this level sequence, overwriting any existing meta-data of the same type
	 * Meta-data may implement the ILevelSequenceMetaData interface in order to hook into default ULevelSequence functionality.
	 */
	template<typename MetaDataType>
	MetaDataType* CopyMetaData(MetaDataType* InMetaData)
	{
		RemoveMetaData<MetaDataType>();

		MetaDataType* NewMetaData = DuplicateObject(InMetaData, this);
		MetaDataObjects.Add(NewMetaData);

		return NewMetaData;
	}

	/**
	 * Remove meta-data of a particular type for this level sequence instance, if it exists
	 */
	template<typename MetaDataType>
	void RemoveMetaData()
	{
		UClass* PredicateClass = MetaDataType::StaticClass();
		MetaDataObjects.RemoveAll([PredicateClass](UObject* In){ return In && In->GetClass() == PredicateClass; });
	}

private:

	/** Array of meta-data objects associated with this level sequence. Each pointer may implement the ILevelSequenceMetaData interface in order to hook into default ULevelSequence functionality. */
	UPROPERTY()
	TArray<UObject*> MetaDataObjects;

#endif
};
