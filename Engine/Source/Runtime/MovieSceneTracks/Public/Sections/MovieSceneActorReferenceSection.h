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
struct FMovieSceneActorReferenceData
{
	GENERATED_BODY()

	FMovieSceneActorReferenceData()
		: DefaultValue()
	{}

	/**
	 * Access an integer that uniquely identifies this channel type.
	 *
	 * @return A static identifier that was allocated using FMovieSceneChannelEntry::RegisterNewID
	 */
	MOVIESCENETRACKS_API static uint32 GetChannelID();

	/**
	 * Access a mutable interface for this channel
	 *
	 * @return An object that is able to manipulate this channel
	 */
	FORCEINLINE TMovieSceneChannel<FMovieSceneActorReferenceKey> GetInterface()
	{
		return TMovieSceneChannel<FMovieSceneActorReferenceKey>(&KeyTimes, &KeyValues, &KeyHandles);
	}

	/**
	 * Access a constant interface for this channel
	 *
	 * @return An object that is able to interrogate this channel
	 */
	FORCEINLINE TMovieSceneChannel<const FMovieSceneActorReferenceKey> GetInterface() const
	{
		return TMovieSceneChannel<const FMovieSceneActorReferenceKey>(&KeyTimes, &KeyValues);
	}

	/**
	 * Evaluate this channel
	 *
	 * @param InTime     The time to evaluate at
	 * @return the result of the evaluation
	 */
	MOVIESCENETRACKS_API FMovieSceneActorReferenceKey Evaluate(FFrameTime InTime) const;

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

inline void ClearChannelDefault(FMovieSceneActorReferenceData* InChannel)
{}
