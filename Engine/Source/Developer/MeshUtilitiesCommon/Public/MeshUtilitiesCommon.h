// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class ELightmapUVVersion : int32
{
	BitByBit = 0,
	Segments = 1,
	SmallChartPacking = 2,
	ScaleChartsOrderingFix = 3,
	ChartJoiningLFix = 4,
	Allocator2DFlipFix = 5,
	ConsiderLightmapPadding = 6,
	Latest = ConsiderLightmapPadding
};

/** Helper struct for building acceleration structures. */
struct FIndexAndZ
{
	float Z;
	int32 Index;

	/** Default constructor. */
	FIndexAndZ() {}

	/** Initialization constructor. */
	FIndexAndZ(int32 InIndex, FVector V)
	{
		Z = 0.30f * V.X + 0.33f * V.Y + 0.37f * V.Z;
		Index = InIndex;
	}
};

/** Sorting function for vertex Z/index pairs. */
struct FCompareIndexAndZ
{
	FORCEINLINE bool operator()(FIndexAndZ const& A, FIndexAndZ const& B) const { return A.Z < B.Z; }
};

/**
* Returns true if the specified points are about equal
*/
inline bool PointsEqual(const FVector& V1, const FVector& V2, float ComparisonThreshold)
{
	if (FMath::Abs(V1.X - V2.X) > ComparisonThreshold
		|| FMath::Abs(V1.Y - V2.Y) > ComparisonThreshold
		|| FMath::Abs(V1.Z - V2.Z) > ComparisonThreshold)
	{
		return false;
	}
	return true;
}
