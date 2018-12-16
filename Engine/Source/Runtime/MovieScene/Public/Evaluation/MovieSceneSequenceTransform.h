// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/FrameTime.h"
#include "MovieSceneSequenceTransform.generated.h"

/**
 * Movie scene sequence transform class that transforms from one time-space to another.
 *
 * @note The transform can be thought of as the top row of a 2x2 matrix, where the bottom row is the identity:
 * 			| TimeScale	Offset	|
 *			| 0			1		|
 *
 * As such, traditional matrix mathematics can be applied to transform between different sequence's time-spaces.
 * Transforms apply offset first, then time scale.
 */
USTRUCT()
struct FMovieSceneSequenceTransform
{
	GENERATED_BODY()

	/**
	 * Default construction to the identity transform
	 */
	FMovieSceneSequenceTransform()
		: TimeScale(1.f)
		, Offset(0)
	{}

	/**
	 * Construction from an offset, and a scale
	 *
	 * @param InOffset 			The offset to translate by
	 * @param InTimeScale 		The timescale. For instance, if a sequence is playing twice as fast, pass 2.f
	 */
	FMovieSceneSequenceTransform(FFrameTime InOffset, float InTimeScale = 1.f)
		: TimeScale(InTimeScale)
		, Offset(InOffset)
	{}

	friend bool operator==(const FMovieSceneSequenceTransform& A, const FMovieSceneSequenceTransform& B)
	{
		return A.TimeScale == B.TimeScale && A.Offset == B.Offset;
	}

	friend bool operator!=(const FMovieSceneSequenceTransform& A, const FMovieSceneSequenceTransform& B)
	{
		return A.TimeScale != B.TimeScale || A.Offset != B.Offset;
	}

	/**
	 * Retrieve the inverse of this transform
	 */
	FMovieSceneSequenceTransform Inverse() const
	{
		const FFrameTime NewOffset = -Offset/TimeScale;
		return FMovieSceneSequenceTransform(NewOffset, 1.f/TimeScale);
	}

	/** The sequence's time scale (or play rate) */
	UPROPERTY()
	float TimeScale;

	/** Scalar frame offset applied before the scale */
	UPROPERTY()
	FFrameTime Offset;
};

/**
 * Transform a time by a sequence transform
 *
 * @param InTime 			The time to transform
 * @param RHS 				The transform
 */
inline FFrameTime operator*(FFrameTime InTime, const FMovieSceneSequenceTransform& RHS)
{
	// Avoid floating point conversion when in the same time-space
	if (RHS.TimeScale == 1.f)
	{
		return InTime + RHS.Offset;
	}

	return RHS.Offset + InTime * RHS.TimeScale;
}

/**
 * Transform a time by a sequence transform
 *
 * @param InTime 			The time to transform
 * @param RHS 				The transform
 */
inline FFrameTime& operator*=(FFrameTime& InTime, const FMovieSceneSequenceTransform& RHS)
{
	InTime = InTime * RHS;
	return InTime;
}

/**
 * Transform a time range by a sequence transform
 *
 * @param LHS 				The time range to transform
 * @param RHS 				The transform
 */
template<typename T>
TRange<T> operator*(const TRange<T>& LHS, const FMovieSceneSequenceTransform& RHS)
{
	TRangeBound<T> SourceLower = LHS.GetLowerBound();
	TRangeBound<T> TransformedLower =
		SourceLower.IsOpen() ? 
			TRangeBound<T>() : 
			SourceLower.IsInclusive() ?
				TRangeBound<T>::Inclusive(SourceLower.GetValue() * RHS) :
				TRangeBound<T>::Exclusive(SourceLower.GetValue() * RHS);

	TRangeBound<T> SourceUpper = LHS.GetUpperBound();
	TRangeBound<T> TransformedUpper =
		SourceUpper.IsOpen() ? 
			TRangeBound<T>() : 
			SourceUpper.IsInclusive() ?
				TRangeBound<T>::Inclusive(SourceUpper.GetValue() * RHS) :
				TRangeBound<T>::Exclusive(SourceUpper.GetValue() * RHS);

	return TRange<T>(TransformedLower, TransformedUpper);
}

inline TRange<FFrameNumber> operator*(const TRange<FFrameNumber>& LHS, const FMovieSceneSequenceTransform& RHS)
{
	TRangeBound<FFrameNumber> SourceLower = LHS.GetLowerBound();
	TRangeBound<FFrameNumber> TransformedLower =
		SourceLower.IsOpen() ? 
			TRangeBound<FFrameNumber>() : 
			SourceLower.IsInclusive() ?
				TRangeBound<FFrameNumber>::Inclusive((SourceLower.GetValue() * RHS).FloorToFrame()) :
				TRangeBound<FFrameNumber>::Exclusive((SourceLower.GetValue() * RHS).FloorToFrame());

	TRangeBound<FFrameNumber> SourceUpper = LHS.GetUpperBound();
	TRangeBound<FFrameNumber> TransformedUpper =
		SourceUpper.IsOpen() ? 
			TRangeBound<FFrameNumber>() : 
			SourceUpper.IsInclusive() ?
				TRangeBound<FFrameNumber>::Inclusive((SourceUpper.GetValue() * RHS).FloorToFrame()) :
				TRangeBound<FFrameNumber>::Exclusive((SourceUpper.GetValue() * RHS).FloorToFrame());

	return TRange<FFrameNumber>(TransformedLower, TransformedUpper);
}

/**
 * Transform a time range by a sequence transform
 *
 * @param InTime 			The time range to transform
 * @param RHS 				The transform
 */
template<typename T>
TRange<T>& operator*=(TRange<T>& LHS, const FMovieSceneSequenceTransform& RHS)
{
	LHS = LHS * RHS;
	return LHS;
}

/**
 * Multiply 2 transforms together, resulting in a single transform that gets from RHS parent to LHS space
 * @note Transforms apply from right to left
 */
inline FMovieSceneSequenceTransform operator*(const FMovieSceneSequenceTransform& LHS, const FMovieSceneSequenceTransform& RHS)
{
	// The matrix multiplication occurs as follows:
	//
	// | TimeScaleA	, OffsetA	|	.	| TimeScaleB, OffsetB	|
	// | 0			, 1			|		| 0			, 1			|

	const FFrameTime ScaledOffsetRHS = LHS.TimeScale == 1.f ? RHS.Offset : RHS.Offset*LHS.TimeScale;
	return FMovieSceneSequenceTransform(
		LHS.Offset + ScaledOffsetRHS,		// New Offset
		LHS.TimeScale * RHS.TimeScale		// New TimeScale
		);
}
