// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Channels/MovieSceneChannel.h"


namespace MovieScene
{
	void EvaluateTime(TArrayView<const FFrameNumber> InTimes, FFrameTime InTime, int32& OutIndex1, int32& OutIndex2)
	{
		const int32 Index2 = Algo::UpperBound(InTimes, InTime.FrameNumber);
		const int32 Index1 = Index2 - 1;

		OutIndex1 = Index1 >= 0            ? Index1 : INDEX_NONE;
		OutIndex2 = Index2 < InTimes.Num() ? Index2 : INDEX_NONE;
	}

	void EvaluateTime(TArrayView<const FFrameNumber> InTimes, FFrameTime InTime, int32& OutIndex1, int32& OutIndex2, float& OutInterp)
	{
		const int32 Index2 = Algo::UpperBound(InTimes, InTime.FrameNumber);
		const int32 Index1 = Index2 - 1;

		OutIndex1 = Index1 >= 0            ? Index1 : INDEX_NONE;
		OutIndex2 = Index2 < InTimes.Num() ? Index2 : INDEX_NONE;

		if (Index1 >= 0 && Index2 < InTimes.Num())
		{
			// Stay in integer space as long as possible
			const int32 Time1 = InTimes[Index1].Value, Time2 = InTimes[Index2].Value;
			const int32 Difference = Time2 - Time1;

			OutInterp = (InTime.FrameNumber.Value - Time1) / float(Difference) + InTime.GetSubFrame() / Difference;
		}
		else
		{
			OutInterp = 0.f;
		}
	}

	void FindRange(TArrayView<const FFrameNumber> InTimes, FFrameNumber PredicateTime, FFrameNumber InTolerance, int32 MaxNum, int32& OutMin, int32& OutMax)
	{
		const int32 LowerBound = Algo::LowerBound(InTimes, PredicateTime);

		int32 MinIndex = LowerBound, MaxIndex = LowerBound;
		int32 FwdIndex = LowerBound, BwdIndex = LowerBound-1;

		for (;MaxIndex - MinIndex < MaxNum;)
		{
			const bool bConsiderFwdIndex = FwdIndex < InTimes.Num() && FMath::Abs(InTimes[FwdIndex] - PredicateTime) <= InTolerance;
			const bool bConsiderBwdIndex = BwdIndex >= 0            && FMath::Abs(InTimes[BwdIndex] - PredicateTime) <= InTolerance;

			if (bConsiderFwdIndex && bConsiderBwdIndex)
			{
				const FFrameNumber FwdDifference = FMath::Abs(PredicateTime - InTimes[FwdIndex]);
				const FFrameNumber BwdDifference = FMath::Abs(PredicateTime - InTimes[BwdIndex]);

				if (FwdDifference < BwdDifference)
				{
					MaxIndex = ++FwdIndex;
				}
				else
				{
					MinIndex = BwdIndex--;
				}
			}
			else if (bConsiderFwdIndex)
			{
				MaxIndex = ++FwdIndex;
				// Stop considering backwards
				BwdIndex = INDEX_NONE;
			}
			else if (bConsiderBwdIndex)
			{
				MinIndex = BwdIndex--;
				// Stop considering forwards
				FwdIndex = InTimes.Num();
			}
			else
			{
				break;
			}
		}

		OutMin = MinIndex;
		OutMax = MaxIndex;
	}
} // namespace MovieScene

FMovieSceneChannel::FMovieSceneChannel(TArray<FFrameNumber>* InTimes, FKeyHandleLookupTable* InKeyHandles)
	: Times(InTimes), KeyHandles(InKeyHandles)
{}

FKeyHandle FMovieSceneChannel::GetHandle(int32 Index)
{
	check(Times->IsValidIndex(Index));
	ensureMsgf(KeyHandles, TEXT("This channel does not contain key handles"));
	return KeyHandles ? KeyHandles->FindOrAddKeyHandle(Index) : FKeyHandle();
}

int32 FMovieSceneChannel::GetIndex(FKeyHandle Handle)
{
	ensureMsgf(KeyHandles, TEXT("This channel does not contain key handles"));
	return KeyHandles ? KeyHandles->GetIndex(Handle) : INDEX_NONE;
}

int32 FMovieSceneChannel::FindKey(FFrameNumber InTime, int32 InTolerance)
{
	int32 MinIndex = 0, MaxIndex = 0;
	MovieScene::FindRange(*Times, InTime, InTolerance, 1, MinIndex, MaxIndex);
	if (Times->IsValidIndex(MinIndex) && FMath::Abs((*Times)[MinIndex] - InTime) <= InTolerance)
	{
		return MinIndex;
	}

	return INDEX_NONE;
}

void FMovieSceneChannel::FindKeys(FFrameNumber InTime, int32 MaxNum, int32& OutMinIndex, int32& OutMaxIndex, int32 InTolerance)
{
	MovieScene::FindRange(*Times, InTime, InTolerance, MaxNum, OutMinIndex, OutMaxIndex);
}

int32 FMovieSceneChannel::AddKeyInternal(FFrameNumber InTime)
{
	const int32 InsertIndex = Algo::UpperBound(*Times, InTime);

	Times->Insert(InTime, InsertIndex);
	if (KeyHandles)
	{
		KeyHandles->AllocateHandle(InsertIndex);
	}
	return InsertIndex;
}

int32 FMovieSceneChannel::MoveKeyInternal(int32 KeyIndex, FFrameNumber InNewTime)
{
	check(Times->IsValidIndex(KeyIndex));

	int32 NewIndex = Algo::LowerBound(*Times, InNewTime);
	if (NewIndex < KeyIndex || NewIndex > KeyIndex+1)
	{
		// If we're inserting after this key, decrement the new index since we will remove this key
		if (NewIndex > KeyIndex)
		{
			--NewIndex;
		}

		// We have to remove the key and re-add it in the right place
		// This could probably be done better by just shuffling up/down the items that need to move, without ever changing the size of the array
		Times->RemoveAt(KeyIndex, 1, false);
		Times->Insert(InNewTime, NewIndex);

		if (KeyHandles)
		{
			KeyHandles->MoveHandle(KeyIndex, NewIndex);
		}
		return NewIndex;
	}
	else
	{
		(*Times)[KeyIndex] = InNewTime;
		return KeyIndex;
	}
}