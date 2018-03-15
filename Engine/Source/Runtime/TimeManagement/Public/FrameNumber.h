// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "FrameNumber.generated.h"

/**
 * Typesafe 32-bit signed frame number. Defined in this way to prevent erroneous float->int conversions and afford type-safe operator overloading.
 */
USTRUCT(BlueprintType)
struct FFrameNumber
{
	GENERATED_BODY()

	constexpr FFrameNumber()
		: Value(0)
	{}

	/**
	 * Implicit construction from a signed integer frame number, whilst disallowing any construction from other types.
	 */
	template<typename T, typename U = typename TEnableIf<TIsSame<T, int32>::Value>::Type>
	constexpr FFrameNumber(T /*int32*/ InValue)
		: Value(InValue)
	{}

	/**
	 * Serializes the given FrameNumber from or into the specified archive.
	 *
	 * @param Ar            The archive to serialize from or into.
	 * @param FrameNumber   The bound to serialize.
	 * @return The archive used for serialization.
	 */
	friend FArchive& operator<<(FArchive& Ar, FFrameNumber& FrameNumber)
	{
		return Ar << FrameNumber.Value;
	}

	bool Serialize(FArchive& Ar)
	{
		Ar << Value;
		return true;
	}

	FFrameNumber& operator+=(FFrameNumber RHS)                    { Value += RHS.Value; return *this; }
	FFrameNumber& operator-=(FFrameNumber RHS)                    { Value -= RHS.Value; return *this; }

	FFrameNumber& operator++()                                    { ++Value; return *this; }
	FFrameNumber& operator--()                                    { --Value; return *this; }

	FFrameNumber operator++(int32)                                { FFrameNumber Ret = *this; ++Value; return Ret; }
	FFrameNumber operator--(int32)                                { FFrameNumber Ret = *this; --Value; return Ret; }

	friend bool operator==(FFrameNumber A, FFrameNumber B)        { return A.Value == B.Value; }
	friend bool operator!=(FFrameNumber A, FFrameNumber B)        { return A.Value != B.Value; }

	friend bool operator< (FFrameNumber A, FFrameNumber B)        { return A.Value < B.Value; }
	friend bool operator> (FFrameNumber A, FFrameNumber B)        { return A.Value > B.Value; }
	friend bool operator<=(FFrameNumber A, FFrameNumber B)        { return A.Value <= B.Value; }
	friend bool operator>=(FFrameNumber A, FFrameNumber B)        { return A.Value >= B.Value; }

	friend FFrameNumber operator+(FFrameNumber A, FFrameNumber B) { return FFrameNumber(A.Value + B.Value); }
	friend FFrameNumber operator-(FFrameNumber A, FFrameNumber B) { return FFrameNumber(A.Value - B.Value); }

	friend FFrameNumber operator-(FFrameNumber A)                 { return FFrameNumber(-A.Value); }

	friend FFrameNumber operator*(FFrameNumber A, float Scalar)   { return FFrameNumber(static_cast<int32>(FMath::FloorToDouble(double(A.Value) * Scalar))); }
	friend FFrameNumber operator/(FFrameNumber A, float Scalar)   { return FFrameNumber(static_cast<int32>(FMath::FloorToDouble(double(A.Value) / Scalar))); }

	/**
	 * The value of the frame number
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=General)
	int32 Value;
};

template<>
struct TStructOpsTypeTraits<FFrameNumber> : public TStructOpsTypeTraitsBase2<FFrameNumber>
{
	enum { WithSerializer = true, WithIdenticalViaEquality = true };
};


inline uint32 GetTypeHash(FFrameNumber A)
{
	return A.Value;
}

template<>
struct TNumericLimits<FFrameNumber>
{
	typedef FFrameNumber NumericType;

	static constexpr NumericType Min()
	{
		return MIN_int32;
	}

	static constexpr NumericType Max()
	{
		return MAX_int32;
	}

	static constexpr NumericType Lowest()
	{
		return Min();
	}
};