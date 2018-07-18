// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SequencerHotspots.h"
#include "DisplayNodes/SequencerObjectBindingNode.h"
#include "SequencerCommonHelpers.h"
#include "SSequencer.h"
#include "Tools/EditToolDragOperations.h"
#include "SequencerContextMenus.h"
#include "SSequencerTrackArea.h"
#include "Tools/SequencerEditTool_Movement.h"
#include "Tools/SequencerEditTool_Selection.h"
#include "SequencerTrackNode.h"
#include "Widgets/Layout/SBox.h"
#include "Channels/MovieSceneChannel.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "MovieSceneTimeHelpers.h"

#define LOCTEXT_NAMESPACE "SequencerHotspots"

TSharedRef<ISequencerSection> FSectionHandle::GetSectionInterface() const
{
	return TrackNode->GetSections()[SectionIndex];
}

UMovieSceneSection* FSectionHandle::GetSectionObject() const
{
	return GetSectionInterface()->GetSectionObject();
}

void FKeyHotspot::UpdateOnHover(SSequencerTrackArea& InTrackArea, ISequencer& InSequencer) const
{
	InTrackArea.AttemptToActivateTool(FSequencerEditTool_Movement::Identifier);
}

TOptional<FFrameNumber> FKeyHotspot::GetTime() const
{
	FFrameNumber Time = 0;

	if (Keys.Num())
	{
		TArrayView<const FSequencerSelectedKey> FirstKey(&Keys[0], 1);
		TArrayView<FFrameNumber> FirstKeyTime(&Time, 1);
		GetKeyTimes(FirstKey, FirstKeyTime);
	}

	return Time;
}

bool FKeyHotspot::PopulateContextMenu(FMenuBuilder& MenuBuilder, ISequencer& InSequencer, FFrameTime MouseDownTime)
{
	FSequencer& Sequencer = static_cast<FSequencer&>(InSequencer);
	FKeyContextMenu::BuildMenu(MenuBuilder, Sequencer);
	return true;
}

TOptional<FFrameNumber> FSectionHotspot::GetTime() const
{
	UMovieSceneSection* ThisSection = Section.GetSectionObject();
	return ThisSection && ThisSection->HasStartFrame() ? ThisSection->GetInclusiveStartFrame() : TOptional<FFrameNumber>();
}

TOptional<FFrameTime> FSectionHotspot::GetOffsetTime() const
{
	UMovieSceneSection* ThisSection = Section.GetSectionObject();
	return ThisSection ? ThisSection->GetOffsetTime() : TOptional<FFrameTime>();
}

void FSectionHotspot::UpdateOnHover(SSequencerTrackArea& InTrackArea, ISequencer& InSequencer) const
{
	UMovieSceneSection* ThisSection = Section.GetSectionObject();

	// Move sections if they are selected
	if (InSequencer.GetSelection().IsSelected(ThisSection))
	{
		InTrackArea.AttemptToActivateTool(FSequencerEditTool_Movement::Identifier);
	}
	else
	{
		// Activate selection mode if the section has keys
		for (const FMovieSceneChannelEntry& Entry : ThisSection->GetChannelProxy().GetAllEntries())
		{
			for (const FMovieSceneChannel* Channel : Entry.GetChannels())
			{
				if (Channel->GetNumKeys() != 0)
				{
					InTrackArea.AttemptToActivateTool(FSequencerEditTool_Selection::Identifier);
					return;
				}
			}
		}

		// Activate selection mode if the section is infinite, otherwise just move it
		if (ThisSection->GetRange() == TRange<FFrameNumber>::All())
		{
			InTrackArea.AttemptToActivateTool(FSequencerEditTool_Selection::Identifier);
		}
		else
		{
			InTrackArea.AttemptToActivateTool(FSequencerEditTool_Movement::Identifier);
		}
	}
}

bool FSectionHotspot::PopulateContextMenu(FMenuBuilder& MenuBuilder, ISequencer& InSequencer, FFrameTime MouseDownTime)
{
	FSequencer& Sequencer = static_cast<FSequencer&>(InSequencer);

	TSharedPtr<ISequencerSection> SectionInterface = Section.TrackNode->GetSections()[Section.SectionIndex];

	FGuid ObjectBinding;
	if (Section.TrackNode.IsValid())
	{
		TSharedPtr<FSequencerObjectBindingNode> ObjectBindingNode = Section.TrackNode->FindParentObjectBindingNode();
		if (ObjectBindingNode.IsValid())
		{
			ObjectBinding = ObjectBindingNode->GetObjectBinding();
		}
	}

	FSectionContextMenu::BuildMenu(MenuBuilder, Sequencer, MouseDownTime);

	SectionInterface->BuildSectionContextMenu(MenuBuilder, ObjectBinding);

	return true;
}

TOptional<FFrameNumber> FSectionResizeHotspot::GetTime() const
{
	UMovieSceneSection* ThisSection = Section.GetSectionObject();
	if (!ThisSection)
	{
		return TOptional<FFrameNumber>();
	}
	return HandleType == Left ? ThisSection->GetInclusiveStartFrame() : ThisSection->GetExclusiveEndFrame();
}

void FSectionResizeHotspot::UpdateOnHover(SSequencerTrackArea& InTrackArea, ISequencer& InSequencer) const
{
	InTrackArea.AttemptToActivateTool(FSequencerEditTool_Movement::Identifier);
}

TSharedPtr<ISequencerEditToolDragOperation> FSectionResizeHotspot::InitiateDrag(ISequencer& Sequencer)
{
	const auto& SelectedSections = Sequencer.GetSelection().GetSelectedSections();
	auto SectionHandles = StaticCastSharedRef<SSequencer>(Sequencer.GetSequencerWidget())->GetSectionHandles(SelectedSections);
	
	if (!SelectedSections.Contains(Section.GetSectionObject()))
	{
		Sequencer.GetSelection().Empty();
		Sequencer.GetSelection().AddToSelection(Section.GetSectionObject());
		SequencerHelpers::UpdateHoveredNodeFromSelectedSections(static_cast<FSequencer&>(Sequencer));

		SectionHandles.Empty();
		SectionHandles.Add(Section);
	}
	const bool bIsSlipping = false;
	return MakeShareable( new FResizeSection(static_cast<FSequencer&>(Sequencer), SectionHandles, HandleType == Right, bIsSlipping) );
}

TOptional<FFrameNumber> FSectionEasingHandleHotspot::GetTime() const
{
	UMovieSceneSection* ThisSection = Section.GetSectionObject();
	if (ThisSection)
	{
		if (HandleType == ESequencerEasingType::In && !ThisSection->GetEaseInRange().IsEmpty())
		{
			return MovieScene::DiscreteExclusiveUpper(ThisSection->GetEaseInRange());
		}
		else if (HandleType == ESequencerEasingType::Out && !ThisSection->GetEaseOutRange().IsEmpty())
		{
			return MovieScene::DiscreteInclusiveLower(ThisSection->GetEaseOutRange());
		}
	}
	return TOptional<FFrameNumber>();
}

void FSectionEasingHandleHotspot::UpdateOnHover(SSequencerTrackArea& InTrackArea, ISequencer& InSequencer) const
{
	InTrackArea.AttemptToActivateTool(FSequencerEditTool_Movement::Identifier);
}

bool FSectionEasingHandleHotspot::PopulateContextMenu(FMenuBuilder& MenuBuilder, ISequencer& Sequencer, FFrameTime MouseDownTime)
{
	FEasingContextMenu::BuildMenu(MenuBuilder, { FEasingAreaHandle{Section, HandleType} }, static_cast<FSequencer&>(Sequencer), MouseDownTime);
	return true;
}

TSharedPtr<ISequencerEditToolDragOperation> FSectionEasingHandleHotspot::InitiateDrag(ISequencer& Sequencer)
{
	return MakeShareable( new FManipulateSectionEasing(static_cast<FSequencer&>(Sequencer), Section, HandleType == ESequencerEasingType::In) );
}

bool FSectionEasingAreaHotspot::PopulateContextMenu(FMenuBuilder& MenuBuilder, ISequencer& Sequencer, FFrameTime MouseDownTime)
{
	FEasingContextMenu::BuildMenu(MenuBuilder, Easings, static_cast<FSequencer&>(Sequencer), MouseDownTime);

	TSharedPtr<ISequencerSection> SectionInterface = Section.TrackNode->GetSections()[Section.SectionIndex];

	FGuid ObjectBinding;
	if (Section.TrackNode.IsValid())
	{
		TSharedPtr<FSequencerObjectBindingNode> ObjectBindingNode = Section.TrackNode->FindParentObjectBindingNode();
		if (ObjectBindingNode.IsValid())
		{
			ObjectBinding = ObjectBindingNode->GetObjectBinding();
		}
	}

	SectionInterface->BuildSectionContextMenu(MenuBuilder, ObjectBinding);

	return true;
}

#undef LOCTEXT_NAMESPACE
