// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/KeyHandle.h"
#include "Channels/MovieSceneChannelHandle.h"

struct FFrameNumber;

class IKeyArea;
class UMovieSceneSection;

template<typename> class TArrayView;

/**
 * Represents a selected key in the sequencer
 */
struct FSequencerSelectedKey
{	
	/** Section that the key belongs to */
	UMovieSceneSection* Section;

	/** Key area providing the key */
	TSharedPtr<IKeyArea> KeyArea;

	/** Index of the key in the key area */
	TOptional<FKeyHandle> KeyHandle;

public:

	/** Create and initialize a new instance. */
	FSequencerSelectedKey(UMovieSceneSection& InSection, TSharedPtr<IKeyArea> InKeyArea, FKeyHandle InKeyHandle)
		: Section(&InSection)
		, KeyArea(InKeyArea)
		, KeyHandle(InKeyHandle)
	{}

	/** Default constructor. */
	FSequencerSelectedKey()
		: Section(nullptr)
		, KeyArea(nullptr)
		, KeyHandle()
	{}

	/** @return Whether or not this is a valid selected key */
	bool IsValid() const { return Section != nullptr && KeyArea.IsValid() && KeyHandle.IsSet(); }

	friend uint32 GetTypeHash(const FSequencerSelectedKey& SelectedKey)
	{
		return GetTypeHash(SelectedKey.Section) ^ GetTypeHash(SelectedKey.KeyArea) ^ 
			(SelectedKey.KeyHandle.IsSet() ? GetTypeHash(SelectedKey.KeyHandle.GetValue()) : 0);
	} 

	bool operator==(const FSequencerSelectedKey& OtherKey) const
	{
		return Section == OtherKey.Section && KeyArea == OtherKey.KeyArea &&
			KeyHandle.IsSet() && OtherKey.KeyHandle.IsSet() &&
			KeyHandle.GetValue() == OtherKey.KeyHandle.GetValue();
	}
};

/**
 * Structure representing a number of keys selected on a movie scene channel
 */
struct FSelectedChannelInfo
{
	explicit FSelectedChannelInfo(FMovieSceneChannelHandle InChannel, UMovieSceneSection* InOwningSection)
		: Channel(InChannel), OwningSection(InOwningSection)
	{}

	/** The channel on which the keys are selected */
	FMovieSceneChannelHandle Channel;

	/** The section that owns this channel */
	UMovieSceneSection* OwningSection;

	/** The key handles that are selected on this channel */
	TArray<FKeyHandle> KeyHandles;

	/** The index of each key handle in the original unordered key array supplied to FSelectedKeysByChannel */
	TArray<int32> OriginalIndices;
};

/**
 * Structure that groups an arbitrarily ordered array of selected keys into their respective channels
 */
struct FSelectedKeysByChannel
{
	explicit FSelectedKeysByChannel(TArrayView<const FSequencerSelectedKey> InSelectedKeys);

	/** Array storing all selected keys for each channel */
	TArray<FSelectedChannelInfo> SelectedChannels;
};

/**
 * Populate the specified key times array with the times of all the specified keys. Array sizes must match.
 *
 * @param InSelectedKeys    Array of selected keys
 * @param OutTimes          Pre-allocated array of key times to fill with the times of the above keys
 */
void GetKeyTimes(TArrayView<const FSequencerSelectedKey> InSelectedKeys, TArrayView<FFrameNumber> OutTimes);

/**
 * Set the key times for each of the specified keys. Array sizes must match.
 *
 * @param InSelectedKeys    Array of selected keys
 * @param InTimes           Array of times to apply, one per selected key index
 */
void SetKeyTimes(TArrayView<const FSequencerSelectedKey> InSelectedKeys, TArrayView<const FFrameNumber> InTimes);

/**
 * Duplicate the specified keys, populating another array with the duplicated key handles. Array sizes must match.
 *
 * @param InSelectedKeys    Array of selected keys
 * @param OutNewHandles     Pre-allocated array to receive duplicated key handles, one per input key.
 */
void DuplicateKeys(TArrayView<const FSequencerSelectedKey> InSelectedKeys, TArrayView<FKeyHandle> OutNewHandles);