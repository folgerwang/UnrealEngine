// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraEmitter.h"
#include "MovieSceneSection.h"
#include "MovieSceneKeyStruct.h"
#include "MovieSceneClipboard.h"
#include "Channels/MovieSceneChannel.h"
#include "Channels/MovieSceneChannelData.h"
#include "Misc/FrameNumber.h"
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
struct FMovieSceneNiagaraEmitterChannel : public FMovieSceneChannel
{
	GENERATED_BODY()

	/**
	 * Access a mutable interface for this channel
	 *
	 * @return An object that is able to manipulate this channel
	 */
	FORCEINLINE TMovieSceneChannelData<FMovieSceneBurstKey> GetData()
	{
		return TMovieSceneChannelData<FMovieSceneBurstKey>(&Times, &Values, &KeyHandles);
	}

	/**
	 * Access a constant interface for this channel
	 *
	 * @return An object that is able to interrogate this channel
	 */
	FORCEINLINE TMovieSceneChannelData<const FMovieSceneBurstKey> GetData() const
	{
		return TMovieSceneChannelData<const FMovieSceneBurstKey>(&Times, &Values);
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

public:

	// ~ FMovieSceneChannel Interface
	virtual void GetKeys(const TRange<FFrameNumber>& WithinRange, TArray<FFrameNumber>* OutKeyTimes, TArray<FKeyHandle>* OutKeyHandles) override;
	virtual void GetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<FFrameNumber> OutKeyTimes) override;
	virtual void SetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<const FFrameNumber> InKeyTimes) override;
	virtual void DuplicateKeys(TArrayView<const FKeyHandle> InHandles, TArrayView<FKeyHandle> OutNewHandles) override;
	virtual void DeleteKeys(TArrayView<const FKeyHandle> InHandles) override;
	virtual void ChangeFrameResolution(FFrameRate SourceRate, FFrameRate DestinationRate) override;
	virtual TRange<FFrameNumber> ComputeEffectiveRange() const override;
	virtual int32 GetNumKeys() const override;
	virtual void Reset() override;
	virtual void Offset(FFrameNumber DeltaPosition) override;
	virtual void Optimize(const FKeyDataOptimizationParams& InParameters) override {}
	virtual void ClearDefault() override {}

private:
	/** Storage for the key times in the burst curve. */
	TArray<FFrameNumber> Times;

	/** Storage for the burst value times in the burst curve. */
	TArray<FMovieSceneBurstKey> Values;

	FMovieSceneKeyHandleMap KeyHandles;
};


template<>
struct TMovieSceneChannelTraits<FMovieSceneNiagaraEmitterChannel> : TMovieSceneChannelTraitsBase<FMovieSceneNiagaraEmitterChannel>
{
	enum { SupportsDefaults = false };
};

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