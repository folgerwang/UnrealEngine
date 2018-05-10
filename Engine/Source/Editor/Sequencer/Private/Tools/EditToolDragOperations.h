// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/CursorReply.h"
#include "Curves/KeyHandle.h"
#include "SequencerSelectedKey.h"
#include "ISequencerEditTool.h"
#include "SequencerHotspots.h"
#include "ScopedTransaction.h"
#include "Tools/SequencerSnapField.h"
#include "Channels/MovieSceneChannelHandle.h"


class FSequencer;
class FSlateWindowElementList;
class FVirtualTrackArea;
class USequencerSettings;

/**
 * Abstract base class for drag operations that handle an operation for an edit tool.
 */
class FEditToolDragOperation
	: public ISequencerEditToolDragOperation
{
public:

	/** Create and initialize a new instance. */
	FEditToolDragOperation( FSequencer& InSequencer );

public:

	// ISequencerEditToolDragOperation interface

	virtual FCursorReply GetCursor() const override;
	virtual int32 OnPaint(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const override;

protected:

	/** begin a new scoped transaction for this drag */
	void BeginTransaction( TArray< FSectionHandle >& Sections, const FText& TransactionDesc );

	/** End an existing scoped transaction if one exists */
	void EndTransaction();

protected:

	/** Scoped transaction for this drag operation */
	TUniquePtr<FScopedTransaction> Transaction;

	/** The current sequencer settings, cached on construction */
	const USequencerSettings* Settings;

	/** Reference to the sequencer itself */
	FSequencer& Sequencer;
};


/**
 * An operation to resize a section by dragging its left or right edge
 */
class FResizeSection
	: public FEditToolDragOperation
{
public:

	/** Create and initialize a new instance. */
	FResizeSection( FSequencer& InSequencer, TArray<FSectionHandle> Sections, bool bInDraggingByEnd, bool bIsSlipping );

public:

	// FEditToolDragOperation interface

	virtual void OnBeginDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea) override;
	virtual void OnDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea) override;
	virtual void OnEndDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea) override;
	virtual FCursorReply GetCursor() const override { return FCursorReply::Cursor( EMouseCursor::ResizeLeftRight ); }

private:

	/** The sections we are interacting with */
	TArray<FSectionHandle> Sections;

	/********************************************************/
	struct FPreDragChannelData
	{
		/** Weak handle to the base channel ptr */
		FMovieSceneChannelHandle Channel;

		/** Array of all the handles in the section at the start of the drag */
		TArray<FKeyHandle> Handles;
		/** Array of all the above handle's times, one per index of Handles */
		TArray<FFrameNumber> FrameNumbers;
	};

	struct FPreDragSectionData
	{
		/** Pointer to the movie section, this section is only valid during a drag operation*/
		UMovieSceneSection * MovieSection;
		/** The initial range of the section before it was resized */
		TRange<FFrameNumber> InitialRange;
		/** Array of all the channels in the section before it was resized */
		TArray<FPreDragChannelData> Channels;
	};
	TArray<FPreDragSectionData> PreDragSectionData;

	/** true if dragging  the end of the section, false if dragging the start */
	bool bDraggingByEnd;

	/** true if slipping, adjust only the start offset */
	bool bIsSlipping;

	/** Time where the mouse is pressed */
	FFrameTime MouseDownTime;

	/** The section start or end times when the mouse is pressed */
	TMap<TWeakObjectPtr<UMovieSceneSection>, FFrameNumber> SectionInitTimes;

	/** Optional snap field to use when dragging */
	TOptional<FSequencerSnapField> SnapField;
};

/**
 * This drag operation handles moving both keys and sections depending on what you have selected.
 */
class FMoveKeysAndSections
	: public FEditToolDragOperation
{
public:
	FMoveKeysAndSections(FSequencer& InSequencer, const TSet<FSequencerSelectedKey>& InSelectedKeys, TArray<FSectionHandle> InSelectedSections, bool InbHotspotWasSection);
	~FMoveKeysAndSections();

	// FEditToolDragOperation interface
	virtual void OnBeginDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea) override;
	virtual void OnDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea) override;
	virtual void OnEndDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea) override;
	virtual FCursorReply GetCursor() const override { return FCursorReply::Cursor(EMouseCursor::CardinalCross); }
	// ~FEditToolDragOperation interface

protected:
	/** Calculate the possible horizontal movement we can, constrained by sections running into things. */
	TOptional<FFrameNumber> GetMovementDeltaX(FFrameTime MouseTime);
	/** Move selected sections, if any. */
	bool HandleSectionMovement(FFrameTime MouseTime, FVector2D VirtualMousePos, FVector2D LocalMousePos, TOptional<FFrameNumber> MaxDeltaX, FFrameNumber DesiredDeltaX);
	/** Move selected keys, if any. */
	void HandleKeyMovement(TOptional<FFrameNumber> MaxDeltaX, FFrameNumber DesiredDeltaX);

	void OnSequencerNodeTreeUpdated();

	/** Calls Modify on sections that own keys we're moving, as the need to be notified the data is about to change too. */
	void ModifyNonSelectedSections();

protected:
	/** Array of sections that we're moving. */
	TArray<FSectionHandle> Sections;

	/** Set of keys that are being moved. */
	TSet<FSequencerSelectedKey> Keys;
	TArray<FSequencerSelectedKey> KeysAsArray;

	/** What was the time of the mouse for the previous frame? Used to calculate a per-frame delta. */
	FFrameTime MouseTimePrev;

	struct FRelativeOffset
	{
		FRelativeOffset()
			: StartOffset()
			, EndOffset()
		{
		}

		/**
		 * The offset for the start of the section. Can be unset in the case of a section with no lower bound.
		 * Keys are represented only by StartOffset and do not have an End Offset (which would imply a range).
		 */
		TOptional<FFrameTime> StartOffset;

		/**
		 * The offset for the end of the section. Can be unset in the case of a section with no upper bound.
		 */
		TOptional<FFrameTime> EndOffset;
	};

	/** Array of relative offsets for each selected item. Keys + Sections are both added to this array. */
	TArray<FRelativeOffset> RelativeOffsets;

	struct FInitialRowIndex
	{
		UMovieSceneSection* Section;
		int32 RowIndex;
	};

	/** Store the row each section starts on when we start dragging. */
	TArray<FInitialRowIndex> InitialSectionRowIndicies;

	/** Array of sections that we called Modify on because we're editing keys that belong to these sections, but not actually moving these sections. */
	TArray<UMovieSceneSection*> ModifiedNonSelectedSections;

	/** Optional snap field to use when dragging */
	TOptional<FSequencerSnapField> SnapField;

	/** A handle for the sequencer node tree updated delegate. */
	FDelegateHandle SequencerNodeTreeUpdatedHandle;

	/** If the user is moving them via clicking on the Section then we'll allow vertical re-arranging, otherwise not. */
	bool bHotspotWasSection;
};

/**
 * Operation to drag-duplicate the currently selected keys and sections.
 */
class FDuplicateKeysAndSections : public FMoveKeysAndSections
{
public:

	FDuplicateKeysAndSections( FSequencer& InSequencer, const TSet<FSequencerSelectedKey>& InSelectedKeys, TArray<FSectionHandle> InSelectedSections, bool InbHotspotWasSection)
		: FMoveKeysAndSections(InSequencer, InSelectedKeys, InSelectedSections, InbHotspotWasSection)
	{}

public:

	virtual void OnBeginDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea) override;
	virtual void OnEndDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea) override;
};

/**
 * An operation to change a section's ease in/out by dragging its left or right handle
 */
class FManipulateSectionEasing
	: public FEditToolDragOperation
{
public:

	/** Create and initialize a new instance. */
	FManipulateSectionEasing( FSequencer& InSequencer, FSectionHandle InSection, bool bEaseIn );

public:

	// FEditToolDragOperation interface

	virtual void OnBeginDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea) override;
	virtual void OnDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea) override;
	virtual void OnEndDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea) override;
	virtual FCursorReply GetCursor() const override { return FCursorReply::Cursor( EMouseCursor::ResizeLeftRight ); }

private:

	/** The sections we are interacting with */
	FSectionHandle Handle;

	/** true if editing the section's ease in, false for ease out */
	bool bEaseIn;

	/** Time where the mouse is pressed */
	FFrameTime MouseDownTime;

	/** The section ease in/out when the mouse was pressed */
	TOptional<int32> InitValue;

	/** Optional snap field to use when dragging */
	TOptional<FSequencerSnapField> SnapField;
};
