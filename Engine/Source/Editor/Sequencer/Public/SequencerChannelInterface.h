// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISequencerChannelInterface.h"
#include "SequencerChannelTraits.h"
#include "MovieSceneSection.h"
#include "IKeyArea.h"
#include "CurveModel.h"

/**
 * Templated channel interface that calls overloaded functions matching the necessary channel types.
 * Designed this way to allow for specific customization of key-channel behavior without having to reimplement swathes of boilerplate.
 * This base interface implements common functions that do not require specialized editor data.
 *
 * Behavior can be overridden for any channel type by declaring an overloaded function for the relevant channel type in the same namespace as the channel.
 * For instance, to implement how to retrieve key times from a channel, implement the following function:
 *
 * void GetKeyTimes(FMyChannelType* InChannel, TArrayView<const FKeyHandle> InHandles, TArrayView<FFrameNumber> OutKeyTimes);
 */
template<typename ChannelType>
struct TSequencerChannelInterfaceCommon : ISequencerChannelInterface
{
	/**
	 * Check whether any of the specified channels have any keys
	 *
	 * @return true if so, false otherwise
	 */
	virtual bool HasAnyKeys_Raw(TArrayView<void* const> Ptrs) const override
	{
		using namespace Sequencer;

		for (void* Ptr : Ptrs)
		{
			if (Ptr && HasAnyKeys(static_cast<ChannelType*>(Ptr)))
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * Get key information pertaining to all keys that exist within the specified range
	 *
	 * @param Channel               The channel to query
	 * @param WithinRange           The range within which to return key information
	 * @param OutKeyTimes           (Optional) Array to receive key times
	 * @param OutKeyHandles         (Optional) Array to receive key handles
	 */
	virtual void GetKeys_Raw(void* InChannel, const TRange<FFrameNumber>& WithinRange, TArray<FFrameNumber>* OutKeyTimes, TArray<FKeyHandle>* OutKeyHandles) const override
	{
		using namespace Sequencer;
		GetKeys(static_cast<ChannelType*>(InChannel), WithinRange, OutKeyTimes, OutKeyHandles);
	}

	/**
	 * Get all key times for the specified key handles
	 *
	 * @param Channel               The channel to query
	 * @param InHandles             Array of handles to get times for
	 * @param OutKeyTimes           Pre-sized array of key times to set. Invalid key handles will not assign to this array. Must match size of InHandles
	 */
	virtual void GetKeyTimes_Raw(void* InChannel, TArrayView<const FKeyHandle> InHandles, TArrayView<FFrameNumber> OutKeyTimes) const override
	{
		check(InHandles.Num() == OutKeyTimes.Num());

		using namespace Sequencer;
		GetKeyTimes(static_cast<ChannelType*>(InChannel), InHandles, OutKeyTimes);
	}

	/**
	 * Set key times for the specified key handles
	 *
	 * @param Channel               The channel to query
	 * @param InHandles             Array of handles to get times for
	 * @param InKeyTimes            Array of times to apply - one per handle
	 */
	virtual void SetKeyTimes_Raw(void* InChannel, TArrayView<const FKeyHandle> InHandles, TArrayView<const FFrameNumber> InKeyTimes) const override
	{
		check(InHandles.Num() == InKeyTimes.Num());

		using namespace Sequencer;
		SetKeyTimes(static_cast<ChannelType*>(InChannel), InHandles, InKeyTimes);
	}

	/**
	 * Duplicate the keys for the specified key handles
	 *
	 * @param Channel               The channel to query
	 * @param InHandles             Array of handles to duplicate
	 * @param OutKeyTimes           Pre-sized array to receive duplicated key handles. Invalid key handles will not be assigned to this array. Must match size of InHandles
	 */
	virtual void DuplicateKeys_Raw(void* InChannel, TArrayView<const FKeyHandle> InHandles, TArrayView<FKeyHandle> OutNewHandles) const override
	{
		check(InHandles.Num() == OutNewHandles.Num());

		using namespace Sequencer;
		DuplicateKeys(static_cast<ChannelType*>(InChannel), InHandles, OutNewHandles);
	}

	/**
	 * Delete the keys for the specified key handles
	 *
	 * @param Channel               The channel to query
	 * @param InHandles             Array of handles to delete
	 */
	virtual void DeleteKeys_Raw(void* InChannel, TArrayView<const FKeyHandle> InHandles) const override
	{
		using namespace Sequencer;
		DeleteKeys(static_cast<ChannelType*>(InChannel), InHandles);
	}

	/**
	 * Copy all the keys specified in KeyMask to the specified clipboard
	 *
	 * @param Channel               The channel to copy from
	 * @param Section               The section that owns the channel
	 * @param KeyAreaName           The name of the key area
	 * @param ClipboardBuilder      The structure responsible for building clipboard information for each key
	 * @param KeyMask               A specific set of keys to copy
	 */
	virtual void CopyKeys_Raw(void* InChannel, const UMovieSceneSection* Section, FName KeyAreaName, FMovieSceneClipboardBuilder& ClipboardBuilder, TArrayView<const FKeyHandle> KeyMask) const override
	{
		using namespace Sequencer;
		CopyKeys(static_cast<ChannelType*>(InChannel), Section, KeyAreaName, ClipboardBuilder, KeyMask);
	}

	/**
	 * Paste the specified key track into the specified channel
	 *
	 * @param Channel               The channel to copy from
	 * @param Section               The section that owns the channel
	 * @param KeyTrack              The source clipboard data to paste
	 * @param SrcEnvironment        The environment the source data was copied from
	 * @param DstEnvironment        The environment we're pasting into
	 * @param OutPastedKeys         Array to receive key handles for any pasted keys
	 */
	virtual void PasteKeys_Raw(void* InChannel, UMovieSceneSection* Section, const FMovieSceneClipboardKeyTrack& KeyTrack, const FMovieSceneClipboardEnvironment& SrcEnvironment, const FSequencerPasteEnvironment& DstEnvironment, TArray<FKeyHandle>& OutPastedKeys) const override
	{
		using namespace Sequencer;
		PasteKeys(static_cast<ChannelType*>(InChannel), Section, KeyTrack, SrcEnvironment, DstEnvironment, OutPastedKeys);
	}

	/**
	 * Get an editable key struct for the specified key
	 *
	 * @param Channel               The channel on which the key resides
	 * @param KeyHandle             Handle of the key to get
	 * @return A shared editable key struct
	 */
	virtual TSharedPtr<FStructOnScope> GetKeyStruct_Raw(TMovieSceneChannelHandle<void> InChannel, FKeyHandle KeyHandle) const override
	{
		using namespace Sequencer;
		return GetKeyStruct(InChannel.Cast<ChannelType>(), KeyHandle);
	}

	/**
	 * Check whether an editor on the sequencer node tree can be created for the specified channel
	 *
	 * @param Channel               The channel to check
	 * @return true if a key editor should be constructed, false otherwise
	 */
	virtual bool CanCreateKeyEditor_Raw(void* InChannel) const override
	{
		using namespace Sequencer;
		return CanCreateKeyEditor(static_cast<ChannelType*>(InChannel));
	}

	/**
	 * Extend the key context menu
	 *
	 * @param MenuBuilder           The menu builder used to create this context menu
	 * @param Channels              Array of channels and handles that are being shown in the context menu
	 * @param InSequencer           The currently active sequencer
	 */
	virtual void ExtendKeyMenu_Raw(FMenuBuilder& MenuBuilder, TArrayView<const TChannelAndHandles<void>> ChannelsAndHandles, TWeakPtr<ISequencer> InSequencer) const override
	{
		using namespace Sequencer;
		TArray<TChannelAndHandles<ChannelType>> TypedChannels;

		for (const TChannelAndHandles<void>& Ptr : ChannelsAndHandles)
		{
			TChannelAndHandles<ChannelType> TypedChannelAndHandles;
			TypedChannelAndHandles.Section = Ptr.Section;
			TypedChannelAndHandles.Handles = Ptr.Handles;
			TypedChannelAndHandles.Channel = Ptr.Channel.Cast<ChannelType>();

			TypedChannels.Add(MoveTemp(TypedChannelAndHandles));
		}

		ExtendKeyMenu(MenuBuilder, MoveTemp(TypedChannels), InSequencer);
	}

	/**
	 * Extend the section context menu
	 *
	 * @param MenuBuilder           The menu builder used to create this context menu
	 * @param Channels              Array of type specific channels that exist in the selected sections
	 * @param Sections              Array of sections being shown on the context menu
	 * @param InSequencer           The currently active sequencer
	 */
	virtual void ExtendSectionMenu_Raw(FMenuBuilder& MenuBuilder, TArrayView<TMovieSceneChannelHandle<void> const> Channels, TArrayView<UMovieSceneSection* const> Sections, TWeakPtr<ISequencer> InSequencer) const override
	{
		using namespace Sequencer;
		TArray<TMovieSceneChannelHandle<ChannelType>> TypedChannels;

		for (const TMovieSceneChannelHandle<void>& RawHandle : Channels)
		{
			TypedChannels.Add(RawHandle.Cast<ChannelType>());
		}

		ExtendSectionMenu(MenuBuilder, MoveTemp(TypedChannels), Sections, InSequencer);
	}

	/**
	 * Gather information on how to draw the specified keys
	 *
	 * @param Channel               The channel to query
	 * @param InKeyHandles          Array of handles to duplicate
	 * @param OutKeyDrawParams      Pre-sized array to receive key draw parameters. Invalid key handles will not be assigned to this array. Must match size of InKeyHandles.
	 */
	virtual void DrawKeys_Raw(void* InChannel, TArrayView<const FKeyHandle> InKeyHandles, TArrayView<FKeyDrawParams> OutKeyDrawParams) const override
	{
		check(InKeyHandles.Num() == OutKeyDrawParams.Num());

		using namespace Sequencer;
		DrawKeys(static_cast<ChannelType*>(InChannel), InKeyHandles, OutKeyDrawParams);
	}

	/**
	 * Create a new model for this channel that can be used on the curve editor interface
	 *
	 * @return (Optional) A new model to be added to a curve editor
	 */
	virtual TUniquePtr<FCurveModel> CreateCurveEditorModel_Raw(TMovieSceneChannelHandle<void> InChannel, UMovieSceneSection* OwningSection, TSharedRef<ISequencer> InSequencer) const override
	{
		using namespace Sequencer;
		return CreateCurveEditorModel(InChannel.Cast<ChannelType>(), OwningSection, InSequencer);
	}
};

template<typename ChannelType, bool HasSpecializedData> struct TSequencerChannelInterfaceBase;


/**
 * Specialized base interface for channel types that do not specify specialized editor data (ie, TMovieSceneChannelTraits<>::EditorDataType is void)
 */
template<typename ChannelType>
struct TSequencerChannelInterfaceBase<ChannelType, false> : TSequencerChannelInterfaceCommon<ChannelType>
{
	/**
	 * Add (or update) a key to the specified channel using it's current value at that time, or some external value specified by the specialized editor data
	 *
	 * @param Channel               The channel to add a key to
	 * @param SpecializedEditorData A pointer to the specialized editor data for this channel of type TMovieSceneChannelTraits<>::EditorDataType
	 * @param InTime                The time at which to add a key
	 * @param InSequencer           The currently active sequencer
	 * @param ObjectBindingID       The object binding ID for the track that this channel resides within
	 * @param PropertyBindings      (Optional) Property bindings where this channel exists on a property track
	 * @return A handle to the new or updated key
	 */
	virtual FKeyHandle AddOrUpdateKey_Raw(void* InChannel, const void* SpecializedEditorData, FFrameNumber InTime, ISequencer& Sequencer, const FGuid& ObjectBindingID, FTrackInstancePropertyBindings* PropertyBindings) const override
	{
		using namespace Sequencer;
		return AddOrUpdateKey(static_cast<ChannelType*>(InChannel), InTime, Sequencer, ObjectBindingID, PropertyBindings);
	}

	/**
	 * Create an editor on the sequencer node tree
	 *
	 * @param Channel               The channel to check
	 * @param SpecializedEditorData A pointer to the specialized editor data for this channel of type TMovieSceneChannelTraits<>::EditorDataType
	 * @param Section               The section that owns this channel
	 * @param InObjectBindingID     The ID of the object this key area's track is bound to
	 * @param PropertyBindings      (Optional) Property bindings where this channel exists on a property track
	 * @param Sequencer             The currently active sequencer
	 * @return The editor widget to display on the node tree
	 */
	virtual TSharedRef<SWidget> CreateKeyEditor_Raw(void* InChannel, const void* SpecializedEditorData, UMovieSceneSection* Section, const FGuid& InObjectBindingID, TWeakPtr<FTrackInstancePropertyBindings> PropertyBindings, TWeakPtr<ISequencer> Sequencer) const override
	{
		using namespace Sequencer;
		return CreateKeyEditor(static_cast<ChannelType*>(InChannel), Section, InObjectBindingID, PropertyBindings, Sequencer);
	}
};

/**
 * Specialized base interface for channel types that specify specialized editor data (ie, TMovieSceneChannelTraits<>::EditorDataType is not void)
 */
template<typename ChannelType>
struct TSequencerChannelInterfaceBase<ChannelType, true> : TSequencerChannelInterfaceCommon<ChannelType>
{
	typedef typename TMovieSceneChannelTraits<ChannelType>::EditorDataType EditorDataType;

	/**
	 * Add (or update) a key to the specified channel using it's current value at that time, or some external value specified by the specialized editor data
	 *
	 * @param Channel               The channel to add a key to
	 * @param SpecializedEditorData A pointer to the specialized editor data for this channel of type TMovieSceneChannelTraits<>::EditorDataType
	 * @param InTime                The time at which to add a key
	 * @param InSequencer           The currently active sequencer
	 * @param ObjectBindingID       The object binding ID for the track that this channel resides within
	 * @param PropertyBindings      (Optional) Property bindings where this channel exists on a property track
	 * @return A handle to the new or updated key
	 */
	virtual FKeyHandle AddOrUpdateKey_Raw(void* InChannel, const void* SpecializedEditorData, FFrameNumber InTime, ISequencer& Sequencer, const FGuid& ObjectBindingID, FTrackInstancePropertyBindings* PropertyBindings) const override
	{
		using namespace Sequencer;

		// Specialized data must be available for this interface
		check(SpecializedEditorData);

		const auto* TypedEditorData = static_cast<const EditorDataType*>(SpecializedEditorData);
		return AddOrUpdateKey(static_cast<ChannelType*>(InChannel), *TypedEditorData, InTime, Sequencer, ObjectBindingID, PropertyBindings);
	}

	/**
	 * Create an editor on the sequencer node tree
	 *
	 * @param Channel               The channel to check
	 * @param SpecializedEditorData A pointer to the specialized editor data for this channel of type TMovieSceneChannelTraits<>::EditorDataType
	 * @param Section               The section that owns this channel
	 * @param InObjectBindingID     The ID of the object this key area's track is bound to
	 * @param PropertyBindings      (Optional) Property bindings where this channel exists on a property track
	 * @param Sequencer             The currently active sequencer
	 * @return The editor widget to display on the node tree
	 */
	virtual TSharedRef<SWidget> CreateKeyEditor_Raw(void* InChannel, const void* SpecializedEditorData, UMovieSceneSection* Section, const FGuid& InObjectBindingID, TWeakPtr<FTrackInstancePropertyBindings> PropertyBindings, TWeakPtr<ISequencer> Sequencer) const override
	{
		using namespace Sequencer;

		// Specialized data must be available for this interface
		check(SpecializedEditorData);

		const auto* TypedEditorData = static_cast<const EditorDataType*>(SpecializedEditorData);
		return CreateKeyEditor(static_cast<ChannelType*>(InChannel), *TypedEditorData, Section, InObjectBindingID, PropertyBindings, Sequencer);
	}
};

/** Generic sequencer channel interface to any channel type */
template<typename ChannelType>
struct TSequencerChannelInterface : TSequencerChannelInterfaceBase<ChannelType, !TIsSame<typename TMovieSceneChannelTraits<ChannelType>::EditorDataType, void>::Value>
{
};


/**
 * Implementation of the RegisterChannelInterface function that is the only function that needs to see the definition of TSequencerChannelInterface
 * Defined in here to prevent all the necessary headers being included wherever ISequencerModule is needed.
 */
template<typename ChannelType>
void ISequencerModule::RegisterChannelInterface()
{
	const uint32 ChannelID = ChannelType::GetChannelID();
	check(!ChannelToEditorInterfaceMap.Contains(ChannelID));
	ChannelToEditorInterfaceMap.Add(ChannelID, TUniquePtr<ISequencerChannelInterface>(new TSequencerChannelInterface<ChannelType>()));
}