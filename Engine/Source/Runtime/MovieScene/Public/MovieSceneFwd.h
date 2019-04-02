// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Misc/FrameRate.h"
#include "MovieSceneSequenceID.h"

UENUM()
namespace EMovieScenePlayerStatus
{
	enum Type
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
}

UENUM()
enum class EMovieSceneEvaluationType : uint8
{
	/** Play the sequence frame-locked to its playback rate (snapped to the tick resolution - no sub-frames) */
	FrameLocked,

	/** Play the sequence in real-time, with sub-frame interpolation if necessary */
	WithSubFrames,
};

/**
 * Enum used to define how to update to a particular time
 */
UENUM()
enum class EUpdateClockSource : uint8
{
	/** Use the default world tick delta for timing. Honors world and actor pause state, but is susceptible to accumulation errors */
	Tick,

	/** Use the platform clock for timing. Does not honor world or actor pause state. */
	Platform,

	/** Use the audio clock for timing. Does not honor world or actor pause state. */
	Audio,

	/** Use current timecode provider for timing. Does not honor world or actor pause state. */
	Timecode,
};

MOVIESCENE_API DECLARE_LOG_CATEGORY_EXTERN(LogMovieScene, Log, All);
DECLARE_STATS_GROUP(TEXT("Movie Scene Evaluation"), STATGROUP_MovieSceneEval, STATCAT_Advanced);

MOVIESCENE_API FFrameRate GetLegacyConversionFrameRate();
MOVIESCENE_API void EmitLegacyOutOfBoundsError(UObject* ErrorContext, FFrameRate InFrameRate, double InTime);
MOVIESCENE_API FFrameNumber UpgradeLegacyMovieSceneTime(UObject* ErrorContext, FFrameRate InFrameRate, double InTime);

#ifndef MOVIESCENE_DETAILED_STATS
	#define MOVIESCENE_DETAILED_STATS 0
#endif

#if MOVIESCENE_DETAILED_STATS
	#define MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER SCOPE_CYCLE_COUNTER
#else
	#define MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(...)
#endif
