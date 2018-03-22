// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Curves/KeyHandle.h"
#include "FrameNumber.h"
#include "FrameTime.h"

struct FKeyDataOptimizationParams;

struct FMovieSceneKeyHandleMap : FKeyHandleLookupTable
{
	FMovieSceneKeyHandleMap() = default;
	FMovieSceneKeyHandleMap(const FMovieSceneKeyHandleMap& RHS){}
	FMovieSceneKeyHandleMap& operator=(const FMovieSceneKeyHandleMap& RHS)
	{
		Reset();
		return *this;
	}
};

namespace MovieScene
{
	MOVIESCENE_API void EvaluateTime(TArrayView<const FFrameNumber> InTimes, FFrameTime InTime, int32& OutIndex1, int32& OutIndex2);

	MOVIESCENE_API void EvaluateTime(TArrayView<const FFrameNumber> InTimes, FFrameTime InTime, int32& OutIndex1, int32& OutIndex2, float& OutInterp);

	MOVIESCENE_API void FindRange(TArrayView<const FFrameNumber> InTimes, FFrameNumber PredicateTime, FFrameNumber InTolerance, int32 MaxNum, int32& OutMin, int32& OutMax);
}

struct FMovieSceneChannel
{
	MOVIESCENE_API FMovieSceneChannel(TArray<FFrameNumber>* InTimes, FKeyHandleLookupTable* InKeyHandles);

	MOVIESCENE_API FKeyHandle GetHandle(int32 Index);

	MOVIESCENE_API int32 GetIndex(FKeyHandle Handle);

	MOVIESCENE_API int32 FindKey(FFrameNumber InTime, int32 InTolerance = 0);

	MOVIESCENE_API void FindKeys(FFrameNumber InTime, int32 MaxNum, int32& OutMinIndex, int32& OutMaxIndex, int32 InTolerance);

	// mutable access to key times - any usage *must* keep times sorted. Any reordering of times will not be reflected in the values array.
	FORCEINLINE TArrayView<FFrameNumber>       GetTimes()       { return *Times; }
	FORCEINLINE TArrayView<const FFrameNumber> GetTimes() const { return *Times; }

protected:

	MOVIESCENE_API int32 MoveKeyInternal(int32 KeyIndex, FFrameNumber InNewTime);

	MOVIESCENE_API int32 AddKeyInternal(FFrameNumber InTime);

	TArray<FFrameNumber>*     Times;
	FKeyHandleLookupTable* KeyHandles;
};

template<typename ValueType>
struct TMovieSceneChannel : FMovieSceneChannel
{
	typedef typename TCallTraits<ValueType>::ParamType ParamType;

	TMovieSceneChannel(TArray<FFrameNumber>* InTimes, TArray<ValueType>* InValues, FKeyHandleLookupTable* InKeyHandles)
		: FMovieSceneChannel(InTimes, InKeyHandles), Values(InValues)
	{
		check(Times && Values);
	}

	operator TMovieSceneChannel<const ValueType>()
	{
		return TMovieSceneChannel<const ValueType>(Times, Values);
	}

	void RemoveKey(int32 KeyIndex)
	{
		check(Times->IsValidIndex(KeyIndex));
		Times->RemoveAt(KeyIndex, 1, false);
		Values->RemoveAt(KeyIndex, 1, false);

		if (KeyHandles)
		{
			KeyHandles->DeallocateHandle(KeyIndex);
		}
	}

	int32 MoveKey(int32 KeyIndex, FFrameNumber InNewTime)
	{
		int32 NewIndex = MoveKeyInternal(KeyIndex, InNewTime);
		if (NewIndex != KeyIndex)
		{
			// We have to remove the key and re-add it in the right place
			// This could probably be done better by just shuffling up/down the items that need to move, without ever changing the size of the array
			ValueType OldValue = (*Values)[KeyIndex];
			Values->RemoveAt(KeyIndex, 1, false);
			Values->Insert(OldValue, NewIndex);
		}
		return NewIndex;
	}

	int32 AddKey(FFrameNumber InTime, ParamType InValue)
	{
		int32 KeyIndex = AddKeyInternal(InTime);
		Values->Insert(InValue, KeyIndex);
		return KeyIndex;
	}

	int32 SetKeyTime(int32 KeyIndex, FFrameNumber InNewTime)
	{
		return MoveKey(KeyIndex, InNewTime);
	}

	FKeyHandle UpdateOrAddKey(FFrameNumber InTime, ParamType InValue)
	{
		int32 ExistingKey = FindKey(InTime);
		if (ExistingKey != INDEX_NONE)
		{
			(*Values)[ExistingKey] = InValue;
		}
		else
		{
			ExistingKey = AddKey(InTime, InValue);
		}

		return GetHandle(ExistingKey);
	}

	void Reset()
	{
		Times->Reset();
		Values->Reset();
		if (KeyHandles)
		{
			KeyHandles->Reset();
		}
	}

	TArrayView<const ValueType> GetValues() const
	{
		return *Values;
	}

	TArrayView<ValueType> GetValues()
	{
		return *Values;
	}

private:

	TArray<ValueType>* Values;
};


template<typename ValueType>
struct TMovieSceneChannel<const ValueType>
{
	typedef typename TCallTraits<ValueType>::ParamType ParamType;

	TMovieSceneChannel(const TArray<FFrameNumber>* InTimes, const TArray<ValueType>* InValues)
		: Times(InTimes), Values(InValues)
	{
		check(Times && Values);
	}

	int32 FindKey(FFrameNumber InTime, FFrameNumber InTolerance) const
	{
		int32 MinIndex = 0, MaxIndex = 0;
		MovieScene::FindRange(*Times, InTime, InTolerance, 1, MinIndex, MaxIndex);
		return MinIndex;
	}

	void FindKeys(FFrameNumber InTime, int32 MaxNum, int32& OutMinIndex, int32& OutMaxIndex, FFrameNumber InTolerance) const
	{
		MovieScene::FindRange(*Times, InTime, InTolerance, MaxNum, OutMinIndex, OutMaxIndex);
	}

	TArrayView<const FFrameNumber> GetTimes() const
	{
		return *Times;
	}

	TArrayView<const ValueType> GetValues() const
	{
		return *Values;
	}

private:

	const TArray<FFrameNumber>* Times;
	const TArray<ValueType>*    Values;
};