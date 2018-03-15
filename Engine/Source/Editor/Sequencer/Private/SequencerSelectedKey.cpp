// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SequencerSelectedKey.h"
#include "ISequencerModule.h"
#include "Modules/ModuleManager.h"
#include "IKeyArea.h"
#include "ISequencerChannelInterface.h"

FSelectedKeysByChannelType::FSelectedKeysByChannelType(TArrayView<const FSequencerSelectedKey> InSelectedKeys)
{
	for (int32 Index = 0; Index < InSelectedKeys.Num(); ++Index)
	{
		FSequencerSelectedKey Key = InSelectedKeys[Index];
		void* RawChannelPtr = Key.KeyArea->GetChannelPtr();

		if (Key.KeyHandle.IsSet() && RawChannelPtr)
		{
			FSelectedChannelInfo* ChannelInfo = ChannelToKeyHandleMap.Find(RawChannelPtr);
			if (!ChannelInfo)
			{
				ChannelInfo = &ChannelToKeyHandleMap.Add(RawChannelPtr, FSelectedChannelInfo(Key.KeyArea->GetChannelTypeID(), Key.KeyArea->GetOwningSection()));
			}

			ChannelInfo->KeyHandles.Add(Key.KeyHandle.GetValue());
			ChannelInfo->OriginalIndices.Add(Index);
		}
	}
}

void GetKeyTimes(TArrayView<const FSequencerSelectedKey> InSelectedKeys, TArrayView<FFrameNumber> OutTimes)
{
	check(InSelectedKeys.Num() == OutTimes.Num());

	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");

	FSelectedKeysByChannelType KeysByChannel(InSelectedKeys);

	TArray<FFrameNumber> KeyTimesScratch;

	for (TTuple<void*, FSelectedChannelInfo>& Pair : KeysByChannel.ChannelToKeyHandleMap)
	{
		void* Channel = Pair.Key;
		const FSelectedChannelInfo& ChannelInfo = Pair.Value;

		ISequencerChannelInterface* ChannelInterface = SequencerModule.FindChannelInterface(Pair.Value.ChannelTypeID);
		if (ChannelInterface)
		{
			// Resize the scratch buffer to the correct size
			const int32 NumKeys = ChannelInfo.KeyHandles.Num();
			KeyTimesScratch.Reset(NumKeys);
			KeyTimesScratch.SetNum(NumKeys);

			// Populating the key times scratch buffer with the times for these handles
			ChannelInterface->GetKeyTimes_Raw(Channel, ChannelInfo.KeyHandles, KeyTimesScratch);

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

	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");

	FSelectedKeysByChannelType KeysByChannel(InSelectedKeys);

	TArray<FFrameNumber> KeyTimesScratch;

	for (TTuple<void*, FSelectedChannelInfo>& Pair : KeysByChannel.ChannelToKeyHandleMap)
	{
		void* Channel = Pair.Key;
		const FSelectedChannelInfo& ChannelInfo = Pair.Value;

		ISequencerChannelInterface* ChannelInterface = SequencerModule.FindChannelInterface(Pair.Value.ChannelTypeID);
		if (ChannelInterface)
		{
			KeyTimesScratch.Reset(ChannelInfo.OriginalIndices.Num());
			for (int32 Index : ChannelInfo.OriginalIndices)
			{
				KeyTimesScratch.Add(InTimes[Index]);
			}

			ChannelInterface->SetKeyTimes_Raw(Channel, ChannelInfo.KeyHandles, KeyTimesScratch);
		}
	}
}

void DuplicateKeys(TArrayView<const FSequencerSelectedKey> InSelectedKeys, TArrayView<FKeyHandle> OutNewHandles)
{
	check(InSelectedKeys.Num() == OutNewHandles.Num());

	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");

	FSelectedKeysByChannelType KeysByChannel(InSelectedKeys);

	TArray<FKeyHandle> KeyHandlesScratch;
	for (TTuple<void*, FSelectedChannelInfo>& Pair : KeysByChannel.ChannelToKeyHandleMap)
	{
		void* Channel = Pair.Key;
		const FSelectedChannelInfo& ChannelInfo = Pair.Value;

		ISequencerChannelInterface* ChannelInterface = SequencerModule.FindChannelInterface(Pair.Value.ChannelTypeID);
		if (ChannelInterface)
		{
			// Resize the scratch buffer to the correct size
			const int32 NumKeys = ChannelInfo.KeyHandles.Num();
			KeyHandlesScratch.Reset(NumKeys);
			KeyHandlesScratch.SetNum(NumKeys);

			// Duplicate the keys, populating the handles scratch buffer
			ChannelInterface->DuplicateKeys_Raw(Channel, ChannelInfo.KeyHandles, KeyHandlesScratch);

			// Copy the duplicated key handles to the output array view
			for(int32 Index = 0; Index < KeyHandlesScratch.Num(); ++Index)
			{
				int32 OriginalIndex = ChannelInfo.OriginalIndices[Index];
				OutNewHandles[OriginalIndex] = KeyHandlesScratch[Index];
			}
		}
	}
}
