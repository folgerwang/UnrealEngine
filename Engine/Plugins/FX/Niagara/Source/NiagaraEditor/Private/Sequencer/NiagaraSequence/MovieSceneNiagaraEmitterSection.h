// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraEmitter.h"
#include "MovieSceneSection.h"
#include "MovieSceneKeyStruct.h"
#include "MovieSceneClipboard.h"
#include "Channels/MovieSceneChannel.h"
#include "FrameNumber.h"
#include "MovieSceneNiagaraEmitterSection.generated.h"

class FNiagaraEmitterHandleViewModel;

/** Defines data for burst keys in this emitter section. */
USTRUCT()
struct FMovieSceneBurstKey
{
	GENERATED_BODY()

	FMovieSceneBurstKey()
		: TimeRange(0)
		, SpawnMinimum(0)
		, SpawnMaximum(0)
	{
	}

	friend bool operator==(const FMovieSceneBurstKey& A, const FMovieSceneBurstKey& B)
	{
		return A.TimeRange == B.TimeRange && A.SpawnMinimum == B.SpawnMinimum && A.SpawnMaximum == B.SpawnMaximum;
	}

	friend bool operator!=(const FMovieSceneBurstKey& A, const FMovieSceneBurstKey& B)
	{
		return A.TimeRange != B.TimeRange || A.SpawnMinimum != B.SpawnMinimum || A.SpawnMaximum != B.SpawnMaximum;
	}

	/** The time range which will be used around the key time which is used for randomly bursting. */
	UPROPERTY(EditAnywhere, Category = "Burst")
	FFrameNumber TimeRange;

	/** The minimum number of particles to spawn with this burst. */
	UPROPERTY(EditAnywhere, Category = "Burst")
	uint32 SpawnMinimum;

	/** The maximum number of particles to span with this burst. */
	UPROPERTY(EditAnywhere, Category = "Burst")
	uint32 SpawnMaximum;
};



namespace MovieSceneClipboard
{
	template<> inline FName GetKeyTypeName<FMovieSceneBurstKey>()
	{
		return "FMovieSceneBurstKey";
	}
}

USTRUCT()
struct FMovieSceneNiagaraEmitterChannel
{
	GENERATED_BODY()

	/**
	 * Access an integer that uniquely identifies this channel type.
	 *
	 * @return A static identifier that was allocated using FMovieSceneChannelEntry::RegisterNewID
	 */
	NIAGARAEDITOR_API static uint32 GetChannelID();

	/**
	 * Access a mutable interface for this channel
	 *
	 * @return An object that is able to manipulate this channel
	 */
	FORCEINLINE TMovieSceneChannel<FMovieSceneBurstKey> GetInterface()
	{
		return TMovieSceneChannel<FMovieSceneBurstKey>(&Times, &Values, &KeyHandles);
	}

	/**
	 * Access a constant interface for this channel
	 *
	 * @return An object that is able to interrogate this channel
	 */
	FORCEINLINE TMovieSceneChannel<const FMovieSceneBurstKey> GetInterface() const
	{
		return TMovieSceneChannel<const FMovieSceneBurstKey>(&Times, &Values);
	}

	/**
	 * Const access to this channel's times
	 */
	FORCEINLINE TArrayView<const FFrameNumber> GetTimes() const
	{
		return Times;
	}

	/**
	 * Const access to this channel's values
	 */
	FORCEINLINE TArrayView<const FMovieSceneBurstKey> GetValues() const
	{
		return Values;
	}

	/**
	 * Evaluate this channel
	 *
	 * @param InTime     The time to evaluate at
	 * @param OutValue   A value to receive the result
	 * @return true if the channel was evaluated successfully, false otherwise
	 */
	bool Evaluate(FFrameTime InTime, FMovieSceneBurstKey& OutValue) const;

private:
	/** Storage for the key times in the burst curve. */
	TArray<FFrameNumber> Times;

	/** Storage for the burst value times in the burst curve. */
	TArray<FMovieSceneBurstKey> Values;

	FMovieSceneKeyHandleMap KeyHandles;
};

/** Stub out unnecessary functions */
inline void Optimize(FMovieSceneNiagaraEmitterChannel* InChannel, const FKeyDataOptimizationParams& Params)
{}
inline void SetChannelDefault(FMovieSceneNiagaraEmitterChannel* Channel, const FMovieSceneBurstKey& DefaultValue)
{}
inline void ClearChannelDefault(FMovieSceneNiagaraEmitterChannel* InChannel)
{}

/** 
 *	Niagara editor movie scene section; represents one emitter in the timeline
 */
UCLASS()
class UMovieSceneNiagaraEmitterSection : public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()
public:

	/** Gets the emitter handle for the emitter which this section represents. */
	TSharedPtr<FNiagaraEmitterHandleViewModel> GetEmitterHandle();

	/** Sets the emitter handle for the emitter which this section represents. */
	void SetEmitterHandle(TSharedRef<FNiagaraEmitterHandleViewModel> InEmitterHandleViewModel);

private:
	/** The view model for the handle to the emitter this section represents. */
	TWeakPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel;

	/** Channel data */
	FMovieSceneNiagaraEmitterChannel Channel;
};