// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SequencerKeyTimeCache.h"
#include "MovieSceneSection.h"
#include "Algo/Sort.h"
#include "Algo/BinarySearch.h"

void FSequencerCachedKeys::Update(TSharedRef<IKeyArea> InKeyArea, FFrameRate SourceResolution)
{
	UMovieSceneSection* Section = InKeyArea->GetOwningSection();
	if (!Section || !CachedSignature.IsValid() || Section->GetSignature() != CachedSignature || SourceResolution != CachedTickResolution)
	{
		CachedSignature = Section ? Section->GetSignature() : FGuid();
		CachedTickResolution = SourceResolution;

		CachedKeyFrames.Reset();

		TArray<FKeyHandle> Handles;
		InKeyArea->GetKeyInfo(&Handles, &CachedKeyFrames);

		CachedKeyTimes.Reset(CachedKeyFrames.Num());
		CachedKeyHandles.Reset(CachedKeyFrames.Num());

		// Generate and cache
		for (int32 Index = 0; Index < CachedKeyFrames.Num(); ++Index)
		{
			CachedKeyTimes.Add(CachedKeyFrames[Index] / SourceResolution);
			CachedKeyHandles.Add(Handles[Index]);
		}

		KeyArea = InKeyArea;
	}
}

void FSequencerCachedKeys::GetKeysInRange(const TRange<double>& Range, TArrayView<const double>* OutTimes, TArrayView<const FFrameNumber>* OutKeyFrames, TArrayView<const FKeyHandle>* OutHandles) const
{
	// Binary search the first time that's >= the lower bound
	int32 FirstVisibleIndex = Algo::LowerBound(CachedKeyTimes, Range.GetLowerBoundValue());
	// Binary search the last time that's > the upper bound
	int32 LastVisibleIndex = Algo::UpperBound(CachedKeyTimes, Range.GetUpperBoundValue());

	int32 Num = LastVisibleIndex - FirstVisibleIndex;
	if (CachedKeyTimes.IsValidIndex(FirstVisibleIndex) && LastVisibleIndex <= CachedKeyTimes.Num())
	{
		if (OutTimes)
		{
			*OutTimes = MakeArrayView(&CachedKeyTimes[FirstVisibleIndex], Num);
		}

		if (OutKeyFrames)
		{
			*OutKeyFrames = MakeArrayView(&CachedKeyFrames[FirstVisibleIndex], Num);
		}

		if (OutHandles)
		{
			*OutHandles = MakeArrayView(&CachedKeyHandles[FirstVisibleIndex], Num);
		}
	}
}