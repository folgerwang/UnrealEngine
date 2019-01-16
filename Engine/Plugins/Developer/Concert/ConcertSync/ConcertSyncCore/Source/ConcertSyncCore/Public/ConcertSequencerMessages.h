// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Misc/QualifiedFrameTime.h"
#include "ConcertSequencerMessages.generated.h"


/**
 * Enum for the current Sequencer player status, should match EMovieScenePlayerStatus::Type
 * Defined here to not have a dependency on the MovieScene module.
 */
UENUM()
enum class EConcertMovieScenePlayerStatus : uint8
{
	Stopped,
	Playing,
	Recording,
	Scrubbing,
	Jumping,
	Stepping,
	Paused,
	MAX
};

USTRUCT()
struct FConcertSequencerState
{
	GENERATED_BODY()

	/** The full path name to the root sequence that is open on the sequencer */
	UPROPERTY()
	FString SequenceObjectPath;

	/** The time that the sequence is at */
	UPROPERTY()
	FQualifiedFrameTime Time;

	/** The current status of the sequencer player */
	UPROPERTY()
	EConcertMovieScenePlayerStatus PlayerStatus;

	/** The current playback speed */
	UPROPERTY()
	float PlaybackSpeed;

	FConcertSequencerState()
		: PlayerStatus(EConcertMovieScenePlayerStatus::Stopped)
		, PlaybackSpeed(1.0f)
	{}
};

/**
 * Event that signals a Sequencer just been opened.
 */
USTRUCT()
struct FConcertSequencerOpenEvent
{
	GENERATED_BODY()

	/** The full path name to the root sequence of the sequencer that just opened. */
	UPROPERTY()
	FString SequenceObjectPath;
};

/**
 * Event that signals a Sequencer just been closed.
 */
USTRUCT()
struct FConcertSequencerCloseEvent
{
	GENERATED_BODY()

	/** The full path name to the root sequence of the sequencer that just closed. */
	UPROPERTY()
	FString SequenceObjectPath;

	UPROPERTY()
	bool bMasterClose;
};

/**
 * Event that signals a sequencer UI has changed the current state
 */
USTRUCT()
struct FConcertSequencerStateEvent
{
	GENERATED_BODY()

	/** The new state that the sequence is at */
	UPROPERTY()
	FConcertSequencerState State;
};

/**
 * Event that represent the current open sequencer states to a newly connected client
 */
USTRUCT()
struct FConcertSequencerStateSyncEvent
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FConcertSequencerState> SequencerStates;
};
