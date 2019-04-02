// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Containers/Array.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Misc/FrameNumber.h"

struct FKeyHandle;
struct FKeyDrawParams;
struct FSequencerPasteEnvironment;
struct FMovieSceneClipboardEnvironment;

class SWidget;
class ISequencer;
class FCurveModel;
class FMenuBuilder;
class FStructOnScope;
class UMovieSceneSection;
class ISectionLayoutBuilder;
class FMovieSceneClipboardBuilder;
class FMovieSceneClipboardKeyTrack;
class FTrackInstancePropertyBindings;

/** Utility struct representing a number of selected keys on a single channel */
struct FExtendKeyMenuParams
{
	/** The section on which the channel resides */
	TWeakObjectPtr<UMovieSceneSection> Section;

	/** The channel on which the keys reside */
	FMovieSceneChannelHandle Channel;

	/** An array of key handles to operante on */
	TArray<FKeyHandle> Handles;
};

/**
 * Abstract interface that defines all sequencer interactions for any channel type
 * Channels are stored internally as FMovieSceneChannel*, with this interface providing a common set of operations for all channels through a safe cast from the FMovieSceneChannel*.
 * Implementations are found in TSequencerChanelInterface which calls overloaded free functions for each channel.
 */
struct ISequencerChannelInterface
{
	virtual ~ISequencerChannelInterface() {}

	/**
	 * Add (or update) a key to the specified channel using it's current value at that time, or some external value specified by the extended editor data
	 *
	 * @param Channel               The channel to add a key to
	 * @param SectionToKey          The SectionToKey
	 * @param ExtendedEditorData    A pointer to the extended editor data for this channel of type TMovieSceneChannelTraits<>::ExtendedEditorDataType
	 * @param InTime                The time at which to add a key
	 * @param InSequencer           The currently active sequencer
	 * @param ObjectBindingID       The object binding ID for the track that this channel resides within
	 * @param PropertyBindings      (Optional) Property bindings where this channel exists on a property track
	 * @return A handle to the new or updated key
	 */
	virtual FKeyHandle AddOrUpdateKey_Raw(FMovieSceneChannel* Channel, UMovieSceneSection* SectionToKey, const void* ExtendedEditorData, FFrameNumber InTime, ISequencer& InSequencer, const FGuid& ObjectBindingID, FTrackInstancePropertyBindings* PropertyBindings) const = 0;

	/**
	 * Copy all the keys specified in KeyMask to the specified clipboard
	 *
	 * @param Channel               The channel to copy from
	 * @param Section               The section that owns the channel
	 * @param KeyAreaName           The name of the key area
	 * @param ClipboardBuilder      The structure responsible for building clipboard information for each key
	 * @param KeyMask               A specific set of keys to copy
	 */
	virtual void CopyKeys_Raw(FMovieSceneChannel* Channel, const UMovieSceneSection* Section, FName KeyAreaName, FMovieSceneClipboardBuilder& ClipboardBuilder, TArrayView<const FKeyHandle> KeyMask) const = 0;

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
	virtual void PasteKeys_Raw(FMovieSceneChannel* Channel, UMovieSceneSection* Section, const FMovieSceneClipboardKeyTrack& KeyTrack, const FMovieSceneClipboardEnvironment& SrcEnvironment, const FSequencerPasteEnvironment& DstEnvironment, TArray<FKeyHandle>& OutPastedKeys) const = 0;

	/**
	 * Get an editable key struct for the specified key
	 *
	 * @param Channel               The channel on which the key resides
	 * @param KeyHandle             Handle of the key to get
	 * @return A shared editable key struct
	 */
	virtual TSharedPtr<FStructOnScope> GetKeyStruct_Raw(FMovieSceneChannelHandle Channel, FKeyHandle KeyHandle) const = 0;

	/**
	 * Check whether an editor on the sequencer node tree can be created for the specified channel
	 *
	 * @param Channel               The channel to check
	 * @return true if a key editor should be constructed, false otherwise
	 */
	virtual bool CanCreateKeyEditor_Raw(const FMovieSceneChannel* Channel) const = 0;

	/**
	 * Create an editor on the sequencer node tree
	 *
	 * @param Channel               The channel handle to create a key editor for
	 * @param Section               The section that owns this channel
	 * @param InObjectBindingID     The ID of the object this key area's track is bound to
	 * @param PropertyBindings      (Optional) Property bindings where this channel exists on a property track
	 * @param Sequencer             The currently active sequencer
	 * @return The editor widget to display on the node tree
	 */
	virtual TSharedRef<SWidget> CreateKeyEditor_Raw(const FMovieSceneChannelHandle& Channel, UMovieSceneSection* Section, const FGuid& InObjectBindingID, TWeakPtr<FTrackInstancePropertyBindings> PropertyBindings, TWeakPtr<ISequencer> Sequencer) const = 0;

	/**
	 * Extend the key context menu
	 *
	 * @param MenuBuilder           The menu builder used to create this context menu
	 * @param Channels              Array of channels and handles that are being shown in the context menu
	 * @param InSequencer           The currently active sequencer
	 */
	virtual void ExtendKeyMenu_Raw(FMenuBuilder& MenuBuilder, TArrayView<const FExtendKeyMenuParams> Parameters, TWeakPtr<ISequencer> InSequencer) const = 0;

	/**
	 * Extend the section context menu
	 *
	 * @param MenuBuilder           The menu builder used to create this context menu
	 * @param Channels              Array of type specific channels that exist in the selected sections
	 * @param Sections              Array of sections being shown on the context menu
	 * @param InSequencer           The currently active sequencer
	 */
	virtual void ExtendSectionMenu_Raw(FMenuBuilder& MenuBuilder, TArrayView<const FMovieSceneChannelHandle> Channels, TArrayView<UMovieSceneSection* const> Sections, TWeakPtr<ISequencer> InSequencer) const = 0;

	/**
	 * Gather information on how to draw the specified keys
	 *
	 * @param Channel               The channel to query
	 * @param InKeyHandles          Array of handles to duplicate
	 * @param OutKeyDrawParams      Pre-sized array to receive key draw parameters. Invalid key handles will not be assigned to this array. Must match size of InKeyHandles.
	 */
	virtual void DrawKeys_Raw(FMovieSceneChannel* Channel, TArrayView<const FKeyHandle> InKeyHandles, TArrayView<FKeyDrawParams> OutKeyDrawParams) const = 0;

	/**
	 * Create a new model for this channel that can be used on the curve editor interface
	 *
	 * @return (Optional) A new model to be added to a curve editor
	 */
	virtual TUniquePtr<FCurveModel> CreateCurveEditorModel_Raw(const FMovieSceneChannelHandle& Channel, UMovieSceneSection* OwningSection, TSharedRef<ISequencer> InSequencer) const = 0;
};