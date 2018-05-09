// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

struct FMovieSceneChannel;
struct FMovieSceneChannelProxy;
struct FMovieSceneChannelMetaData;
template<typename> struct TMovieSceneChannelHandle;
template<typename> struct TMovieSceneChannelTraits;

/**
 * Handle to a specific channel in a UMovieSceneSection. Will become nullptr when the FMovieSceneChannelProxy it was created with is reallocated.
 * Implemented internally as a weak pointer, but not advertised as such to ensure that no shared references are held to FMovieSceneChannelProxy
 * outside of the owning MovieSceneSection's reference.
 */
struct MOVIESCENE_API FMovieSceneChannelHandle
{
	/**
	 * Default constructor
	 */
	FMovieSceneChannelHandle();

	/**
	 * Construction from a weak channel proxy, the channel's type, and its index
	 */
	FMovieSceneChannelHandle(TWeakPtr<FMovieSceneChannelProxy> InWeakChannelProxy, FName InChannelTypeName, int32 InChannelIndex);

	/**
	 * Cast this handle to a handle of a related type. Callee is responsible for ensuring that the type's are compatible.
	 */
	template<typename OtherChannelType>
	TMovieSceneChannelHandle<OtherChannelType> Cast() const;

	/**
	 * Access this channel's type identifier
	 */
	FName GetChannelTypeName() const;

	/**
	 * Get the channel pointer this handle represents.
	 *
	 * @return the channel's pointer, or nullptr if the proxy it was created with is no longer alive.
	 */
	FMovieSceneChannel* Get() const;

#if WITH_EDITOR

	/**
	 * Get the meta data associated with this channel
	 *
	 * @return the channel's meta data, or nullptr if the proxy it was created with is no longer alive.
	 */
	const FMovieSceneChannelMetaData* GetMetaData() const;

	/**
	 * Get the extended editor data associated with this channel
	 *
	 * @return the channel's extended editor data, or nullptr if the proxy it was created with is no longer alive.
	 */
	const void* GetExtendedEditorData() const;

#endif // WITH_EDITOR

private:

	/** Weak pointer to the channel, proxy alisased to the channel proxy's shared reference controller to ensure it becomes null when the proxy is re-allocated */
	TWeakPtr<FMovieSceneChannelProxy> WeakChannelProxy;

	/** The type name for the channel in the proxy */
	FName ChannelTypeName;

	/** The index of the channel within the typed channels array */
	int32 ChannelIndex;
};

/**
 * Handle to a specific channel in a UMovieSceneSection. Will become nullptr when the FMovieSceneChannelProxy it was created with is reallocated.
 * Implemented internally as a weak pointer, but not advertised as such to ensure that no shared references are held to FMovieSceneChannelProxy
 * outside of the owning MovieSceneSection's reference.
 */
template<typename ChannelType>
struct TMovieSceneChannelHandle : FMovieSceneChannelHandle
{
	TMovieSceneChannelHandle()
	{}

	/**
	 * Construction from a weak channel proxy, and the channel's index
	 */
	TMovieSceneChannelHandle(TWeakPtr<FMovieSceneChannelProxy> InWeakChannelProxy, int32 InChannelIndex)
		: FMovieSceneChannelHandle(InWeakChannelProxy, ChannelType::StaticStruct()->GetFName(), InChannelIndex)
	{}

	/**
	 * Get the channel pointer this handle represents.
	 *
	 * @return the channel's pointer, or nullptr if the proxy it was created with is no longer alive.
	 */
	ChannelType* Get() const
	{
		return static_cast<ChannelType*>(FMovieSceneChannelHandle::Get());
	}

#if WITH_EDITOR

	/**
	 * Get the extended editor data associated with this channel
	 *
	 * @return the channel's extended editor data, or nullptr if the proxy it was created with is no longer alive.
	 */
	const typename TMovieSceneChannelTraits<ChannelType>::ExtendedEditorDataType* GetExtendedEditorData() const
	{
		return static_cast<const typename TMovieSceneChannelTraits<ChannelType>::ExtendedEditorDataType*>(FMovieSceneChannelHandle::GetExtendedEditorData());
	}

#endif // WITH_EDITOR
};


template<typename OtherChannelType>
TMovieSceneChannelHandle<OtherChannelType> FMovieSceneChannelHandle::Cast() const
{
	check(OtherChannelType::StaticStruct()->GetFName() == ChannelTypeName);
	return TMovieSceneChannelHandle<OtherChannelType>(WeakChannelProxy, ChannelIndex);
}
