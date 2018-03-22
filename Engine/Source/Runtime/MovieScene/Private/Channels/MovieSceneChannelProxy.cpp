// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Channels/MovieSceneChannelProxy.h"
#include "Algo/BinarySearch.h"


uint32 FMovieSceneChannelEntry::RegisterNewID()
{
	static FThreadSafeCounter CurrentID = 0;
	check(CurrentID.GetValue() < TNumericLimits<int32>::Max());
	return CurrentID.Increment();
}

const FMovieSceneChannelEntry* FMovieSceneChannelProxy::FindEntry(uint32 ChannelTypeID) const
{
	const int32 ChannelTypeIndex = Algo::BinarySearchBy(Entries, ChannelTypeID, &FMovieSceneChannelEntry::ChannelID);

	if (ChannelTypeIndex != INDEX_NONE)
	{
		return &Entries[ChannelTypeIndex];
	}

	return nullptr;
}

int32 FMovieSceneChannelProxy::FindIndex(uint32 ChannelTypeID, void* ChannelPtr) const
{
	const FMovieSceneChannelEntry* FoundEntry = FindEntry(ChannelTypeID);
	if (FoundEntry)
	{
		return FoundEntry->GetChannels().IndexOfByKey(ChannelPtr);
	}

	return INDEX_NONE;
}

void* FMovieSceneChannelProxy::GetChannel(uint32 ChannelTypeID, int32 ChannelIndex) const
{
	if (const FMovieSceneChannelEntry* Entry = FindEntry(ChannelTypeID))
	{
		TArrayView<void* const> Channels = Entry->GetChannels();
		return Channels.IsValidIndex(ChannelIndex) ? Channels[ChannelIndex] : nullptr;
	}
	return nullptr;
}