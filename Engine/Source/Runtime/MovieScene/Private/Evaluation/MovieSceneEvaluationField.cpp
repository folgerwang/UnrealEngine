// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneEvaluationField.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Evaluation/MovieSceneSequenceTemplateStore.h"
#include "MovieSceneCommonHelpers.h"
#include "Algo/Sort.h"

#include "MovieSceneSequence.h"

int32 FMovieSceneEvaluationField::GetSegmentFromTime(FFrameNumber Time) const
{
	// Linear search
	// @todo: accelerated search based on the last evaluated index?
	for (int32 Index = 0; Index < Ranges.Num(); ++Index)
	{
		if (Ranges[Index].Value.Contains(Time))
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

TRange<int32> FMovieSceneEvaluationField::OverlapRange(TRange<FFrameNumber> Range) const
{
	int32 StartIndex = 0, Num = 0;
	for (int32 Index = 0; Index < Ranges.Num(); ++Index)
	{
		if (Ranges[Index].Value.Overlaps(Range))
		{
			if (Num == 0)
			{
				StartIndex = Index;
			}
			++Num;
		}
		else if (Num != 0)
		{
			break;
		}
	}

	return Num != 0 ? TRange<int32>(StartIndex, StartIndex + Num) : TRange<int32>::Empty();
}

void FMovieSceneEvaluationField::Invalidate(TRange<FFrameNumber> Range)
{
	TRange<int32> OverlappingRange = OverlapRange(Range);
	if (!OverlappingRange.IsEmpty())
	{
		Ranges.RemoveAt(OverlappingRange.GetLowerBoundValue(), OverlappingRange.Size<int32>(), false);
		Groups.RemoveAt(OverlappingRange.GetLowerBoundValue(), OverlappingRange.Size<int32>(), false);
		MetaData.RemoveAt(OverlappingRange.GetLowerBoundValue(), OverlappingRange.Size<int32>(), false);

#if WITH_EDITORONLY_DATA
		Signature = FGuid::NewGuid();
#endif
	}
}

int32 FMovieSceneEvaluationField::Insert(FFrameNumber InsertTime, TRange<FFrameNumber> InRange, FMovieSceneEvaluationGroup&& InGroup, FMovieSceneEvaluationMetaData&& InMetaData)
{
	const int32 InsertIndex = Algo::UpperBoundBy(Ranges, TRangeBound<FFrameNumber>(InsertTime), &FMovieSceneFrameRange::GetLowerBound, MovieSceneHelpers::SortLowerBounds);

	// Intersect the supplied range with the allowable space between adjacent existing ranges
	TRange<FFrameNumber> InsertSpace(
		Ranges.IsValidIndex(InsertIndex-1) ? TRangeBound<FFrameNumber>::FlipInclusion(Ranges[InsertIndex-1].GetUpperBound()) : TRangeBound<FFrameNumber>::Open(),
		Ranges.IsValidIndex(InsertIndex  ) ? TRangeBound<FFrameNumber>::FlipInclusion(Ranges[InsertIndex  ].GetLowerBound()) : TRangeBound<FFrameNumber>::Open()
		);

	InRange = TRange<FFrameNumber>::Intersection(InRange, InsertSpace);

	// @todo: Remove this code and enforce the check below outright when we have proper time representation
	if (Ranges.IsValidIndex(InsertIndex  ) && Ranges[InsertIndex  ].Value.Overlaps(InRange))
	{
		InRange = TRange<FFrameNumber>(InRange.GetLowerBound(), TRangeBound<FFrameNumber>::FlipInclusion(Ranges[InsertIndex].Value.GetLowerBound()));
	}
	if (Ranges.IsValidIndex(InsertIndex-1) && Ranges[InsertIndex-1].Value.Overlaps(InRange))
	{
		InRange = TRange<FFrameNumber>(TRangeBound<FFrameNumber>::FlipInclusion(Ranges[InsertIndex-1].Value.GetUpperBound()), InRange.GetUpperBound());
	}

	if (!ensure(!InRange.IsEmpty()))
	{
		return INDEX_NONE;
	}

	const bool bOverlapping = 
		(Ranges.IsValidIndex(InsertIndex  ) && Ranges[InsertIndex  ].Value.Overlaps(InRange)) ||
		(Ranges.IsValidIndex(InsertIndex-1) && Ranges[InsertIndex-1].Value.Overlaps(InRange));

	if (!ensureAlwaysMsgf(!bOverlapping, TEXT("Attempting to insert an overlapping range into the evaluation field.")))
	{
		return INDEX_NONE;
	}

	Ranges.Insert(InRange, InsertIndex);
	MetaData.Insert(MoveTemp(InMetaData), InsertIndex);
	Groups.Insert(MoveTemp(InGroup), InsertIndex);

#if WITH_EDITORONLY_DATA
	Signature = FGuid::NewGuid();
#endif

	return InsertIndex;
}

void FMovieSceneEvaluationField::Add(TRange<FFrameNumber> InRange, FMovieSceneEvaluationGroup&& InGroup, FMovieSceneEvaluationMetaData&& InMetaData)
{
	if (ensureAlwaysMsgf(!Ranges.Num() || !Ranges.Last().Value.Overlaps(InRange), TEXT("Attempting to add overlapping ranges to sequence evaluation field.")))
	{
		Ranges.Add(InRange);
		MetaData.Add(MoveTemp(InMetaData));
		Groups.Add(MoveTemp(InGroup));

#if WITH_EDITORONLY_DATA
		Signature = FGuid::NewGuid();
#endif
	}
}

void FMovieSceneEvaluationMetaData::DiffSequences(const FMovieSceneEvaluationMetaData& LastFrame, TArray<FMovieSceneSequenceID>* NewSequences, TArray<FMovieSceneSequenceID>* ExpiredSequences) const
{
	// This algorithm works on the premise that each array is sorted, and each ID can only appear once
	auto ThisFrameIDs = ActiveSequences.CreateConstIterator();
	auto LastFrameIDs = LastFrame.ActiveSequences.CreateConstIterator();

	// Iterate both arrays together
	while (ThisFrameIDs && LastFrameIDs)
	{
		FMovieSceneSequenceID ThisID = *ThisFrameIDs;
		FMovieSceneSequenceID LastID = *LastFrameIDs;

		// If they're the same, skip
		if (ThisID == LastID)
		{
			++ThisFrameIDs;
			++LastFrameIDs;
			continue;
		}

		if (LastID < ThisID)
		{
			// Last frame iterator is less than this frame's, which means the last ID is no longer evaluated
			if (ExpiredSequences)
			{
				ExpiredSequences->Add(LastID);
			}
			++LastFrameIDs;
		}
		else
		{
			// Last frame iterator is greater than this frame's, which means this ID is new
			if (NewSequences)
			{
				NewSequences->Add(ThisID);
			}

			++ThisFrameIDs;
		}
	}

	// Add any remaining expired sequences
	if (ExpiredSequences)
	{
		while (LastFrameIDs)
		{
			ExpiredSequences->Add(*LastFrameIDs);
			++LastFrameIDs;
		}
	}

	// Add any remaining new sequences
	if (NewSequences)
	{
		while (ThisFrameIDs)
		{
			NewSequences->Add(*ThisFrameIDs);
			++ThisFrameIDs;
		}
	}
}

void FMovieSceneEvaluationMetaData::DiffEntities(const FMovieSceneEvaluationMetaData& LastFrame, TArray<FMovieSceneOrderedEvaluationKey>* NewKeys, TArray<FMovieSceneOrderedEvaluationKey>* ExpiredKeys) const
{
	// This algorithm works on the premise that each array is sorted, and each ID can only appear once
	auto ThisFrameKeys = ActiveEntities.CreateConstIterator();
	auto LastFrameKeys = LastFrame.ActiveEntities.CreateConstIterator();

	// Iterate both arrays together
	while (ThisFrameKeys && LastFrameKeys)
	{
		FMovieSceneOrderedEvaluationKey ThisKey = *ThisFrameKeys;
		FMovieSceneOrderedEvaluationKey LastKey = *LastFrameKeys;

		// If they're the same, skip
		if (ThisKey.Key == LastKey.Key)
		{
			++ThisFrameKeys;
			++LastFrameKeys;
			continue;
		}

		if (LastKey.Key < ThisKey.Key)
		{
			// Last frame iterator is less than this frame's, which means the last entity is no longer evaluated
			if (ExpiredKeys)
			{
				ExpiredKeys->Add(LastKey);
			}
			++LastFrameKeys;
		}
		else
		{
			// Last frame iterator is greater than this frame's, which means this entity is new
			if (NewKeys)
			{
				NewKeys->Add(ThisKey);
			}

			++ThisFrameKeys;
		}
	}

	// Add any remaining expired entities
	if (ExpiredKeys)
	{
		while (LastFrameKeys)
		{
			ExpiredKeys->Add(*LastFrameKeys);
			++LastFrameKeys;
		}

		// Expired keys are torn down in reverse
		Algo::SortBy(*ExpiredKeys, [](const FMovieSceneOrderedEvaluationKey& In){ return uint32(-1) - In.EvaluationIndex; });
	}

	// Add any remaining new entities
	if (NewKeys)
	{
		while (ThisFrameKeys)
		{
			NewKeys->Add(*ThisFrameKeys);
			++ThisFrameKeys;
		}

		Algo::SortBy(*NewKeys, &FMovieSceneOrderedEvaluationKey::EvaluationIndex);
	}
}

bool FMovieSceneEvaluationMetaData::IsDirty(const FMovieSceneSequenceHierarchy& RootHierarchy, IMovieSceneSequenceTemplateStore& TemplateStore, TRange<FFrameNumber>* OutSubRangeToInvalidate, TSet<UMovieSceneSequence*>* OutDirtySequences) const
{
	bool bDirty = false;

	for (const TTuple<FMovieSceneSequenceID, uint32>& Pair : SubTemplateSerialNumbers)
	{
		// Sequence IDs at this point are relative to the root override template
		const FMovieSceneSubSequenceData* SubData = RootHierarchy.FindSubData(Pair.Key);
		UMovieSceneSequence* SubSequence = SubData ? SubData->GetSequence() : nullptr;

		bool bThisSequenceIsDirty = true;
		if (SubSequence)
		{
			FMovieSceneEvaluationTemplate& Template = TemplateStore.AccessTemplate(*SubSequence);

			bThisSequenceIsDirty = Template.TemplateSerialNumber.GetValue() != Pair.Value || Template.SequenceSignature != SubSequence->GetSignature();

			if (bThisSequenceIsDirty && OutDirtySequences)
			{
				OutDirtySequences->Add(SubSequence);
			}
		}

		if (bThisSequenceIsDirty)
		{
			bDirty = true;

			if (OutSubRangeToInvalidate)
			{
				TRange<FFrameNumber> DirtyRange = SubData ? TRange<FFrameNumber>::Hull(TRange<FFrameNumber>::Hull(SubData->PreRollRange.Value, SubData->PlayRange.Value), SubData->PostRollRange.Value) * SubData->RootToSequenceTransform.Inverse() : TRange<FFrameNumber>::All();
				*OutSubRangeToInvalidate = TRange<FFrameNumber>::Hull(*OutSubRangeToInvalidate, DirtyRange);
			}
		}
	}

	return bDirty;
}
