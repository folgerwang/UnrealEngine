// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Allocator2D.h"
#include "MeshDescription.h"
#include "MeshDescriptionHelper.h"

#define NEW_UVS_ARE_SAME THRESH_POINTS_ARE_SAME
#define LEGACY_UVS_ARE_SAME (1.0f / 1024.0f)

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
};

struct FAllocator2DShader
{
	FAllocator2D*	Allocator2D;

	FAllocator2DShader( FAllocator2D* InAllocator2D )
	: Allocator2D( InAllocator2D )
	{}

	FORCEINLINE void Process( uint32 x, uint32 y )
	{
		Allocator2D->SetBit( x, y );
	}
};

class MESHUTILITIES_API FLayoutUV
{
public:
				FLayoutUV( UMeshDescription* InMesh, uint32 InSrcChannel, uint32 InDstChannel, uint32 InTextureResolution );

	void		FindCharts( const TMultiMap<int32,int32>& OverlappingCorners );
	bool		FindBestPacking();
	void		CommitPackedUVs();

	void		SetVersion(FMeshDescriptionHelper::ELightmapUVVersion Version ) { LayoutVersion = Version; }

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

	float		GetUVEqualityThreshold() const { return LayoutVersion >= FMeshDescriptionHelper::ELightmapUVVersion::SmallChartPacking ? NEW_UVS_ARE_SAME : LEGACY_UVS_ARE_SAME; }

	UMeshDescription* MeshDescription;
	uint32		SrcChannel;
	uint32		DstChannel;
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

	FMeshDescriptionHelper::ELightmapUVVersion LayoutVersion;
};


#define THRESH_UVS_ARE_SAME (GetUVEqualityThreshold())


inline bool FLayoutUV::PositionsMatch( uint32 a, uint32 b ) const
{
	const FVertexInstanceID VertexInstanceIDA(a);
	const FVertexInstanceID VertexInstanceIDB(b);
	check(MeshDescription->VertexInstances().IsValid(VertexInstanceIDA));
	check(MeshDescription->VertexInstances().IsValid(VertexInstanceIDB));
	const FMeshVertexInstance& VertexInstanceA = MeshDescription->GetVertexInstance(VertexInstanceIDA);
	const FMeshVertexInstance& VertexInstanceB = MeshDescription->GetVertexInstance(VertexInstanceIDB);
	const FMeshVertex& VertexA = MeshDescription->GetVertex(VertexInstanceA.VertexID);
	const FMeshVertex& VertexB = MeshDescription->GetVertex(VertexInstanceB.VertexID);
	return VertexA.VertexPosition.Equals(VertexB.VertexPosition, THRESH_POINTS_ARE_SAME);
}

inline bool FLayoutUV::NormalsMatch( uint32 a, uint32 b ) const
{
	const FVertexInstanceID VertexInstanceIDA(a);
	const FVertexInstanceID VertexInstanceIDB(b);
	check(MeshDescription->VertexInstances().IsValid(VertexInstanceIDA));
	check(MeshDescription->VertexInstances().IsValid(VertexInstanceIDB));
	const FMeshVertexInstance& VertexInstanceA = MeshDescription->GetVertexInstance(VertexInstanceIDA);
	const FMeshVertexInstance& VertexInstanceB = MeshDescription->GetVertexInstance(VertexInstanceIDB);

	if (!VertexInstanceA.VertexUVs.IsValidIndex(SrcChannel))
	{
		return true;
	}
	return VertexInstanceA.Normal.Equals(VertexInstanceB.Normal, THRESH_NORMALS_ARE_SAME);
}

inline bool FLayoutUV::UVsMatch( uint32 a, uint32 b ) const
{
	const FVertexInstanceID VertexInstanceIDA(a);
	const FVertexInstanceID VertexInstanceIDB(b);
	check(MeshDescription->VertexInstances().IsValid(VertexInstanceIDA));
	check(MeshDescription->VertexInstances().IsValid(VertexInstanceIDB));
	const FMeshVertexInstance& VertexInstanceA = MeshDescription->GetVertexInstance(VertexInstanceIDA);
	const FMeshVertexInstance& VertexInstanceB = MeshDescription->GetVertexInstance(VertexInstanceIDB);

	if (!VertexInstanceA.VertexUVs.IsValidIndex(SrcChannel))
	{
		return true;
	}
	return VertexInstanceA.VertexUVs[SrcChannel].Equals(VertexInstanceB.VertexUVs[SrcChannel], THRESH_UVS_ARE_SAME);
}

inline bool FLayoutUV::VertsMatch( uint32 a, uint32 b ) const
{
	const FVertexInstanceID VertexInstanceIDA(a);
	const FVertexInstanceID VertexInstanceIDB(b);
	check(MeshDescription->VertexInstances().IsValid(VertexInstanceIDA));
	check(MeshDescription->VertexInstances().IsValid(VertexInstanceIDB));
	const FMeshVertexInstance& VertexInstanceA = MeshDescription->GetVertexInstance(VertexInstanceIDA);
	const FMeshVertexInstance& VertexInstanceB = MeshDescription->GetVertexInstance(VertexInstanceIDB);
	bool IsUVsMatch = false;
	if (!VertexInstanceA.VertexUVs.IsValidIndex(SrcChannel))
	{
		IsUVsMatch = true;
	}
	else
	{
		IsUVsMatch = VertexInstanceA.VertexUVs[SrcChannel].Equals(VertexInstanceB.VertexUVs[SrcChannel], THRESH_UVS_ARE_SAME);
	}
	if (!IsUVsMatch)
	{
		return false;
	}
	const FMeshVertex& VertexA = MeshDescription->GetVertex(VertexInstanceA.VertexID);
	const FMeshVertex& VertexB = MeshDescription->GetVertex(VertexInstanceB.VertexID);
	return VertexA.VertexPosition.Equals(VertexB.VertexPosition, THRESH_POINTS_ARE_SAME);
}

// Signed UV area
inline float FLayoutUV::TriangleUVArea( uint32 Tri ) const
{
	FVector2D UVs[3];
	for( int k = 0; k < 3; k++ )
	{
		FVertexInstanceID VertexInstanceID((3*Tri) + k);
		check(MeshDescription->VertexInstances().IsValid(VertexInstanceID));
		const FMeshVertexInstance& VertexInstance = MeshDescription->GetVertexInstance(VertexInstanceID);
		check(VertexInstance.VertexUVs.IsValidIndex(SrcChannel));
		UVs[k] = VertexInstance.VertexUVs[ SrcChannel ];
	}

	FVector2D EdgeUV1 = UVs[1] - UVs[0];
	FVector2D EdgeUV2 = UVs[2] - UVs[0];
	return 0.5f * ( EdgeUV1.X * EdgeUV2.Y - EdgeUV1.Y * EdgeUV2.X );
}

inline void FLayoutUV::DisconnectChart( FMeshChart& Chart, uint32 Side )
{
	if( Chart.Join[ Side ] != -1 )
	{
		Charts[ Chart.Join[ Side ] ].Join[ Side ^ 1 ] = -1;
		Chart.Join[ Side ] = -1;
	}
}
