// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "IMovieScenePlayer.h"
#include "Evaluation/MovieScenePlayback.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Evaluation/PersistentEvaluationData.h"
#include "MovieSceneSequencePlayer.h"
#include "Misc/QualifiedFrameTime.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.generated.h"

class AActor;
class ALevelSequenceActor;
class FLevelSequenceSpawnRegister;
class FViewportClient;
class UCameraComponent;

struct UE_DEPRECATED(4.15, "Please use FMovieSceneSequencePlaybackSettings.") FLevelSequencePlaybackSettings
	: public FMovieSceneSequencePlaybackSettings
{};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLevelSequencePlayerCameraCutEvent, UCameraComponent*, CameraComponent);

USTRUCT(BlueprintType)
struct FLevelSequenceSnapshotSettings
{
	GENERATED_BODY()

	FLevelSequenceSnapshotSettings()
		: ZeroPadAmount(4), FrameRate(30, 1)
	{}

	FLevelSequenceSnapshotSettings(int32 InZeroPadAmount, FFrameRate InFrameRate)
		: ZeroPadAmount(InZeroPadAmount), FrameRate(InFrameRate)
	{}

	/** Zero pad frames */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	uint8 ZeroPadAmount;

	/** Playback framerate */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	FFrameRate FrameRate;
};

/**
 * Frame snapshot information for a level sequence
 */
USTRUCT(BlueprintType)
struct FLevelSequencePlayerSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	FString MasterName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	FQualifiedFrameTime MasterTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	FQualifiedFrameTime SourceTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	FString CurrentShotName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	FQualifiedFrameTime CurrentShotLocalTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	FQualifiedFrameTime CurrentShotSourceTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	FString SourceTimecode;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	UCameraComponent* CameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "General")
	FLevelSequenceSnapshotSettings Settings;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "General")
	ULevelSequence* ActiveShot;

	UPROPERTY()
	FMovieSceneSequenceID ShotID;
};

/**
 * ULevelSequencePlayer is used to actually "play" an level sequence asset at runtime.
 *
 * This class keeps track of playback state and provides functions for manipulating
 * an level sequence while its playing.
 */
UCLASS(BlueprintType)
class LEVELSEQUENCE_API ULevelSequencePlayer
	: public UMovieSceneSequencePlayer
{
public:
	ULevelSequencePlayer(const FObjectInitializer&);

	GENERATED_BODY()

	/**
	 * Initialize the player.
	 *
	 * @param InLevelSequence The level sequence to play.
	 * @param InLevel The level that the animation is played in.
	 * @param Settings The desired playback settings
	 */
	void Initialize(ULevelSequence* InLevelSequence, ULevel* InLevel, const FMovieSceneSequencePlaybackSettings& Settings);

public:

	/**
	 * Create a new level sequence player.
	 *
	 * @param WorldContextObject Context object from which to retrieve a UWorld.
	 * @param LevelSequence The level sequence to play.
	 * @param Settings The desired playback settings
	 * @param OutActor The level sequence actor created to play this sequence.
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic", meta=(WorldContext="WorldContextObject", DynamicOutputParam="OutActor"))
	static ULevelSequencePlayer* CreateLevelSequencePlayer(UObject* WorldContextObject, ULevelSequence* LevelSequence, FMovieSceneSequencePlaybackSettings Settings, ALevelSequenceActor*& OutActor);

	/** Set the settings used to capture snapshots with */
	void SetSnapshotSettings(const FLevelSequenceSnapshotSettings& InSettings) { SnapshotSettings = InSettings; }

	/** Event triggered when there is a camera cut */
	UPROPERTY(BlueprintAssignable, Category="Game|Cinematic")
	FOnLevelSequencePlayerCameraCutEvent OnCameraCut;

	/** Get the active camera cut camera */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	UCameraComponent* GetActiveCameraComponent() const { return CachedCameraComponent.Get(); }

public:

	/**
	 * Access the level sequence this player is playing
	 * @return the level sequence currently assigned to this player
	 */
	UE_DEPRECATED(4.15, "Please use GetSequence instead.")
	ULevelSequence* GetLevelSequence() const { return Cast<ULevelSequence>(Sequence); }

	// IMovieScenePlayer interface
	virtual UObject* GetPlaybackContext() const override;
	virtual TArray<UObject*> GetEventContexts() const override;

protected:

	// IMovieScenePlayer interface
	virtual void UpdateCameraCut(UObject* CameraObject, UObject* UnlockIfCameraObject, bool bJumpCut) override;
	virtual void NotifyBindingUpdate(const FGuid& InGuid, FMovieSceneSequenceIDRef InSequenceID, TArrayView<TWeakObjectPtr<>> Objects) override;
	virtual void ResolveBoundObjects(const FGuid& InBindingId, FMovieSceneSequenceID SequenceID, UMovieSceneSequence& InSequence, UObject* ResolutionContext, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const override;

	//~ UMovieSceneSequencePlayer interface
	virtual bool CanPlay() const override;
	virtual void OnStartedPlaying() override;
	virtual void OnStopped() override;
	virtual void UpdateMovieSceneInstance(FMovieSceneEvaluationRange InRange, EMovieScenePlayerStatus::Type PlayerStatus, bool bHasJumped = false) override;

public:

	/** Populate the specified array with any given event contexts for the specified world */
	static void GetEventContexts(UWorld& InWorld, TArray<UObject*>& OutContexts);

	/** Take a snapshot of the current state of this player */
	void TakeFrameSnapshot(FLevelSequencePlayerSnapshot& OutSnapshot) const;

	/** Set the offset time for the snapshot in play rate frames. */
	void SetSnapshotOffsetFrames(int32 InFrameOffset) { SnapshotOffsetTime = TOptional<int32>(InFrameOffset); }

private:

	void EnableCinematicMode(bool bEnable);

private:

	/** The world this player will spawn actors in, if needed */
	TWeakObjectPtr<UWorld> World;

	/** The world this player will spawn actors in, if needed */
	TWeakObjectPtr<ULevel> Level;

	/** The full asset path (/Game/Folder/MapName.MapName) of the streaming level this player resides within. Bindings to actors with the same FSoftObjectPath::GetAssetPathName are resolved within the cached level, rather than globally.. */
	FName StreamedLevelAssetPath;

	/** The last view target to reset to when updating camera cuts to null */
	TWeakObjectPtr<AActor> LastViewTarget;

	/** The last aspect ratio axis constraint to reset to when the camera cut is null */
	EAspectRatioAxisConstraint LastAspectRatioAxisConstraint;

protected:

	/** How to take snapshots */
	FLevelSequenceSnapshotSettings SnapshotSettings;

	TOptional<int32> SnapshotOffsetTime;

	TWeakObjectPtr<UCameraComponent> CachedCameraComponent;

	/** Set of actors that have been added as tick prerequisites to the parent actor */
	TSet<FObjectKey> PrerequisiteActors;

private:

	TOptional<FLevelSequencePlayerSnapshot> PreviousSnapshot;
};
