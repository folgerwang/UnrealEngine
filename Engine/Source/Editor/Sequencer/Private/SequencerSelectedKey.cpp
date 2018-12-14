// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SequencerSelectedKey.h"
#include "Modules/ModuleManager.h"
#include "IKeyArea.h"
#include "Channels/MovieSceneChannel.h"
#include "ISequencerChannelInterface.h"

FSelectedKeysByChannel::FSelectedKeysByChannel(TArrayView<const FSequencerSelectedKey> InSelectedKeys)
{
	TMap<const IKeyArea*, int32> KeyAreaToChannelIndex;

	for (int32 Index = 0; Index < InSelectedKeys.Num(); ++Index)
	{
		FSequencerSelectedKey Key = InSelectedKeys[Index];
		const IKeyArea* KeyArea = Key.KeyArea.Get();

		if (KeyArea && Key.KeyHandle.IsSet())
		{
			const int32* ChannelArrayIndex = KeyAreaToChannelIndex.Find(KeyArea);
			if (!ChannelArrayIndex)
			{
				int32 NewIndex = SelectedChannels.Add(FSelectedChannelInfo(Key.KeyArea->GetChannel(), Key.KeyArea->GetOwningSection()));
				ChannelArrayIndex = &KeyAreaToChannelIndex.Add(KeyArea, NewIndex);
			}

			FSelectedChannelInfo& ThisChannelInfo = SelectedChannels[*ChannelArrayIndex];
			ThisChannelInfo.KeyHandles.Add(Key.KeyHandle.GetValue());
			ThisChannelInfo.OriginalIndices.Add(Index);
		}
	}
}

void GetKeyTimes(TArrayView<const FSequencerSelectedKey> InSelectedKeys, TArrayView<FFrameNumber> OutTimes)
{
	check(InSelectedKeys.Num() == OutTimes.Num());

	FSelectedKeysByChannel KeysByChannel(InSelectedKeys);

	TArray<FFrameNumber> KeyTimesScratch;

	for (const FSelectedChannelInfo& ChannelInfo : KeysByChannel.SelectedChannels)
	{
		FMovieSceneChannel* Channel = ChannelInfo.Channel.Get();
		if (Channel)
		{
			// Resize the scratch buffer to the correct size
			const int32 NumKeys = ChannelInfo.KeyHandles.Num();
			KeyTimesScratch.Reset(NumKeys);
			KeyTimesScratch.SetNum(NumKeys);

			// Populating the key times scratch buffer with the times for these handles
			Channel->GetKeyTimes(ChannelInfo.KeyHandles, KeyTimesScratch);

			for(int32 Index = 0; Index < KeyTimesScratch.Num(); ++Index)
			{
				int32 OriginalIndex = ChannelInfo.OriginalIndices[Index];
				OutTimes[OriginalIndex] = KeyTimesScratch[Index];
			}
		}
	}
}

void SetKeyTimes(TArrayView<const FSequencerSelectedKey> InSelectedKeys, TArrayView<const FFrameNumber> InTimes)
{
	check(InSelectedKeys.Num() == InTimes.Num());

	FSelectedKeysByChannel KeysByChannel(InSelectedKeys);

	TArray<FFrameNumber> KeyTimesScratch;

	for (const FSelectedChannelInfo& ChannelInfo : KeysByChannel.SelectedChannels)
	{
		FMovieSceneChannel* Channel = ChannelInfo.Channel.Get();
		if (Channel)
		{
			KeyTimesScratch.Reset(ChannelInfo.OriginalIndices.Num());
			for (int32 Index : ChannelInfo.OriginalIndices)
			{
				KeyTimesScratch.Add(InTimes[Index]);
			}

			Channel->SetKeyTimes(ChannelInfo.KeyHandles, KeyTimesScratch);
		}
	}
}

void DuplicateKeys(TArrayView<const FSequencerSelectedKey> InSelectedKeys, TArrayView<FKeyHandle> OutNewHandles)
{
	check(InSelectedKeys.Num() == OutNewHandles.Num());

	FSelectedKeysByChannel KeysByChannel(InSelectedKeys);

	TArray<FKeyHandle> KeyHandlesScratch;
	for (const FSelectedChannelInfo& ChannelInfo : KeysByChannel.SelectedChannels)
	{
		FMovieSceneChannel* Channel = ChannelInfo.Channel.Get();
		if (Channel)
		{
			// Resize the scratch buffer to the correct size
			const int32 NumKeys = ChannelInfo.KeyHandles.Num();
			KeyHandlesScratch.Reset(NumKeys);
			KeyHandlesScratch.SetNum(NumKeys);

			// Duplicate the keys, populating the handles scratch buffer
			Channel->DuplicateKeys(ChannelInfo.KeyHandles, KeyHandlesScratch);

			// Copy the duplicated key handles to the output array view
			for(int32 Index = 0; Index < KeyHandlesScratch.Num(); ++Index)
			{
				int32 OriginalIndex = ChannelInfo.OriginalIndices[Index];
				OutNewHandles[OriginalIndex] = KeyHandlesScratch[Index];
			}
		}
	}
}
