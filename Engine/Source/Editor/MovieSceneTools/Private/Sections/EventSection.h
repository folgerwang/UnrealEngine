// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISequencerSection.h"
#include "ISequencer.h"

class FSequencerSectionPainter;

/**
* An implementation of event sections.
*/
class FEventSection
	: public FSequencerSection
{
public:

	/**
	* Creates a new event section for editing event sections.
	*
	* @param InSectionObject The section object which is being displayed and edited.
	* @param InDisplayName A display name for the section being displayed and edited.
	*/
	FEventSection(UMovieSceneSection& InSectionObject, TWeakPtr<ISequencer> InSequencer)
		: FSequencerSection(InSectionObject)
		, Sequencer(InSequencer)
	{}

public:

	virtual int32 OnPaintSection(FSequencerSectionPainter& Painter) const override;

private:

	TWeakPtr<ISequencer> Sequencer;
};
