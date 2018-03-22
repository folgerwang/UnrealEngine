// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SequencerKeyCollection.h"
#include "IKeyArea.h"
#include "DisplayNodes/SequencerDisplayNode.h"
#include "DisplayNodes/SequencerTrackNode.h"
#include "DisplayNodes/SequencerSectionKeyAreaNode.h"

FSequencerKeyCollectionSignature FSequencerKeyCollectionSignature::FromNodes(const TArray<FSequencerDisplayNode*>& InNodes, FFrameNumber InDuplicateThresholdTime)
{
	FSequencerKeyCollectionSignature Result;
	Result.DuplicateThresholdTime = InDuplicateThresholdTime;

	for (const FSequencerDisplayNode* Node : InNodes)
	{
		const FSequencerSectionKeyAreaNode* KeyAreaNode = nullptr;

		check(Node);
		if (Node->GetType() == ESequencerNode::KeyArea)
		{
			KeyAreaNode = static_cast<const FSequencerSectionKeyAreaNode*>(Node);
		}
		else if (Node->GetType() == ESequencerNode::Track)
		{
			KeyAreaNode = static_cast<const FSequencerTrackNode*>(Node)->GetTopLevelKeyNode().Get();
		}

		if (KeyAreaNode)
		{
			for (const TSharedRef<IKeyArea>& KeyArea : KeyAreaNode->GetAllKeyAreas())
			{
				const UMovieSceneSection* Section = KeyArea->GetOwningSection();
				Result.KeyAreaToSignature.Add(KeyArea, Section ? Section->GetSignature() : FGuid());
			}
		}
	}

	return Result;
}

FSequencerKeyCollectionSignature FSequencerKeyCollectionSignature::FromNodesRecursive(const TArray<FSequencerDisplayNode*>& InNodes, FFrameNumber InDuplicateThresholdTime)
{
	FSequencerKeyCollectionSignature Result;
	Result.DuplicateThresholdTime = InDuplicateThresholdTime;

	TArray<TSharedRef<FSequencerSectionKeyAreaNode>> AllKeyAreaNodes;
	AllKeyAreaNodes.Reserve(36);
	for (FSequencerDisplayNode* Node : InNodes)
	{
		if (Node->GetType() == ESequencerNode::KeyArea)
		{
			AllKeyAreaNodes.Add(StaticCastSharedRef<FSequencerSectionKeyAreaNode>(Node->AsShared()));
		}

		Node->GetChildKeyAreaNodesRecursively(AllKeyAreaNodes);
	}

	for (const TSharedRef<FSequencerSectionKeyAreaNode>& Node : AllKeyAreaNodes)
	{
		for (const TSharedRef<IKeyArea>& KeyArea : Node->GetAllKeyAreas())
		{
			const UMovieSceneSection* Section = KeyArea->GetOwningSection();
			Result.KeyAreaToSignature.Add(KeyArea, Section ? Section->GetSignature() : FGuid());
		}
	}

	return Result;
}

FSequencerKeyCollectionSignature FSequencerKeyCollectionSignature::FromNodeRecursive(FSequencerDisplayNode& InNode, UMovieSceneSection* InSection, FFrameNumber InDuplicateThresholdTime)
{
	FSequencerKeyCollectionSignature Result;
	Result.DuplicateThresholdTime = InDuplicateThresholdTime;

	TArray<TSharedRef<FSequencerSectionKeyAreaNode>> AllKeyAreaNodes;
	AllKeyAreaNodes.Reserve(36);
	InNode.GetChildKeyAreaNodesRecursively(AllKeyAreaNodes);

	for (const auto& Node : AllKeyAreaNodes)
	{
		TSharedPtr<IKeyArea> KeyArea = Node->GetKeyArea(InSection);
		if (KeyArea.IsValid())
		{
			Result.KeyAreaToSignature.Add(KeyArea.ToSharedRef(), InSection ? InSection->GetSignature() : FGuid());
		}
	}

	return Result;
}

bool FSequencerKeyCollectionSignature::HasUncachableContent() const
{
	for (auto& Pair : KeyAreaToSignature)
	{
		if (!Pair.Value.IsValid())
		{
			return true;
		}
	}
	return false;
}

bool operator!=(const FSequencerKeyCollectionSignature& A, const FSequencerKeyCollectionSignature& B)
{
	if (A.HasUncachableContent() || B.HasUncachableContent())
	{
		return true;
	}

	if (A.KeyAreaToSignature.Num() != B.KeyAreaToSignature.Num() || A.DuplicateThresholdTime != B.DuplicateThresholdTime)
	{
		return true;
	}

	for (auto& Pair : A.KeyAreaToSignature)
	{
		const FGuid* BSig = B.KeyAreaToSignature.Find(Pair.Key);
		if (!BSig || *BSig != Pair.Value)
		{
			return true;
		}
	}

	return false;
}

bool operator==(const FSequencerKeyCollectionSignature& A, const FSequencerKeyCollectionSignature& B)
{
	if (A.HasUncachableContent() || B.HasUncachableContent())
	{
		return false;
	}

	if (A.KeyAreaToSignature.Num() != B.KeyAreaToSignature.Num() || A.DuplicateThresholdTime != B.DuplicateThresholdTime)
	{
		return false;
	}

	for (auto& Pair : A.KeyAreaToSignature)
	{
		const FGuid* BSig = B.KeyAreaToSignature.Find(Pair.Key);
		if (!BSig || *BSig != Pair.Value)
		{
			return false;
		}
	}

	return true;
}

bool FSequencerKeyCollection::Update(const FSequencerKeyCollectionSignature& InSignature)
{
	if (InSignature == Signature)
	{
		return false;
	}

	TArray<FFrameNumber> AllTimes;

	// Get all the key times for the key areas
	for (auto& Pair : InSignature.GetKeyAreas())
	{
		Pair.Key->GetKeyTimes(AllTimes);
	}

	AllTimes.Sort();

	GroupedTimes.Reset(AllTimes.Num());
	int32 Index = 0;
	while ( Index < AllTimes.Num() )
	{
		FFrameNumber PredicateTime = AllTimes[Index];
		GroupedTimes.Add(PredicateTime);
		while (Index < AllTimes.Num() && FMath::Abs(AllTimes[Index] - PredicateTime) <= InSignature.GetDuplicateThreshold())
		{
			++Index;
		}
	}
	GroupedTimes.Shrink();

	Signature = InSignature;

	return true;
}

TOptional<FFrameNumber> FSequencerKeyCollection::FindFirstKeyInRange(const TRange<FFrameNumber>& Range, EFindKeyDirection Direction) const
{
	TArrayView<const FFrameNumber> KeysInRange = GetKeysInRange(Range);
	if (KeysInRange.Num())
	{
		return Direction == EFindKeyDirection::Forwards ? KeysInRange[0] : KeysInRange[KeysInRange.Num()-1];
	}
	return TOptional<FFrameNumber>();
}

TArrayView<const FFrameNumber> FSequencerKeyCollection::GetKeysInRange(const TRange<FFrameNumber>& Range) const
{
	// Binary search the first time that's >= the lower bound
	int32 FirstVisibleIndex = Range.GetLowerBound().IsClosed() ? Algo::LowerBound(GroupedTimes, Range.GetLowerBoundValue()) : 0;
	// Binary search the last time that's > the upper bound
	int32 LastVisibleIndex  = Range.GetUpperBound().IsClosed() ? Algo::UpperBound(GroupedTimes, Range.GetUpperBoundValue()) : GroupedTimes.Num();

	int32 Num = LastVisibleIndex - FirstVisibleIndex;
	if (GroupedTimes.IsValidIndex(FirstVisibleIndex) && LastVisibleIndex <= GroupedTimes.Num())
	{
		return MakeArrayView(&GroupedTimes[FirstVisibleIndex], Num);
	}

	return TArrayView<const FFrameNumber>();
}
/*
FFrameNumber FSequencerKeyCollection::GetNextKey(FFrameNumber FrameNumber, EFindKeyDirection Direction) const
{
	FFrameNumber NextFrame = FrameNumber;
	int32 Num = GroupedTimes.Num();
	if (Num > 0)
	{
		if (FrameNumber < GroupedTimes[0])
		{
			NextFrame = GroupedTimes[0];
		}
		else if (FrameNumber > GroupedTimes[Num - 1])
		{
			NextFrame = GroupedTimes[Num - 1];
		}
		else
		{
			int32 Index = Algo::LowerBound(GroupedTimes, FrameNumber);
			if (GroupedTimes[Index] != FrameNumber)
			{
				if (Direction == EFindKeyDirection::Forwards)
				{
					NextFrame = GroupedTimes[Index];
				}
				else
				{
					NextFrame = GroupedTimes[Index - 1];
				}
			}
			else {
				if (Direction == EFindKeyDirection::Forwards)
				{
					if (++Index >= Num)
					{
						Index = Num - 1;
					}
					NextFrame = GroupedTimes[Index];
				}
				else
				{
					if (--Index < 0)
					{
						Index = 0;
					}
					NextFrame = GroupedTimes[Index];
				}
			}
		}
	}
	return NextFrame;

}

*/

TOptional<FFrameNumber> FSequencerKeyCollection::GetNextKey(FFrameNumber FrameNumber, EFindKeyDirection Direction) const
{
	int32 Index = INDEX_NONE;
	if (Direction == EFindKeyDirection::Forwards)
	{
		Index = Algo::UpperBound(GroupedTimes, FrameNumber);
	}
	else
	{
		Index = Algo::LowerBound(GroupedTimes, FrameNumber) - 1;
	}

	if (GroupedTimes.IsValidIndex(Index))
	{
		return GroupedTimes[Index];
	}
	else
	{
		return TOptional<FFrameNumber>();
	}
}
