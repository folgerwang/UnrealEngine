// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/UnrealTemplate.h"
#include "Misc/InlineValue.h"
#include "Channels/BatchChannelInterface.h"
#include "Channels/MovieSceneChannelEditorDataEntry.h"
#include "Channels/MovieSceneChannelHandle.h"

struct FMovieSceneChannelData;
struct FMovieSceneChannelProxy;



/**
 * An entry within FMovieSceneChannelProxy that contains all channels (and editor data) for any given channel type
 */
struct FMovieSceneChannelEntry : FMovieSceneChannelEditorDataEntry
{
	/**
	 * Register a new, unique identifier that should be associated with a channel type through ChannelType::GetChannelID
	 */
	MOVIESCENE_API static uint32 RegisterNewID();

	/** 
	 * Get the ID of the channels stored in this entry
	 */
	uint32 GetChannelID() const
	{
		return ChannelID;
	}

	/** 
	 * Access the multi-channel interface that can interact with this entry's channels
	 */
	const IBatchChannelInterface& GetBatchChannelInterface() const
	{
		return SectionDataInterface.GetValue();
	}

	/** 
	 * Access all the channels contained within this entry
	 */
	TArrayView<void* const> GetChannels() const
	{
		return Channels;
	}

#if WITH_EDITOR

	/**
	 * Access specialized typed editor data for channels stored in this entry
	 */
	template<typename ChannelType>
	TArrayView<const typename TMovieSceneChannelTraits<ChannelType>::EditorDataType> GetAllSpecializedEditorData() const
	{
		check(ChannelType::GetChannelID() == ChannelID);
		return FMovieSceneChannelEditorDataEntry::GetAllSpecializedEditorData<ChannelType>();
	}

#endif

private:

	// only FMovieSceneChannelData and FMovieSceneChannelProxy can create entries
	friend FMovieSceneChannelData;
	friend FMovieSceneChannelProxy;

	/** Templated constructor from the channel and its ID */
	template<typename ChannelType>
	explicit FMovieSceneChannelEntry(uint32 InChannelID, const ChannelType& Channel)
		: FMovieSceneChannelEditorDataEntry(Channel)
		, ChannelID(InChannelID)
		, SectionDataInterface(TBatchChannelInterface<ChannelType>())
	{
		check(InChannelID == ChannelType::GetChannelID());
	}

	/** The ID of the channels contained in this entry, generated from FMovieSceneChannelEntry::RegisterNewID */
	uint32 ChannelID;

	/** Pointers to the channels that this entry contains. Pointers are assumed to stay alive as long as this entry is. If channels are reallocated, a new channel proxy should be created */
	TArray<void*> Channels;

	/** Implementation of the channel interface that can interact with the typed channels */
	TInlineValue<IBatchChannelInterface, 8> SectionDataInterface;
};




/**
 * Construction helper that is required to create a new FMovieSceneChannelProxy from multiple channels
 */
struct FMovieSceneChannelData
{
#if WITH_EDITOR

	/**
	 * Add a new channel to the proxy. Channel's address is stored internally as a voic* and should exist as long as the channel proxy does
	 *
	 * @param InChannel          The channel to add to this proxy. Should live for as long as the proxy does. Any re-allocation should be accompanied with a re-creation of the proxy
	 * @param InCommonEditorData The editor data to be associated with this channel
	 */
	template<typename ChannelType>
	void Add(ChannelType& InChannel, const FMovieSceneChannelEditorData& InCommonEditorData)
	{
		static_assert(TIsSame<typename TMovieSceneChannelTraits<ChannelType>::EditorDataType, void>::Value, "Must supply typed editor data according to the channel's traits.");

		// Add the channel
		const int32 ChannelTypeIndex = AddInternal(InChannel);
		// Add the editor data at the same index
		Entries[ChannelTypeIndex].AddEditorData<ChannelType>(InCommonEditorData);
	}

	/**
	 * Add a new channel to the proxy. Channel's address is stored internally as a voic* and should exist as long as the channel proxy does
	 *
	 * @param InChannel          The channel to add to this proxy. Should live for as long as the proxy does. Any re-allocation should be accompanied with a re-creation of the proxy
	 * @param InCommonEditorData The editor data to be associated with this channel
	 * @param InTypedEditorData  Additional editor data to be associated with this channel as per its traits
	 */
	template<typename ChannelType, typename TypedEditorData>
	void Add(ChannelType& InChannel, const FMovieSceneChannelEditorData& InCommonEditorData, TypedEditorData&& InTypedEditorData)
	{
		// Add the channel
		const int32 ChannelTypeIndex = AddInternal(InChannel);
		// Add the editor data at the same index
		Entries[ChannelTypeIndex].AddEditorData<ChannelType>(InCommonEditorData, Forward<TypedEditorData>(InTypedEditorData));
	}

#else

	/**
	 * Add a new channel to the proxy. Channel's address is stored internally as a voic* and should exist as long as the channel proxy does
	 *
	 * @param InChannel    The channel to add to this proxy. Should live for as long as the proxy does. Any re-allocation should be accompanied with a re-creation of the proxy
	 */
	template<typename ChannelType>
	void Add(ChannelType& InChannel)
	{
		AddInternal(InChannel);
	}

#endif

private:

	/**
	 * Implementation that adds a channel to an entry, creating a new entry for this channel type if necessary
	 *
	 * @param InChannel    The channel to add to this proxy. Should live for as long as the proxy does. Any re-allocation should be accompanied with a re-creation of the proxy
	 */
	template<typename ChannelType>
	int32 AddInternal(ChannelType& InChannel);

	friend struct FMovieSceneChannelProxy;
	/** Array of entryies, one per channel type. Inline allocation space for one entry since most sections only have one channel type. */
	TArray<FMovieSceneChannelEntry, TInlineAllocator<1>> Entries;
};


/**
 * Proxy type stored inside UMovieSceneSection for access to all its channels. Construction via either a single channel, or a FMovieSceneChannelData structure
 * This proxy exists as a generic accessor to any channel data existing in derived types
 */
struct MOVIESCENE_API FMovieSceneChannelProxy : TSharedFromThis<FMovieSceneChannelProxy>
{
public:

	/** Default construction - emtpy proxy */
	FMovieSceneChannelProxy(){}

	/**
	 * Construction via multiple channels
	 */
	FMovieSceneChannelProxy(FMovieSceneChannelData&& InChannels)
		: Entries(MoveTemp(InChannels.Entries))
	{}

	/** Not copyable or moveable to ensure that previously retrieved pointers remain valid for the lifetime of this object. */
	FMovieSceneChannelProxy(const FMovieSceneChannelProxy&) = delete;
	FMovieSceneChannelProxy& operator=(const FMovieSceneChannelProxy&) = delete;

	FMovieSceneChannelProxy(FMovieSceneChannelProxy&&) = delete;
	FMovieSceneChannelProxy& operator=(FMovieSceneChannelProxy&&) = delete;

public:

	/**
	 * Const access to all the entries in this proxy
	 *
	 * @return Array view of all this proxy's entries (channels grouped by type)
	 */
	TArrayView<const FMovieSceneChannelEntry> GetAllEntries() const
	{
		return Entries;
	}

	/**
	 * Find an entry by its channel type ID
	 *
	 * @return A pointer to the channel, or nullptr
	 */
	const FMovieSceneChannelEntry* FindEntry(uint32 ChannelTypeID) const;

	/**
	 * Find the index of the specified channel ptr in this proxy
	 *
	 * @param ChannelTypeID    The type ID of the channel
	 * @param ChannelPtr       The channel pointer to find
	 * @return The index of the channel if found, else INDEX_NONE
	 */
	int32 FindIndex(uint32 ChannelTypeID, void* ChannelPtr) const;

	/**
	 * Get all channels of the specified type
	 *
	 * @return A possibly empty array view of all the channels in this proxy that match the template type
	 */
	template<typename ChannelType>
	TArrayView<ChannelType*> GetChannels() const;

	/**
	 * Get the channel for the specified index of a particular type.
	 *
	 * @return A pointer to the channel, or nullptr if the index was invalid, or the type was not present
	 */
	template<typename ChannelType>
	ChannelType* GetChannel(int32 ChannelIndex) const;

	/**
	 * Get the channel for the specified index of a particular type.
	 *
	 * @return A pointer to the channel, or nullptr if the index was invalid, or the type was not present
	 */
	void* GetChannel(uint32 ChannelTypeID, int32 ChannelIndex) const;

	/**
	 * Make a channel handle out of the specified raw pointer that will become nullptr if this proxy is re-allocated
	 *
	 * @return A handle to the supplied channel that will become nullptr when the proxy is reallocated
	 */
	template<typename ChannelType>
	TMovieSceneChannelHandle<ChannelType> MakeHandle(ChannelType* RawChannelPtr) const;

#if !WITH_EDITOR


	/**
	 * Construction via a single channel, and its editor data
	 * Channel's address is stored internally as a voic* and should exist as long as this channel proxy does.
	 */
	template<typename ChannelType>
	FMovieSceneChannelProxy(ChannelType& InChannel);

#else

	/**
	 * Construction via a single channel, and its editor data
	 * Channel's address is stored internally as a voic* and should exist as long as this channel proxy does.
	 */
	template<typename ChannelType>
	FMovieSceneChannelProxy(ChannelType& InChannel, const FMovieSceneChannelEditorData& InCommonEditorData);

	/**
	 * Construction via a single channel, and its editor data
	 * Channel's address is stored internally as a voic* and should exist as long as this channel proxy does.
	 */
	template<typename ChannelType, typename TypedEditorData>
	FMovieSceneChannelProxy(ChannelType& InChannel, const FMovieSceneChannelEditorData& InCommonEditorData, TypedEditorData&& InTypedEditorData);


	/**
	 * Access all the common editor data for the templated channel type
	 *
	 * @return A potentially empty array view for all the common editor data for the template channel type
	 */
	template<typename ChannelType>
	TArrayView<const FMovieSceneChannelEditorData> GetEditorData() const;

	/**
	 * Access all the specialized data for the templated channel type
	 *
	 * @return A potentially empty array view for all the specialized editor data for the template channel type
	 */
	template<typename ChannelType>
	TArrayView<const typename TMovieSceneChannelTraits<ChannelType>::EditorDataType> GetAllSpecializedEditorData() const;

#endif

private:

	/** Do not expose shared-ownership semantics of this object */
	using TSharedFromThis::AsShared;

	/** Array of channel entries, one per channel type. Should never be changed or reallocated after construction to keep pointers alive. */
	TArray<FMovieSceneChannelEntry, TInlineAllocator<1>> Entries;
};


/**
 * Implementation that adds a channel to an entry, creating a new entry for this channel type if necessary
 *
 * @param InChannel    The channel to add to this proxy. Should live for as long as the proxy does. Any re-allocation should be accompanied with a re-creation of the proxy
 */
template<typename ChannelType>
int32 FMovieSceneChannelData::AddInternal(ChannelType& InChannel)
{
	// Find the entry for this channel's type
	const uint32 ChannelID = ChannelType::GetChannelID();

	// Find the first entry that has a >= channel ID
	int32 ChannelTypeIndex = Algo::LowerBoundBy(Entries, ChannelID, &FMovieSceneChannelEntry::ChannelID);

	// If the index we found isn't valid, or it's not the channel we want, we need to add a new entry there
	if (ChannelTypeIndex >= Entries.Num() || Entries[ChannelTypeIndex].GetChannelID() != ChannelID)
	{
		Entries.Insert(FMovieSceneChannelEntry(ChannelID, InChannel), ChannelTypeIndex);
	}

	check(Entries.IsValidIndex(ChannelTypeIndex));

	// Add the channel to the channels array
	Entries[ChannelTypeIndex].Channels.Add(&InChannel);
	return ChannelTypeIndex;
}


/**
 * Get all channels of the specified type
 *
 * @return A possibly empty array view of all the channels in this proxy that match the template type
 */
template<typename ChannelType>
TArrayView<ChannelType*> FMovieSceneChannelProxy::GetChannels() const
{
	const uint32 ChannelTypeID = ChannelType::GetChannelID();
	const FMovieSceneChannelEntry* FoundEntry = FindEntry(ChannelTypeID);

	if (FoundEntry)
	{
		return TArrayView<ChannelType*>((ChannelType**)(FoundEntry->Channels.GetData()), FoundEntry->Channels.Num());
	}
	return TArrayView<ChannelType*>();
}


/**
 * Get the channel for the specified index of a particular type.
 *
 * @return A pointer to the channel, or nullptr if the index was invalid, or the type was not present
 */
template<typename ChannelType>
ChannelType* FMovieSceneChannelProxy::GetChannel(int32 ChannelIndex) const
{
	TArrayView<ChannelType*> Channels = GetChannels<ChannelType>();
	return Channels.IsValidIndex(ChannelIndex) ? Channels[ChannelIndex] : nullptr;
}


/**
 * Make a channel handle out of the specified raw pointer that will become nullptr if this proxy is re-allocated
 *
 * @return A handle to the supplied channel that will become nullptr when the proxy is reallocated
 */
template<typename ChannelType>
TMovieSceneChannelHandle<ChannelType> FMovieSceneChannelProxy::MakeHandle(ChannelType* RawChannelPtr) const
{
	TWeakPtr<ChannelType> WeakChannel = TSharedPtr<ChannelType>(AsShared(), RawChannelPtr);
	return TMovieSceneChannelHandle<ChannelType>(WeakChannel);
}


#if !WITH_EDITOR


/**
 * Construction via a single channel, and its editor data
 * Channel's address is stored internally as a voic* and should exist as long as this channel proxy does.
 */
template<typename ChannelType>
FMovieSceneChannelProxy::FMovieSceneChannelProxy(ChannelType& InChannel)
{
	const uint32 ChannelID = ChannelType::GetChannelID();
	Entries.Add(FMovieSceneChannelEntry(ChannelID, InChannel));
	Entries[0].Channels.Add(&InChannel);
}


#else	// !WITH_EDITOR


/**
 * Construction via a single channel, and its editor data
 * Channel's address is stored internally as a voic* and should exist as long as this channel proxy does.
 */
template<typename ChannelType>
FMovieSceneChannelProxy::FMovieSceneChannelProxy(ChannelType& InChannel, const FMovieSceneChannelEditorData& InCommonEditorData)
{
	static_assert(TIsSame<typename TMovieSceneChannelTraits<ChannelType>::EditorDataType, void>::Value, "Must supply typed editor data according to the channel's traits.");

	const uint32 ChannelID = ChannelType::GetChannelID();
	Entries.Add(FMovieSceneChannelEntry(ChannelID, InChannel));
	Entries[0].Channels.Add(&InChannel);
	Entries[0].AddEditorData<ChannelType>(InCommonEditorData);
}


/**
 * Construction via a single channel, its editor data, and its specialized editor data. Compulsary for channel types with a TMovieSceneChannelTraits<ChannelType>::EditorDataType.
 * Channel's address is stored internally as a voic* and should exist as long as this channel proxy does.
 */
template<typename ChannelType, typename TypedEditorData>
FMovieSceneChannelProxy::FMovieSceneChannelProxy(ChannelType& InChannel, const FMovieSceneChannelEditorData& InCommonEditorData, TypedEditorData&& InTypedEditorData)
{
	const uint32 ChannelID = ChannelType::GetChannelID();
	Entries.Add(FMovieSceneChannelEntry(ChannelID, InChannel));
	Entries[0].Channels.Add(&InChannel);
	Entries[0].AddEditorData<ChannelType>(InCommonEditorData, Forward<TypedEditorData>(InTypedEditorData));
}


/**
 * Access all the specialized data for the templated channel type
 *
 * @return A potentially empty array view for all the specialized editor data for the template channel type
 */
template<typename ChannelType>
TArrayView<const typename TMovieSceneChannelTraits<ChannelType>::EditorDataType> FMovieSceneChannelProxy::GetAllSpecializedEditorData() const
{
	const uint32 ChannelTypeID = ChannelType::GetChannelID();
	const FMovieSceneChannelEntry* FoundEntry = FindEntry(ChannelTypeID);

	if (FoundEntry)
	{
		return FoundEntry->GetAllSpecializedEditorData<ChannelType>();
	}
	return TArrayView<const typename TMovieSceneChannelTraits<ChannelType>::EditorDataType>();
}


/**
 * Access all the common editor data for the templated channel type
 *
 * @return A potentially empty array view for all the common editor data for the template channel type
 */
template<typename ChannelType>
TArrayView<const FMovieSceneChannelEditorData> FMovieSceneChannelProxy::GetEditorData() const
{
	const uint32 ChannelTypeID = ChannelType::GetChannelID();
	const FMovieSceneChannelEntry* FoundEntry = FindEntry(ChannelTypeID);

	if (FoundEntry)
	{
		return FoundEntry->GetCommonEditorData();
	}
	return TArrayView<const FMovieSceneChannelEditorData>();
}

#endif	// !WITH_EDITOR