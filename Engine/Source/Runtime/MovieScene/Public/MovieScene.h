// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Misc/Timecode.h"
#include "Templates/SubclassOf.h"
#include "Templates/Casts.h"
#include "MovieSceneFwd.h"
#include "MovieSceneSpawnable.h"
#include "MovieSceneBinding.h"
#include "MovieScenePossessable.h"
#include "MovieSceneSignedObject.h"
#include "MovieSceneSequenceID.h"
#include "MovieSceneObjectBindingID.h"
#include "MovieSceneFrameMigration.h"
#include "MovieSceneTimeController.h"
#include "MovieScene.generated.h"

class UMovieSceneFolder;
class UMovieSceneSection;
class UMovieSceneTrack;

/** @todo: remove this type when support for intrinsics on TMap values is added? */
USTRUCT()
struct FMovieSceneExpansionState
{
	GENERATED_BODY()

	FMovieSceneExpansionState(bool bInExpanded = true) : bExpanded(bInExpanded) {}

	UPROPERTY()
	bool bExpanded;
};

USTRUCT()
struct FMovieSceneTimecodeSource
{
	GENERATED_BODY()

	FMovieSceneTimecodeSource(FTimecode InTimecode)
		: Timecode(InTimecode)
		, DeltaFrame(FFrameNumber())
	{}

	FMovieSceneTimecodeSource()
		: Timecode(FTimecode())
		, DeltaFrame(FFrameNumber())
	{}

public:

	/** The global timecode at which this target is based (ie. the timecode at the beginning of the movie scene section when it was recorded) */
	UPROPERTY()
	FTimecode Timecode;

	/** The delta from the original placement of this target */
	UPROPERTY()
	FFrameNumber DeltaFrame;
};

/**
 * Editor only data that needs to be saved between sessions for editing but has no runtime purpose
 */
USTRUCT()
struct FMovieSceneEditorData
{
	GENERATED_USTRUCT_BODY()

	FMovieSceneEditorData()
		: ViewStart(0.0), ViewEnd(0.0)
		, WorkStart(0.0), WorkEnd(0.0)
	{}

	TRange<double> GetViewRange() const
	{
		return TRange<double>(ViewStart, ViewEnd);
	}

	TRange<double> GetWorkingRange() const
	{
		return TRange<double>(WorkStart, WorkEnd);
	}

	/** Map of node path -> expansion state. */
	UPROPERTY()
	TMap<FString, FMovieSceneExpansionState> ExpansionStates;

	/** The last view-range start that the user was observing */
	UPROPERTY()
	double ViewStart;
	/** The last view-range end that the user was observing */
	UPROPERTY()
	double ViewEnd;

	/** User-defined working range start in which the entire sequence should reside. */
	UPROPERTY()
	double WorkStart;
	/** User-defined working range end in which the entire sequence should reside. */
	UPROPERTY()
	double WorkEnd;

	/** The set of user-marked frames for display convenience */
	UPROPERTY()
	TSet<FFrameNumber> MarkedFrames;

	UPROPERTY()
	FFloatRange WorkingRange_DEPRECATED;
	UPROPERTY()
	FFloatRange ViewRange_DEPRECATED;
};


/**
 * Structure for labels that can be assigned to movie scene tracks.
 */
USTRUCT()
struct FMovieSceneTrackLabels
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FString> Strings;

	void FromString(const FString& LabelString)
	{
		LabelString.ParseIntoArray(Strings, TEXT(" "));
	}

	FString ToString() const
	{
		return FString::Join(Strings, TEXT(" "));
	}
};


/**
 * Implements a movie scene asset.
 */
UCLASS(DefaultToInstanced)
class MOVIESCENE_API UMovieScene
	: public UMovieSceneSignedObject
{
	GENERATED_UCLASS_BODY()

public:

	/**~ UObject implementation */
	virtual void Serialize( FArchive& Ar ) override;
	virtual bool IsPostLoadThreadSafe() const override;

public:

#if WITH_EDITOR
	/**
	 * Add a spawnable to this movie scene's list of owned blueprints.
	 *
	 * These objects are stored as "inners" of the MovieScene.
	 *
	 * @param Name Name of the spawnable.
	 * @param ObjectTemplate The object template to use for the spawnable
	 * @return Guid of the newly-added spawnable.
	 */
	FGuid AddSpawnable(const FString& Name, UObject& ObjectTemplate);

	/*
	 * Adds an existing spawnable to this movie scene.
	 *
	 * @param InNewSpawnable The posssesable to add.
	 * @param InNewBinding The object binding to add.
	 */
	void AddSpawnable(const FMovieSceneSpawnable& InNewSpawnable, const FMovieSceneBinding& InNewBinding);

	/**
	 * Removes a spawnable from this movie scene.
	 *
	 * @param Guid The guid of a spawnable to find and remove.
	 * @return true if anything was removed.
	 */
	bool RemoveSpawnable(const FGuid& Guid);

	/**
	 * Attempt to find a spawnable using some custom prodeicate
	 *
	 * @param InPredicate A predicate to test each spawnable against
	 * @return Spawnable object that was found (or nullptr if not found).
	 */
	FMovieSceneSpawnable* FindSpawnable( const TFunctionRef<bool(FMovieSceneSpawnable&)>& InPredicate );

#endif //WITH_EDITOR

	/**
	 * Tries to locate a spawnable in this MovieScene for the specified spawnable GUID.
	 *
	 * @param Guid The spawnable guid to search for.
	 * @return Spawnable object that was found (or nullptr if not found).
	 */
	FMovieSceneSpawnable* FindSpawnable(const FGuid& Guid);

	/**
	 * Grabs a reference to a specific spawnable by index.
	 *
	 * @param Index of spawnable to return. Must be between 0 and GetSpawnableCount()
	 * @return Returns the specified spawnable by index.
	 */
	FMovieSceneSpawnable& GetSpawnable(int32 Index);

	/**
	 * Get the number of spawnable objects in this scene.
	 *
	 * @return Spawnable object count.
	 */
	int32 GetSpawnableCount() const;
	
public:

	/**
	 * Adds a possessable to this movie scene.
	 *
	 * @param Name Name of the possessable.
	 * @param Class The class of object that will be possessed.
	 * @return Guid of the newly-added possessable.
	 */
	FGuid AddPossessable(const FString& Name, UClass* Class);

	/*
	 * Adds an existing possessable to this movie scene.
	 *
	 * @param InNewPossessable The posssesable to add.
	 * @param InNewBinding The object binding to add.
	 */
	void AddPossessable(const FMovieScenePossessable& InNewPossessable, const FMovieSceneBinding& InNewBinding);

	/**
	 * Removes a possessable from this movie scene.
	 *
	 * @param PossessableGuid Guid of possessable to remove.
	 */
	bool RemovePossessable(const FGuid& PossessableGuid);
	
	/*
	* Replace an existing possessable with another 
	*/
	bool ReplacePossessable(const FGuid& OldGuid, const FMovieScenePossessable& InNewPosessable);

	/**
	 * Tries to locate a possessable in this MovieScene for the specified possessable GUID.
	 *
	 * @param Guid The possessable guid to search for.
	 * @return Possessable object that was found (or nullptr if not found).
	 */
	struct FMovieScenePossessable* FindPossessable(const FGuid& Guid);

	/**
	 * Attempt to find a possessable using some custom prdeicate
	 *
	 * @param InPredicate A predicate to test each possessable against
	 * @return Possessable object that was found (or nullptr if not found).
	 */
	FMovieScenePossessable* FindPossessable( const TFunctionRef<bool(FMovieScenePossessable&)>& InPredicate );

	/**
	 * Grabs a reference to a specific possessable by index.
	 *
	 * @param Index of possessable to return.
	 * @return Returns the specified possessable by index.
	 */
	FMovieScenePossessable& GetPossessable(const int32 Index);

	/**
	 * Get the number of possessable objects in this scene.
	 *
	 * @return Possessable object count.
	 */
	int32 GetPossessableCount() const;

public:

	/**
	 * Adds a track.
	 *
	 * Note: The type should not already exist.
	 *
	 * @param TrackClass The class of the track to create.
	 * @param ObjectGuid The runtime object guid that the type should bind to.
	 * @param Type The newly created type.
	 * @see  FindTrack, RemoveTrack
	 */
	UMovieSceneTrack* AddTrack(TSubclassOf<UMovieSceneTrack> TrackClass, const FGuid& ObjectGuid);

	/**
	* Adds a given track.
	*
	* @param InTrack The track to add.
	* @param ObjectGuid The runtime object guid that the type should bind to
	* @see  FindTrack, RemoveTrack
	* @return true if the track is successfully added, false otherwise.
	*/
	bool AddGivenTrack(UMovieSceneTrack* InTrack, const FGuid& ObjectGuid);

	/**
	 * Adds a track.
	 *
	 * Note: The type should not already exist.
	 *
	 * @param TrackClass The class of the track to create.
	 * @param ObjectGuid The runtime object guid that the type should bind to.
	 * @param Type The newly created type.
	 * @see FindTrack, RemoveTrack
	 */
	template<typename TrackClass>
	TrackClass* AddTrack(const FGuid& ObjectGuid)
	{
		return Cast<TrackClass>(AddTrack(TrackClass::StaticClass(), ObjectGuid));
	}

	/**
	 * Finds a track.
	 *
	 * @param TrackClass The class of the track to find.
	 * @param ObjectGuid The runtime object guid that the track is bound to.
	 * @param TrackName The name of the track to differentiate the one we are searching for from other tracks of the same class (optional).
	 * @return The found track or nullptr if one does not exist.
	 * @see AddTrack, RemoveTrack
	 */
	UMovieSceneTrack* FindTrack(TSubclassOf<UMovieSceneTrack> TrackClass, const FGuid& ObjectGuid, const FName& TrackName = NAME_None) const;
	
	/**
	 * Finds a track.
	 *
	 * @param TrackClass The class of the track to find.
	 * @param ObjectGuid The runtime object guid that the track is bound to.
	 * @param TrackName The name of the track to differentiate the one we are searching for from other tracks of the same class (optional).
	 * @return The found track or nullptr if one does not exist.
	 * @see AddTrack, RemoveTrack
	 */
	template<typename TrackClass>
	TrackClass* FindTrack(const FGuid& ObjectGuid, const FName& TrackName = NAME_None) const
	{
		return Cast<TrackClass>(FindTrack(TrackClass::StaticClass(), ObjectGuid, TrackName));
	}

	/**
	 * Removes a track.
	 *
	 * @param Track The track to remove.
	 * @return true if anything was removed.
	 * @see AddTrack, FindTrack
	 */
	bool RemoveTrack(UMovieSceneTrack& Track);

	/**
	 * Find a track binding Guid from a UMovieSceneTrack
	 * 
	 * @param	InTrack		The track to find the binding for.
	 * @param	OutGuid		The binding's Guid if one was found.
	 * @return true if a binding was found for this track.
	 */
	bool FindTrackBinding(const UMovieSceneTrack& InTrack, FGuid& OutGuid) const;

public:

	/**
	 * Adds a master track.
	 *
	 * Note: The type should not already exist.
	 *
	 * @param TrackClass The class of the track to create
	 * @param Type	The newly created type
	 * @see FindMasterTrack, GetMasterTracks, IsMasterTrack, RemoveMasterTrack
	 */
	UMovieSceneTrack* AddMasterTrack(TSubclassOf<UMovieSceneTrack> TrackClass);
	
	/**
	 * Adds a master track.
	 *
	 * Note: The type should not already exist.
	 *
	 * @param TrackClass The class of the track to create
	 * @param Type	The newly created type
	 * @see FindMasterTrack, GetMasterTracks, IsMasterTrack, RemoveMasterTrack
	 */
	template<typename TrackClass>
	TrackClass* AddMasterTrack()
	{
		return Cast<TrackClass>(AddMasterTrack(TrackClass::StaticClass()));
	}

	/**
	* Adds a given track as a master track
	*
	* @param InTrack The track to add.
	* @see  FindTrack, RemoveTrack
	* @return true if the track is successfully added, false otherwise.
	*/
	bool AddGivenMasterTrack(UMovieSceneTrack* InTrack);

	/**
	 * Finds a master track (one not bound to a runtime objects).
	 *
	 * @param TrackClass The class of the track to find.
	 * @return The found track or nullptr if one does not exist.
	 * @see AddMasterTrack, GetMasterTracks, IsMasterTrack, RemoveMasterTrack
	 */
	UMovieSceneTrack* FindMasterTrack(TSubclassOf<UMovieSceneTrack> TrackClass) const;

	/**
	 * Finds a master track (one not bound to a runtime objects).
	 *
	 * @param TrackClass The class of the track to find.
	 * @return The found track or nullptr if one does not exist.
	 * @see AddMasterTrack, GetMasterTracks, IsMasterTrack, RemoveMasterTrack
	 */
	template<typename TrackClass>
	TrackClass* FindMasterTrack() const
	{
		return Cast<TrackClass>(FindMasterTrack(TrackClass::StaticClass()));
	}

	/**
	 * Get all master tracks.
	 *
	 * @return Track collection.
	 * @see AddMasterTrack, FindMasterTrack, IsMasterTrack, RemoveMasterTrack
	 */
	const TArray<UMovieSceneTrack*>& GetMasterTracks() const
	{
		return MasterTracks;
	}

	/**
	 * Check whether the specified track is a master track in this scene.
	 *
	 * @return true if the track is a master track, false otherwise.
	 * @see AddMasterTrack, FindMasterTrack, GetMasterTracks, RemoveMasterTrack
	 */
	bool IsAMasterTrack(const UMovieSceneTrack& Track) const;

	/**
	 * Removes a master track.
	 *
	 * @param Track The track to remove.
	 * @return true if anything was removed.
	 * @see AddMasterTrack, FindMasterTrack, GetMasterTracks, IsMasterTrack
	 */
	bool RemoveMasterTrack(UMovieSceneTrack& Track);

	/**
	 * Move all the contents (tracks, child bindings) of the specified binding ID onto another
	 *
	 * @param SourceBindingId The identifier of the binding ID to move all tracks and children from
	 * @param DestinationBindingId The identifier of the binding ID to move the contents to
	 */
	void MoveBindingContents(const FGuid& SourceBindingId, const FGuid& DestinationBindingId);

public:

	// @todo sequencer: the following methods really shouldn't be here

	/**
	 * Adds a new camera cut track if it doesn't exist 
	 * A camera cut track allows for cutting between camera views
	 * There is only one per movie scene. 
	 *
	 * @param TrackClass  The camera cut track class type
	 * @return The created camera cut track
	 */
	UMovieSceneTrack* AddCameraCutTrack( TSubclassOf<UMovieSceneTrack> TrackClass );
	
	/** @return The camera cut track if it exists. */
	UMovieSceneTrack* GetCameraCutTrack();

	/** Removes the camera cut track if it exists. */
	void RemoveCameraCutTrack();

	void SetCameraCutTrack(UMovieSceneTrack* Track);

public:

	/**
	 * Returns all sections and their associated binding data.
	 *
	 * @return A list of sections with object bindings and names.
	 */
	TArray<UMovieSceneSection*> GetAllSections() const;

	/**
	 * @return All object bindings.
	 */
	const TArray<FMovieSceneBinding>& GetBindings() const
	{
		return ObjectBindings;
	}

	/** Get the current selection range. */
	TRange<FFrameNumber> GetSelectionRange() const
	{
		return SelectionRange.Value;
	}

	/**
	 * Get the display name of the object with the specified identifier.
	 *
	 * @param ObjectId The object identifier.
	 * @return The object's display name.
	 */
	FText GetObjectDisplayName(const FGuid& ObjectId);

	/** Get the playback time range of this movie scene, relative to its 0-time offset. */
	TRange<FFrameNumber> GetPlaybackRange() const
	{
		return PlaybackRange.Value;
	}

	/**
	 * Retrieve the tick resolution at which all frame numbers within this movie scene are defined
	 */
	FFrameRate GetTickResolution() const
	{
		return TickResolution;
	}

	/**
	 * Directly set the tick resolution for this movie scene without applying any conversion whatsoever, or modifying the data
	 */
	void SetTickResolutionDirectly(FFrameRate InTickResolution)
	{
		TickResolution = InTickResolution;
	}

	/**
	 * Retrieve the display frame rate for this data, in which frame numbers should be displayed on UI, and interacted with in movie scene players
	 */
	FFrameRate GetDisplayRate() const
	{
		return DisplayRate;
	}

	/**
	 * Set the play rate for this movie scene
	 */
	void SetDisplayRate(FFrameRate InDisplayRate)
	{
		DisplayRate = InDisplayRate;
	}

	/**
	 * Retrieve a value signifying how to evaluate this movie scene data
	 */
	EMovieSceneEvaluationType GetEvaluationType() const
	{
		return EvaluationType;
	}

	/**
	 * Assign a value signifying how to evaluate this movie scene data
	 */
	void SetEvaluationType(EMovieSceneEvaluationType InNewEvaluationType)
	{
		EvaluationType = InNewEvaluationType;

		if (EvaluationType == EMovieSceneEvaluationType::FrameLocked && ClockSource == EUpdateClockSource::Tick)
		{
			ClockSource = EUpdateClockSource::Platform;
		}
	}

	/**
	 * retrieve the clock source to be used for this moviescene
	 */
	EUpdateClockSource GetClockSource() const
	{
		return ClockSource;
	}

	/**
	 * Assign the clock source to be used for this moviescene
	 */
	void SetClockSource(EUpdateClockSource InNewClockSource)
	{
		ClockSource = InNewClockSource;
	}

	/*
	* Replace an existing binding with another 
	*/
	void ReplaceBinding(const FGuid& OldGuid, const FGuid& NewGuid, const FString& Name);

	/*
	* Replace an existing binding with another. Assumes ownership of any
	* tracks listed in the binding. Does nothing if no binding can be found.
	* @param BindingToReplaceGuid	Binding Guid that should be replaced
	* @param NewBinding				Binding Data that should replace the original one specified by BindingToReplaceGuid.
	*/
	void ReplaceBinding(const FGuid& BindingToReplaceGuid, const FMovieSceneBinding& NewBinding);

#if WITH_EDITORONLY_DATA
	/**
	 */
	TMap<FString, FMovieSceneTrackLabels>& GetObjectsToLabels()
	{
		return ObjectsToLabels;
	}

	/** Set the selection range. */
	void SetSelectionRange(TRange<FFrameNumber> Range)
	{
		SelectionRange.Value = Range;
	}

	/**
	 * Get the display name of the object with the specified identifier.
	 *
	 * @param ObjectId The object identifier.
	 * @return The object's display name.
	 */
	void SetObjectDisplayName(const FGuid& ObjectId, const FText& DisplayName);

	/**
	 * Gets the root folders for this movie scene.
	 */
	TArray<UMovieSceneFolder*>& GetRootFolders();
#endif

	/**
	 * Set the start and end playback positions (playback range) for this movie scene
	 *
	 * @param Start The offset from 0-time to start playback of this movie scene
	 * @param Duration The number of frames the movie scene should play for
	 * @param bAlwaysMarkDirty Whether to always mark the playback range dirty when changing it. 
	 *        In the case where the playback range is dynamic and based on section bounds, the playback range doesn't need to be dirtied when set
	 */
	void SetPlaybackRange(FFrameNumber Start, int32 Duration, bool bAlwaysMarkDirty = true);

	/**
	 * Set the playback range for this movie scene
	 *
	 * @param Range The new playback range. Must not have any open bounds (ie must be a finite range)
	 * @param bAlwaysMarkDirty Whether to always mark the playback range dirty when changing it. 
	 *        In the case where the playback range is dynamic and based on section bounds, the playback range doesn't need to be dirtied when set
	 */
	void SetPlaybackRange(const TRange<FFrameNumber>& NewRange, bool bAlwaysMarkDirty = true);

	/**
	 * Set the start and end working range (outer) for this movie scene
	 *
	 * @param Start The offset from 0-time to view this movie scene.
	 * @param End The offset from 0-time to view this movie scene
	 */
	void SetWorkingRange(float Start, float End);

	/**
	 * Set the start and end view range (inner) for this movie scene
	 *
	 * @param Start The offset from 0-time to view this movie scene
	 * @param End The offset from 0-time to view this movie scene
	 */
	void SetViewRange(float Start, float End);

#if WITH_EDITORONLY_DATA

public:

	/*
	* @return Returns whether this movie scene is read only
	*/
	bool IsReadOnly() const { return bReadOnly; }

	/**
	* Set whether this movie scene is read only.
	*/
	void SetReadOnly(bool bInReadOnly) { bReadOnly = bInReadOnly; }

	/**
	* Return whether the playback range is locked.
	*/
	bool IsPlaybackRangeLocked() const;

	/**
	* Set whether the playback range is locked.
	*/
	void SetPlaybackRangeLocked(bool bLocked);

	/**
	 * @return The editor only data for use with this movie scene
	 */
	FMovieSceneEditorData& GetEditorData()
	{
		return EditorData;
	}

	void SetEditorData(FMovieSceneEditorData& InEditorData)
	{
		EditorData = InEditorData;
	}

	/** The timecode at which this movie scene section is based (ie. when it was recorded) */
	UPROPERTY()
	FMovieSceneTimecodeSource TimecodeSource;

#endif	// WITH_EDITORONLY_DATA

protected:

	/**
	 * Removes animation data bound to a GUID.
	 *
	 * @param Guid The guid bound to animation data to remove
	 */
	void RemoveBinding(const FGuid& Guid);

#if WITH_EDITOR
	/** Templated helper for optimizing lists of possessables and spawnables for cook */
	template<typename T>
	void OptimizeObjectArray(TArray<T>& ObjectArray);
#endif
	
protected:

	/** Called after this object has been deserialized */
	virtual void PostLoad() override;

	/** Called before this object is being deserialized. */
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;

	/** Perform legacy upgrade of time ranges */
	void UpgradeTimeRanges();

private:

	/**
	 * Data-only blueprints for all of the objects that we we're able to spawn.
	 * These describe objects and actors that we may instantiate at runtime,
	 * or create proxy objects for previewing in the editor.
	 */
	UPROPERTY()
	TArray<FMovieSceneSpawnable> Spawnables;

	/** Typed slots for already-spawned objects that we are able to control with this MovieScene */
	UPROPERTY()
	TArray<FMovieScenePossessable> Possessables;

	/** Tracks bound to possessed or spawned objects */
	UPROPERTY()
	TArray<FMovieSceneBinding> ObjectBindings;

	/** Master tracks which are not bound to spawned or possessed objects */
	UPROPERTY(Instanced)
	TArray<UMovieSceneTrack*> MasterTracks;

	/** The camera cut track is a specialized track for switching between cameras on a cinematic */
	UPROPERTY(Instanced)
	UMovieSceneTrack* CameraCutTrack;

	/** User-defined selection range. */
	UPROPERTY()
	FMovieSceneFrameRange SelectionRange;

	/** User-defined playback range for this movie scene. Must be a finite range. Relative to this movie-scene's 0-time origin. */
	UPROPERTY()
	FMovieSceneFrameRange PlaybackRange;

	/** The resolution at which all frame numbers within this movie-scene data are stored */
	UPROPERTY()
	FFrameRate TickResolution;

	/** The rate at which we should interact with this moviescene data on UI, and to movie scene players. Also defines the frame locked frame rate. */
	UPROPERTY()
	FFrameRate DisplayRate;

	/** The type of evaluation to use when playing back this sequence */
	UPROPERTY()
	EMovieSceneEvaluationType EvaluationType;

	UPROPERTY()
	EUpdateClockSource ClockSource;

#if WITH_EDITORONLY_DATA

	/** Indicates whether this movie scene is read only */
	UPROPERTY()
	bool bReadOnly;

	/** User-defined playback range is locked. */
	UPROPERTY()
	bool bPlaybackRangeLocked;

	/** Maps object GUIDs to user defined display names. */
	UPROPERTY()
	TMap<FString, FText> ObjectsToDisplayNames;

	/** Maps object GUIDs to user defined labels. */
	UPROPERTY()
	TMap<FString, FMovieSceneTrackLabels> ObjectsToLabels;

	/** Editor only data that needs to be saved between sessions for editing but has no runtime purpose */
	UPROPERTY()
	FMovieSceneEditorData EditorData;

	/** The root folders for this movie scene. */
	UPROPERTY()
	TArray<UMovieSceneFolder*> RootFolders;

private:

	UPROPERTY()
	float InTime_DEPRECATED;

	UPROPERTY()
	float OutTime_DEPRECATED;

	UPROPERTY()
	float StartTime_DEPRECATED;

	UPROPERTY()
	float EndTime_DEPRECATED;

	UPROPERTY()
	bool bForceFixedFrameIntervalPlayback_DEPRECATED;

	UPROPERTY()
	float FixedFrameInterval_DEPRECATED;

#endif
};
