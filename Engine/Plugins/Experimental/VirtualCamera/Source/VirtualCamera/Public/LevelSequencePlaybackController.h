// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
	#include "ISequenceRecorder.h"
	#include "SequenceRecorderSettings.h"
#endif //WITH_EDITOR

#include "LevelSequencePlayer.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "IAssetRegistry.h"
#include "LevelSequencePlaybackController.generated.h"

DECLARE_DELEGATE_OneParam(FRecordEnabledStateChanged, bool)

USTRUCT(BlueprintType)
struct FLevelSequenceData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Sequence Info")
	FString AssetPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Sequence Info")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Sequence Info")
	FDateTime LastEdited;

	FLevelSequenceData(const FString InAssetPath = "", const FString InDisplayName = "", const FDateTime InLastEdited = FDateTime::Now())
	{
		AssetPath = InAssetPath;
		DisplayName = InDisplayName;
		LastEdited = InLastEdited;
	}
};

UCLASS()
class VIRTUALCAMERA_API ULevelSequencePlaybackController : public ULevelSequencePlayer
{
	GENERATED_UCLASS_BODY()

public:
	/** Notify whether recording is enabled or disabled for the current sequence*/
	FRecordEnabledStateChanged OnRecordEnabledStateChanged;

	/** Tracks whether or not pawn is currently recording */
	bool bIsRecording;

	/**
	 * Records current movement into a new take sequence.
	 */
	void StartRecording();

	/**
	 * Stops recording and cleans up scene from temporary actors only needed when recording.
	 */
	UFUNCTION()
	void StopRecording();

	/**
	 * Plays current level sequence from the current time.
	 */
	void ResumeLevelSequencePlay();

	/**
	 * Returns the names of each level sequence actor that is present in the level.
	 * @param &OutLevelSequenceNames - UponReturn, array will contain all level sequence names.
	 */
	void GetLevelSequences(TArray<FLevelSequenceData>& OutLevelSequenceNames);

	/**
	 * Returns the asset name of the currently selected sequence
	 * @return the name of the crrent selected sequence; returns empty string if no selected sequence
	 */
	FString GetActiveLevelSequenceName() const;

	/**
	 * Changes the active level sequence to a new level sequence.
	 * @param LevelSequencePath - The name of the level sequence to select
	 * @return true if a valid level sequence player was found, false if no level sequence player is currently available
	 */
	bool SetActiveLevelSequence(const FString& LevelSequencePath);

	/**
	 * Clears the current level sequence player, needed when recording clean takes of something.
	 */
	void ClearActiveLevelSequence();

	/**
	 * Pilot the controlled camera during recording, copying over settings from the pawn.
	 * @param FilmbackSettings - Optional override for filmback settings 
	 */
	void PilotTargetedCamera(FCameraFilmbackSettings* FilmbackSettingsOverride = nullptr);

	/**
	 * Set the target camera component we need to be tracking during recording
	 * @param NewCameraCompToFollow - A pointer to the camera component whose movement and settings should be recorded
	 */
	void SetCameraComponentToFollow(UCineCameraComponent* NewCameraCompToFollow) { CameraToFollow = NewCameraCompToFollow;  }

	/**
	 * Plays current level sequence from beginning.
	 */
	void PlayFromBeginning();

	/**
	 * Ensure the sequence recorder settings contain crucial values for recording
	 */
	void SetupSequenceRecorderSettings(const TArray<FName>& RequiredSettings);

	/**
	 * Get the frame rate at which Sequencer Recorder will record
	 * @return the record rate in frames per second
	 */
	float GetCurrentRecordingFrameRate() const;

	/**
	 * Get the current amount of time that this sequence has been recording
	 * @return - The length of time in seconds that this sequence has been recording
	 */
	float GetCurrentRecordingLength() const;

	/**
	 * Get the scene name of the sequence that will be created by the next or current recording
	 * @return - The name of the current or next sequence that will be recorded
	 */
	FString GetCurrentRecordingSceneName();

	/**
	 * Get the take number of the sequence that will be created by the next or current recording
	 * @return - The name of the current or next sequence that will be recorded
	 */
	FString GetCurrentRecordingTakeName() const;

protected:
	/** Whether the sequence is reversed */
	bool bIsReversed;

	/** Used to detect when the sequence name changes so we can do updates */
	FString CachedSequenceName;

	/** Reference to the camera being driven while we record */
	ACineCameraActor* TargetCamera;

	/** The camera component that our dummy camera should mirror */
	UCineCameraComponent* CameraToFollow;

	/** Pointer to the sequence recorder module */
	IAssetRegistry* AssetRegistry;

#if WITH_EDITOR
	/** Pointer to the sequence recorder module */
	ISequenceRecorder* Recorder;

	/** The UE4 Sequence Recorder Settings */
	USequenceRecorderSettings* RecorderSettings;
#endif //WITH_EDITOR

	/** The take number of the next take to render */
	int32 NextTakeNumber;

	/**
	 * Play to the end of the current sequence and stop
	 */
	void PlayToEnd();

	/**
	 * Update the next take number to use
	 */
	void UpdateNextTakeNumber();

	/**
	 * Updates the camera bindings for the target camera in Sequence Recorder
	 */
	void SetupTargetCamera();
};
