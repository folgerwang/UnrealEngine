// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

/**
 * Handle to a specific channel in a UMovieSceneSection. Will become nullptr when the FMovieSceneChannelProxy it was created with is reallocated.
 * Implemented internally as a weak pointer, but not advertised as such to ensure that no shared references are held to FMovieSceneChannelProxy
 * outside of the owning MovieSceneSection's reference.
 */
template<typename ChannelType>
struct TMovieSceneChannelHandle
{
	TMovieSceneChannelHandle()
	{}

	/** Construction from a weak pointer produced from FMovieSceneChannelProxy */
	TMovieSceneChannelHandle(TWeakPtr<ChannelType> InWeakChannel)
		: WeakChannel(InWeakChannel)
	{}

	/**
	 * Get the channel pointer this handle represents.
	 *
	 * @return the channel's pointer, or nullptr if the proxy it was created with is no longer alive.
	 */
	ChannelType* Get() const
	{
		return WeakChannel.Pin().Get();
	}

	/**
	 * Cast this handle to a handle of a related type
	 */
	template<typename OtherChannelType>
	TMovieSceneChannelHandle<OtherChannelType> Cast() const;

private:

	/** Weak pointer to the channel, proxy alisased to the channel proxy's shared reference controller to ensure it becomes null when the proxy is re-allocated */
	TWeakPtr<ChannelType> WeakChannel;
};



template<typename ChannelType>
template<typename OtherChannelType>
TMovieSceneChannelHandle<OtherChannelType> TMovieSceneChannelHandle<ChannelType>::Cast() const
{
	TSharedPtr<ChannelType> StrongRef = WeakChannel.Pin();
	if (StrongRef.IsValid())
	{
		return TWeakPtr<OtherChannelType>(StaticCastSharedPtr<OtherChannelType>(StrongRef));
	}

	return TMovieSceneChannelHandle<OtherChannelType>();
}
