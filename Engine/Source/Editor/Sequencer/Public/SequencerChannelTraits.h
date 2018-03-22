// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "ISequencerChannelInterface.h"
#include "KeyDrawParams.h"
#include "Channels/MovieSceneChannel.h"
#include "MovieSceneClipboard.h"
#include "SequencerClipboardReconciler.h"
#include "SequencerGenericKeyStruct.h"
#include "Widgets/SNullWidget.h"
#include "ISequencer.h"

struct FSequencerGenericKeyStruct;

/**
 * Stub/default implementations for ISequencerChannelInterface functions.
 * Custom behaviour should be implemented by overloading the relevant function with the necessary channel/data types.
 * For example, to overload HasAnyKeys for FMyCustomChannelType, implement the following function in the same namespace as FMyCustomChannelType:
 *
 * bool HasAnyKeys(const FMyCustomChannelType* InChannel)
 * {
 *    return InChannel.GetKeys().Num() != 0; }
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
	void ExtendKeyMenu(FMenuBuilder& MenuBuilder, TArray<TChannelAndHandles<ChannelType>>&& InChannels, TWeakPtr<ISequencer> InSequencer)
	{}


	/**
	 * Determine if the specified channel has any keys
	 *
	 * @param InChannel      The channel to check
	 * @return true if the channel has any keys, false otherwise
	 */
	template<typename ChannelType>
	bool HasAnyKeys(const ChannelType* InChannel)
	{
		return InChannel->GetInterface().GetTimes().Num() != 0;
	}


	/**
	 * Get a transient key structure that can be added to a details panel to enable editing of a single key
	 *
	 * @param ChannelHandle  Handle to the channel in which the key resides
	 * @param KeyHandle      A handle to the key to edit
	 * @return A shared struct object, or nullptr
	 */
	template<typename ChannelType>
	TSharedPtr<FStructOnScope> GetKeyStruct(TMovieSceneChannelHandle<ChannelType> ChannelHandle, FKeyHandle KeyHandle)
	{
		ChannelType* Channel = ChannelHandle.Get();
		if (Channel)
		{
			auto ChannelInterface = Channel->GetInterface();
			const int32 KeyIndex = ChannelInterface.GetIndex(KeyHandle);

			if (KeyIndex != INDEX_NONE)
			{
				TSharedRef<FStructOnScope> NewStruct = MakeShared<FStructOnScope>(FSequencerGenericKeyStruct::StaticStruct());
				FSequencerGenericKeyStruct* StructPtr = (FSequencerGenericKeyStruct*)NewStruct->GetStructMemory();

				StructPtr->Time = ChannelInterface.GetTimes()[KeyIndex];
				StructPtr->CustomizationImpl = MakeShared<TMovieSceneKeyStructCustomization<ChannelType>>(ChannelHandle, KeyHandle);

				return NewStruct;
			}
		}
		return nullptr;
	}



	/**
	 * Check whether the specified channel can create a key editor widget that should be placed on the sequencer node tree 
	 *
	 * @param InChannel      The channel to check
	 * @return true if a key editor can be created, false otherwise
	 */
	inline bool CanCreateKeyEditor(void* InChannel)
	{
		return false;
	}


	/**
	 * Create a key editor widget for the specified channel. Such widgets are placed on the sequencer node tree for a given key area node.
	 *
	 * @param InChannel          The channel to create a key editor for
	 * @param InOwningSection    The section that owns the channel
	 * @param InObjectBindingID  The object binding ID that this section's track is bound to
	 * @param InPropertyBindings Optionally supplied helper for accessing an object's property pertaining to this channel
	 * @param InSequencer        The sequencer currently active
	 * @return true if a key editor can be created, false otherwise
	 */
	inline TSharedRef<SWidget> CreateKeyEditor(
		void*                                    InChannel,
		UMovieSceneSection*                      InOwningSection,
		const FGuid&                             InObjectBindingID,
		TWeakPtr<FTrackInstancePropertyBindings> InPropertyBindings,
		TWeakPtr<ISequencer>                     InSequencer
		)
	{
		return SNullWidget::NullWidget;
	}


	/**
	 * Create a key editor widget for the specified channel with the channel's specialized editor data. Such widgets are placed on the sequencer node tree for a given key area node.
	 *
	 * @param InChannel          The channel to create a key editor for
	 * @param InEditorData       The channel's specialized editor data
	 * @param InOwningSection    The section that owns the channel
	 * @param InObjectBindingID  The object binding ID that this section's track is bound to
	 * @param InPropertyBindings Optionally supplied helper for accessing an object's property pertaining to this channel
	 * @param InSequencer        The sequencer currently active
	 * @return The key editor widget
	 */
	template<typename SpecializedEditorDataType>
	inline TSharedRef<SWidget> CreateKeyEditor(
		void*                                    InChannel,
		const SpecializedEditorDataType&         InEditorData,
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
	 * @param InChannelInterface The channel's interface
	 * @param InTime             The time at which to add a key
	 * @param InSequencer        The currently active sequencer
	 * @param InDefaultValue     The default value to use if evaluation of the channel failed
	 * @return A handle to the added (or updated) key
	 */
	template<typename ChannelType, typename ValueType>
	FKeyHandle EvaluateAndAddKey(ChannelType* InChannel, const TMovieSceneChannel<ValueType>& InChanneInterface, FFrameNumber InTime, ISequencer& InSequencer, ValueType InDefaultValue = ValueType{})
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
	 * @param InTime             The time at which to add a key
	 * @param InSequencer        The currently active sequencer
	 * @param InObjectBindingID  The object binding ID that this section's track is bound to
	 * @param InPropertyBindings Optionally supplied helper for accessing an object's property pertaining to this channel
	 * @return A handle to the added (or updated) key
	 */
	template<typename ChannelType>
	FKeyHandle AddOrUpdateKey(
		ChannelType*                    InChannel,
		FFrameNumber                    InTime,
		ISequencer&                     InSequencer,
		const FGuid&                    InObjectBindingID,
		FTrackInstancePropertyBindings* InPropertyBindings
		)
	{
		return EvaluateAndAddKey(InChannel, InChannel->GetInterface(), InTime, InSequencer);
	}


	/**
	 * Add or update a key for this channel's current value, using an external value if possible
	 *
	 * @param InChannel          The channel to create a key for
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
	 * Get all the keys in the given range. Resulting arrays must be the same size where indices correspond to both arrays.
	 *
	 * @param InChannel          The channel to get keys from
	 * @param WithinRange        The bounds to get keys for
	 * @param OutKeyTimes        Array to receive all key times within the given range
	 * @param OutKeyHandles      Array to receive all key handles within the given range
	 */
	template<typename ChannelType>
	void GetKeys(ChannelType* InChannel, const TRange<FFrameNumber>& WithinRange, TArray<FFrameNumber>* OutKeyTimes, TArray<FKeyHandle>* OutKeyHandles)
	{
		auto ChannelInterface = InChannel->GetInterface();
		TArrayView<const FFrameNumber> Times = ChannelInterface.GetTimes();
		if (!Times.Num())
		{
			return;
		}

		const int32 FirstIndex = WithinRange.GetLowerBound().IsClosed() ? Algo::LowerBound(Times, WithinRange.GetLowerBoundValue()) : 0;
		const int32 LastIndex  = WithinRange.GetUpperBound().IsClosed() ? Algo::UpperBound(Times, WithinRange.GetUpperBoundValue()) : Times.Num();

		const int32 NumInRange = LastIndex - FirstIndex;
		if (NumInRange > 0)
		{
			if (OutKeyTimes)
			{
				OutKeyTimes->Reserve(OutKeyTimes->Num() + NumInRange);
				OutKeyTimes->Append(&Times[FirstIndex], NumInRange);
			}

			if (OutKeyHandles)
			{
				OutKeyHandles->Reserve(OutKeyHandles->Num() + NumInRange);

				for (int32 Index = FirstIndex; Index < LastIndex; ++Index)
				{
					OutKeyHandles->Add(ChannelInterface.GetHandle(Index));
				}
			}
		}
	}


	/**
	 * Get key times for a number of keys in the specified channel
	 *
	 * @param InChannel          The channel to set key times on
	 * @param InHandles          Array of key handles that should have their times set
	 * @param OutKeyTimes        Array of times that should be set for each key handle. Must be exactly the size of InHandles
	 */
	template<typename ChannelType>
	void GetKeyTimes(ChannelType* InChannel, TArrayView<const FKeyHandle> InHandles, TArrayView<FFrameNumber> OutKeyTimes)
	{
		check(InHandles.Num() == OutKeyTimes.Num());

		auto ChannelInterface = InChannel->GetInterface();
		auto Times = ChannelInterface.GetTimes();
		for (int32 Index = 0; Index < InHandles.Num(); ++Index)
		{
			const int32 KeyIndex = ChannelInterface.GetIndex(InHandles[Index]);
			if (KeyIndex != INDEX_NONE)
			{
				OutKeyTimes[Index] = Times[KeyIndex];
			}
		}
	}


	/**
	 * Set key times for a number of keys in the specified channel
	 *
	 * @param InChannel          The channel to set key times on
	 * @param InHandles          Array of key handles that should have their times set
	 * @param InKeyTimes         Array of new times for each handle of the above array
	 */
	template<typename ChannelType>
	void SetKeyTimes(ChannelType* InChannel, TArrayView<const FKeyHandle> InHandles, TArrayView<const FFrameNumber> InKeyTimes)
	{
		check(InHandles.Num() == InKeyTimes.Num());

		auto ChannelInterface = InChannel->GetInterface();
		for (int32 Index = 0; Index < InHandles.Num(); ++Index)
		{
			const int32 KeyIndex = ChannelInterface.GetIndex(InHandles[Index]);
			if (KeyIndex != INDEX_NONE)
			{
				ChannelInterface.MoveKey(KeyIndex, InKeyTimes[Index]);
			}
		}
	}


	/**
	 * Duplicate a number of keys within the specified channel
	 *
	 * @param InChannel          The channel to duplicate keys in
	 * @param InHandles          Array of key handles that should be duplicated
	 * @param OutNewHandles      Array view to receive key handles for each duplicated key. Must exactly mathc the size of InHandles.
	 */
	template<typename ChannelType>
	void DuplicateKeys(ChannelType* InChannel, TArrayView<const FKeyHandle> InHandles, TArrayView<FKeyHandle> OutNewHandles)
	{
		auto ChannelInterface = InChannel->GetInterface();
		for (int32 Index = 0; Index < InHandles.Num(); ++Index)
		{
			const int32 KeyIndex = ChannelInterface.GetIndex(InHandles[Index]);
			if (KeyIndex == INDEX_NONE)
			{
				// we must add a handle even if the supplied handle does not relate to a key in this channel
				OutNewHandles[Index] = FKeyHandle::Invalid();
			}
			else
			{
				// Do not cache value and time arrays since they can be reallocated during this loop
				auto KeyCopy = ChannelInterface.GetValues()[KeyIndex];
				int32 NewKeyIndex = ChannelInterface.AddKey(ChannelInterface.GetTimes()[KeyIndex], MoveTemp(KeyCopy));
				OutNewHandles[Index] = ChannelInterface.GetHandle(NewKeyIndex);
			}
		}
	}


	/**
	 * Delete a number of keys from the specified channel
	 *
	 * @param InChannel          The channel to delete keys in
	 * @param InHandles          Array of key handles that should be deleted
	 */
	template<typename ChannelType>
	void DeleteKeys(ChannelType* InChannel, TArrayView<const FKeyHandle> InHandles)
	{
		auto ChannelInterface = InChannel->GetInterface();
		for (int32 Index = 0; Index < InHandles.Num(); ++Index)
		{
			const int32 KeyIndex = ChannelInterface.GetIndex(InHandles[Index]);
			if (KeyIndex != INDEX_NONE)
			{
				ChannelInterface.RemoveKey(KeyIndex);
			}
		}
	}


	/**
	 * Gather key draw information from a channel for a specific set of keys
	 *
	 * @param InChannel          The channel to duplicate keys in
	 * @param InHandles          Array of key handles that should be deleted
	 * @param OutKeyDrawParams   Array to receive key draw information. Must be exactly the size of InHandles.
	 */
	SEQUENCER_API void DrawKeys(void* Channel, TArrayView<const FKeyHandle> InHandles, TArrayView<FKeyDrawParams> OutKeyDrawParams);


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

		auto ChannelInterface = InChannel->GetInterface();
		TArrayView<const FFrameNumber> Times = ChannelInterface.GetTimes();
		auto Values = ChannelInterface.GetValues();

		for (FKeyHandle Handle : InHandles)
		{
			const int32 KeyIndex = ChannelInterface.GetIndex(Handle);
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

		auto ChannelInterface = InChannel->GetInterface();

		TRange<FFrameNumber> NewRange = Section->GetRange();

		auto ForEachKey = [Section, PasteAt, &NewRange, &ChannelInterface, &OutPastedKeys, &SrcEnvironment, &DstEnvironment](const FMovieSceneClipboardKey& Key)
		{
			FFrameNumber Time = (PasteAt + FFrameRate::TransformTime(Key.GetTime(), SrcEnvironment.FrameResolution, DstEnvironment.FrameResolution)).FloorToFrame();

			NewRange = TRange<FFrameNumber>::Hull(NewRange, TRange<FFrameNumber>(Time));

			typedef typename TDecay<decltype(ChannelInterface.GetValues()[0])>::Type KeyType;
			KeyType NewKey = Key.GetValue<KeyType>();

			FKeyHandle KeyHandle = ChannelInterface.UpdateOrAddKey(Time, NewKey);
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
	template<typename ChannelType>
	TUniquePtr<FCurveModel> CreateCurveEditorModel(TMovieSceneChannelHandle<ChannelType> ChannelHandle, UMovieSceneSection* OwningSection, TSharedRef<ISequencer> InSequencer)
	{
		return nullptr;
	}

}	// namespace Sequencer


// include generic key struct function definitions
#include "SequencerGenericKeyStruct.inl"