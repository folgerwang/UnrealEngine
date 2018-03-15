// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "FrameNumber.h"
#include "MovieSceneChannel.h"
#include "MovieSceneChannelTraits.h"
#include "MovieSceneByteChannel.generated.h"


USTRUCT()
struct FMovieSceneByteChannel
{
	GENERATED_BODY()

	FMovieSceneByteChannel()
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
	FORCEINLINE TMovieSceneChannel<uint8> GetInterface()
	{
		return TMovieSceneChannel<uint8>(&Times, &Values, &KeyHandles);
	}

	/**
	 * Access a constant interface for this channel
	 *
	 * @return An object that is able to interrogate this channel
	 */
	FORCEINLINE TMovieSceneChannel<const uint8> GetInterface() const
	{
		return TMovieSceneChannel<const uint8>(&Times, &Values);
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
	FORCEINLINE TArrayView<const uint8> GetValues() const
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
	MOVIESCENE_API bool Evaluate(FFrameTime InTime, uint8& OutValue) const;

public:

	/**
	 * Set this channel's default value that should be used when no keys are present
	 *
	 * @param InDefaultValue The desired default value
	 */
	FORCEINLINE void SetDefault(uint8 InDefaultValue)
	{
		bHasDefaultValue = true;
		DefaultValue = InDefaultValue;
	}

	/**
	 * Get this channel's default value that will be used when no keys are present
	 *
	 * @return (Optional) The channel's default value
	 */
	FORCEINLINE TOptional<uint8> GetDefault() const
	{
		return bHasDefaultValue ? TOptional<uint8>(DefaultValue) : TOptional<uint8>();
	}

	/**
	 * Remove this channel's default value causing the channel to have no effect where no keys are present
	 */
	FORCEINLINE void RemoveDefault()
	{
		bHasDefaultValue = false;
	}


public:

	UEnum* GetEnum() const
	{
		return Enum;
	}

	void SetEnum(UEnum* InEnum)
	{
		Enum = InEnum;
	}

private:

	UPROPERTY()
	TArray<FFrameNumber> Times;

	UPROPERTY()
	uint8 DefaultValue;

	UPROPERTY()
	bool bHasDefaultValue;

	UPROPERTY()
	TArray<uint8> Values;

	UPROPERTY()
	UEnum* Enum;

	FMovieSceneKeyHandleMap KeyHandles;
};

template<>
struct TStructOpsTypeTraits<FMovieSceneByteChannel> : public TStructOpsTypeTraitsBase2<FMovieSceneByteChannel>
{
	enum { WithSerializeFromMismatchedTag = true };
};

template<>
struct TMovieSceneChannelTraits<FMovieSceneByteChannel>
{
#if WITH_EDITORONLY_DATA

	/** Byte channels can have external values (ie, they can get their values from external objects for UI purposes) */
	typedef TMovieSceneExternalValue<uint8> EditorDataType;

#endif
};