// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Misc/FrameNumber.h"
#include "UObject/ObjectMacros.h"
#include "Channels/MovieSceneChannel.h"
#include "Channels/MovieSceneChannelData.h"
#include "Channels/MovieSceneChannelTraits.h"
#include "MovieSceneStringChannel.generated.h"

USTRUCT()
struct FMovieSceneStringChannel : public FMovieSceneChannel
{
	GENERATED_BODY()

	FMovieSceneStringChannel()
		: DefaultValue(), bHasDefaultValue(false)
	{}

	/**
	 * Serialize this type from another
	 */
	MOVIESCENETRACKS_API bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FArchive& Ar);

	/**
	 * Access a mutable interface for this channel's data
	 *
	 * @return An object that is able to manipulate this channel's data
	 */
	FORCEINLINE TMovieSceneChannelData<FString> GetData()
	{
		return TMovieSceneChannelData<FString>(&Times, &Values, &KeyHandles);
	}

	/**
	 * Access a constant interface for this channel's data
	 *
	 * @return An object that is able to interrogate this channel's data
	 */
	FORCEINLINE TMovieSceneChannelData<const FString> GetData() const
	{
		return TMovieSceneChannelData<const FString>(&Times, &Values);
	}

	/**
	 * Evaluate this channel
	 *
	 * @param InTime     The time to evaluate at
	 * @return A pointer to the string, or nullptr
	 */
	MOVIESCENETRACKS_API const FString* Evaluate(FFrameTime InTime) const;

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
	virtual void Optimize(const FKeyDataOptimizationParams& InParameters) override;
	virtual void ClearDefault() override;

public:

	/**
	 * Set this channel's default value that should be used when no keys are present
	 *
	 * @param InDefaultValue The desired default value
	 */
	FORCEINLINE void SetDefault(FString InDefaultValue)
	{
		bHasDefaultValue = true;
		DefaultValue = InDefaultValue;
	}

	/**
	 * Get this channel's default value that will be used when no keys are present
	 *
	 * @return (Optional) The channel's default value
	 */
	FORCEINLINE TOptional<FString> GetDefault() const
	{
		return bHasDefaultValue ? TOptional<FString>(DefaultValue) : TOptional<FString>();
	}

	/**
	 * Remove this channel's default value causing the channel to have no effect where no keys are present
	 */
	FORCEINLINE void RemoveDefault()
	{
		bHasDefaultValue = false;
	}

private:

	UPROPERTY()
	TArray<FFrameNumber> Times;

	/** Array of values that correspond to each key time */
	UPROPERTY()
	TArray<FString> Values;

	/** Default value used when there are no keys */
	UPROPERTY()
	FString DefaultValue;

	/** */
	UPROPERTY()
	bool bHasDefaultValue;

	FMovieSceneKeyHandleMap KeyHandles;
};

template<>
struct TStructOpsTypeTraits<FMovieSceneStringChannel> : public TStructOpsTypeTraitsBase2<FMovieSceneStringChannel>
{
	enum { WithSerializeFromMismatchedTag = true };
};


template<>
struct TMovieSceneChannelTraits<FMovieSceneStringChannel> : TMovieSceneChannelTraitsBase<FMovieSceneStringChannel>
{
#if WITH_EDITOR

	/** String channels can have external values (ie, they can get their values from external objects for UI purposes) */
	typedef TMovieSceneExternalValue<FString> ExtendedEditorDataType;

#endif
};

inline bool EvaluateChannel(const FMovieSceneStringChannel* InChannel, FFrameTime InTime, FString& OutValue)
{
	if (const FString* Result = InChannel->Evaluate(InTime))
	{
		OutValue = *Result;
		return true;
	}
	return false;
}

inline bool ValueExistsAtTime(const FMovieSceneStringChannel* Channel, FFrameNumber InFrameNumber, const FString& Value)
{
	const FFrameTime FrameTime(InFrameNumber);

	const FString* ExistingValue = Channel->Evaluate(FrameTime);
	return ExistingValue && Value == *ExistingValue;
}

