// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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

				FLayoutUV( IMeshView& InMeshView, uint32 InTextureResolution );

	int32		FindCharts( const FOverlappingCorners& OverlappingCorners );
	bool		FindBestPacking();
	void		CommitPackedUVs();

	void		SetVersion( ELightmapUVVersion Version ) { LayoutVersion = Version; }

private:
	bool		PositionsMatch( uint32 a, uint32 b ) const;
	bool		NormalsMatch( uint32 a, uint32 b ) const;
	bool		UVsMatch( uint32 a, uint32 b ) const;
	bool		VertsMatch( uint32 a, uint32 b ) const;
	float		TriangleUVArea( uint32 Tri ) const;
	void		DisconnectChart( FMeshChart& Chart, uint32 Side );

	void		ScaleCharts( float UVScale );
	bool		PackCharts();
	void		OrientChart( FMeshChart& Chart, int32 Orientation );
	void		RasterizeChart( const FMeshChart& Chart, uint32 RectW, uint32 RectH );

	float		GetUVEqualityThreshold() const;

	IMeshView&	MeshView;
	uint32		TextureResolution;
	
	TArray< FVector2D >		TexCoords;
	TArray< uint32 >		SortedTris;
	TArray< FMeshChart >	Charts;
	float					TotalUVArea;
	float					MaxChartSize;

	FAllocator2D		LayoutRaster;
	FAllocator2D		ChartRaster;
	FAllocator2D		BestChartRaster;
	FAllocator2DShader	ChartShader;

	ELightmapUVVersion	LayoutVersion;

	int32				NextMeshChartId;
};
