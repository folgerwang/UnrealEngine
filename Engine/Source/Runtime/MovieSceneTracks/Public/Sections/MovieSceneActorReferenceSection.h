// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Curves/KeyHandle.h"
#include "MovieSceneObjectBindingID.h"
#include "MovieSceneSection.h"
#include "Curves/IntegralCurve.h"
#include "Channels/MovieSceneChannel.h"
#include "Channels/MovieSceneChannelData.h"
#include "Channels/MovieSceneChannelTraits.h"
#include "MovieSceneActorReferenceSection.generated.h"

USTRUCT()
struct FMovieSceneActorReferenceKey
{
	GENERATED_BODY()

	FMovieSceneActorReferenceKey()
	{}

	FMovieSceneActorReferenceKey(const FMovieSceneObjectBindingID& InBindingID)
		: Object(InBindingID)
	{}

	friend bool operator==(const FMovieSceneActorReferenceKey& A, const FMovieSceneActorReferenceKey& B)
	{
		return A.Object == B.Object;
	}

	friend bool operator!=(const FMovieSceneActorReferenceKey& A, const FMovieSceneActorReferenceKey& B)
	{
		return A.Object != B.Object;
	}

	UPROPERTY(EditAnywhere, Category="Key")
	FMovieSceneObjectBindingID Object;
};

/** A curve of events */
USTRUCT()
struct MOVIESCENETRACKS_API FMovieSceneActorReferenceData : public FMovieSceneChannel
{
	GENERATED_BODY()

	FMovieSceneActorReferenceData()
		: DefaultValue()
	{}

	/**
	 * Access a mutable interface for this channel's data
	 *
	 * @return An object that is able to manipulate this channel's data
	 */
	FORCEINLINE TMovieSceneChannelData<FMovieSceneActorReferenceKey> GetData()
	{
		return TMovieSceneChannelData<FMovieSceneActorReferenceKey>(&KeyTimes, &KeyValues, &KeyHandles);
	}

	/**
	 * Access a constant interface for this channel's data
	 *
	 * @return An object that is able to interrogate this channel's data
	 */
	FORCEINLINE TMovieSceneChannelData<const FMovieSceneActorReferenceKey> GetData() const
	{
		return TMovieSceneChannelData<const FMovieSceneActorReferenceKey>(&KeyTimes, &KeyValues);
	}

	/**
	 * Evaluate this channel
	 *
	 * @param InTime     The time to evaluate at
	 * @return the result of the evaluation
	 */
	FMovieSceneActorReferenceKey Evaluate(FFrameTime InTime) const;

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
	virtual void ClearDefault() override;

public:

	/**
	 * Set this channel's default value that should be used when no keys are present
	 *
	 * @param InDefaultValue The desired default value
	 */
	FORCEINLINE void SetDefault(FMovieSceneActorReferenceKey InDefaultValue)
	{
		DefaultValue = InDefaultValue;
	}

	/**
	 * Get this channel's default value that will be used when no keys are present
	 *
	 * @return (Optional) The channel's default value
	 */
	FORCEINLINE FMovieSceneActorReferenceKey GetDefault() const
	{
		return DefaultValue;
	}

	/**
	 * Upgrade legacy data by appending to the end of the array. Assumes sorted data
	 */
	void UpgradeLegacyTime(UObject* Context, double Time, FMovieSceneActorReferenceKey Value)
	{
		FFrameRate LegacyFrameRate = GetLegacyConversionFrameRate();
		FFrameNumber KeyTime = UpgradeLegacyMovieSceneTime(nullptr, LegacyFrameRate, Time);

		check(KeyTimes.Num() == 0 || KeyTime >= KeyTimes.Last());

		KeyTimes.Add(KeyTime);
		KeyValues.Add(Value);
	}

private:

	/** Sorted array of key times */
	UPROPERTY()
	TArray<FFrameNumber> KeyTimes;

	/** Default value used when there are no keys */
	FMovieSceneActorReferenceKey DefaultValue;

	/** Array of values that correspond to each key time */
	UPROPERTY()
	TArray<FMovieSceneActorReferenceKey> KeyValues;

	FMovieSceneKeyHandleMap KeyHandles;
};

/**
 * A single actor reference point section
 */
UCLASS(MinimalAPI)
class UMovieSceneActorReferenceSection
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:

	//~ UObject interface
	virtual void PostLoad() override;

	const FMovieSceneActorReferenceData& GetActorReferenceData() const { return ActorReferenceData; }

private:

	UPROPERTY()
	FMovieSceneActorReferenceData ActorReferenceData;

private:

	/** Curve data */
	UPROPERTY()
	FIntegralCurve ActorGuidIndexCurve_DEPRECATED;

	UPROPERTY()
	TArray<FString> ActorGuidStrings_DEPRECATED;
};

inline bool EvaluateChannel(const FMovieSceneActorReferenceData* InChannel, FFrameTime InTime, FMovieSceneActorReferenceKey& OutValue)
{
	OutValue = InChannel->Evaluate(InTime);
	return true;
}