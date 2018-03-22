// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "FrameNumber.h"
#include "UObject/ObjectMacros.h"
#include "Channels/MovieSceneChannelTraits.h"
#include "Channels/MovieSceneChannel.h"
#include "MovieSceneStringChannel.generated.h"

USTRUCT()
struct FMovieSceneStringChannel
{
	GENERATED_BODY()

	FMovieSceneStringChannel()
		: DefaultValue(), bHasDefaultValue(false)
	{}

	/**
	 * Access an integer that uniquely identifies this channel type.
	 *
	 * @return A static identifier that was allocated using FMovieSceneChannelEntry::RegisterNewID
	 */
	MOVIESCENETRACKS_API static uint32 GetChannelID();

	/**
	 * Serialize this type from another
	 */
	MOVIESCENETRACKS_API bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FArchive& Ar);

	/**
	 * Access a mutable interface for this channel
	 *
	 * @return An object that is able to manipulate this channel
	 */
	FORCEINLINE TMovieSceneChannel<FString> GetInterface()
	{
		return TMovieSceneChannel<FString>(&Times, &Values, &KeyHandles);
	}

	/**
	 * Access a constant interface for this channel
	 *
	 * @return An object that is able to interrogate this channel
	 */
	FORCEINLINE TMovieSceneChannel<const FString> GetInterface() const
	{
		return TMovieSceneChannel<const FString>(&Times, &Values);
	}

	/**
	 * Evaluate this channel
	 *
	 * @param InTime     The time to evaluate at
	 * @return A pointer to the string, or nullptr
	 */
	MOVIESCENETRACKS_API const FString* Evaluate(FFrameTime InTime) const;

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
struct TMovieSceneChannelTraits<FMovieSceneStringChannel>
{
#if WITH_EDITORONLY_DATA

	/** String channels can have external values (ie, they can get their values from external objects for UI purposes) */
	typedef TMovieSceneExternalValue<FString> EditorDataType;

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

