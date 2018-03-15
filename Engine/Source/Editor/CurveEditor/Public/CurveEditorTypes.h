// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/TypeHash.h"
#include "Curves/KeyHandle.h"

/**
 * Enum for representing the type of a key point in the curve editor
 */
enum class ECurvePointType : uint8
{
	Key, ArriveTangent, LeaveTangent
};


/**
 * A unique identifier for a curve model existing on a curve editor
 */
struct FCurveModelID
{
	/**
	 * Generate a new curve model ID
	 */
	static FCurveModelID Unique();

	/**
	 * Check two IDs for equality
	 */
	FORCEINLINE friend bool operator==(FCurveModelID A, FCurveModelID B)
	{
		return A.ID == B.ID;
	}

	/**
	 * Hash a curve model ID
	 */
	FORCEINLINE friend uint32 GetTypeHash(FCurveModelID In)
	{
		return GetTypeHash(In.ID);
	}

private:
	FCurveModelID() {}

	/** Internal serial ID */
	uint32 ID;
};




/**
 * A unique handle to a particular point handle (key, tangent handle etc) on a curve, represented by the key's handle, its curve ID, and its type
 */
struct FCurvePointHandle
{
	FCurvePointHandle(FCurveModelID InCurveID, ECurvePointType InPointType, FKeyHandle InKeyHandle)
		: CurveID(InCurveID), PointType(InPointType), KeyHandle(InKeyHandle)
	{}

	/** The curve ID of the key's curve */
	FCurveModelID CurveID;
	/** The type of this point */
	ECurvePointType PointType;
	/** The key handle for the underlying key */
	FKeyHandle KeyHandle;
};