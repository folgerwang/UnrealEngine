// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/CursorReply.h"
#include "Misc/FrameTime.h"

class FMenuBuilder;
class ISequencer;
class ISequencerEditToolDragOperation;
class SSequencerTrackArea;

enum class ESequencerHotspot
{
	Key,
	Section,
	SectionResize_L,
	SectionResize_R,
	EaseInHandle,
	EaseOutHandle,
	EasingArea,
};


/** A sequencer hotspot is used to identify specific areas on the sequencer track area */ 
struct ISequencerHotspot
{
	ISequencerHotspot() : bIsLocked(false) {}

	virtual ~ISequencerHotspot() { }
	virtual ESequencerHotspot GetType() const = 0;
	virtual void UpdateOnHover(SSequencerTrackArea& InTrackArea, ISequencer& InSequencer) const = 0;
	virtual TOptional<FFrameNumber> GetTime() const { return TOptional<FFrameNumber>(); }
	virtual TOptional<FFrameTime> GetOffsetTime() const { return TOptional<FFrameTime>(); }
	virtual TSharedPtr<ISequencerEditToolDragOperation> InitiateDrag(ISequencer&) { return nullptr; }
	virtual bool PopulateContextMenu(FMenuBuilder& MenuBuilder, ISequencer& Sequencer, FFrameTime MouseDownTime){ return false; }
	virtual FCursorReply GetCursor() const { return FCursorReply::Unhandled(); }

public:

	/** Whether this hotspot should be considered locked (ie, cannot be changed) */
	bool bIsLocked;
};
