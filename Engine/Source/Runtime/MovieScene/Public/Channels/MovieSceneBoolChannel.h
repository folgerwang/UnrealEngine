// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "FrameNumber.h"
#include "MovieSceneChannel.h"
#include "MovieSceneChannelTraits.h"
#include "MovieSceneBoolChannel.generated.h"


USTRUCT()
struct FMovieSceneBoolChannel
{
	GENERATED_BODY()

	FMovieSceneBoolChannel()
		: DefaultValue(), bHasDefaultValue(false)
	{}

	/**
	 * Serialize this type from another
	 */
	MOVIESCENE_API bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FArchive& Ar);

	/**
	 * Access an integer that uniquely identifies this channel type.
	 *
	 * @return A static identifier that was allocated using FMovieSceneChannelEntry::RegisterNewID
	 */
	MOVIESCENE_API static uint32 GetChannelID();

	/**
	 * Access a mutable interface for this channel
	 *
	 * @return An object that is able to manipulate this channel
	 */
	FORCEINLINE TMovieSceneChannel<bool> GetInterface()
	{
		return TMovieSceneChannel<bool>(&Times, &Values, &KeyHandles);
	}

	/**
	 * Access a constant interface for this channel
	 *
	 * @return An object that is able to interrogate this channel
	 */
	FORCEINLINE TMovieSceneChannel<const bool> GetInterface() const
	{
		return TMovieSceneChannel<const bool>(&Times, &Values);
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
	FORCEINLINE TArrayView<const bool> GetValues() const
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
	MOVIESCENE_API bool Evaluate(FFrameTime InTime, bool& OutValue) const;

public:

	/**
	 * Set this channel's default value that should be used when no keys are present
	 *
	 * @param InDefaultValue The desired default value
	 */
	FORCEINLINE void SetDefault(bool InDefaultValue)
	{
		bHasDefaultValue = true;
		DefaultValue = InDefaultValue;
	}

	/**
	 * Get this channel's default value that will be used when no keys are present
	 *
	 * @return (Optional) The channel's default value
	 */
	FORCEINLINE TOptional<bool> GetDefault() const
	{
		return bHasDefaultValue ? TOptional<bool>(DefaultValue) : TOptional<bool>();
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

	UPROPERTY()
	bool DefaultValue;

	UPROPERTY()
	bool bHasDefaultValue;

	UPROPERTY()
	TArray<bool> Values;

	FMovieSceneKeyHandleMap KeyHandles;
};

template<>
struct TStructOpsTypeTraits<FMovieSceneBoolChannel> : public TStructOpsTypeTraitsBase2<FMovieSceneBoolChannel>
{
	enum { WithSerializeFromMismatchedTag = true };
};

template<>
struct TMovieSceneChannelTraits<FMovieSceneBoolChannel>
{
#if WITH_EDITORONLY_DATA

	/** Bool channels can have external values (ie, they can get their values from external objects for UI purposes) */
	typedef TMovieSceneExternalValue<bool> EditorDataType;

#endif
};