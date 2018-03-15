// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "FrameNumber.h"
#include "FrameTime.h"
#include "FrameRate.h"
#include "MovieSceneChannelEditorData.h"

enum class EMovieSceneKeyInterpolation : uint8;

/**
 * Parameter structure passed to MovieScene::Optimize that defines optimization parameters
 */
struct FKeyDataOptimizationParams
{
	/** An arbitrary tolerance under which keys should be optimizied */
	float Tolerance = KINDA_SMALL_NUMBER;

	/** Whether to automatically set interpolation tangents or not */
	bool bAutoSetInterpolation = false;

	/** A range inside which to optimize keys */
	TRange<FFrameNumber> Range = TRange<FFrameNumber>::All();
};

/**
 * Traits structure to be specialized for any channel type passed to FMovieSceneChannelProxy
 */
template<typename ChannelType>
struct TMovieSceneChannelTraitsBase
{
};

/**
 * Traits structure to be specialized for any channel type passed to FMovieSceneChannelProxy
 */
template<typename ChannelType>
struct TMovieSceneChannelTraits : TMovieSceneChannelTraitsBase<ChannelType>
{
#if WITH_EDITOR

	/** Type that specifies what editor data should be associated with ChannelType */
	typedef void EditorDataType;

#endif
};

namespace MovieScene
{
	/**
	 * Called to evaluate a channel. Overload with specific channel types for custom behaviour.
	 *
	 * @param InChannel   The channel to evaluate
	 * @param InTime      The time to evaluate at
	 * @param OutValue    Value to receive the result
	 * @return true if the channel was evaluated successfully, false otherwise
	 */
	template<typename ChannelType, typename ValueType>
	bool EvaluateChannel(const ChannelType* InChannel, FFrameTime InTime, ValueType& OutValue)
	{
		return InChannel->Evaluate(InTime, OutValue);
	}

	/**
	 * Called to assign a specific value in a channel.
	 *
	 * @param InChannel     The channel the value is contained within
	 * @param InValueIndex  The index of the value to set
	 * @param InValue       The new value
	 */
	template<typename ChannelType, typename ValueType>
	void AssignValue(ChannelType* InChannel, int32 InValueIndex, ValueType&& InValue)
	{
		InChannel->GetInterface().GetValues()[InValueIndex] = Forward<ValueType>(InValue);
	}

	/**
	 * Add a key to a channel, or update an existing key if one already exists at this time
	 *
	 * @param InChannel     The channel the value is contained within
	 * @param InTime        The time to add or update the key at
	 * @param InValue       The new value
	 * @return A handle to the key that was added
	 */
	template<typename ChannelType, typename ValueType>
	FKeyHandle AddKeyToChannel(ChannelType* InChannel, FFrameNumber InTime, ValueType&& Value, EMovieSceneKeyInterpolation Interpolation)
	{
		auto ChannelInterface = InChannel->GetInterface();
		int32 ExistingIndex = ChannelInterface.FindKey(InTime);
		if (ExistingIndex != INDEX_NONE)
		{
			AssignValue(InChannel, ExistingIndex, Forward<ValueType>(Value));
		}
		else
		{
			ExistingIndex = ChannelInterface.AddKey(InTime, Forward<ValueType>(Value));
		}
		return ChannelInterface.GetHandle(ExistingIndex);
	}

	/**
	 * Check whether the specified value already exists at the specified time
	 *
	 * @param InChannel     The channel to check
	 * @param InTime        The time to check for
	 * @param InValue       The value to check - compared against the curve's existing value at this time
	 * @return true if this value already exists at the time, false otherwise
	 */
	template<typename ChannelType, typename ValueType>
	bool ValueExistsAtTime(const ChannelType* InChannel, FFrameNumber InTime, const ValueType& InValue)
	{
		ValueType ExistingValue{};
		return EvaluateChannel(InChannel, InTime, ExistingValue) && ExistingValue == InValue;
	}

	/**
	 * Compute the effective range of the specified channel. Generally just means the range of its keys.
	 *
	 * @param InChannel     The channel to compute the range for
	 * @return The range of this channel
	 */
	template<typename ChannelType>
	TRange<FFrameNumber> ComputeEffectiveRange(const ChannelType* InChannel)
	{
		TArrayView<const FFrameNumber> Times = InChannel->GetInterface().GetTimes();
		return Times.Num() ? TRange<FFrameNumber>(Times[0], TRangeBound<FFrameNumber>::Inclusive(Times[Times.Num()-1])) : TRange<FFrameNumber>::Empty();
	}

	/**
	 * Convert the frame resolution of a movie scene channel by moving the key times to the equivalent frame time
	 *
	 * @param InChannel       The channel to compute the range for
	 * @param SourceRate      The frame rate the channel is currently in
	 * @param DestinationRate The new frame rate to convert the channel to
	 */
	template<typename ChannelType>
	void ChangeFrameResolution(ChannelType* InChannel, FFrameRate SourceRate, FFrameRate DestinationRate)
	{
		TArrayView<FFrameNumber> Times = InChannel->GetInterface().GetTimes();
		for (int32 Index = 0; Index < Times.Num(); ++Index)
		{
			Times[Index] = ConvertFrameTime(Times[Index], SourceRate, DestinationRate).RoundToFrame();
		}
	}

	/**
	 * Get the number of keys contained within the specified channel
	 *
	 * @param InChannel     The channel to get the number of keys for
	 * @return The number of keys in this channel
	 */
	template<typename ChannelType>
	int32 GetNumKeys(const ChannelType* InChannel)
	{
		return InChannel->GetInterface().GetTimes().Num();
	}

	/**
	 * Reset the specified channel back to its default state
	 *
	 * @param InChannel     The channel to reset
	 */
	template<typename ChannelType>
	void Reset(ChannelType* InChannel)
	{
		*InChannel = ChannelType();
	}

	/**
	 * Offset the specified channel by a given delta time
	 *
	 * @param InChannel     The channel to offset
	 * @param DeltaTime     The time to offset by
	 */
	template<typename ChannelType>
	void Offset(ChannelType* InChannel, FFrameNumber DeltaTime)
	{
		TArrayView<FFrameNumber> Times = InChannel->GetInterface().GetTimes();
		for (FFrameNumber& Time : Times)
		{
			Time += DeltaTime;
		}
	}

	/**
	 * Dilate the specified channel with a given factor and origin
	 *
	 * @param InChannel      The channel to offset
	 * @param Origin         The origin around which key times should scale
	 * @param DilationFactor The amount to dilate by
	 */
	template<typename ChannelType>
	void Dilate(ChannelType* InChannel, FFrameNumber Origin, float DilationFactor)
	{
		// @todo: sequencer-timecode: need to rethink this
		// TArrayView<FFrameNumber> Times = InChannel->GetInterface().GetTimes();
		// for (FFrameNumber& Time : Times)
		// {
		// 	Time = Origin + (Time - Origin) * DilationFactor;
		// }
	}

	/**
	 * Set a channel's default value
	 *
	 * @param InChannel      The channel to set the default on
	 * @param DefaultValue   The new default value
	 */
	template<typename ChannelType, typename ValueType>
	void SetChannelDefault(ChannelType* Channel, ValueType&& DefaultValue)
	{
		Channel->SetDefault(DefaultValue);
	}

	/**
	 * Clear a channel's default value
	 *
	 * @param InChannel      The channel to clear the default on
	 */
	template<typename ChannelType>
	void ClearChannelDefault(ChannelType* InChannel)
	{
		InChannel->RemoveDefault();
	}

	/**
	 * Optimize the specified channel by removing any redundant keys
	 *
	 * @param InChannel     The channel to optimize
	 * @param Params        Optimization parameters
	 */
	template<typename ChannelType>
	void Optimize(ChannelType* InChannel, const FKeyDataOptimizationParams& Params)
	{
		using namespace MovieScene;

		auto ChannelInterface = InChannel->GetInterface();
		if (ChannelInterface.GetTimes().Num() > 1)
		{
			int32 StartIndex = 0;
			int32 EndIndex = 0;

			{
				TArrayView<const FFrameNumber> Times = ChannelInterface.GetTimes();
				StartIndex = Params.Range.GetLowerBound().IsClosed() ? Algo::LowerBound(Times, Params.Range.GetLowerBoundValue()) : 0;
				EndIndex   = Params.Range.GetUpperBound().IsClosed() ? Algo::UpperBound(Times, Params.Range.GetUpperBoundValue()) : Times.Num();
			}

			for (int32 Index = StartIndex; Index < EndIndex && Index < ChannelInterface.GetTimes().Num(); ++Index)
			{
				// Reget times and values as they may be reallocated
				FFrameNumber Time   = ChannelInterface.GetTimes()[Index];
				auto  OriginalValue = ChannelInterface.GetValues()[Index];

				// If the channel evaluates the same with this key removed, we can leave it out
				ChannelInterface.RemoveKey(Index);
				if (ValueExistsAtTime(InChannel, Time, OriginalValue))
				{
					Index--;
				}
				else
				{
					ChannelInterface.AddKey(Time, MoveTemp(OriginalValue));
				}
			}
		}
	}
}