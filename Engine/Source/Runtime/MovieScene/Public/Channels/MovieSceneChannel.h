// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/CoreDefines.h"
#include "Misc/FrameNumber.h"
#include "Misc/FrameRate.h"
#include "Containers/ArrayView.h"
#include "Math/Range.h"

#include "MovieSceneChannel.generated.h"

struct FKeyHandle;
struct FFrameRate;
struct FFrameNumber;
struct FKeyDataOptimizationParams;

USTRUCT()
struct MOVIESCENE_API FMovieSceneChannel
{
	GENERATED_BODY()

	FMovieSceneChannel() {}
	virtual ~FMovieSceneChannel() {}

	/**
	 * Get the time for the specified key handle
	 *
	 * @param InHandle              The handle of the key to get the time for
	 * @param OutKeyTime            Out parameter to receive the key's time
	 */
	void GetKeyTime(const FKeyHandle InHandle, FFrameNumber& OutKeyTime);

	/**
	 * Set the time for the specified key handle
	 *
	 * @param InHandle              The handle of the key to get the time for
	 * @param InKeyTime             The new time for the key
	 */
	void SetKeyTime(const FKeyHandle InHandle, const FFrameNumber InKeyTime);

	/**
	 * Get key information pertaining to all keys that exist within the specified range
	 *
	 * @param WithinRange           The range within which to return key information
	 * @param OutKeyTimes           (Optional) Array to receive key times
	 * @param OutKeyHandles         (Optional) Array to receive key handles
	 */
	virtual void GetKeys(const TRange<FFrameNumber>& WithinRange, TArray<FFrameNumber>* OutKeyTimes, TArray<FKeyHandle>* OutKeyHandles)
	{}

	/**
	 * Get all key times for the specified key handles
	 *
	 * @param InHandles             Array of handles to get times for
	 * @param OutKeyTimes           Pre-sized array of key times to set. Invalid key handles will not assign to this array. Must match size of InHandles
	 */
	virtual void GetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<FFrameNumber> OutKeyTimes)
	{}

	/**
	 * Set key times for the specified key handles
	 *
	 * @param InHandles             Array of handles to get times for
	 * @param InKeyTimes            Array of times to apply - one per handle
	 */
	virtual void SetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<const FFrameNumber> InKeyTimes)
	{}

	/**
	 * Duplicate the keys for the specified key handles
	 *
	 * @param InHandles             Array of handles to duplicate
	 * @param OutKeyTimes           Pre-sized array to receive duplicated key handles. Invalid key handles will not be assigned to this array. Must match size of InHandles
	 */
	virtual void DuplicateKeys(TArrayView<const FKeyHandle> InHandles, TArrayView<FKeyHandle> OutNewHandles)
	{}

	/**
	 * Delete the keys for the specified key handles
	 *
	 * @param InHandles             Array of handles to delete
	 */
	virtual void DeleteKeys(TArrayView<const FKeyHandle> InHandles)
	{}

	/**
	 * Called when the frame resolution of this channel is to be changed.
	 *
	 * @param SourceRate      The previous frame resolution that the channel is currently in
	 * @param DestinationRate The desired new frame resolution. All keys should be transformed into this time-base.
	 */
	virtual void ChangeFrameResolution(FFrameRate SourceRate, FFrameRate DestinationRate)
	{}

	/**
	 * Compute the effective range of this channel, for example, the extents of its key times
	 *
	 * @return A range that represents the greatest range of times occupied by this channel, in the sequence's frame resolution
	 */
	virtual TRange<FFrameNumber> ComputeEffectiveRange() const
	{
		return TRange<FFrameNumber>::Empty();
	}

	/**
	 * Get the total number of keys on this channel
	 *
	 * @return The number of keys on this channel
	 */
	virtual int32 GetNumKeys() const
	{
		return 0;
	}

	/**
	 * Reset this channel back to its original state
	 */
	virtual void Reset()
	{}

	/**
	 * Offset the keys within this channel by a given delta position
	 *
	 * @param DeltaPosition   The number of frames to offset by, in the sequence's frame resolution
	 */
	virtual void Offset(FFrameNumber DeltaPosition)
	{}

	/**
	 * Optimize this channel by removing any redundant data according to the specified parameters
	 *
	 * @param InParameters    Parameter struct specifying how to optimize the channel
	 */
	virtual void Optimize(const FKeyDataOptimizationParams& InParameters)
	{}

	/**
	 * Clear all the default value on this channel
	 */
	virtual void ClearDefault()
	{}

	/**
	 * Perfor a possibly heavy operation after an edit change 
	 *
	 */
	virtual void PostEditChange() {}

};