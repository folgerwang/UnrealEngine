// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Tools/SequencerSnapField.h"
#include "MovieScene.h"
#include "SSequencer.h"
#include "SSequencerTreeView.h"
#include "MovieSceneTimeHelpers.h"
#include "MovieSceneSequence.h"
#include "ISequencerSection.h"

struct FSnapGridVisitor : ISequencerEntityVisitor
{
	FSnapGridVisitor(ISequencerSnapCandidate& InCandidate, uint32 EntityMask)
		: ISequencerEntityVisitor(EntityMask)
		, Candidate(InCandidate)
	{}

	virtual void VisitKey(FKeyHandle KeyHandle, FFrameNumber KeyTime, const TSharedPtr<IKeyArea>& KeyArea, UMovieSceneSection* Section, TSharedRef<FSequencerDisplayNode>) const
	{
		if (Candidate.IsKeyApplicable(KeyHandle, KeyArea, Section))
		{
			Snaps.Add(FSequencerSnapPoint{ FSequencerSnapPoint::Key, KeyTime });
		}
	}
	virtual void VisitSection(UMovieSceneSection* Section, TSharedRef<FSequencerDisplayNode> Node) const
	{
		if (Candidate.AreSectionBoundsApplicable(Section))
		{
			if (Section->HasStartFrame())
			{
				Snaps.Add(FSequencerSnapPoint{ FSequencerSnapPoint::SectionBounds, Section->GetInclusiveStartFrame() });
			}

			if (Section->HasEndFrame())
			{
				Snaps.Add(FSequencerSnapPoint{ FSequencerSnapPoint::SectionBounds, Section->GetExclusiveEndFrame() });
			}
		}

		if (Candidate.AreSectionCustomSnapsApplicable(Section))
		{
			TArray<FFrameNumber> CustomSnaps;
			Section->GetSnapTimes(CustomSnaps, false);
			for (FFrameNumber Time : CustomSnaps)
			{
				Snaps.Add(FSequencerSnapPoint{ FSequencerSnapPoint::CustomSection, Time });
			}
		}
	}

	ISequencerSnapCandidate& Candidate;
	mutable TArray<FSequencerSnapPoint> Snaps;
};

FSequencerSnapField::FSequencerSnapField(const ISequencer& InSequencer, ISequencerSnapCandidate& Candidate, uint32 EntityMask)
{
	TSharedPtr<SSequencerTreeView> TreeView = StaticCastSharedRef<SSequencer>(InSequencer.GetSequencerWidget())->GetTreeView();

	TArray<TSharedRef<FSequencerDisplayNode>> VisibleNodes;
	for (const SSequencerTreeView::FCachedGeometry& Geometry : TreeView->GetAllVisibleNodes())
	{
		VisibleNodes.Add(Geometry.Node);
	}

	TRange<double> ViewRange = InSequencer.GetViewRange();
	FSequencerEntityWalker Walker(
		FSequencerEntityRange(ViewRange, InSequencer.GetFocusedTickResolution()),
		FVector2D(SequencerSectionConstants::KeySize));

	// Traverse the visible space, collecting snapping times as we go
	FSnapGridVisitor Visitor(Candidate, EntityMask);
	Walker.Traverse(Visitor, VisibleNodes);

	// Add the playback range start/end bounds as potential snap candidates
	TRange<FFrameNumber> PlaybackRange = InSequencer.GetFocusedMovieSceneSequence()->GetMovieScene()->GetPlaybackRange();
	if(MovieScene::DiscreteSize(PlaybackRange) > 0)
	{
		Visitor.Snaps.Add(FSequencerSnapPoint{ FSequencerSnapPoint::PlaybackRange, MovieScene::DiscreteInclusiveLower(PlaybackRange)});
		Visitor.Snaps.Add(FSequencerSnapPoint{ FSequencerSnapPoint::PlaybackRange, MovieScene::DiscreteExclusiveUpper(PlaybackRange) - 1});
	}

	// Add the current time as a potential snap candidate
	Visitor.Snaps.Add(FSequencerSnapPoint{ FSequencerSnapPoint::CurrentTime, InSequencer.GetLocalTime().Time.FrameNumber });

	// Add the selection range bounds as a potential snap candidate
	TRange<FFrameNumber> SelectionRange = InSequencer.GetFocusedMovieSceneSequence()->GetMovieScene()->GetSelectionRange();
	if (MovieScene::DiscreteSize(SelectionRange) > 0)
	{
		Visitor.Snaps.Add(FSequencerSnapPoint{ FSequencerSnapPoint::InOutRange, MovieScene::DiscreteInclusiveLower(SelectionRange)});
		Visitor.Snaps.Add(FSequencerSnapPoint{ FSequencerSnapPoint::InOutRange, MovieScene::DiscreteExclusiveUpper(SelectionRange) - 1});
	}

	// Add in the marked frames
	for (const FMovieSceneMarkedFrame& MarkedFrame : InSequencer.GetFocusedMovieSceneSequence()->GetMovieScene()->GetMarkedFrames())
	{
		Visitor.Snaps.Add( FSequencerSnapPoint{ FSequencerSnapPoint::Mark, MarkedFrame.FrameNumber } );
	}

	// Sort
	Visitor.Snaps.Sort([](const FSequencerSnapPoint& A, const FSequencerSnapPoint& B){
		return A.Time < B.Time;
	});

	// Remove duplicates
	for (int32 Index = 0; Index < Visitor.Snaps.Num(); ++Index)
	{
		const FFrameNumber CurrentTime = Visitor.Snaps[Index].Time;

		int32 NumToMerge = 0;
		for (int32 DuplIndex = Index + 1; DuplIndex < Visitor.Snaps.Num(); ++DuplIndex)
		{
			if (CurrentTime != Visitor.Snaps[DuplIndex].Time)
			{
				break;
			}
			++NumToMerge;
		}

		if (NumToMerge)
		{
			Visitor.Snaps.RemoveAt(Index + 1, NumToMerge, false);
		}
	}

	SortedSnaps = MoveTemp(Visitor.Snaps);
}

TOptional<FFrameNumber> FSequencerSnapField::Snap(FFrameNumber InTime, int32 Threshold) const
{
	int32 Min = 0;
	int32 Max = SortedSnaps.Num();

	// Binary search, then linearly search a range
	for ( ; Min != Max ; )
	{
		int32 SearchIndex = Min + (Max - Min) / 2;

		FFrameNumber ProspectiveSnapPos = SortedSnaps[SearchIndex].Time;
		if (ProspectiveSnapPos > InTime + Threshold)
		{
			Max = SearchIndex;
		}
		else if (ProspectiveSnapPos < InTime - Threshold)
		{
			Min = SearchIndex + 1;
		}
		else
		{
			// Linearly search forwards and backwards to find the closest snap

			FFrameNumber SnapDelta = ProspectiveSnapPos - InTime;

			// Search forwards while we're in the threshold
			for (int32 FwdIndex = SearchIndex+1; FwdIndex < Max-1 && SortedSnaps[FwdIndex].Time < InTime + Threshold; ++FwdIndex)
			{
				FFrameNumber ThisSnapDelta = InTime - SortedSnaps[FwdIndex].Time;
				if (FMath::Abs(ThisSnapDelta) < FMath::Abs(SnapDelta))
				{
					SnapDelta = ThisSnapDelta;
					ProspectiveSnapPos = SortedSnaps[FwdIndex].Time;
				}
			}

			// Search backwards while we're in the threshold
			for (int32 BckIndex = SearchIndex-1; BckIndex >= Min && SortedSnaps[BckIndex].Time > InTime + Threshold; --BckIndex)
			{
				FFrameNumber ThisSnapDelta = InTime - SortedSnaps[BckIndex].Time;
				if (FMath::Abs(ThisSnapDelta) < FMath::Abs(SnapDelta))
				{
					SnapDelta = ThisSnapDelta;
					ProspectiveSnapPos = SortedSnaps[BckIndex].Time;
				}
			}

			return ProspectiveSnapPos;
		}
	}

	return TOptional<FFrameNumber>();
}

TOptional<FSequencerSnapField::FSnapResult> FSequencerSnapField::Snap(const TArray<FFrameNumber>& InTimes, int32 Threshold) const
{
	TOptional<FSnapResult> ProspectiveSnap;
	FFrameNumber SnapDelta(0);

	for (FFrameNumber Time : InTimes)
	{
		TOptional<FFrameNumber> ThisSnap = Snap(Time, Threshold);
		if (!ThisSnap.IsSet())
		{
			continue;
		}

		FFrameNumber ThisSnapDelta = ThisSnap.GetValue() - Time;
		if (!ProspectiveSnap.IsSet() || FMath::Abs(ThisSnapDelta) < FMath::Abs(SnapDelta))
		{
			ProspectiveSnap = FSnapResult{ Time, ThisSnap.GetValue() };
			SnapDelta = ThisSnapDelta;
		}
	}

	return ProspectiveSnap;
}
