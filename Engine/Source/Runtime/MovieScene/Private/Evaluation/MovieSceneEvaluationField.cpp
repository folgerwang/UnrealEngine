// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneEvaluationField.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Evaluation/MovieSceneSequenceTemplateStore.h"
#include "Evaluation/MovieSceneEvaluationTree.h"
#include "Compilation/MovieSceneCompiler.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieSceneTimeHelpers.h"
#include "Algo/Sort.h"

#include "MovieSceneSequence.h"

TRange<int32> FMovieSceneEvaluationField::ConditionallyCompileRange(const TRange<FFrameNumber>& InRange, UMovieSceneSequence* InSequence, IMovieSceneSequenceTemplateStore& TemplateStore)
{
	check(InSequence);

	// First off, attempt to find the evaluation group in the existing evaluation field data from the template
	TRange<int32> OverlappingFieldEntries = OverlapRange(InRange);
	int32 EvalFieldStartIndex = OverlappingFieldEntries.GetLowerBoundValue();
	int32 EvalFieldEndIndex   = OverlappingFieldEntries.GetUpperBoundValue();

	bool bIsDirty = OverlappingFieldEntries.IsEmpty();

	const FMovieSceneSequenceHierarchy& RootHierarchy = TemplateStore.AccessTemplate(*InSequence).Hierarchy;

	TArray<TRange<FFrameNumber>, TInlineAllocator<8>> RangesToInvalidate;
	for (int32 Index = EvalFieldStartIndex; Index < EvalFieldEndIndex; ++Index)
	{
		const TRange<FFrameNumber>& ThisRange = Ranges[Index].Value;

		// Check for gaps between the entries.
		if (Index == EvalFieldStartIndex)
		{
			// If the first overlapping range starts after InRange's lower bound, there must be a gap before it
			if (TRangeBound<FFrameNumber>::MinLower(ThisRange.GetLowerBound(), InRange.GetLowerBound()) != ThisRange.GetLowerBound())
			{
				bIsDirty = true;
			}
		}
		if (Index == EvalFieldEndIndex - 1)
		{
			// If the last overlapping range ends before InRange's upper bound, there must be a gap after it
			if (TRangeBound<FFrameNumber>::MaxUpper(ThisRange.GetUpperBound(), InRange.GetUpperBound()) != ThisRange.GetUpperBound())
			{
				bIsDirty = true;
			}
		}

		// If adjacent ranges are not contiguous, we have a gap
		if (Index > EvalFieldStartIndex && Ranges.IsValidIndex(Index-1) && !Ranges[Index-1].Value.Adjoins(ThisRange))
		{
			bIsDirty = true;
		}

		// Verify that this field entry is still valid (all its cached signatures are still the same)
		TRange<FFrameNumber> InvalidatedSubSequenceRange = TRange<FFrameNumber>::Empty();
		if (MetaData[Index].IsDirty(RootHierarchy, TemplateStore, &InvalidatedSubSequenceRange))
		{
			bIsDirty = true;

			if (!InvalidatedSubSequenceRange.IsEmpty())
			{
				// Invalidate this evaluation field
				RangesToInvalidate.Add(InvalidatedSubSequenceRange);
			}
		}
	}

	// Invalidate any areas in the evaluation field that are now out of date
	for (const TRange<FFrameNumber>& Range : RangesToInvalidate)
	{
		Invalidate(Range);
	}

	if (bIsDirty)
	{
		// We need to compile an entry in the evaluation field
		static bool bFullCompile = false;
 		if (bFullCompile)
		{
			FMovieSceneCompiler::Compile(*InSequence, TemplateStore);
		}
		else
		{
			FMovieSceneCompiler::CompileRange(InRange, *InSequence, TemplateStore);
		}

		return OverlapRange(InRange);
	}

	return OverlappingFieldEntries;
}

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

TRange<int32> FMovieSceneEvaluationField::OverlapRange(const TRange<FFrameNumber>& Range) const
{
	if (Ranges.Num() == 0)
	{
		return TRange<int32>::Empty();
	}

	TArrayView<const FMovieSceneFrameRange> RangesToSearch(Ranges);

	// Binary search the first lower bound that's greater than the input range's lower bound
	int32 StartIndex = Algo::UpperBoundBy(RangesToSearch, Range.GetLowerBound(), &FMovieSceneFrameRange::GetLowerBound, MovieSceneHelpers::SortLowerBounds);

	// StartIndex is always <= RangesToSearch.Num(). If the previous range overlaps the input range, include that
	if (StartIndex > 0 && RangesToSearch[StartIndex-1].Value.Overlaps(Range))
	{
		StartIndex = StartIndex - 1;
	}

	if (StartIndex == RangesToSearch.Num())
	{
		return TRange<int32>::Empty();
	}


	// Search the remaining ranges for the last upper bound greater than the input range
	RangesToSearch = RangesToSearch.Slice(StartIndex, RangesToSearch.Num() - StartIndex);

	// Binary search the first lower bound that is greater than or equal to the input range's upper bound
	int32 Length = Range.GetUpperBound().IsOpen() ? RangesToSearch.Num() : Algo::UpperBoundBy(RangesToSearch, Range.GetUpperBound(), &FMovieSceneFrameRange::GetUpperBound, MovieSceneHelpers::SortUpperBounds);

	// Length is always <= RangesToSearch.Num(). If the previous range overlaps the input range, include that
	if (Length < RangesToSearch.Num() && RangesToSearch[Length].Value.Overlaps(Range))
	{
		Length = Length + 1;
	}

	return Length > 0 ? TRange<int32>(StartIndex, StartIndex + Length) : TRange<int32>::Empty();
}

void FMovieSceneEvaluationField::Invalidate(const TRange<FFrameNumber>& Range)
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

int32 FMovieSceneEvaluationField::Insert(const TRange<FFrameNumber>& InRange, FMovieSceneEvaluationGroup&& InGroup, FMovieSceneEvaluationMetaData&& InMetaData)
{
	const int32 InsertIndex = Algo::UpperBoundBy(Ranges, InRange.GetLowerBound(), &FMovieSceneFrameRange::GetLowerBound, MovieSceneHelpers::SortLowerBounds);

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

void FMovieSceneEvaluationField::Add(const TRange<FFrameNumber>& InRange, FMovieSceneEvaluationGroup&& InGroup, FMovieSceneEvaluationMetaData&& InMetaData)
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
