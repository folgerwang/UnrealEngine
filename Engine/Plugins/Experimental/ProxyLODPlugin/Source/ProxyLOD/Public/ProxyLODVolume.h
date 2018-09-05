// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FMeshMergeData;
struct FRawMesh;

class PROXYLODMESHREDUCTION_API IProxyLODVolume
{
public:
	/** Helper class to extract dimensions in voxel size units of OpenVDB volume */
	struct PROXYLODMESHREDUCTION_API FVector3i
	{
		int32 X;
		int32 Y;
		int32 Z;

		FVector3i(int32 InX, int32 InY, int32 InZ)
			: X(InX), Y(InY), Z(InZ)
		{
		}

		FORCEINLINE int32 operator[](int32 Index) const
		{
			return Index == 0 ? X : (Index == 1 ? Y : (Index == 2 ? Z : 0));
		}

		int32 MinIndex() const;
	};

	/** Create OpenVDB volume from input geometry */
	static TUniquePtr<IProxyLODVolume> CreateSDFVolumeFromMeshArray(const TArray<FMeshMergeData>& Geometry, float Step);

	virtual ~IProxyLODVolume() {}

	/** Get size of voxel cell */
	virtual double GetVoxelSize() const = 0;

	/** Get dimensions of bounding box of OpenVDB volume in multiple of size of voxel cell */
	virtual FVector3i GetBBoxSize() const = 0;

	/** Close any gap in OpenVDB volume which radius is less than given one in given maximum iteration */
	virtual void CloseGaps(const double GapRadius, const int32 MaxDilations) = 0;

	/** Extract iso distance 0 from OpenVDB volume as a RawMesh */
	virtual void ConvertToRawMesh(FRawMesh& OutRawMesh) const = 0;

	/** Expand exterior and interior narrow band of OpenVDB volume by given amount */
	virtual void ExpandNarrowBand(float ExteriorWidth, float InteriorWidth) = 0;

	/**
	 * Returns distance between given point and iso distance 0 of OpenVDB volume.
	 * Note: Returned value is clamped between -'dimension of interior narrow band' and +'dimension of exterior narrow band'
	*/
	virtual float QueryDistance(const FVector& Point) const = 0;
};
