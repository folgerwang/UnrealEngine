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
#include "Channels/MovieSceneChannel.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "ISequencerModule.h"

struct FInvalidKeyAndSectionSnappingCandidates : ISequencerSnapCandidate
{
	/**
	 * Keys and Sections added to this ISequencerSnapCandidate will be ignored as potential candidates for snapping.
	 */
	FInvalidKeyAndSectionSnappingCandidates(const TSet<FSequencerSelectedKey>& InKeysToIgnore, const TArray<FSectionHandle>& InSectionsToIgnore)
	{
		KeysToExclude = InKeysToIgnore;
		for (const FSectionHandle& SectionHandle : InSectionsToIgnore)
		{
			SectionsToExclude.Add(SectionHandle.GetSectionObject());
		}
	}

	virtual bool IsKeyApplicable(FKeyHandle KeyHandle, const TSharedPtr<IKeyArea>& KeyArea, UMovieSceneSection* Section) override
	{
		return !KeysToExclude.Contains(FSequencerSelectedKey(*Section, KeyArea, KeyHandle));
	}

	virtual bool AreSectionBoundsApplicable(UMovieSceneSection* Section) override
	{
		return !SectionsToExclude.Contains(Section);
	}
	
protected:
	TSet<FSequencerSelectedKey> KeysToExclude;
	TSet<UMovieSceneSection*> SectionsToExclude;
};

TOptional<FSequencerSnapField::FSnapResult> SnapToInterval(const TArray<FFrameNumber>& InTimes, int32 FrameThreshold, FFrameRate Resolution, FFrameRate DisplayRate, ESequencerScrubberStyle ScrubStyle)
{
	TOptional<FSequencerSnapField::FSnapResult> Result;

	FFrameNumber SnapAmount(0);
	for (FFrameNumber Time : InTimes)
	{
		// Convert from resolution to DisplayRate, round to frame, then back again. We floor to frames when using the frame block scrubber, and round using the vanilla scrubber
		FFrameTime   DisplayTime      = FFrameRate::TransformTime(Time, Resolution, DisplayRate);
		FFrameNumber PlayIntervalTime = ScrubStyle == ESequencerScrubberStyle::FrameBlock ? DisplayTime.FloorToFrame() : DisplayTime.RoundToFrame();
		FFrameNumber IntervalSnap     = FFrameRate::TransformTime(PlayIntervalTime, DisplayRate, Resolution  ).FloorToFrame();

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
	TSet<FSequencerSelectedKey> EmptyKeySet;
	FInvalidKeyAndSectionSnappingCandidates SnapCandidates(EmptyKeySet, Sections);
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
				TArrayView<FMovieSceneChannel* const> ChannelPtrs = Entry.GetChannels();
				for (int32 Index = 0; Index < ChannelPtrs.Num(); ++Index)
				{
					// Populate the cached state of this channel
					FPreDragChannelData& ChannelData = ResizeData.Channels[ResizeData.Channels.Emplace()];
					ChannelData.Channel = Proxy.MakeHandle(Entry.GetChannelTypeName(), Index);

					ChannelPtrs[Index]->GetKeys(TRange<FFrameNumber>::All(), &ChannelData.FrameNumbers, &ChannelData.Handles);
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

	ESequencerScrubberStyle ScrubStyle = Sequencer.GetScrubStyle();

	FFrameRate   TickResolution  = Sequencer.GetFocusedTickResolution();
	FFrameRate   DisplayRate     = Sequencer.GetFocusedDisplayRate();

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
		int32 SnapThreshold   = ( SnapThresholdPx * TickResolution ).FloorToFrame().Value;

		TOptional<FSequencerSnapField::FSnapResult> SnappedTime;

		if (Settings->GetSnapSectionTimesToSections())
		{
			SnappedTime = SnapField->Snap(SectionTimes, SnapThreshold);
		}

		if (!SnappedTime.IsSet() && Settings->GetSnapSectionTimesToInterval())
		{
			int32 IntervalSnapThreshold = FMath::RoundToInt( ( TickResolution / DisplayRate ).AsDecimal() );
			SnappedTime = SnapToInterval(SectionTimes, IntervalSnapThreshold, TickResolution, DisplayRate, ScrubStyle);
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
			// It is only valid to dilate a fixed bound. Tracks can have mixed bounds types (ie: infinite upper, closed lower)
			check(bDraggingByEnd ? Data.InitialRange.GetUpperBound().IsClosed() : Data.InitialRange.GetLowerBound().IsClosed());

			FFrameNumber StartPosition  = bDraggingByEnd ? MovieScene::DiscreteExclusiveUpper(Data.InitialRange) : MovieScene::DiscreteInclusiveLower(Data.InitialRange);

			FFrameNumber DilationOrigin;
			if (bDraggingByEnd)
			{
				if (Data.InitialRange.GetLowerBound().IsClosed())
				{
					DilationOrigin = MovieScene::DiscreteInclusiveLower(Data.InitialRange);
				}
				else
				{
					// We're trying to dilate a track that has an infinite lower bound as its origin.
					// Sections already compute an effective range for UMG's auto-playback range, so we'll use that to have it handle finding either the
					// uppermost key or the overall length of the section.
					DilationOrigin = Data.MovieSection->ComputeEffectiveRange().GetLowerBoundValue();
				}
			}
			else
			{
				if (Data.InitialRange.GetUpperBound().IsClosed())
				{
					DilationOrigin = MovieScene::DiscreteExclusiveUpper(Data.InitialRange);
				}
				else
				{
					// We're trying to dilate a track that has an infinite upper bound as its origin. 
					DilationOrigin = Data.MovieSection->ComputeEffectiveRange().GetUpperBoundValue();

				}
			}

			// Because we can have an one-sided infinite data range, we calculate a new range using our clamped values. 
			TRange<FFrameNumber> DataRange;
			DataRange.SetLowerBound(TRangeBound<FFrameNumber>(DilationOrigin < StartPosition ? DilationOrigin : StartPosition));
			DataRange.SetUpperBound(TRangeBound<FFrameNumber>(DilationOrigin > StartPosition ? DilationOrigin : StartPosition));

			FFrameNumber NewPosition    = bDraggingByEnd ? FMath::Max(StartPosition + DeltaTime, DilationOrigin) : FMath::Min(StartPosition + DeltaTime, DilationOrigin);

			float DilationFactor = FMath::Abs(NewPosition.Value - DilationOrigin.Value) / float(MovieScene::DiscreteSize(DataRange));

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
				// Compute new frame times for each key
				NewFrameNumbers.Reset(ChannelData.FrameNumbers.Num());
				for (FFrameNumber StartFrame : ChannelData.FrameNumbers)
				{
					FFrameNumber NewTime = DilationOrigin + FFrameNumber(FMath::FloorToInt((StartFrame - DilationOrigin).Value * DilationFactor));
					NewFrameNumbers.Add(NewTime);
				}

				// Apply the key times to the channel
				FMovieSceneChannel* Channel = ChannelData.Channel.Get();
				if (Channel)
				{
					Channel->SetKeyTimes(ChannelData.Handles, NewFrameNumbers);
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
						SequencerSection->SlipSection( NewTime/TickResolution );
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
						SequencerSection->SlipSection( NewTime/TickResolution );
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

void FDuplicateKeysAndSections::OnBeginDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	// Begin an editor transaction and mark the section as transactional so it's state will be saved
	BeginTransaction( Sections, NSLOCTEXT("Sequencer", "DuplicateKeysTransaction", "Duplicate Keys or Sections") );

	// Call Modify on all of the sections that own keys we have selected so that when we duplicate keys we can restore them properly.
	ModifyNonSelectedSections();

	// We're going to take our current selection and make a duplicate of each item in it and leave those items behind.
	// This means our existing selection will still refer to the same keys, so we're duplicating and moving the originals.
	// This saves us from modifying the user's selection when duplicating. We can't move the duplicates as we can't get
	// section handles for sections until the tree is rebuilt.
	TArray<FKeyHandle> NewKeyHandles;
	NewKeyHandles.SetNumZeroed(KeysAsArray.Num());

	// Duplicate our keys into the NewKeyHandles array. Duplicating keys automatically updates their sections,
	// so we don't need to actually use the new key handles.
	DuplicateKeys(KeysAsArray, NewKeyHandles);

	// Duplicate our selections as well.
	bool bDelayedStructureRebuild = false;

	TArray<UMovieSceneSection*> SectionsToDuplicate;
	for (const FSectionHandle& SectionHandle : Sections)
	{
		SectionsToDuplicate.Add(SectionHandle.GetSectionObject());
	}

	for (UMovieSceneSection* SectionToDuplicate : SectionsToDuplicate)
	{
		UMovieSceneSection* DuplicatedSection = DuplicateObject<UMovieSceneSection>(SectionToDuplicate, SectionToDuplicate->GetOuter());
		UMovieSceneTrack* OwningTrack = SectionToDuplicate->GetTypedOuter<UMovieSceneTrack>();
		OwningTrack->Modify();
		OwningTrack->AddSection(*DuplicatedSection);

		bDelayedStructureRebuild = true;
	}

	// Now start the move drag
	FMoveKeysAndSections::OnBeginDrag(MouseEvent, LocalMousePos, VirtualTrackArea);

	if (bDelayedStructureRebuild)
	{
		// We need to rebuild the track layout now so that the newly added section shows up, otherwise it won't show up until a section is vertically rearranged.
		Sequencer.NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	}
}

void FDuplicateKeysAndSections::OnEndDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	FMoveKeysAndSections::OnEndDrag(MouseEvent, LocalMousePos, VirtualTrackArea);

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
	ESequencerScrubberStyle ScrubStyle = Sequencer.GetScrubStyle();

	FFrameRate TickResolution  = Sequencer.GetFocusedTickResolution();
	FFrameRate DisplayRate     = Sequencer.GetFocusedDisplayRate();

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
		int32 SnapThreshold   = ( SnapThresholdPx * TickResolution ).FloorToFrame().Value;

		TOptional<FSequencerSnapField::FSnapResult> SnappedTime;

		if (Settings->GetSnapSectionTimesToSections())
		{
			SnappedTime = SnapField->Snap(SnapTimes, SnapThreshold);
		}

		if (!SnappedTime.IsSet() && Settings->GetSnapSectionTimesToInterval())
		{
			int32 IntervalSnapThreshold = FMath::RoundToInt( ( TickResolution / DisplayRate ).AsDecimal() );
			SnappedTime = SnapToInterval(SnapTimes, IntervalSnapThreshold, TickResolution, DisplayRate, ScrubStyle);
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


FMoveKeysAndSections::FMoveKeysAndSections(FSequencer& InSequencer, const TSet<FSequencerSelectedKey>& InSelectedKeys, TArray<FSectionHandle> InSelectedSections, bool InbHotspotWasSection)
	: FEditToolDragOperation(InSequencer)
	, bHotspotWasSection(InbHotspotWasSection)
{
	// Filter out the keys on sections that are read only
	for (auto SelectedKey : InSelectedKeys)
	{
		if (!SelectedKey.Section->IsReadOnly())
		{
			Keys.Add(SelectedKey);
		}
	}

	KeysAsArray = Keys.Array();

	// However, we don't want infinite sections to be movable, so we discard them from our selection.
	// We support partially infinite (infinite on one side) sections however.
	for (const FSectionHandle& SectionHandle : InSelectedSections)
	{
		const UMovieSceneSection* Section = SectionHandle.GetSectionObject();
		if (Section->HasStartFrame() || Section->HasEndFrame())
		{
			Sections.Add(SectionHandle);
		}
	}

	// Register a callback for when the node tree is updated so we can update our local Section Handle array.
	SequencerNodeTreeUpdatedHandle = InSequencer.GetNodeTree()->OnUpdated().AddRaw(this, &FMoveKeysAndSections::OnSequencerNodeTreeUpdated);
}

FMoveKeysAndSections::~FMoveKeysAndSections()
{
	Sequencer.GetNodeTree()->OnUpdated().Remove(SequencerNodeTreeUpdatedHandle);
}

void FMoveKeysAndSections::OnBeginDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	// Early out if we've somehow started a drag operation without any sections or keys. This prevents an empty Undo/Redo Transaction from being created.
	if (!Sections.Num() && !Keys.Num())
	{
		return;
	}

	BeginTransaction(Sections, NSLOCTEXT("Sequencer", "MoveKeyAndSectionTransaction", "Move Keys or Sections"));

	// Tell the Snap Field to ignore our currently selected keys and sections. We can snap to the edges of non-selected
	// sections and keys. The actual snapping field will add other sequencer data (play ranges, playheads, etc.) as snap targets.
	FInvalidKeyAndSectionSnappingCandidates AvoidSnapCanidates(Keys, Sections); 
	SnapField = FSequencerSnapField(Sequencer, AvoidSnapCanidates );

	// Store the frame time of the mouse so we can see how far we've moved from the starting point.
	MouseTimePrev = VirtualTrackArea.PixelToFrame(LocalMousePos.X).FloorToFrame();

	// Now we store a relative offset to each key and section from the start position. This allows us to know how far away from
	// the mouse each valid key/section was so we can restore their offset if needed.
	RelativeOffsets.Reserve(Sections.Num() + Keys.Num());
	for (int32 Index = 0; Index < Sections.Num(); ++Index)
	{
		UMovieSceneSection* Section = Sections[Index].GetSectionObject();
		FRelativeOffset Offset;

		if (Section->HasStartFrame())
		{
			Offset.StartOffset = Section->GetInclusiveStartFrame() - MouseTimePrev;
		}
		if (Section->HasEndFrame())
		{
			Offset.EndOffset = Section->GetExclusiveEndFrame() - MouseTimePrev;
		}

		RelativeOffsets.Add(Offset);
	}

	// Sections can be dragged vertically to adjust their row up or down, so we need to store what row each section is currently on. A section
	// can be dragged above all other sections - this is accomplished by moving all other sections down. We store the row indices for all sections
	// in all tracks that we're modifying so we can get them later to move them.
	TSet<UMovieSceneTrack*> Tracks;
	for (int32 Index = 0; Index < Sections.Num(); ++Index)
	{
		Tracks.Add(Sections[Index].TrackNode->GetTrack());
	}
	for (UMovieSceneTrack* Track : Tracks)
	{
		for (auto Section : Track->GetAllSections())
		{
			InitialSectionRowIndicies.Add(FInitialRowIndex{ Section, Section->GetRowIndex() });
		}
	}

	// Our Key Handles don't store their times so we need to convert the handles into an array of times
	// so that we can store the relative offset to each one.
	TArray<FFrameNumber> KeyTimes;
	KeyTimes.SetNum(Keys.Num());
	GetKeyTimes(KeysAsArray, KeyTimes);

	for (int32 Index = 0; Index < KeyTimes.Num(); ++Index)
	{
		// Key offsets use only the Start offset and don't set the End offset as they do not represent ranges.
		FRelativeOffset KeyOffset;
		KeyOffset.StartOffset = KeyTimes[Index] - MouseTimePrev;

		// These are packed with our Section Offsets (ToDo: Is this actually a good idea?)
		RelativeOffsets.Add(KeyOffset);
	}

	// Keys can be moved within sections without the section itself being moved, so we need to call Modify on any section that owns a key that isn't also being moved.
	ModifyNonSelectedSections();
}

void FMoveKeysAndSections::OnDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	if (!Sections.Num() && !Keys.Num())
	{
		return;
	}

	ESequencerScrubberStyle ScrubStyle = Sequencer.GetScrubStyle();

	FFrameRate TickResolution = Sequencer.GetFocusedTickResolution();
	FFrameRate DisplayRate = Sequencer.GetFocusedDisplayRate();

	// Convert the current mouse position to a time
	FVector2D  VirtualMousePos = VirtualTrackArea.PhysicalToVirtual(LocalMousePos);
	FFrameTime MouseTime = VirtualTrackArea.PixelToFrame(LocalMousePos.X);

	// Calculate snapping first which modifies our MouseTime to reflect where it would have to be for the closest snap to work.
	if (Settings->GetIsSnapEnabled())
	{
		float SnapThresholdPx = VirtualTrackArea.PixelToSeconds(PixelSnapWidth) - VirtualTrackArea.PixelToSeconds(0.f);
		int32 SnapThreshold = (SnapThresholdPx * TickResolution).FloorToFrame().Value;

		// The edge of each bounded section as well as each individual key is a valid marker to try and snap to intervals/sections/etc.
		// We take our stored offsets and add them to our current time to figure out where on the timeline the are currently.
		TArray<FFrameNumber> ValidSnapMarkers;

		// If they have both keys and settings selected then we snap to the interval if either one of them is enabled, otherwise respect the individual setting.
		const bool bSnapToInterval = (KeysAsArray.Num() > 0 && Settings->GetSnapKeyTimesToInterval()) || (Sections.Num() > 0 && Settings->GetSnapSectionTimesToInterval());
		const bool bSnapToLikeTypes = (KeysAsArray.Num() > 0 && Settings->GetSnapKeyTimesToKeys()) || (Sections.Num() > 0 && Settings->GetSnapSectionTimesToSections());

		// RelativeOffsets contains both our sections and our keys, and we add them all as potential things that can snap to stuff.
		for (const FRelativeOffset& Offset : RelativeOffsets)
		{
			if (Offset.StartOffset.IsSet())
			{
				ValidSnapMarkers.Add((Offset.StartOffset.GetValue() + MouseTime).FloorToFrame());
			}
			if (Offset.EndOffset.IsSet())
			{
				ValidSnapMarkers.Add((Offset.EndOffset.GetValue() + MouseTime).FloorToFrame());
			}
		}

		// Now we'll try and snap all of these points to the closest valid snap marker (which may be a section or interval)
		TOptional<FSequencerSnapField::FSnapResult> SnappedTime;

		if (bSnapToLikeTypes)
		{
			// This may or may not set the SnappedTime depending on if there are any sections within the threshold.
			SnappedTime = SnapField->Snap(ValidSnapMarkers, SnapThreshold);
		}

		if (!SnappedTime.IsSet() && bSnapToInterval)
		{
			// Snap to the nearest interval (if enabled). Snapping to other objects has over interval.
			int32 IntervalSnapThreshold = FMath::RoundToInt((TickResolution / DisplayRate).AsDecimal());
			SnappedTime = SnapToInterval(ValidSnapMarkers, IntervalSnapThreshold, TickResolution, DisplayRate, ScrubStyle);
		}

		// If they actually snapped to something (snapping may be on but settings might dictate nothing to snap to) add the difference
		// to our current MouseTime so that MouseTime reflects the amount needed to move to get to the whole snap point.
		if (SnappedTime.IsSet())
		{
			// Add the snapped amount onto the mouse time so the resulting delta brings us in alignment.
			MouseTime += (SnappedTime->Snapped - SnappedTime->Original);
		}
	}

	// We'll calculate a DeltaX based on limits on movement (snapping, section collision) and then use them on keys and sections below.
	TOptional<FFrameNumber> MaxDeltaX = GetMovementDeltaX(MouseTime);

	FFrameNumber MouseDeltaTime = (MouseTime - MouseTimePrev).FloorToFrame();
	MouseTimePrev = MouseTimePrev + MaxDeltaX.Get(MouseDeltaTime);

	// Move sections horizontally (limited by our calculated delta) and vertically based on mouse cursor.
	bool bSectionMovementModifiedStructure = HandleSectionMovement(MouseTime, VirtualMousePos, LocalMousePos, MaxDeltaX, MouseDeltaTime);

	// Update our key times by moving them by our delta.
	HandleKeyMovement(MaxDeltaX, MouseDeltaTime);

	// Get a list of the unique tracks in this selection and update their easing so previews draw interactively as you drag.
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

	// If we changed the layout by rearranging sections we need to tell the Sequencer to rebuild things, otherwise just re-evaluate existing tracks.
	if (bSectionMovementModifiedStructure)
	{
		Sequencer.NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	}
	else
	{
		Sequencer.NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
	}
}

void FMoveKeysAndSections::OnEndDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	if (!Sections.Num() && !Keys.Num())
	{
		return;
	}

	InitialSectionRowIndicies.Empty();
	ModifiedNonSelectedSections.Empty();

	// Tracks can tell us if the row indexes for any sections were changed during our drag/drop operation.
	bool bRowIndicesChanged = false;
	TSet<UMovieSceneTrack*> Tracks;

	for (auto& SectionHandle : Sections)
	{
		// Grab only unique tracks as multiple sections can reside on the same track.
		Tracks.Add(SectionHandle.TrackNode->GetTrack());
	}

	for (UMovieSceneTrack* Track : Tracks)
	{
		// Ensure all of the tracks have updated the row indices for their sections
		bRowIndicesChanged |= Track->FixRowIndices();
	}

	if (bRowIndicesChanged)
	{
		Sequencer.NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	}

	for (const FSectionHandle& SectionHandle : Sections)
	{
		UMovieSceneSection* Section = SectionHandle.GetSectionObject();
		UMovieSceneTrack* OuterTrack = Cast<UMovieSceneTrack>(Section->GetOuter());

		if (OuterTrack)
		{
			OuterTrack->Modify();
			OuterTrack->OnSectionMoved(*Section);
		}
	}

	EndTransaction();
}

void FMoveKeysAndSections::OnSequencerNodeTreeUpdated()
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

void FMoveKeysAndSections::ModifyNonSelectedSections()
{
	for (const FSequencerSelectedKey& Key : Keys)
	{
		UMovieSceneSection* OwningSection = Key.Section;
		const bool bHasBeenModified = ModifiedNonSelectedSections.Contains(OwningSection);
		const bool bIsAlreadySelected = Sections.ContainsByPredicate([OwningSection](FSectionHandle Handle) { return Handle.GetSectionObject() == OwningSection; });
		if (!bHasBeenModified && !bIsAlreadySelected)
		{
			OwningSection->SetFlags(RF_Transactional);
			if (OwningSection->TryModify())
			{
				ModifiedNonSelectedSections.Add(OwningSection);
			}
		}
	}
}

TOptional<FFrameNumber> FMoveKeysAndSections::GetMovementDeltaX(FFrameTime MouseTime)
{
	TOptional<FFrameNumber> DeltaX;

	// The delta of the mouse is the difference in the current mouse time vs when we started dragging
	const FFrameNumber MouseDeltaTime = (MouseTime - MouseTimePrev).FloorToFrame();

	// Disallow movement if any of the sections can't move
	for (int32 Index = 0; Index < Sections.Num(); ++Index)
	{
		const FSectionHandle& SectionHandle = Sections[Index];

		// If we're moving a section that is blending with something then it's OK if it overlaps stuff, the blend amount will get updated at the end.
		UMovieSceneSection* Section = SectionHandle.GetSectionObject();
		if (Section->GetBlendType().IsValid())
		{
			continue;
		}

		// We'll calculate this section's borders and clamp the possible delta time to be less than that
		TRange<FFrameNumber> SectionBoundaries = GetSectionBoundaries(Section, Sections, SectionHandle.TrackNode);

		FFrameNumber LeftMovementMaximum = MovieScene::DiscreteInclusiveLower(SectionBoundaries);
		FFrameNumber RightMovementMaximum = MovieScene::DiscreteExclusiveUpper(SectionBoundaries);
		
		if (Section->HasStartFrame())
		{
			FFrameNumber NewStartTime = Section->GetInclusiveStartFrame() + MouseDeltaTime;
			if (NewStartTime < LeftMovementMaximum)
			{
				FFrameNumber ClampedDeltaTime = LeftMovementMaximum - Section->GetInclusiveStartFrame();
				if (!DeltaX.IsSet() || DeltaX.GetValue() > ClampedDeltaTime)
				{
					DeltaX = ClampedDeltaTime;
				}
			}
		}
		
		if (Section->HasEndFrame())
		{
			FFrameNumber NewEndTime = Section->GetExclusiveEndFrame() + MouseDeltaTime;
			if (NewEndTime > RightMovementMaximum)
			{
				FFrameNumber ClampedDeltaTime = RightMovementMaximum - Section->GetExclusiveEndFrame();
				if (!DeltaX.IsSet() || DeltaX.GetValue() > ClampedDeltaTime)
				{
					DeltaX = ClampedDeltaTime;
				}
			}
		}
	}

	return DeltaX;
}

bool FMoveKeysAndSections::HandleSectionMovement(FFrameTime MouseTime, FVector2D VirtualMousePos, FVector2D LocalMousePos, TOptional<FFrameNumber> MaxDeltaX, FFrameNumber DesiredDeltaX)
{
	// Don't try to process moving sections if we don't have any sections.
	if (Sections.Num() == 0)
	{
		return false;
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

	bool bRowIndexChanged = false;
	for (int32 Index = 0; Index < Sections.Num(); ++Index)
	{
		auto& Handle = Sections[Index];
		UMovieSceneSection* Section = Handle.GetSectionObject();


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

		// Handle vertical dragging to re-arrange tracks. We don't support vertical rearranging if you're dragging via
		// a key, as the built in offset causes it to always jump down a row even without moving the mouse.
		if (Handle.TrackNode->GetTrack()->SupportsMultipleRows() && AllSections.Num() > 1 && bHotspotWasSection)
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
				Handle.TrackNode->TraverseVisible_ParentFirst([&](FSequencerDisplayNode& Node) { VirtualSectionBottom = Node.GetVirtualBottom(); return true; }, true);

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
				if (VirtualMousePos.Y <= VirtualSectionTop || LocalMousePos.Y <= 0)
				{
					TargetRowIndex = -1;
				}
			}
			else if (Handle.TrackNode->GetSubTrackMode() == FSequencerTrackNode::ESubTrackMode::SubTrack)
			{
				TSharedPtr<FSequencerTrackNode> ParentTrack = StaticCastSharedPtr<FSequencerTrackNode>(Handle.TrackNode->GetParent());
				if (ensure(ParentTrack.IsValid()))
				{
					for (int32 ChildIndex = 0; ChildIndex < ParentTrack->GetChildNodes().Num(); ++ChildIndex)
					{
						TSharedRef<FSequencerDisplayNode> ChildNode = ParentTrack->GetChildNodes()[ChildIndex];
						float VirtualSectionTop = ChildNode->GetVirtualTop();
						float VirtualSectionBottom = 0.f;
						ChildNode->TraverseVisible_ParentFirst([&](FSequencerDisplayNode& Node) { VirtualSectionBottom = Node.GetVirtualBottom(); return true; }, true);

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

		bool bDeltaX = DesiredDeltaX != 0;
		bool bDeltaY = TargetRowIndex != Section->GetRowIndex();

		// Horizontal movement
		if (bDeltaX)
		{
			Section->MoveSection(MaxDeltaX.Get(DesiredDeltaX));
		}

		// Vertical movement
		if (bDeltaY && !bSectionsAreOnDifferentRows &&
			(
				Section->GetBlendType().IsValid() ||
				!Section->OverlapsWithSections(NonDraggedSections, TargetRowIndex - Section->GetRowIndex(), DesiredDeltaX.Value)
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
					for (auto InitialRowIndex : InitialSectionRowIndicies)
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
						for (auto InitialRowIndex : InitialSectionRowIndicies)
						{
							if (!SectionsBeingMoved.Contains(InitialRowIndex.Section))
							{
								InitialRowIndex.Section->Modify();
								InitialRowIndex.Section->SetRowIndex(InitialRowIndex.RowIndex + 1);
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

	return bRowIndexChanged;
}

void FMoveKeysAndSections::HandleKeyMovement(TOptional<FFrameNumber> MaxDeltaX, FFrameNumber DesiredDeltaX)
{
	if (KeysAsArray.Num() == 0)
	{
		return;
	}

	// Apply the delta to our key times. We need to get our key time so that we can add the delta
	// to each one so that we come up with a new absolute time for it.
	TArray<FFrameNumber> CurrentKeyTimes;
	CurrentKeyTimes.SetNum(KeysAsArray.Num());
	GetKeyTimes(KeysAsArray, CurrentKeyTimes);

	for (int32 Index = 0; Index < CurrentKeyTimes.Num(); ++Index)
	{
		FSequencerSelectedKey& SelectedKey = KeysAsArray[Index];
		const bool bOwningSectionIsSelected = Sections.ContainsByPredicate([SelectedKey](FSectionHandle Handle) { return Handle.GetSectionObject() == SelectedKey.Section; });

		// We don't want to apply delta if we have the key's section selected as well, otherwise they get double
		// transformed (moving the section moves the keys + we add the delta to the key positions).
		if (!bOwningSectionIsSelected)
		{
			CurrentKeyTimes[Index] += MaxDeltaX.Get(DesiredDeltaX);
		}
	}

	// Now set the times back to the keys.
	SetKeyTimes(KeysAsArray, CurrentKeyTimes);

	// Expand any sections containing those keys to encompass their new location
	for (int32 Index = 0; Index < CurrentKeyTimes.Num(); ++Index)
	{
		FSequencerSelectedKey SelectedKey = KeysAsArray[Index];

		UMovieSceneSection* Section = SelectedKey.Section;
		if (ModifiedNonSelectedSections.Contains(Section))
		{
			// If the key moves outside of the section resize the section to fit the key
			// @todo Sequencer - Doesn't account for hitting other sections 
			const FFrameNumber   NewKeyTime = CurrentKeyTimes[Index];
			TRange<FFrameNumber> SectionRange = Section->GetRange();

			if (!SectionRange.Contains(NewKeyTime))
			{
				TRange<FFrameNumber> NewRange = TRange<FFrameNumber>::Hull(SectionRange, TRange<FFrameNumber>(NewKeyTime));
				Section->SetRange(NewRange);
			}
		}
	}


	// Snap the play time to the new dragged key time if all the keyframes were dragged to the same time
	if (Settings->GetSnapPlayTimeToDraggedKey() && CurrentKeyTimes.Num())
	{
		FFrameNumber FirstFrame = CurrentKeyTimes[0];
		auto         EqualsFirstFrame = [=](FFrameNumber In)
		{
			return In == FirstFrame;
		};

		if (Algo::AllOf(CurrentKeyTimes, EqualsFirstFrame))
		{
			Sequencer.SetLocalTime(FirstFrame);
		}
	}

	for (UMovieSceneSection* Section : ModifiedNonSelectedSections)
	{
		Section->MarkAsChanged();
	}
}
