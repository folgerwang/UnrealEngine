// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "ISequencerChannelInterface.h"
#include "KeyDrawParams.h"
#include "Channels/MovieSceneChannelData.h"
#include "MovieSceneClipboard.h"
#include "SequencerClipboardReconciler.h"
#include "SequencerKeyStructGenerator.h"
#include "Widgets/SNullWidget.h"
#include "ISequencer.h"

/** Utility struct representing a number of selected keys on a single channel */
template<typename ChannelType>
struct TExtendKeyMenuParams
{
	/** The section on which the channel resides */
	TWeakObjectPtr<UMovieSceneSection> Section;

	/** The channel on which the keys reside */
	TMovieSceneChannelHandle<ChannelType> Channel;

	/** An array of key handles to operante on */
	TArray<FKeyHandle> Handles;
};

/**
 * Stub/default implementations for ISequencerChannelInterface functions.
 * Custom behaviour should be implemented by overloading the relevant function with the necessary channel/data types.
 * For example, to overload how to draw keys for FMyCustomChannelType, implement the following function in the same namespace as FMyCustomChannelType:
 *
 * void DrawKeys(FMyCustomChannelType* Channel, TArrayView<const FKeyHandle> InHandles, TArrayView<FKeyDrawParams> OutKeyDrawParams)
 * {
 * ...
 * }
 */
namespace Sequencer
{
	/**
	 * Extend the specified selected section context menu
	 *
	 * @param MenuBuilder    The menu builder that will construct the section context menu
	 * @param Channels       An array of all channels that are currently selected, in no particular order
	 * @param Sections       An array of all sections that the selected channels reside in
	 * @param InSequencer    The sequencer that is currently active
	 */
	template<typename ChannelType>
	void ExtendSectionMenu(FMenuBuilder& MenuBuilder, TArray<TMovieSceneChannelHandle<ChannelType>>&& Channels, TArrayView<UMovieSceneSection* const> Sections, TWeakPtr<ISequencer> InSequencer)
	{}


	/**
	 * Extend the specified selected key context menu
	 *
	 * @param MenuBuilder    The menu builder that will construct the section context menu
	 * @param Channels       An array of all channels that are currently selected, in no particular order
	 * @param Sections       An array of all sections that the selected channels reside in
	 * @param InSequencer    The sequencer that is currently active
	 */
	template<typename ChannelType>
	void ExtendKeyMenu(FMenuBuilder& MenuBuilder, TArray<TExtendKeyMenuParams<ChannelType>>&& InChannels, TWeakPtr<ISequencer> InSequencer)
	{}


	/**
	 * Get a transient key structure that can be added to a details panel to enable editing of a single key
	 *
	 * @param ChannelHandle  Handle to the channel in which the key resides
	 * @param KeyHandle      A handle to the key to edit
	 * @return A shared struct object, or nullptr
	 */
	template<typename ChannelType>
	TSharedPtr<FStructOnScope> GetKeyStruct(const TMovieSceneChannelHandle<ChannelType>& ChannelHandle, FKeyHandle KeyHandle)
	{
		return FSequencerKeyStructGenerator::Get().CreateKeyStructInstance(ChannelHandle, KeyHandle);
	}

	/**
	 * Check whether the specified channel can create a key editor widget that should be placed on the sequencer node tree 
	 *
	 * @param InChannel      The channel to check
	 * @return true if a key editor can be created, false otherwise
	 */
	inline bool CanCreateKeyEditor(const FMovieSceneChannel* InChannel)
	{
		return false;
	}



	/**
	 * Create a key editor widget for the specified channel with the channel's specialized editor data. Such widgets are placed on the sequencer node tree for a given key area node.
	 *
	 * @param InChannel          The channel to create a key editor for
	 * @param InOwningSection    The section that owns the channel
	 * @param InObjectBindingID  The object binding ID that this section's track is bound to
	 * @param InPropertyBindings Optionally supplied helper for accessing an object's property pertaining to this channel
	 * @param InSequencer        The sequencer currently active
	 * @return The key editor widget
	 */
	inline TSharedRef<SWidget> CreateKeyEditor(
		const FMovieSceneChannelHandle&          InChannel,
		UMovieSceneSection*                      InOwningSection,
		const FGuid&                             InObjectBindingID,
		TWeakPtr<FTrackInstancePropertyBindings> InPropertyBindings,
		TWeakPtr<ISequencer>                     InSequencer
		)
	{
		return SNullWidget::NullWidget;
	}



	/**
	 * Add a key at the specified time (or update an existing key) with the channel's current value at that time
	 *
	 * @param InChannel          The channel to create a key for
	 * @param InChannelData      The channel's data
	 * @param InTime             The time at which to add a key
	 * @param InSequencer        The currently active sequencer
	 * @param InDefaultValue     The default value to use if evaluation of the channel failed
	 * @return A handle to the added (or updated) key
	 */
	template<typename ChannelType, typename ValueType>
	FKeyHandle EvaluateAndAddKey(ChannelType* InChannel, const TMovieSceneChannelData<ValueType>& InChannelData, FFrameNumber InTime, ISequencer& InSequencer, ValueType InDefaultValue = ValueType{})
	{
		using namespace MovieScene;

		ValueType ValueAtTime = InDefaultValue;
		EvaluateChannel(InChannel, InTime, ValueAtTime);

		return AddKeyToChannel(InChannel, InTime, ValueAtTime, InSequencer.GetKeyInterpolation());
	}
	


	/**
	 * Retrieve a channel's external value, and add it to the channel as a new key (or update an existing key with its value)
	 *
	 * @param InChannel          The channel to create a key for
	 * @param InExternalValue    The external value definition
	 * @param InTime             The time at which to add a key
	 * @param InSequencer        The currently active sequencer
	 * @param InObjectBindingID  The object binding ID that this section's track is bound to
	 * @param InPropertyBindings Optionally supplied helper for accessing an object's property pertaining to this channel
	 * @return (Optional) A handle to the added (or updated) key
	 */
	template<typename ChannelType, typename ValueType>
	TOptional<FKeyHandle> AddKeyForExternalValue(
		ChannelType*                               InChannel,
		const TMovieSceneExternalValue<ValueType>& InExternalValue,
		FFrameNumber                               InTime,
		ISequencer&                                InSequencer,
		const FGuid&                               InObjectBindingID,
		FTrackInstancePropertyBindings*            InPropertyBindings
		)
	{
		using namespace MovieScene;

		// Add a key for the current value of the valid first object we can find
		if (InExternalValue.OnGetExternalValue && InObjectBindingID.IsValid())
		{
			for (TWeakObjectPtr<> WeakObject : InSequencer.FindBoundObjects(InObjectBindingID, InSequencer.GetFocusedTemplateID()))
			{
				UObject* Object = WeakObject.Get();
				if (Object)
				{
					TOptional<ValueType> Value = InExternalValue.OnGetExternalValue(*Object, InPropertyBindings);
					if (Value.IsSet())
					{
						return AddKeyToChannel(InChannel, InTime, Value.GetValue(), InSequencer.GetKeyInterpolation());
					}
				}
			}
		}
		return TOptional<FKeyHandle>();
	}


	/**
	 * Add or update a key for this channel's current value
	 *
	 * @param InChannel          The channel to create a key for
	 * @param InSectionToKey     The Section to key
	 * @param InTime             The time at which to add a key
	 * @param InSequencer        The currently active sequencer
	 * @param InObjectBindingID  The object binding ID that this section's track is bound to
	 * @param InPropertyBindings Optionally supplied helper for accessing an object's property pertaining to this channel
	 * @return A handle to the added (or updated) key
	 */
	template<typename ChannelType>
	FKeyHandle AddOrUpdateKey(
		ChannelType*                    InChannel,
		UMovieSceneSection*             InSectionToKey,
		FFrameNumber                    InTime,
		ISequencer&                     InSequencer,
		const FGuid&                    InObjectBindingID,
		FTrackInstancePropertyBindings* InPropertyBindings
		)
	{
		return EvaluateAndAddKey(InChannel, InChannel->GetData(), InTime, InSequencer);
	}


	/**
	 * Add or update a key for this channel's current value, using an external value if possible
	 *
	 * @param InChannel          The channel to create a key for
	 * @param InSectionToKey     The Section to key
	 * @param InExternalValue    The external value definition
	 * @param InTime             The time at which to add a key
	 * @param InSequencer        The currently active sequencer
	 * @param InObjectBindingID  The object binding ID that this section's track is bound to
	 * @param InPropertyBindings Optionally supplied helper for accessing an object's property pertaining to this channel
	 * @return A handle to the added (or updated) key
	 */
	template<typename ChannelType, typename ValueType>
	FKeyHandle AddOrUpdateKey(
		ChannelType*                               InChannel,
		UMovieSceneSection*             SectionToKey,
		const TMovieSceneExternalValue<ValueType>& InExternalValue,
		FFrameNumber                               InTime,
		ISequencer&                                InSequencer,
		const FGuid&                               InObjectBindingID,
		FTrackInstancePropertyBindings*            InPropertyBindings)
	{
		using namespace MovieScene;

		TOptional<FKeyHandle> Handle = AddKeyForExternalValue(InChannel, InExternalValue, InTime, InSequencer, InObjectBindingID, InPropertyBindings);
		if (!Handle.IsSet())
		{
			ValueType ValueAtTime{};
			EvaluateChannel(InChannel, InTime, ValueAtTime);
		
			Handle = AddKeyToChannel(InChannel, InTime, ValueAtTime, InSequencer.GetKeyInterpolation());
		}

		return Handle.GetValue();
	}


	/**
	 * Gather key draw information from a channel for a specific set of keys
	 *
	 * @param InChannel          The channel to duplicate keys in
	 * @param InHandles          Array of key handles that should be deleted
	 * @param OutKeyDrawParams   Array to receive key draw information. Must be exactly the size of InHandles.
	 */
	SEQUENCER_API void DrawKeys(FMovieSceneChannel* Channel, TArrayView<const FKeyHandle> InHandles, TArrayView<FKeyDrawParams> OutKeyDrawParams);


	/**
	 * Copy the specified keys from a channel
	 *
	 * @param InChannel          The channel to duplicate keys in
	 * @param InSection          The section that owns this channel
	 * @param KeyAreaName        The name of the key area representing this channel
	 * @param ClipboardBuilder   Structure for populating the clipboard
	 * @param InHandles          Array of key handles that should be copied
	 */
	template<typename ChannelType>
	void CopyKeys(ChannelType* InChannel, const UMovieSceneSection* InSection, FName KeyAreaName, FMovieSceneClipboardBuilder& ClipboardBuilder, TArrayView<const FKeyHandle> InHandles)
	{
		UMovieSceneTrack* Track = InSection ? InSection->GetTypedOuter<UMovieSceneTrack>() : nullptr;
		if (!Track)
		{
			return;
		}

		FMovieSceneClipboardKeyTrack* KeyTrack = nullptr;

		auto ChannelData = InChannel->GetData();
		TArrayView<const FFrameNumber> Times = ChannelData.GetTimes();
		auto Values = ChannelData.GetValues();

		for (FKeyHandle Handle : InHandles)
		{
			const int32 KeyIndex = ChannelData.GetIndex(Handle);
			if (KeyIndex != INDEX_NONE)
			{
				FFrameNumber KeyTime  = Times[KeyIndex];
				auto         KeyValue = Values[KeyIndex];

				if (!KeyTrack)
				{
					KeyTrack = &ClipboardBuilder.FindOrAddKeyTrack<decltype(KeyValue)>(KeyAreaName, *Track);
				}

				KeyTrack->AddKey(KeyTime, KeyValue);
			}
		}
	}

	/**
	 * Paste the clipboard contents onto a channel
	 *
	 * @param InChannel          The channel to duplicate keys in
	 * @param Section            The section that owns this channel
	 * @param KeyTrack           The clipboard track to paste
	 * @param SrcEnvironment     The source clipboard environment that was originally copied
	 * @param DstEnvironment     The destination clipboard environment that we're copying to
	 * @param OutPastedKeys      Array of key handles that should receive any pasted keys
	 */
	template<typename ChannelType>
	void PasteKeys(ChannelType* InChannel, UMovieSceneSection* Section, const FMovieSceneClipboardKeyTrack& KeyTrack, const FMovieSceneClipboardEnvironment& SrcEnvironment, const FSequencerPasteEnvironment& DstEnvironment, TArray<FKeyHandle>& OutPastedKeys)
	{
		if (!Section || !Section->TryModify())
		{
			return;
		}

		FFrameTime PasteAt = DstEnvironment.CardinalTime;

		auto ChannelData = InChannel->GetData();

		TRange<FFrameNumber> NewRange = Section->GetRange();

		auto ForEachKey = [Section, PasteAt, &NewRange, &ChannelData, &OutPastedKeys, &SrcEnvironment, &DstEnvironment](const FMovieSceneClipboardKey& Key)
		{
			FFrameNumber Time = (PasteAt + FFrameRate::TransformTime(Key.GetTime(), SrcEnvironment.TickResolution, DstEnvironment.TickResolution)).FloorToFrame();

			NewRange = TRange<FFrameNumber>::Hull(NewRange, TRange<FFrameNumber>(Time));

			typedef typename TDecay<decltype(ChannelData.GetValues()[0])>::Type KeyType;
			KeyType NewKey = Key.GetValue<KeyType>();

			FKeyHandle KeyHandle = ChannelData.UpdateOrAddKey(Time, NewKey);
			OutPastedKeys.Add(KeyHandle);
			return true;
		};
		KeyTrack.IterateKeys(ForEachKey);

		Section->SetRange(NewRange);
	}



	/**
	 * Create a new model for the specified channel that can be used on the curve editor interface
	 *
	 * @return (Optional) A new model to be added to a curve editor
	 */
	SEQUENCER_API TUniquePtr<FCurveModel> CreateCurveEditorModel(const FMovieSceneChannelHandle& ChannelHandle, UMovieSceneSection* OwningSection, TSharedRef<ISequencer> InSequencer);

}	// namespace Sequencer