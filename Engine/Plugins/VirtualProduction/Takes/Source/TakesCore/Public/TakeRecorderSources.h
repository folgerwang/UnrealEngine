// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Containers/ArrayView.h"
#include "Templates/SubclassOf.h"
#include "Tickable.h"
#include "Misc/FrameRate.h"
#include "Misc/QualifiedFrameTime.h"
#include "Serializers/MovieSceneManifestSerialization.h"
#include "TakeRecorderSources.generated.h"

class UTakeRecorderSource;

enum class RecordPass
{
	PreRecord,
	StartRecord,
	StopRecord,
	PostRecord
};
DECLARE_LOG_CATEGORY_EXTERN(SubSequenceSerialization, Verbose, All);

struct FTakeRecorderSourcesSettings
{
	bool bSaveRecordedAssets;
	bool bRemoveRedundantTracks;
};

/**
 * A list of sources to record for any given take. Stored as meta-data on ULevelSequence through ULevelSequence::FindMetaData<UTakeRecorderSources>
 */
UCLASS(BlueprintType, Blueprintable)
class TAKESCORE_API UTakeRecorderSources : public UObject
{
public:
	GENERATED_BODY()

	UTakeRecorderSources(const FObjectInitializer& ObjInit);

	/**
	 * Add a new source to this source list of the templated type
	 *
	 * @return An instance of the templated type
	 */
	template<typename SourceType>
	SourceType* AddSource()
	{
		return static_cast<SourceType*>(AddSource(SourceType::StaticClass()));
	}


	/**
	 * Add a new source to this source list of the templated type
	 *
	 * @param InSourceType    The class type of the source to add
	 * @return An instance of the specified source type
	 */
	UFUNCTION(BlueprintCallable, Category = "Take Recorder")
	UTakeRecorderSource* AddSource(TSubclassOf<UTakeRecorderSource> InSourceType);

	/**
	 * Remove the specified source from this list
	 *
	 * @param InSource        The source to remove
	 */
	UFUNCTION(BlueprintCallable, Category = "Take Recorder")
	void RemoveSource(UTakeRecorderSource* InSource);


	/**
	 * Access all the sources stored in this list
	 */
	TArrayView<UTakeRecorderSource* const> GetSources() const
	{
		return Sources;
	}

	/**
	* Retrieves a copy of the list of sources that are being recorded. This is intended for Blueprint usages which cannot
	* use TArrayView.
	* DO NOT MODIFY THIS ARRAY, modifications will be lost.
	*/
 	UFUNCTION(BlueprintPure, DisplayName = "Get Sources (Copy)", Category = "Take Recorder")
 	TArray<UTakeRecorderSource*> GetSourcesCopy() const
 	{
		return TArray<UTakeRecorderSource*>(Sources);
	}
	/**
	 * Retrieve the serial number that is incremented when a source is added or removed from this list.
	 * @note: This field is not serialized, and not copied along with UObject duplication.
	 */
	uint32 GetSourcesSerialNumber() const
	{
		return SourcesSerialNumber;
	}
	UFUNCTION(BlueprintPure, Category = "Take Recorder")
	bool GetRecordToSubSequence() const { return bRecordSourcesToSubSequences; }
	UFUNCTION(BlueprintCallable, Category = "Take Recorder")
	void SetRecordToSubSequence(bool bValue) { bRecordSourcesToSubSequences = bValue; }

	/** Calls the recording initialization flows on each of the specified sources. */
	UFUNCTION(BlueprintCallable, Category = "Take Recorder")
	void StartRecordingSource(TArray<UTakeRecorderSource*> InSources,const FTimecode& CurrentTiimecode);
public:

	/**
	 * Bind a callback for when this source list changes
	 *
	 * @param Handler         The delegate to call when the list changes
	 * @return A handle to this specific binding that should be passed to UnbindSourcesChanged
	 */
	FDelegateHandle BindSourcesChanged(const FSimpleDelegate& Handler);


	/**
	 * Unbind a previously bound handler for when this source list changes
	 *
	 * @param Handle          The handle returned from BindSourcesChanged for the handler to remove
	 */
	void UnbindSourcesChanged(FDelegateHandle Handle);

public: 

	/*
	 * Start recording pass
	 *
	 */
	void StartRecording(class ULevelSequence* InSequence, FManifestSerializer* InManifestSerializer);

	/*
	* Tick recording pass
	* @return Current Frame Number we are recording at.
	*/
	FFrameTime TickRecording(class ULevelSequence* InSequence, float DeltaTime);

	/*
	* Stop recording pass
	*
	*/
	void StopRecording(class ULevelSequence* InSequence, FTakeRecorderSourcesSettings TakeRecorderSourcesSettings);

public:
	/*
	*  Static functions used by other parts of the take system
	*/

	/** Creates a sub-sequence asset for the specified sub sequence name based on the given master sequence. */
	static ULevelSequence* CreateSubSequenceForSource(ULevelSequence* InMasterSequence, const FString& SubSequenceName);

private:
	/** Called at the end of each frame in both the Editor and in Game to update all Sources. */
	virtual void Tick(float DeltaTime) {}

	/** Called whenever a property is changed on this class */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/** A list of handlers to invoke when the sources list changes */
	DECLARE_MULTICAST_DELEGATE(FOnSourcesChanged);
	FOnSourcesChanged OnSourcesChangedEvent;

	/** Calls PreRecording on sources recursively allowing them to create other sources which properly get PreRecording called on them as well. */
	void StartRecordingRecursive(TArray<UTakeRecorderSource*> InSources, ULevelSequence* InSequence, const FTimecode& Timecode, FManifestSerializer* InManifestSerializer);

	/** Finds the folder that the given Source should be created in, creating it if necessary. */
	class UMovieSceneFolder* AddFolderForSource(const UTakeRecorderSource* InSource, class UMovieScene* InMovieScene);

	/** Gets the current frame time for recording, optionally resolving out the engine's custom Timecode provider. */
	FQualifiedFrameTime GetCurrentRecordingFrameTime(const FTimecode& InTimeCode, bool& bHasValidTimeCodeSource) const;

	/** Remove object bindings that don't have any tracks and are not bindings for attach/path tracks */
	void RemoveRedundantTracks();

private:

	/** The array of all sources contained within this list */
	UPROPERTY(Instanced)
	TArray<UTakeRecorderSource*> Sources;

	/** Maps each source to the level sequence that was created for that source, or to the master source if a subsequence was not created. */
	UPROPERTY(Transient)
	TMap<UTakeRecorderSource*, ULevelSequence*> SourceSubSequenceMap;

	/** List of sub-sections that we're recording into. Needed to ensure they're all the right size at the end without re-adjusting every sub-section in a sequence. */
	UPROPERTY(Transient)
	TArray<class UMovieSceneSubSection*> ActiveSubSections;

	/** Are we currently in a recording pass and should be ticking our Sources? */
	bool bIsRecording;

	/** Float of how long the recording has been going based on delta tick times. Used when we have no Timecode Synchronization */
	float TimeSinceRecordingStarted;

	/** What Tick Resolution is the target level sequence we're recording into? Used to convert seconds into FrameNumbers. */
	FFrameRate TargetLevelSequenceTickResolution;
	
	/** Non-serialized serial number that is used for updating UI when the source list changes */
	uint32 SourcesSerialNumber;

	/** Should we record our sources to Sub Sequences and place them in the master via a Subscenes track? */
	bool bRecordSourcesToSubSequences;

	/** Manifest Serializer that we are recording into. */
	FManifestSerializer* CachedManifestSerializer;

	/** Level Sequence that we are recording into. Cached so that new sources added mid-recording get placed in the right sequence. */
	ULevelSequence* CachedLevelSequence;

	/** Array of Allocated Serializers created for each sub sequence.  Deleted at the end of the recording so memory is freed. */
	TArray<TSharedPtr<FManifestSerializer>> CreatedManifestSerializers;

	/** Timecode time at start of recording */
	FTimecode StartRecordingTimecodeSource;
};

