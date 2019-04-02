// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "CoreMinimal.h"
#include "Misc/FrameNumber.h"
#include "Misc/FrameRate.h"
#include "Misc/Timecode.h"

#if WITH_EDITOR
#include "ISequencer.h"
#endif

#include "LevelSequencePlaybackController.generated.h"


class ULevelSequence;


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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Sequence Info", meta = (IgnoreForMemberInitializationTest))
	FDateTime LastEdited;

	FLevelSequenceData(const FString InAssetPath = "", const FString InDisplayName = "", const FDateTime InLastEdited = FDateTime::Now())
	{
		AssetPath = InAssetPath;
		DisplayName = InDisplayName;
		LastEdited = InLastEdited;
	}
};

UCLASS()
class VIRTUALCAMERA_API ULevelSequencePlaybackController : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/** Notify whether recording is enabled or disabled for the current sequence*/
	FRecordEnabledStateChanged OnRecordEnabledStateChanged;

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
	 * @return the name of the currently selected sequence; returns empty string if no selected sequence
	 */
	FString GetActiveLevelSequenceName() const;

	/**
	 * @return the currently selected LevelSequence
	 */
	ULevelSequence* GetActiveLevelSequence() { return ActiveLevelSequence; }

	/**
	 * @return the FrameRate of the currently loaded sequence FrameRate
	 */
	FFrameRate GetCurrentSequenceFrameRate() const;

	/**
	 * @return true if the active Sequencer is locked to camera cut
	 */
	bool IsSequencerLockedToCameraCut() const;

	/**
	 * Sets the current Sequencer perspective to be locked to camera cut
	 */
	void SetSequencerLockedToCameraCut(bool bLockView);

	/**
	 * @return The FrameNumber of the sequence's start.
	 */
	FFrameNumber GetCurrentSequencePlaybackStart() const;

	/**
	 * @return The FrameNumber of the sequence's end.
	 */
	FFrameNumber GetCurrentSequencePlaybackEnd() const;

	/**
	 * @return the duration of the sequence in FrameNumber
	 */
	FFrameNumber GetCurrentSequenceDuration() const;

	/**
	 * @return the current FrameTime of the sequence playback.
	 */
	FFrameTime GetCurrentSequencePlaybackPosition() const;

	/**
	 * @return The current Timecode of the sequence playback.
	 */
	FTimecode GetCurrentSequencePlaybackTimecode() const;

	/**
	 * Moves the current sequence to a desired playback position
	 */
	void JumpToPlaybackPosition(const FFrameNumber& InFrameNumber);

	/**
	 * @return true if a valid LevelSequence is being played.
	 */
	bool IsSequencePlaybackActive() const;

	/**
	 * Pause the currently active sequence 
	 */
	void PauseLevelSequence();

	/**
	 * Starts playing the currently active sequence
	 */
	void PlayLevelSequence();

	/**
	 * Starts playing the currently active sequence in reverse
	 */
	void PlayLevelSequenceReverse();

	/**
	 * Stops playing the currently active sequence
	 */
	void StopLevelSequencePlay();

	/**
	 * Changes the active level sequence to a new level sequence.
	 * @param InNewLevelSequence - The LevelSequence to select
	 * @return true if a valid LevelSequence was passed and sequencer was successfully found.
	 */
	bool SetActiveLevelSequence(ULevelSequence* InNewLevelSequence);

	/**
	 * Clears the current level sequence player, needed when recording clean takes of something.
	 */
	void ClearActiveLevelSequence();

	/**
	 * Plays current level sequence from beginning.
	 */
	void PlayFromBeginning();

protected:

	/** The sequence to play back */
	UPROPERTY(Transient)
	ULevelSequence* ActiveLevelSequence;

#if WITH_EDITORONLY_DATA
	/** Weak reference to Sequencer associated with the active LevelSequence */
	TWeakPtr<ISequencer> WeakSequencer;
#endif //WITH_EDITORONLY_DATA

	/**
	 * Play to the end of the current sequence and stop
	 */
	void PlayToEnd();
};
