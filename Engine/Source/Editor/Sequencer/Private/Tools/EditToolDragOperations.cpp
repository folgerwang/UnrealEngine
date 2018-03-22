// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Tools/EditToolDragOperations.h"
#include "ISequencer.h"
#include "MovieSceneTrack.h"
#include "Sequencer.h"
#include "SequencerSettings.h"
#include "SequencerCommonHelpers.h"
#include "VirtualTrackArea.h"
#include "SequencerTrackNode.h"
#include "Algo/AllOf.h"
#include "MovieSceneTimeHelpers.h"
#include "Modules/ModuleManager.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "ISequencerModule.h"

struct FDefaultKeySnappingCandidates : ISequencerSnapCandidate
{
	FDefaultKeySnappingCandidates(const TSet<FSequencerSelectedKey>& InKeysToExclude)
		: KeysToExclude(InKeysToExclude)
	{}

	virtual bool IsKeyApplicable(FKeyHandle KeyHandle, const TSharedPtr<IKeyArea>& KeyArea, UMovieSceneSection* Section) override
	{
		return !KeysToExclude.Contains(FSequencerSelectedKey(*Section, KeyArea, KeyHandle));
	}

	const TSet<FSequencerSelectedKey>& KeysToExclude;
};

struct FDefaultSectionSnappingCandidates : ISequencerSnapCandidate
{
	FDefaultSectionSnappingCandidates(FSectionHandle InSectionToIgnore)
	{
		SectionsToIgnore.Add(InSectionToIgnore.GetSectionObject());
	}

	FDefaultSectionSnappingCandidates(const TArray<FSectionHandle>& InSectionsToIgnore)
	{
		for (auto& SectionHandle : InSectionsToIgnore)
		{
			SectionsToIgnore.Add(SectionHandle.GetSectionObject());
		}
	}

	virtual bool AreSectionBoundsApplicable(UMovieSceneSection* Section) override
	{
		return !SectionsToIgnore.Contains(Section);
	}

	TSet<UMovieSceneSection*> SectionsToIgnore;
};

TOptional<FSequencerSnapField::FSnapResult> SnapToInterval(const TArray<FFrameNumber>& InTimes, int32 FrameThreshold, FFrameRate Resolution, FFrameRate PlayRate)
{
	TOptional<FSequencerSnapField::FSnapResult> Result;

	FFrameNumber SnapAmount(0);
	for (FFrameNumber Time : InTimes)
	{
		// Convert from resolution to playrate, round to frame, then back again
		FFrameNumber PlayIntervalTime = FFrameRate::TransformTime(Time,             Resolution, PlayRate  ).RoundToFrame();
		FFrameNumber IntervalSnap     = FFrameRate::TransformTime(PlayIntervalTime, PlayRate,   Resolution).FloorToFrame();

		FFrameNumber ThisSnapAmount   = IntervalSnap - Time;
		if (FMath::Abs(ThisSnapAmount) <= FrameThreshold)
		{
			if (!Result.IsSet() || FMath::Abs(ThisSnapAmount) < SnapAmount)
			{
				Result = FSequencerSnapField::FSnapResult{Time, IntervalSnap};
				SnapAmount = ThisSnapAmount;
			}
		}
	}

	return Result;
}

/** How many pixels near the mouse has to be before snapping occurs */
const float PixelSnapWidth = 10.f;

TRange<FFrameNumber> GetSectionBoundaries(const UMovieSceneSection* Section, TArray<FSectionHandle>& SectionHandles, TSharedPtr<FSequencerTrackNode> SequencerNode)
{
	// Only get boundaries for the sections that aren't being moved
	TArray<const UMovieSceneSection*> SectionsBeingMoved;
	for (auto SectionHandle : SectionHandles)
	{
		SectionsBeingMoved.Add(SectionHandle.GetSectionObject());
	}

	// Find the borders of where you can drag to
	FFrameNumber LowerBound = TNumericLimits<int32>::Lowest(), UpperBound = TNumericLimits<int32>::Max();

	// Also get the closest borders on either side
	const TArray< TSharedRef<ISequencerSection> >& AllSections = SequencerNode->GetSections();
	for (int32 SectionIndex = 0; SectionIndex < AllSections.Num(); ++SectionIndex)
	{
		const UMovieSceneSection* TestSection = AllSections[SectionIndex]->GetSectionObject();

		if (!SectionsBeingMoved.Contains(TestSection) && Section->GetRowIndex() == TestSection->GetRowIndex())
		{
			if (TestSection->HasEndFrame() && Section->HasStartFrame() && TestSection->GetExclusiveEndFrame() <= Section->GetInclusiveStartFrame() && TestSection->GetExclusiveEndFrame() > LowerBound)
			{
				LowerBound = TestSection->GetExclusiveEndFrame();
			}
			if (TestSection->HasStartFrame() && Section->HasEndFrame() && TestSection->GetInclusiveStartFrame() >= Section->GetExclusiveEndFrame() && TestSection->GetInclusiveStartFrame() < UpperBound)
			{
				UpperBound = TestSection->GetInclusiveStartFrame();
			}
		}
	}

	return TRange<FFrameNumber>(LowerBound, UpperBound);
}

FEditToolDragOperation::FEditToolDragOperation( FSequencer& InSequencer )
	: Sequencer(InSequencer)
{
	Settings = Sequencer.GetSequencerSettings();
}

FCursorReply FEditToolDragOperation::GetCursor() const
{
	return FCursorReply::Cursor( EMouseCursor::Default );
}

int32 FEditToolDragOperation::OnPaint(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	return LayerId;
}

void FEditToolDragOperation::BeginTransaction( TArray< FSectionHandle >& Sections, const FText& TransactionDesc )
{
	// Begin an editor transaction and mark the section as transactional so it's state will be saved
	Transaction.Reset( new FScopedTransaction(TransactionDesc) );

	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); )
	{
		UMovieSceneSection* SectionObj = Sections[SectionIndex].GetSectionObject();

		SectionObj->SetFlags( RF_Transactional );
		// Save the current state of the section
		if (SectionObj->TryModify())
		{
			++SectionIndex;
		}
		else
		{
			Sections.RemoveAt(SectionIndex);
		}
	}
}

void FEditToolDragOperation::EndTransaction()
{
	Transaction.Reset();
	Sequencer.NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
}

FResizeSection::FResizeSection( FSequencer& InSequencer, TArray<FSectionHandle> InSections, bool bInDraggingByEnd, bool bInIsSlipping )
	: FEditToolDragOperation( InSequencer )
	, Sections( MoveTemp(InSections) )
	, bDraggingByEnd(bInDraggingByEnd)
	, bIsSlipping(bInIsSlipping)
	, MouseDownTime(0)
{
}

void FResizeSection::OnBeginDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	BeginTransaction( Sections, NSLOCTEXT("Sequencer", "DragSectionEdgeTransaction", "Resize section") );

	MouseDownTime = VirtualTrackArea.PixelToFrame(LocalMousePos.X);

	// Construct a snap field of unselected sections
	FDefaultSectionSnappingCandidates SnapCandidates(Sections);
	SnapField = FSequencerSnapField(Sequencer, SnapCandidates, ESequencerEntity::Section);

	SectionInitTimes.Empty();

	bool bIsDilating = MouseEvent.IsControlDown();
	ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");

	for (auto& Handle : Sections)
	{
		UMovieSceneSection* Section = Handle.GetSectionObject();

		TSharedRef<ISequencerSection> SectionInterface = Handle.GetSectionInterface();
		if (bIsDilating)
		{
			// Populate the resize data for this section
			PreDragSectionData.Empty();
			FPreDragSectionData ResizeData; 
			ResizeData.MovieSection = Section;
			ResizeData.InitialRange = Section->GetRange();

			// Add the key times for all keys of all channels on this section
			FMovieSceneChannelProxy& Proxy = Section->GetChannelProxy();
			for (const FMovieSceneChannelEntry& Entry : Proxy.GetAllEntries())
			{
				ISequencerChannelInterface* ChannelInterface = SequencerModule.FindChannelInterface(Entry.GetChannelID());

				for (void* Channel : Entry.GetChannels())
				{
					// Populate the cached state of this channel
					FPreDragChannelData& ChannelData = ResizeData.Channels[ResizeData.Channels.Emplace()];
					ChannelData.ChannelType = Entry.GetChannelID();
					ChannelData.Channel = Proxy.MakeHandle(Channel);
					ChannelInterface->GetKeys_Raw(Channel, TRange<FFrameNumber>::All(), &ChannelData.FrameNumbers, &ChannelData.Handles);
				}
			}
			PreDragSectionData.Emplace(ResizeData);
		}
		else
		{
			SectionInterface->BeginResizeSection();
		}

		SectionInitTimes.Add(Section, bDraggingByEnd ? Section->GetExclusiveEndFrame() : Section->GetInclusiveStartFrame());
	}
}

void FResizeSection::OnEndDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	EndTransaction();
}

void FResizeSection::OnDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");

	bool bIsDilating = MouseEvent.IsControlDown();

	FFrameRate   FrameResolution = Sequencer.GetFocusedFrameResolution();
	FFrameRate   PlayRate        = Sequencer.GetFocusedPlayRate();

	// Convert the current mouse position to a time
	FFrameNumber DeltaTime = (VirtualTrackArea.PixelToFrame(LocalMousePos.X) - MouseDownTime).RoundToFrame();

	// Snapping
	if ( Settings->GetIsSnapEnabled() )
	{
		TArray<FFrameNumber> SectionTimes;
		for (const FSectionHandle& Handle : Sections)
		{
			UMovieSceneSection* Section = Handle.GetSectionObject();
			SectionTimes.Add(SectionInitTimes[Section] + DeltaTime);
		}

		float SnapThresholdPx = VirtualTrackArea.PixelToSeconds(PixelSnapWidth) - VirtualTrackArea.PixelToSeconds(0.f);
		int32 SnapThreshold   = ( SnapThresholdPx * FrameResolution ).FloorToFrame().Value;

		TOptional<FSequencerSnapField::FSnapResult> SnappedTime;

		if (Settings->GetSnapSectionTimesToSections())
		{
			SnappedTime = SnapField->Snap(SectionTimes, SnapThreshold);
		}

		if (!SnappedTime.IsSet() && Settings->GetSnapSectionTimesToInterval())
		{
			int32 IntervalSnapThreshold = FMath::RoundToInt( ( FrameResolution / PlayRate ).AsDecimal() );
			SnappedTime = SnapToInterval(SectionTimes, IntervalSnapThreshold, FrameResolution, PlayRate);
		}

		if (SnappedTime.IsSet())
		{
			// Add the snapped amount onto the delta
			DeltaTime += SnappedTime->Snapped - SnappedTime->Original;
		}
	}
	
	/********************************************************************/
	if (bIsDilating)
	{
		for(FPreDragSectionData Data: PreDragSectionData)
		{
			FFrameNumber StartPosition  = bDraggingByEnd ? MovieScene::DiscreteExclusiveUpper(Data.InitialRange) : MovieScene::DiscreteInclusiveLower(Data.InitialRange);
			FFrameNumber DilationOrigin = bDraggingByEnd ? MovieScene::DiscreteInclusiveLower(Data.InitialRange) : MovieScene::DiscreteExclusiveUpper(Data.InitialRange);
			FFrameNumber NewPosition    = bDraggingByEnd ? FMath::Max(StartPosition + DeltaTime, DilationOrigin) : FMath::Min(StartPosition + DeltaTime, DilationOrigin);

			float DilationFactor = FMath::Abs(NewPosition.Value - DilationOrigin.Value) / float(MovieScene::DiscreteSize(Data.InitialRange));

			if (bDraggingByEnd)
			{
				Data.MovieSection->SetRange(TRange<FFrameNumber>(Data.MovieSection->GetRange().GetLowerBound(), TRangeBound<FFrameNumber>::Exclusive(NewPosition)));
			}
			else
			{
				Data.MovieSection->SetRange(TRange<FFrameNumber>(TRangeBound<FFrameNumber>::Inclusive(NewPosition), Data.MovieSection->GetRange().GetUpperBound()));
			}

			TArray<FFrameNumber> NewFrameNumbers;
			for (const FPreDragChannelData& ChannelData : Data.Channels)
			{
				ISequencerChannelInterface* ChannelInterface = SequencerModule.FindChannelInterface(ChannelData.ChannelType);

				// Compute new frame times for each key
				NewFrameNumbers.Reset(ChannelData.FrameNumbers.Num());
				for (FFrameNumber StartFrame : ChannelData.FrameNumbers)
				{
					FFrameNumber NewTime = DilationOrigin + FFrameNumber(FMath::FloorToInt((StartFrame - DilationOrigin).Value * DilationFactor));
					NewFrameNumbers.Add(NewTime);
				}

				// Apply the key times to the channel
				void* RawChannelPtr = ChannelData.Channel.Get();
				if (RawChannelPtr)
				{
					ChannelInterface->SetKeyTimes_Raw(RawChannelPtr, ChannelData.Handles, NewFrameNumbers);
				}
			}
		}
	}
	/********************************************************************/
	else for (const FSectionHandle& Handle : Sections)
	{
		UMovieSceneSection* Section = Handle.GetSectionObject();

		// Find the corresponding sequencer section to this movie scene section
		for (const TSharedRef<ISequencerSection>& SequencerSection : Handle.TrackNode->GetSections())
		{
			if (SequencerSection->GetSectionObject() == Section)
			{
				FFrameNumber NewTime = SectionInitTimes[Section] + DeltaTime;

				if( bDraggingByEnd )
				{
					FFrameNumber MinFrame = Section->HasStartFrame() ? Section->GetInclusiveStartFrame() : TNumericLimits<int32>::Lowest();

					// Dragging the end of a section
					// Ensure we aren't shrinking past the start time
					NewTime = FMath::Max( NewTime, MinFrame );
				    if (bIsSlipping)
					{
						SequencerSection->SlipSection( NewTime/FrameResolution );
					}
					else
					{
						SequencerSection->ResizeSection( SSRM_TrailingEdge, NewTime );
					}
				}
				else
				{
					FFrameNumber MaxFrame = Section->HasEndFrame() ? Section->GetExclusiveEndFrame()-1 : TNumericLimits<int32>::Max();

					// Dragging the start of a section
					// Ensure we arent expanding past the end time
					NewTime = FMath::Min( NewTime, MaxFrame );

					if (bIsSlipping)
					{
						SequencerSection->SlipSection( NewTime/FrameResolution );
					}
					else
					{
						SequencerSection->ResizeSection( SSRM_LeadingEdge, NewTime );
					}
				}

				UMovieSceneTrack* OuterTrack = Section->GetTypedOuter<UMovieSceneTrack>();
				if (OuterTrack)
				{
					OuterTrack->Modify();
					OuterTrack->OnSectionMoved(*Section);
				}

				break;
			}
		}
	}

	{
		TSet<UMovieSceneTrack*> Tracks;
		for (auto SectionHandle : Sections)
		{
			if (UMovieSceneTrack* Track = SectionHandle.GetSectionObject()->GetTypedOuter<UMovieSceneTrack>())
			{
				Tracks.Add(Track);
			}
		}
		for (UMovieSceneTrack* Track : Tracks)
		{
			Track->UpdateEasing();
		}
	}

	Sequencer.NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
}

FMoveSection::FMoveSection( FSequencer& InSequencer, TArray<FSectionHandle> InSections )
	: FEditToolDragOperation( InSequencer )
{
	// Only allow sections that are not infinite to be movable.
	for (const FSectionHandle& InSection : InSections)
	{
		const UMovieSceneSection* Section = InSection.GetSectionObject();
		if (Section->HasStartFrame() && Section->HasEndFrame())
		{
			Sections.Add(InSection);
		}
	}

	SequencerNodeTreeUpdatedHandle = InSequencer.GetNodeTree()->OnUpdated().AddRaw(this, &FMoveSection::OnSequencerNodeTreeUpdated);
}

FMoveSection::~FMoveSection()
{
	Sequencer.GetNodeTree()->OnUpdated().Remove(SequencerNodeTreeUpdatedHandle);
}

void FMoveSection::OnBeginDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	if (!Sections.Num())
	{
		return;
	}

	BeginTransaction( Sections, NSLOCTEXT("Sequencer", "MoveSectionTransaction", "Move Section") );

	// Construct a snap field of unselected sections
	FDefaultSectionSnappingCandidates SnapCandidates(Sections);
	SnapField = FSequencerSnapField(Sequencer, SnapCandidates, ESequencerEntity::Section);

	const FFrameTime InitialPosition = VirtualTrackArea.PixelToFrame(LocalMousePos.X);

	RelativeOffsets.Reserve(Sections.Num());
	for (int32 Index = 0; Index < Sections.Num(); ++Index)
	{
		UMovieSceneSection* Section = Sections[Index].GetSectionObject();
		// Must have a start frame and end frame to be in the Sections array
		RelativeOffsets.Add(FRelativeOffset{ Section->GetInclusiveStartFrame() - InitialPosition, Section->GetExclusiveEndFrame() - InitialPosition });
	}

	TSet<UMovieSceneTrack*> Tracks;
	for (int32 Index = 0; Index < Sections.Num(); ++Index)
	{
		Tracks.Add(Sections[Index].TrackNode->GetTrack());
	}
	for (UMovieSceneTrack* Track : Tracks)
	{
		for (auto Section : Track->GetAllSections())
		{
			InitialRowIndices.Add(FInitialRowIndex{ Section, Section->GetRowIndex() });
		}
	}
}

void FMoveSection::OnEndDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	if (!Sections.Num())
	{
		return;
	}

	InitialRowIndices.Empty();

	TSet<UMovieSceneTrack*> Tracks;
	bool bRowIndicesFixed = false;
	for (auto& Handle : Sections)
	{
		Tracks.Add(Handle.TrackNode->GetTrack());
	}
	for (UMovieSceneTrack* Track : Tracks)
	{
		bRowIndicesFixed |= Track->FixRowIndices();
	}
	if (bRowIndicesFixed)
	{
		Sequencer.NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	}

	for (auto& Handle : Sections)
	{
		UMovieSceneSection* Section = Handle.GetSectionObject();
		UMovieSceneTrack* OuterTrack = Cast<UMovieSceneTrack>(Section->GetOuter());

		if (OuterTrack)
		{
			OuterTrack->Modify();
			OuterTrack->OnSectionMoved(*Section);
		}
	}

	EndTransaction();
}

void FMoveSection::OnDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	if (!Sections.Num())
	{
		return;
	}

	LocalMousePos.Y = FMath::Clamp(LocalMousePos.Y, 0.f, VirtualTrackArea.GetPhysicalSize().Y);

	FFrameRate FrameResolution = Sequencer.GetFocusedFrameResolution();
	FFrameRate PlayRate        = Sequencer.GetFocusedPlayRate();

	// Convert the current mouse position to a time
	FVector2D  VirtualMousePos = VirtualTrackArea.PhysicalToVirtual(LocalMousePos);
	FFrameTime MouseTime       = VirtualTrackArea.PixelToFrame(LocalMousePos.X);

	// Snapping
	if ( Settings->GetIsSnapEnabled() )
	{

		float SnapThresholdPx = VirtualTrackArea.PixelToSeconds(PixelSnapWidth) - VirtualTrackArea.PixelToSeconds(0.f);
		int32 SnapThreshold   = ( SnapThresholdPx * FrameResolution ).FloorToFrame().Value;

		TArray<FFrameNumber> SectionTimes;
		SectionTimes.Reserve(RelativeOffsets.Num());
		for (const auto& Offset : RelativeOffsets)
		{
			SectionTimes.Add((Offset.StartTime + MouseTime).FloorToFrame());
			SectionTimes.Add((Offset.EndTime   + MouseTime).FloorToFrame());
		}

		TOptional<FSequencerSnapField::FSnapResult> SnappedTime;

		if (Settings->GetSnapSectionTimesToSections())
		{
			SnappedTime = SnapField->Snap(SectionTimes, SnapThreshold);
		}

		if (!SnappedTime.IsSet() && Settings->GetSnapSectionTimesToInterval())
		{
			int32 IntervalSnapThreshold = FMath::RoundToInt( ( FrameResolution / PlayRate ).AsDecimal() );
			SnappedTime = SnapToInterval(SectionTimes, IntervalSnapThreshold, FrameResolution, PlayRate);
		}

		if (SnappedTime.IsSet())
		{
			// Add the snapped amount onto the delta
			MouseTime += (SnappedTime->Snapped - SnappedTime->Original);
		}
	}

	// If sections are all on different rows, don't set row indices for anything because it leads to odd behavior.
	bool bSectionsAreOnDifferentRows = false;
	int32 FirstRowIndex = Sections[0].GetSectionObject()->GetRowIndex();
	TArray<const UMovieSceneSection*> SectionsBeingMoved;
	for (auto SectionHandle : Sections)
	{
		if (FirstRowIndex != SectionHandle.GetSectionObject()->GetRowIndex())
		{
			bSectionsAreOnDifferentRows = true;
		}
		SectionsBeingMoved.Add(SectionHandle.GetSectionObject());
	}

	TOptional<FFrameNumber> MinDeltaXTime;

	// Disallow movement if any of the sections can't move
	for (int32 Index = 0; Index < Sections.Num(); ++Index)
	{
		auto& Handle = Sections[Index];
		UMovieSceneSection* Section = Handle.GetSectionObject();
		if (Section->GetBlendType().IsValid())
		{
			continue;
		}

		const FFrameNumber DeltaTime = (MouseTime + RelativeOffsets[Index].StartTime - Section->GetInclusiveStartFrame()).FloorToFrame();

		// Find the borders of where you can move to
		TRange<FFrameNumber> SectionBoundaries = GetSectionBoundaries(Section, Sections, Handle.TrackNode);

		FFrameNumber LeftMovementMaximum  = MovieScene::DiscreteInclusiveLower(SectionBoundaries);
		FFrameNumber RightMovementMaximum = MovieScene::DiscreteExclusiveUpper(SectionBoundaries);
		FFrameNumber NewStartTime         = Section->GetInclusiveStartFrame() + DeltaTime;
		FFrameNumber NewEndTime           = Section->GetExclusiveEndFrame()   + DeltaTime;

		if (NewStartTime < LeftMovementMaximum || NewEndTime > RightMovementMaximum)
		{
			FFrameNumber ClampedDeltaTime = NewStartTime < LeftMovementMaximum ? LeftMovementMaximum - Section->GetInclusiveStartFrame() : RightMovementMaximum - Section->GetExclusiveEndFrame();

			if (!MinDeltaXTime.IsSet() || MinDeltaXTime.GetValue() > ClampedDeltaTime)
			{
				MinDeltaXTime = ClampedDeltaTime;
			}
		}
	}

	bool bRowIndexChanged = false;
	for (int32 Index = 0; Index < Sections.Num(); ++Index)
	{
		auto& Handle = Sections[Index];
		UMovieSceneSection* Section = Handle.GetSectionObject();

		const FFrameNumber DeltaTime = (MouseTime + RelativeOffsets[Index].StartTime - Section->GetInclusiveStartFrame()).FloorToFrame();

		const TArray<UMovieSceneSection*>& AllSections = Handle.TrackNode->GetTrack()->GetAllSections();

		TArray<UMovieSceneSection*> NonDraggedSections;
		for (UMovieSceneSection* TrackSection : AllSections)
		{
			if (!SectionsBeingMoved.Contains(TrackSection))
			{
				NonDraggedSections.Add(TrackSection);
			}
		}

		int32 TargetRowIndex = Section->GetRowIndex();

		// vertical dragging
		if (Handle.TrackNode->GetTrack()->SupportsMultipleRows() && AllSections.Num() > 1)
		{
			// Compute the max row index whilst disregarding the one we're dragging
			int32 MaxRowIndex = 0;
			for (UMovieSceneSection* NonDraggedSection : NonDraggedSections)
			{
				if (NonDraggedSection != Section)
				{
					MaxRowIndex = FMath::Max(NonDraggedSection->GetRowIndex() + 1, MaxRowIndex);
				}
			}

			// Handle sub-track and non-sub-track dragging
			if (Handle.TrackNode->GetSubTrackMode() == FSequencerTrackNode::ESubTrackMode::None)
			{
				const int32 NumRows = FMath::Max(Section->GetRowIndex() + 1, MaxRowIndex);

				// Find the total height of the track - this is necessary because tracks may contain key areas, but they will not use sub tracks unless there is more than one row
				float VirtualSectionBottom = 0.f;
				Handle.TrackNode->TraverseVisible_ParentFirst([&](FSequencerDisplayNode& Node){ VirtualSectionBottom = Node.GetVirtualBottom(); return true; }, true);

				// Assume same height rows
				const float VirtualSectionTop = Handle.TrackNode->GetVirtualTop();
				const float VirtualSectionHeight = VirtualSectionBottom - Handle.TrackNode->GetVirtualTop();

				const float VirtualRowHeight = VirtualSectionHeight / NumRows;
				const float MouseOffsetWithinRow = VirtualMousePos.Y - (VirtualSectionTop + (VirtualRowHeight * TargetRowIndex));

				if (MouseOffsetWithinRow < VirtualRowHeight || MouseOffsetWithinRow > VirtualRowHeight)
				{
					const int32 NewIndex = FMath::FloorToInt((VirtualMousePos.Y - VirtualSectionTop) / VirtualRowHeight);
					TargetRowIndex = FMath::Clamp(NewIndex, 0, MaxRowIndex);
				}
				
				// If close to the top of the row, move else everything down
				if (VirtualMousePos.Y <= VirtualSectionTop)
				{
					TargetRowIndex = -1;
				}
			}
			else if(Handle.TrackNode->GetSubTrackMode() == FSequencerTrackNode::ESubTrackMode::SubTrack)
			{
				TSharedPtr<FSequencerTrackNode> ParentTrack = StaticCastSharedPtr<FSequencerTrackNode>(Handle.TrackNode->GetParent());
				if (ensure(ParentTrack.IsValid()))
				{
					for (int32 ChildIndex = 0; ChildIndex < ParentTrack->GetChildNodes().Num(); ++ChildIndex)
					{
						TSharedRef<FSequencerDisplayNode> ChildNode = ParentTrack->GetChildNodes()[ChildIndex];
						float VirtualSectionTop = ChildNode->GetVirtualTop();
						float VirtualSectionBottom = 0.f;
						ChildNode->TraverseVisible_ParentFirst([&](FSequencerDisplayNode& Node){ VirtualSectionBottom = Node.GetVirtualBottom(); return true; }, true);

						if (VirtualMousePos.Y < VirtualSectionBottom)
						{
							TargetRowIndex = ChildIndex;
							break;
						}
						else
						{
							TargetRowIndex = ChildIndex + 1;
						}
					}
				}
			}
		}

		bool bDeltaX = DeltaTime != 0;
		bool bDeltaY = TargetRowIndex != Section->GetRowIndex();

		// Horizontal movement
		if (bDeltaX)
		{
			Section->MoveSection(MinDeltaXTime.Get(DeltaTime));
		}

		// Vertical movement
		if (bDeltaY && !bSectionsAreOnDifferentRows &&
			(
				Section->GetBlendType().IsValid() ||
				!Section->OverlapsWithSections(NonDraggedSections, TargetRowIndex - Section->GetRowIndex(), DeltaTime.Value)
			)
		)
		{
			// Reached the top, move everything else we're not moving downwards
			if (TargetRowIndex == -1)
			{
				if (!bSectionsAreOnDifferentRows)
				{
					// If the sections being moved are all at the top, and all others are below it, do nothing
					bool bSectionsBeingMovedAreAtTop = true;
					for (auto InitialRowIndex : InitialRowIndices)
					{
						if (!SectionsBeingMoved.Contains(InitialRowIndex.Section))
						{
							if (InitialRowIndex.RowIndex <= FirstRowIndex)
							{
								bSectionsBeingMovedAreAtTop = false;
								break;
							}
						}
					}

					if (!bSectionsBeingMovedAreAtTop)
					{
						for (auto InitialRowIndex : InitialRowIndices)
						{
							if (!SectionsBeingMoved.Contains(InitialRowIndex.Section))
							{
								InitialRowIndex.Section->Modify();
								InitialRowIndex.Section->SetRowIndex(InitialRowIndex.RowIndex+1);
								bRowIndexChanged = true;
							}
						}
					}
				}
			}
			else
			{
				Section->Modify();
				Section->SetRowIndex(TargetRowIndex);
				bRowIndexChanged = true;
			}
		}
	}

	{
		TSet<UMovieSceneTrack*> Tracks;
		for (auto SectionHandle : Sections)
		{
			if (UMovieSceneTrack* Track = SectionHandle.GetSectionObject()->GetTypedOuter<UMovieSceneTrack>())
			{
				Tracks.Add(Track);
			}
		}
		for (UMovieSceneTrack* Track : Tracks)
		{
			Track->UpdateEasing();
		}
	}

	Sequencer.NotifyMovieSceneDataChanged( bRowIndexChanged 
		? EMovieSceneDataChangeType::MovieSceneStructureItemsChanged 
		: EMovieSceneDataChangeType::TrackValueChanged );
}

void CollateTrackNodesByTrack(const TArray<TSharedRef<FSequencerDisplayNode>>& DisplayNodes, TMap<UMovieSceneTrack*, TArray<TSharedRef<FSequencerTrackNode>>>& TrackToTrackNodesMap)
{
	for (TSharedRef<FSequencerDisplayNode> DisplayNode : DisplayNodes)
	{
		if (DisplayNode->GetType() == ESequencerNode::Track)
		{
			TSharedRef<FSequencerTrackNode> TrackNode = StaticCastSharedRef<FSequencerTrackNode>(DisplayNode);
			TArray<TSharedRef<FSequencerTrackNode>>* TrackNodes = TrackToTrackNodesMap.Find(TrackNode->GetTrack());
			if (TrackNodes == nullptr)
			{
				TrackNodes = &TrackToTrackNodesMap.Add(TrackNode->GetTrack());
			}
			TrackNodes->Add(TrackNode);
		}

		CollateTrackNodesByTrack(DisplayNode->GetChildNodes(), TrackToTrackNodesMap);
	}
}

bool TryUpdateHandleFromNewTrackNodes(const TArray<TSharedRef<FSequencerTrackNode>>& NewTrackNodes, FSectionHandle& SectionHandle)
{
	UMovieSceneSection* MovieSceneSection = SectionHandle.GetSectionObject();
	for (TSharedRef<FSequencerTrackNode> NewTrackNode : NewTrackNodes)
	{
		const TArray<TSharedRef<ISequencerSection>> SequencerSections = NewTrackNode->GetSections();
		for (int32 i = 0; i < SequencerSections.Num(); i++)
		{
			if (SequencerSections[i]->GetSectionObject() == MovieSceneSection)
			{
				SectionHandle.TrackNode = NewTrackNode;
				SectionHandle.SectionIndex = i;
				return true;
			}
		}
	}
	return false;
}

void FMoveSection::OnSequencerNodeTreeUpdated()
{
	TMap<UMovieSceneTrack*, TArray<TSharedRef<FSequencerTrackNode>>> TrackToTrackNodesMap;
	CollateTrackNodesByTrack(Sequencer.GetNodeTree()->GetRootNodes(), TrackToTrackNodesMap);

	// Update the track nodes in the handles based on the original track and section index.
	for (FSectionHandle& SectionHandle : Sections)
	{
		TArray<TSharedRef<FSequencerTrackNode>>* NewTrackNodes = TrackToTrackNodesMap.Find(SectionHandle.TrackNode->GetTrack());
		ensureMsgf(NewTrackNodes != nullptr, TEXT("Error rebuilding section handles:  Track not found after node tree update."));

		if (NewTrackNodes != nullptr)
		{
			bool bHandleUpdated = TryUpdateHandleFromNewTrackNodes(*NewTrackNodes, SectionHandle);
			ensureMsgf(bHandleUpdated, TEXT("Error rebuilding section handles: Track node with correct track and section index could not be found."));
		}
	}
}

void FMoveKeys::OnBeginDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	check( SelectedKeys.Num() > 0 )


	FDefaultKeySnappingCandidates SnapCandidates(SelectedKeys);
	SnapField = FSequencerSnapField(Sequencer, SnapCandidates);

	SelectedKeyArray = SelectedKeys.Array();

	// Begin an editor transaction and mark the section as transactional so it's state will be saved
	TArray<FSectionHandle> DummySections;
	BeginTransaction( DummySections, NSLOCTEXT("Sequencer", "MoveKeysTransaction", "Move Keys") );

	// Populate the relative offset for each key
	TArray<FFrameNumber> KeyTimes;
	KeyTimes.SetNum(SelectedKeyArray.Num());
	GetKeyTimes(SelectedKeyArray, KeyTimes);

	const FFrameTime MouseTime = VirtualTrackArea.PixelToFrame(LocalMousePos.X);

	RelativeOffsets.Reserve(KeyTimes.Num());
	for (FFrameNumber Time : KeyTimes)
	{
		RelativeOffsets.Add(Time - MouseTime);
	}

	for( const FSequencerSelectedKey& SelectedKey : SelectedKeyArray )
	{
		UMovieSceneSection* OwningSection = SelectedKey.Section;

		// Only modify sections once
		if( !ModifiedSections.Contains( OwningSection ) )
		{
			OwningSection->SetFlags( RF_Transactional );

			// Save the current state of the section
			if (OwningSection->TryModify())
			{
				// Section has been modified
				ModifiedSections.Add( OwningSection );
			}
		}
	}
}

void FMoveKeys::OnDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	if( MouseEvent.GetCursorDelta().X == 0.0f )
	{
		return;
	}

	FFrameRate FrameResolution = Sequencer.GetFocusedFrameResolution();
	FFrameRate PlayRate        = Sequencer.GetFocusedPlayRate();

	FFrameTime MouseTime       = VirtualTrackArea.PixelToFrame(LocalMousePos.X);

	TArray<FFrameNumber> KeyTimes;
	KeyTimes.Reserve(RelativeOffsets.Num());

	for( FFrameTime Time : RelativeOffsets )
	{
		KeyTimes.Add( (MouseTime + Time).FloorToFrame() );
	}

	// Snapping
	if ( Settings->GetIsSnapEnabled() )
	{

		float SnapThresholdPx = VirtualTrackArea.PixelToSeconds(PixelSnapWidth) - VirtualTrackArea.PixelToSeconds(0.f);
		int32 SnapThreshold   = ( SnapThresholdPx * FrameResolution ).FloorToFrame().Value;

		TOptional<FSequencerSnapField::FSnapResult> SnappedTime;

		if (Settings->GetSnapKeyTimesToKeys())
		{
			SnappedTime = SnapField->Snap(KeyTimes, SnapThreshold);
		}

		if (!SnappedTime.IsSet() && Settings->GetSnapKeyTimesToInterval())
		{
			int32 IntervalSnapThreshold = FMath::RoundToInt( ( FrameResolution / PlayRate ).AsDecimal() );
			SnappedTime = SnapToInterval(KeyTimes, IntervalSnapThreshold, FrameResolution, PlayRate);
		}

		if (SnappedTime.IsSet())
		{
			MouseTime += SnappedTime->Snapped - SnappedTime->Original;

			// Reset the new key times to account for the snap position
			for( int32 Index = 0; Index < KeyTimes.Num(); ++Index )
			{
				KeyTimes[Index] = (MouseTime + RelativeOffsets[Index]).FloorToFrame();
			}
		}
	}

	// Apply new key times to the selection
	SetKeyTimes(SelectedKeyArray, KeyTimes);

	for (int32 Index = 0; Index < KeyTimes.Num(); ++Index)
	{
		FSequencerSelectedKey SelectedKey = SelectedKeyArray[Index];

		UMovieSceneSection* Section = SelectedKey.Section;
		if (ModifiedSections.Contains(Section))
		{
			// If the key moves outside of the section resize the section to fit the key
			// @todo Sequencer - Doesn't account for hitting other sections 
			const FFrameNumber   NewKeyTime   = KeyTimes[Index];
			TRange<FFrameNumber> SectionRange = Section->GetRange();

			if (!SectionRange.Contains(NewKeyTime))
			{
				TRange<FFrameNumber> NewRange = TRange<FFrameNumber>::Hull( SectionRange, TRange<FFrameNumber>(NewKeyTime) );
				Section->SetRange(NewRange);
			}
		}
	}


	// Snap the play time to the new dragged key time if all the keyframes were dragged to the same time
	if (Settings->GetSnapPlayTimeToDraggedKey() && KeyTimes.Num())
	{
		FFrameNumber FirstFrame        = KeyTimes[0];
		auto         EqualsFirstFrame  = [=](FFrameNumber In)
		{
			return In == FirstFrame;
		};

		if (Algo::AllOf(KeyTimes, EqualsFirstFrame))
		{
			Sequencer.SetLocalTime(FirstFrame);
		}
	}

	for (UMovieSceneSection* Section : ModifiedSections)
	{
		Section->MarkAsChanged();
	}
	Sequencer.NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
}

void FMoveKeys::OnEndDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	ModifiedSections.Empty();
	EndTransaction();
}

void FDuplicateKeys::OnBeginDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	// Begin an editor transaction and mark the section as transactional so it's state will be saved
	TArray<FSectionHandle> DummySections;
	BeginTransaction( DummySections, NSLOCTEXT("Sequencer", "DuplicateKeysTransaction", "Duplicate Keys") );

	// Modify all the sections first
	for (const FSequencerSelectedKey& SelectedKey : SelectedKeys)
	{
		UMovieSceneSection* OwningSection = SelectedKey.Section;

		// Only modify sections once
		if (!ModifiedSections.Contains(OwningSection ))
		{
			OwningSection->SetFlags(RF_Transactional);
			// Save the current state of the section
			if (OwningSection->TryModify())
			{
				// Section has been modified
				ModifiedSections.Add( OwningSection );
			}
		}
	}

	// Then duplicate the keys

	// @todo sequencer: selection in transactions
	FSequencerSelection& Selection = Sequencer.GetSelection();

	TArray<FSequencerSelectedKey> DuplicatedKeyArray = SelectedKeys.Array();
	TArray<FKeyHandle> NewKeyHandles;
	// Ideally we'd memset here, but there's no existing method to copy n bytes to an array n times
	NewKeyHandles.SetNumUninitialized(DuplicatedKeyArray.Num());
	for (FKeyHandle& Handle : NewKeyHandles)
	{
		Handle = FKeyHandle::Invalid();
	}

	Selection.EmptySelectedKeys();
	DuplicateKeys(DuplicatedKeyArray, NewKeyHandles);
	
	for (int32 Index = 0; Index < NewKeyHandles.Num(); ++Index)
	{
		FSequencerSelectedKey NewKey = DuplicatedKeyArray[Index];
		NewKey.KeyHandle = NewKeyHandles[Index];
		Selection.AddToSelection(NewKey);
	}

	// Now start the move drag
	FMoveKeys::OnBeginDrag(MouseEvent, LocalMousePos, VirtualTrackArea);
}

void FDuplicateKeys::OnEndDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	FMoveKeys::OnEndDrag(MouseEvent, LocalMousePos, VirtualTrackArea);

	EndTransaction();
}

FManipulateSectionEasing::FManipulateSectionEasing( FSequencer& InSequencer, FSectionHandle InSection, bool _bEaseIn )
	: FEditToolDragOperation(InSequencer)
	, Handle(InSection)
	, bEaseIn(_bEaseIn)
	, MouseDownTime(0)
{
}

void FManipulateSectionEasing::OnBeginDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	Transaction.Reset( new FScopedTransaction(NSLOCTEXT("Sequencer", "DragSectionEasing", "Change Section Easing")) );

	UMovieSceneSection* Section = Handle.GetSectionObject();
	Section->SetFlags( RF_Transactional );
	Section->Modify();

	MouseDownTime = VirtualTrackArea.PixelToFrame(LocalMousePos.X);

	if (Settings->GetSnapSectionTimesToSections())
	{
		// Construct a snap field of all section bounds
		ISequencerSnapCandidate SnapCandidates;
		SnapField = FSequencerSnapField(Sequencer, SnapCandidates, ESequencerEntity::Section);
	}

	InitValue = bEaseIn ? Section->Easing.GetEaseInDuration() : Section->Easing.GetEaseOutDuration();
}

void FManipulateSectionEasing::OnDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	FFrameRate FrameResolution = Sequencer.GetFocusedFrameResolution();
	FFrameRate PlayRate        = Sequencer.GetFocusedPlayRate();

	// Convert the current mouse position to a time
	FFrameTime  DeltaTime = VirtualTrackArea.PixelToFrame(LocalMousePos.X) - MouseDownTime;

	// Snapping
	if (Settings->GetIsSnapEnabled())
	{
		TArray<FFrameNumber> SnapTimes;

		UMovieSceneSection* Section = Handle.GetSectionObject();
		if (bEaseIn)
		{
			FFrameNumber DesiredTime = (DeltaTime + Section->GetInclusiveStartFrame() + InitValue.Get(0)).RoundToFrame();
			SnapTimes.Add(DesiredTime);
		}
		else
		{
			FFrameNumber DesiredTime = (Section->GetExclusiveEndFrame() - InitValue.Get(0) + DeltaTime).RoundToFrame();
			SnapTimes.Add(DesiredTime);
		}

		float SnapThresholdPx = VirtualTrackArea.PixelToSeconds(PixelSnapWidth) - VirtualTrackArea.PixelToSeconds(0.f);
		int32 SnapThreshold   = ( SnapThresholdPx * FrameResolution ).FloorToFrame().Value;

		TOptional<FSequencerSnapField::FSnapResult> SnappedTime;

		if (Settings->GetSnapSectionTimesToSections())
		{
			SnappedTime = SnapField->Snap(SnapTimes, SnapThreshold);
		}

		if (!SnappedTime.IsSet() && Settings->GetSnapSectionTimesToInterval())
		{
			int32 IntervalSnapThreshold = FMath::RoundToInt( ( FrameResolution / PlayRate ).AsDecimal() );
			SnappedTime = SnapToInterval(SnapTimes, IntervalSnapThreshold, FrameResolution, PlayRate);
		}

		if (SnappedTime.IsSet())
		{
			// Add the snapped amount onto the delta
			DeltaTime += SnappedTime->Snapped - SnappedTime->Original;
		}
	}

	UMovieSceneSection* Section = Handle.GetSectionObject();

	const int32 MaxEasingDuration = Section->HasStartFrame() && Section->HasEndFrame() ? MovieScene::DiscreteSize(Section->GetRange()) : TNumericLimits<int32>::Max() / 2;

	if (bEaseIn)
	{
		Section->Easing.bManualEaseIn = true;
		Section->Easing.ManualEaseInDuration  = FMath::Clamp(InitValue.Get(0) + DeltaTime.RoundToFrame().Value, 0, MaxEasingDuration);
	}
	else
	{
		Section->Easing.bManualEaseOut = true;
		Section->Easing.ManualEaseOutDuration = FMath::Clamp(InitValue.Get(0) - DeltaTime.RoundToFrame().Value, 0, MaxEasingDuration);
	}

	UMovieSceneTrack* OuterTrack = Section->GetTypedOuter<UMovieSceneTrack>();
	if (OuterTrack)
	{
		OuterTrack->MarkAsChanged();
	}

	Sequencer.NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
}

void FManipulateSectionEasing::OnEndDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	EndTransaction();
}

