// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "TimeSynchronizableMediaSource.h"



UTimeSynchronizableMediaSource::UTimeSynchronizableMediaSource()
	: bUseTimeSynchronization(false)
{ }


/*
* IMediaOptions interface
*/

bool UTimeSynchronizableMediaSource::GetMediaOption(const FName& Key, bool DefaultValue) const
{
	if (Key == TimeSynchronizableMedia::UseTimeSynchronizatioOption)
	{
		return bUseTimeSynchronization;
	}

	return Super::GetMediaOption(Key, DefaultValue);
}

bool UTimeSynchronizableMediaSource::HasMediaOption(const FName& Key) const
{
	if (Key == TimeSynchronizableMedia::UseTimeSynchronizatioOption)
	{
		return true;
	}

	return Super::HasMediaOption(Key);
}

