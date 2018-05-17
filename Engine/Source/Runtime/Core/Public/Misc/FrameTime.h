// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/FrameNumber.h"
#include "Misc/AssertionMacros.h"
#include "Templates/EnableIf.h"

/**
 * Structure representing a time by a context-free frame number, plus a sub frame value in the range [0:1)
 * Conversion to and from time in seconds is achieved in combination with FFrameRate.
 * Only the frame number part of this representation can be negative, sub frames are always a positive value between the frame number and its next logical frame
 */
struct FFrameTime
{

	static CORE_API const float MaxSubframe;

	/**
	 * Default constructor initializing to zero
	 */
	FFrameTime();

	/**
	 * Implicit construction from a single integer, while disallowing implicit conversion from any other numeric type
	 */
	template<typename T, typename = typename TEnableIf<TIsSame<T, int32>::Value>::Type>
	FFrameTime(T /* int32 */ InFrameNumber);

	/**
	 * Implicit construction from a type-safe frame number
	 */
	FFrameTime(FFrameNumber InFrameNumber);

	/**
	 * Construction from a frame number and a sub frame
	 */
	FFrameTime(FFrameNumber InFrameNumber, float InSubFrame);

	/**
	 * Assignment from a type-safe frame number
	 */
	FFrameTime& operator=(FFrameNumber InFrameNumber);

public:

	/**
	 * Access this time's frame number
	 */
	FORCEINLINE FFrameNumber GetFrame() const
	{
		return FrameNumber;
	}

	/**
	 * Access this time's sub frame
	 */
	FORCEINLINE float GetSubFrame() const
	{
		return SubFrame;
	}

	/**
	 * Return the first frame number less than or equal to this frame time
	 */
	FFrameNumber FloorToFrame() const;

	/**
	 * Return the next frame number greater than or equal to this frame time
	 */
	FFrameNumber CeilToFrame() const;

	/**
	 * Round to the nearest frame number
	 */
	FFrameNumber RoundToFrame() const;

	/**
	 * Retrieve a decimal representation of this frame time
	 * Sub frames are always added to the current frame number, so for negative frame times, a time of -10 [sub frame 0.25] will yield a decimal value of -9.75.
	 */
	double AsDecimal() const;

	/**
	 * Convert a decimal representation to a frame time
	 * Note that sub frames are always positive, so negative decimal representations result in an inverted sub frame and floored frame number
	 */
	static FFrameTime FromDecimal(double InDecimalFrame);


private:

	/*~ Built in operators */
	friend bool operator==(FFrameTime A, FFrameTime B);
	friend bool operator!=(FFrameTime A, FFrameTime B);
	friend bool operator> (FFrameTime A, FFrameTime B);
	friend bool operator>=(FFrameTime A, FFrameTime B);
	friend bool operator< (FFrameTime A, FFrameTime B);
	friend bool operator<=(FFrameTime A, FFrameTime B);

	friend FFrameTime& operator+=(FFrameTime& LHS, FFrameTime RHS);
	friend FFrameTime  operator+(FFrameTime A, FFrameTime B);
	friend FFrameTime  operator-(FFrameTime A, FFrameTime B);

	friend FFrameTime  operator-(FFrameTime A);

	friend FFrameTime  operator*(FFrameTime A, float Scalar);
	friend FFrameTime  operator/(FFrameTime A, float Scalar);

public:
	FFrameNumber FrameNumber;

private:

	/** Must be 0.f <= SubFrame < 1.f */
	float SubFrame;
};


inline FFrameTime::FFrameTime()
	: FrameNumber(0), SubFrame(0.f)
{}


template<typename T, typename>
FFrameTime::FFrameTime(T InFrameNumber)
	: FrameNumber(InFrameNumber), SubFrame(0.f)
{}


inline FFrameTime::FFrameTime(FFrameNumber InFrameNumber)
	: FrameNumber(InFrameNumber), SubFrame(0.f)
{}


inline FFrameTime::FFrameTime(FFrameNumber InFrameNumber, float InSubFrame)
	: FrameNumber(InFrameNumber), SubFrame(InSubFrame)
{
	// Hack to ensure that SubFrames are in a sensible range of precision to work around
	// problems with FloorToXYZ returning the wrong thing for very small negative numbers
	SubFrame = FMath::Clamp(SubFrame + 0.5f - 0.5f, 0.f, MaxSubframe);
	checkSlow(InSubFrame >= 0.f && InSubFrame < 1.f);
}


inline FFrameTime& FFrameTime::operator=(FFrameNumber InFrameNumber)
{
	FrameNumber = InFrameNumber;
	SubFrame    = 0.f;
	return *this;
}


FORCEINLINE_DEBUGGABLE bool operator==(FFrameTime A, FFrameTime B)
{
	return A.FrameNumber == B.FrameNumber && A.SubFrame == B.SubFrame;
}


FORCEINLINE_DEBUGGABLE bool operator!=(FFrameTime A, FFrameTime B)
{
	return A.FrameNumber != B.FrameNumber || A.SubFrame != B.SubFrame;
}


FORCEINLINE_DEBUGGABLE bool operator> (FFrameTime A, FFrameTime B)
{
	return A.FrameNumber >  B.FrameNumber || ( A.FrameNumber == B.FrameNumber && A.SubFrame > B.SubFrame );
}


FORCEINLINE_DEBUGGABLE bool operator>=(FFrameTime A, FFrameTime B)
{
	return A.FrameNumber > B.FrameNumber || ( A.FrameNumber == B.FrameNumber && A.SubFrame >= B.SubFrame );
}


FORCEINLINE_DEBUGGABLE bool operator< (FFrameTime A, FFrameTime B)
{
	return A.FrameNumber <  B.FrameNumber || ( A.FrameNumber == B.FrameNumber && A.SubFrame < B.SubFrame );
}


FORCEINLINE_DEBUGGABLE bool operator<=(FFrameTime A, FFrameTime B)
{
	return A.FrameNumber < B.FrameNumber || ( A.FrameNumber == B.FrameNumber && A.SubFrame <= B.SubFrame );
}


FORCEINLINE_DEBUGGABLE FFrameTime& operator+=(FFrameTime& LHS, FFrameTime RHS)
{
	float NewSubFrame = LHS.SubFrame + RHS.SubFrame;

	LHS.FrameNumber = LHS.FrameNumber + RHS.FrameNumber + FFrameNumber(FMath::FloorToInt(NewSubFrame));
	LHS.SubFrame    = FMath::Frac(NewSubFrame);

	return LHS;
}


FORCEINLINE_DEBUGGABLE FFrameTime operator+(FFrameTime A, FFrameTime B)
{
	const float        NewSubFrame    = A.SubFrame + B.SubFrame;
	const FFrameNumber NewFrameNumber = A.FrameNumber + B.FrameNumber + FFrameNumber(FMath::FloorToInt(NewSubFrame));

	return FFrameTime(NewFrameNumber, FMath::Frac(NewSubFrame));
}


FORCEINLINE_DEBUGGABLE FFrameTime operator-(FFrameTime A, FFrameTime B)
{
	// Ensure SubFrame is always between 0 and 1
	// Note that the difference between frame -1.5 and 1.5 is 2, not 3, since sub frame positions are always positive
	const float        NewSubFrame     = A.SubFrame - B.SubFrame;
	const int          FlooredSubFrame = FMath::FloorToInt(NewSubFrame);
	const FFrameNumber NewFrameNumber  = A.FrameNumber - B.FrameNumber + FFrameNumber(FlooredSubFrame);

	return FFrameTime(NewFrameNumber, NewSubFrame - FlooredSubFrame);
}


FORCEINLINE_DEBUGGABLE FFrameTime operator-(FFrameTime A)
{
	return A.GetSubFrame() == 0.f
		? FFrameTime(-A.FrameNumber)
		: FFrameTime(-A.FrameNumber - 1, 1.f-A.GetSubFrame());
}


FORCEINLINE FFrameTime operator*(FFrameTime A, float Scalar)
{
	return FFrameTime::FromDecimal(A.AsDecimal() * Scalar);
}


FORCEINLINE FFrameTime operator/(FFrameTime A, float Scalar)
{
	return FFrameTime::FromDecimal(A.AsDecimal() / Scalar);
}


FORCEINLINE_DEBUGGABLE FFrameNumber FFrameTime::FloorToFrame() const
{
	return FrameNumber;
}


FORCEINLINE_DEBUGGABLE FFrameNumber FFrameTime::CeilToFrame() const
{
	return SubFrame == 0.f ? FrameNumber : FrameNumber+1;
}


FORCEINLINE_DEBUGGABLE FFrameNumber FFrameTime::RoundToFrame() const
{
	return SubFrame < .5f ? FrameNumber : FrameNumber+1;
}


FORCEINLINE_DEBUGGABLE double FFrameTime::AsDecimal() const
{
	return double(FrameNumber.Value) + SubFrame;
}

FORCEINLINE_DEBUGGABLE FFrameTime FFrameTime::FromDecimal(double InDecimalFrame)
{
	int32 NewFrame = static_cast<int32>(FMath::FloorToDouble(InDecimalFrame));

	// Ensure fractional parts above the highest sub frame float precision do not round to 0.0
	double Fraction = InDecimalFrame - FMath::FloorToDouble(InDecimalFrame);
	return FFrameTime(NewFrame, FMath::Clamp(Fraction, 0.0, (double)MaxSubframe));
}