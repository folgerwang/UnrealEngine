// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"

/** A normal vector, quantized and packed into 32-bits. */
struct FPackedNormal
{
	union
	{
		struct
		{
			int8	X, Y, Z, W;

		};
		uint32		Packed;
	}				Vector;

	// Constructors.

	FPackedNormal() { Vector.Packed = 0; }
	FPackedNormal(const FVector& InVector) { *this = InVector; }
	FPackedNormal(const FVector4& InVector) { *this = InVector; }

	// Conversion operators.

	void operator=(const FVector& InVector);
	void operator=(const FVector4& InVector);
	VectorRegister GetVectorRegister() const;

	FVector ToFVector() const;
	FVector4 ToFVector4() const;

	// Set functions.
	void Set(const FVector& InVector) { *this = InVector; }

	// Equality operator.

	bool operator==(const FPackedNormal& B) const;
	bool operator!=(const FPackedNormal& B) const;

	// Serializer.

	FString ToString() const
	{
		return FString::Printf(TEXT("X=%d Y=%d Z=%d W=%d"), Vector.X, Vector.Y, Vector.Z, Vector.W);
	}

	friend RENDERCORE_API FArchive& operator<<(FArchive& Ar, FPackedNormal& N);
};

class FDeprecatedSerializedPackedNormal
{
	union
	{
		struct
		{
			uint8 X, Y, Z, W;
		};
		uint32		Packed;
	} Vector;

public:
	operator FVector() const
	{
		// Rescale [0..255] range to [-1..1]
		VectorRegister VectorToUnpack = VectorLoadByte4(this);
		VectorToUnpack = VectorMultiplyAdd(VectorToUnpack, VectorSetFloat1(1.0f / 127.5f), VectorSetFloat1(-1.0f));

		FVector UnpackedVector;
		VectorStoreFloat3(VectorToUnpack, &UnpackedVector);

		return UnpackedVector;
	}

	operator FVector4() const
	{
		// Rescale [0..255] range to [-1..1]
		VectorRegister VectorToUnpack = VectorLoadByte4(this);
		VectorToUnpack = VectorMultiplyAdd(VectorToUnpack, VectorSetFloat1(1.0f / 127.5f), VectorSetFloat1(-1.0f));

		FVector4 UnpackedVector;
		VectorStore(VectorToUnpack, &UnpackedVector);

		return UnpackedVector;
	}

	friend RENDERCORE_API FArchive& operator<<(FArchive& Ar, FDeprecatedSerializedPackedNormal& N);
};

FORCEINLINE void FPackedNormal::operator=(const FVector& InVector)
{
	const float Scale = MAX_int8;
	Vector.X = (int8)FMath::Clamp<int32>(FMath::RoundToInt(InVector.X * Scale), MIN_int8, MAX_int8);
	Vector.Y = (int8)FMath::Clamp<int32>(FMath::RoundToInt(InVector.Y * Scale), MIN_int8, MAX_int8);
	Vector.Z = (int8)FMath::Clamp<int32>(FMath::RoundToInt(InVector.Z * Scale), MIN_int8, MAX_int8);
	Vector.W = MAX_int8;
}

FORCEINLINE void FPackedNormal::operator=(const FVector4& InVector)
{
	const float Scale = MAX_int8;
	Vector.X = (int8)FMath::Clamp<int32>(FMath::RoundToInt(InVector.X * Scale), MIN_int8, MAX_int8);
	Vector.Y = (int8)FMath::Clamp<int32>(FMath::RoundToInt(InVector.Y * Scale), MIN_int8, MAX_int8);
	Vector.Z = (int8)FMath::Clamp<int32>(FMath::RoundToInt(InVector.Z * Scale), MIN_int8, MAX_int8);
	Vector.W = (int8)FMath::Clamp<int32>(FMath::RoundToInt(InVector.W * Scale), MIN_int8, MAX_int8);
}

FORCEINLINE bool FPackedNormal::operator==(const FPackedNormal& B) const
{
	return Vector.Packed == B.Vector.Packed;
}

FORCEINLINE bool FPackedNormal::operator!=(const FPackedNormal& B) const
{
	return !(*this == B);
}

FORCEINLINE FVector FPackedNormal::ToFVector() const
{
	VectorRegister VectorToUnpack = GetVectorRegister();
	// Write to FVector and return it.
	FVector UnpackedVector;
	VectorStoreFloat3(VectorToUnpack, &UnpackedVector);
	return UnpackedVector;
}

FORCEINLINE FVector4 FPackedNormal::ToFVector4() const
{
	VectorRegister VectorToUnpack = GetVectorRegister();
	// Write to FVector4 and return it.
	FVector4 UnpackedVector;
	VectorStore(VectorToUnpack, &UnpackedVector);
	return UnpackedVector;
}

FORCEINLINE VectorRegister FPackedNormal::GetVectorRegister() const
{
	// Rescale [0..255] range to [-1..1]
	VectorRegister VectorToUnpack		= VectorLoadSignedByte4( this );
	VectorToUnpack = VectorMultiply(VectorToUnpack, VectorSetFloat1(1.0f / 127.0f));
	// Return unpacked vector register.
	return VectorToUnpack;
}

/** A vector, quantized and packed into 32-bits. */
struct FPackedRGB10A2N
{
	union
	{
		struct
		{
#if PLATFORM_LITTLE_ENDIAN
			uint32 X : 10;
			uint32 Y : 10;
			uint32 Z : 10;
			uint32 W : 2;
#else
			uint32 W : 2;
			uint32 Z : 10;
			uint32 Y : 10;
			uint32 X : 10;
#endif
		};

		struct  
		{
			uint32 Packed;
		};
	} Vector;

	// Constructors.

	FPackedRGB10A2N() { Vector.Packed = 0; }
	FPackedRGB10A2N(const FVector& InVector) { *this = InVector; }
	FPackedRGB10A2N(const FVector4& InVector) { *this = InVector; }

	// Conversion operators.

	void operator=(const FVector& InVector);
	void operator=(const FVector4& InVector);

	VectorRegister GetVectorRegister() const;

	// Set functions.
	void Set(const FVector& InVector) { *this = InVector; }
	void Set(const FVector4& InVector) { *this = InVector; }

	// Equality operator.

	bool operator==(const FPackedRGB10A2N& B) const;
	bool operator!=(const FPackedRGB10A2N& B) const;

	// Serializer.

	friend RENDERCORE_API FArchive& operator<<(FArchive& Ar, FPackedRGB10A2N& N);

	FString ToString() const
	{
		return FString::Printf(TEXT("X=%d Y=%d Z=%d W=%d"), Vector.X, Vector.Y, Vector.Z, Vector.W);
	}

	static RENDERCORE_API FPackedRGB10A2N ZeroVector;
};

FORCEINLINE void FPackedRGB10A2N::operator=(const FVector& InVector)
{
	Vector.X = FMath::Clamp(FMath::TruncToInt(InVector.X * 511.5f + 511.5f), 0, 1023);
	Vector.Y = FMath::Clamp(FMath::TruncToInt(InVector.Y * 511.5f + 511.5f), 0, 1023);
	Vector.Z = FMath::Clamp(FMath::TruncToInt(InVector.Z * 511.5f + 511.5f), 0, 1023);
	Vector.W = 3;
}

FORCEINLINE void FPackedRGB10A2N::operator=(const FVector4& InVector)
{
	Vector.X = FMath::Clamp(FMath::TruncToInt(InVector.X * 511.5f + 511.5f), 0, 1023);
	Vector.Y = FMath::Clamp(FMath::TruncToInt(InVector.Y * 511.5f + 511.5f), 0, 1023);
	Vector.Z = FMath::Clamp(FMath::TruncToInt(InVector.Z * 511.5f + 511.5f), 0, 1023);
	Vector.W = FMath::Clamp(FMath::TruncToInt(InVector.W * 1.5f   + 1.5f),   0, 3);
}

FORCEINLINE bool FPackedRGB10A2N::operator==(const FPackedRGB10A2N& B) const
{
	return Vector.Packed == B.Vector.Packed;
}

FORCEINLINE bool FPackedRGB10A2N::operator!=(const FPackedRGB10A2N& B) const
{
	return !(*this == B);
}

FORCEINLINE VectorRegister FPackedRGB10A2N::GetVectorRegister() const
{
	VectorRegister VectorToUnpack = VectorLoadURGB10A2N((void*)this);
	VectorToUnpack = VectorMultiplyAdd(VectorToUnpack, MakeVectorRegister(2.0f, 2.0f, 2.0f, 2.0f), MakeVectorRegister(-1.0f, -1.0f, -1.0f, -1.0f));
	VectorResetFloatRegisters();
	// Return unpacked vector register.
	return VectorToUnpack;
}

/** A vector, quantized and packed into 32-bits. */
struct FPackedRGBA16N
{
	struct
	{
		int16 X;
		int16 Y;
		int16 Z;
		int16 W;
	};

	// Constructors.

	FPackedRGBA16N() { X = 0; Y = 0; Z = 0; W = 0; }
	FPackedRGBA16N(const FVector& InVector) { *this = InVector; }
	FPackedRGBA16N(const FVector4& InVector) { *this = InVector; }
	//FPackedRGBA16N(uint16 InX, uint16 InY, uint16 InZ, uint16 InW) { X = InX; Y = InY; Z = InZ; W = InW; }

	// Conversion operators.

	void operator=(const FVector& InVector);
	void operator=(const FVector4& InVector);

	FVector ToFVector() const;
	FVector4 ToFVector4() const;

	VectorRegister GetVectorRegister() const;

	// Set functions.
	void Set(const FVector& InVector) { *this = InVector; }
	void Set(const FVector4& InVector) { *this = InVector; }

	// Equality operator.

	bool operator==(const FPackedRGBA16N& B) const;
	bool operator!=(const FPackedRGBA16N& B) const;

	FString ToString() const
	{
		return FString::Printf(TEXT("X=%d Y=%d Z=%d W=%d"), X, Y, Z, W);
	}

	friend RENDERCORE_API FArchive& operator<<(FArchive& Ar, FPackedRGBA16N& N);
};

FORCEINLINE void FPackedRGBA16N::operator=(const FVector& InVector)
{
	const float Scale = MAX_int16;
	X = (int16)FMath::Clamp<int32>(FMath::RoundToInt(InVector.X * Scale), MIN_int16, MAX_int16);
	Y = (int16)FMath::Clamp<int32>(FMath::RoundToInt(InVector.Y * Scale), MIN_int16, MAX_int16);
	Z = (int16)FMath::Clamp<int32>(FMath::RoundToInt(InVector.Z * Scale), MIN_int16, MAX_int16);
	W = MAX_int16;
}

FORCEINLINE void FPackedRGBA16N::operator=(const FVector4& InVector)
{
	const float Scale = MAX_int16;
	X = (int16)FMath::Clamp<int32>(FMath::RoundToInt(InVector.X * Scale), MIN_int16, MAX_int16);
	Y = (int16)FMath::Clamp<int32>(FMath::RoundToInt(InVector.Y * Scale), MIN_int16, MAX_int16);
	Z = (int16)FMath::Clamp<int32>(FMath::RoundToInt(InVector.Z * Scale), MIN_int16, MAX_int16);
	W = (int16)FMath::Clamp<int32>(FMath::RoundToInt(InVector.W * Scale), MIN_int16, MAX_int16);
}

FORCEINLINE bool FPackedRGBA16N::operator==(const FPackedRGBA16N& B) const
{
	return (X == B.X) && (Y == B.Y) && (Z == B.Z) && (W == B.W);
}

FORCEINLINE bool FPackedRGBA16N::operator!=(const FPackedRGBA16N& B) const
{
	return !(*this == B);
}

FORCEINLINE FVector FPackedRGBA16N::ToFVector() const
{
	VectorRegister VectorToUnpack = GetVectorRegister();
	// Write to FVector and return it.
	FVector UnpackedVector;
	VectorStoreFloat3(VectorToUnpack, &UnpackedVector);
	return UnpackedVector;
}

FORCEINLINE FVector4 FPackedRGBA16N::ToFVector4() const
{
	VectorRegister VectorToUnpack = GetVectorRegister();
	// Write to FVector4 and return it.
	FVector4 UnpackedVector;
	VectorStore(VectorToUnpack, &UnpackedVector);
	return UnpackedVector;
}

FORCEINLINE VectorRegister FPackedRGBA16N::GetVectorRegister() const
{
	VectorRegister VectorToUnpack = VectorLoadSRGBA16N((void*)this);
	VectorToUnpack = VectorMultiply(VectorToUnpack, VectorSetFloat1(1.0f / 32767.0f));
	// Return unpacked vector register.
	return VectorToUnpack;
}
