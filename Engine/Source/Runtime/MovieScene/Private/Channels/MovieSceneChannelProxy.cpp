// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Channels/MovieSceneChannelProxy.h"
#include "Algo/BinarySearch.h"


FMovieSceneChannelHandle FMovieSceneChannelProxy::MakeHandle(FName ChannelTypeName, int32 Index)
{
	TWeakPtr<FMovieSceneChannelProxy> WeakProxy = TSharedPtr<FMovieSceneChannelProxy>(AsShared());
	return FMovieSceneChannelHandle(WeakProxy, ChannelTypeName, Index);
}

const FMovieSceneChannelEntry* FMovieSceneChannelProxy::FindEntry(FName ChannelTypeName) const
{
	const int32 ChannelTypeIndex = Algo::BinarySearchBy(Entries, ChannelTypeName, &FMovieSceneChannelEntry::ChannelTypeName);

	if (ChannelTypeIndex != INDEX_NONE)
	{
		return &Entries[ChannelTypeIndex];
	}

	return nullptr;
}

int32 FMovieSceneChannelProxy::FindIndex(FName ChannelTypeName, const FMovieSceneChannel* ChannelPtr) const
{
	const FMovieSceneChannelEntry* FoundEntry = FindEntry(ChannelTypeName);
	if (FoundEntry)
	{
		return FoundEntry->GetChannels().IndexOfByKey(ChannelPtr);
	}

	return INDEX_NONE;
}

FMovieSceneChannel* FMovieSceneChannelProxy::GetChannel(FName ChannelTypeName, int32 ChannelIndex) const
{
	if (const FMovieSceneChannelEntry* Entry = FindEntry(ChannelTypeName))
	{
		TArrayView<FMovieSceneChannel* const> Channels = Entry->GetChannels();
		return Channels.IsValidIndex(ChannelIndex) ? Channels[ChannelIndex] : nullptr;
	}
	return nullptr;
}