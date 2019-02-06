// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshUtilitiesCommon.h"
#include "Allocator2D.h"

struct FMeshChart
{
	uint32		FirstTri;
	uint32		LastTri;
	
	FVector2D	MinUV;
	FVector2D	MaxUV;
	
	float		UVArea;
	FVector2D	UVScale;
	FVector2D	WorldScale;
	
	FVector2D	PackingScaleU;
	FVector2D	PackingScaleV;
	FVector2D	PackingBias;

	int32		Join[4];

	int32		Id; // Store a unique id so that we can come back to the initial Charts ordering when necessary
};

struct FOverlappingCorners;

class MESHUTILITIESCOMMON_API FLayoutUV
{
public:

	/**
	 * Abstract triangle mesh view interface that may be used by any module without introducing
	 * a dependency on a concrete mesh type (and thus potentially circular module references).
	 * This abstraction results in a performance penalty due to virtual dispatch,
	 * however it is expected to be insignificant compared to the rest of work done by FLayoutUV
	 * and cache misses due to indexed vertex data access.
	*/
	struct IMeshView
	{
		virtual ~IMeshView() {}

		virtual uint32      GetNumIndices() const = 0;
		virtual FVector     GetPosition(uint32 Index) const = 0;
		virtual FVector     GetNormal(uint32 Index) const = 0;
		virtual FVector2D   GetInputTexcoord(uint32 Index) const = 0;

		virtual void        InitOutputTexcoords(uint32 Num) = 0;
		virtual void        SetOutputTexcoord(uint32 Index, const FVector2D& Value) = 0;
	};

	FLayoutUV( IMeshView& InMeshView );
	void SetVersion( ELightmapUVVersion Version ) { LayoutVersion = Version; }
	int32 FindCharts( const FOverlappingCorners& OverlappingCorners );
	bool FindBestPacking( uint32 InTextureResolution );
	void CommitPackedUVs();

private:
	IMeshView& MeshView;
	ELightmapUVVersion LayoutVersion;

	TArray< FVector2D > MeshTexCoords;
	TArray< uint32 > MeshSortedTris;
	TArray< FMeshChart > MeshCharts;
	uint32 PackedTextureResolution;

	struct FChartFinder;
	struct FChartPacker;
};
