// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Allocator2D.h"
#include "MeshDescription.h"
#include "MeshDescriptionOperations.h"
#include "MeshAttributes.h"

#define NEW_UVS_ARE_SAME THRESH_POINTS_ARE_SAME
#define LEGACY_UVS_ARE_SAME (1.0f / 1024.0f)
namespace MeshDescriptionOp
{
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

	struct FAllocator2DShader
	{
		FAllocator2D*	Allocator2D;

		FAllocator2DShader(FAllocator2D* InAllocator2D)
			: Allocator2D(InAllocator2D)
		{}

		FORCEINLINE void Process(uint32 x, uint32 y)
		{
			Allocator2D->SetBit(x, y);
		}
	};

	class FLayoutUV
	{
	public:
		FLayoutUV(FMeshDescription& InMesh, uint32 InSrcChannel, uint32 InDstChannel, uint32 InTextureResolution);

		void		FindCharts(const TMultiMap<int32, int32>& OverlappingCorners);
		bool		FindBestPacking();
		void		CommitPackedUVs();

		void		SetVersion(FMeshDescriptionOperations::ELightmapUVVersion Version) { LayoutVersion = Version; }

	private:
		bool		PositionsMatch(uint32 a, uint32 b) const;
		bool		NormalsMatch(uint32 a, uint32 b) const;
		bool		UVsMatch(uint32 a, uint32 b) const;
		bool		VertsMatch(uint32 a, uint32 b) const;
		float		TriangleUVArea(uint32 Tri) const;
		void		DisconnectChart(FMeshChart& Chart, uint32 Side);

		void		ScaleCharts(float UVScale);
		bool		PackCharts();
		void		OrientChart(FMeshChart& Chart, int32 Orientation);
		void		RasterizeChart(const FMeshChart& Chart, uint32 RectW, uint32 RectH);

		float		GetUVEqualityThreshold() const { return LayoutVersion >= FMeshDescriptionOperations::ELightmapUVVersion::SmallChartPacking ? NEW_UVS_ARE_SAME : LEGACY_UVS_ARE_SAME; }

		FMeshDescription&	MeshDescription;
		uint32				SrcChannel;
		uint32				DstChannel;
		uint32				TextureResolution;

		TArray< FVector2D >		TexCoords;
		TArray< uint32 >		SortedTris;
		TArray< FMeshChart >	Charts;
		float					TotalUVArea;
		float					MaxChartSize;
		TArray< int32 >			VertexIndexToID;
		TArray< int32 >			VertexIDToIndex;

		FAllocator2D		LayoutRaster;
		FAllocator2D		ChartRaster;
		FAllocator2D		BestChartRaster;
		FAllocator2DShader	ChartShader;

		FMeshDescriptionOperations::ELightmapUVVersion LayoutVersion;

		int32				NextMeshChartId;
	};


	inline bool FLayoutUV::PositionsMatch(uint32 a, uint32 b) const
	{
		const FVertexInstanceID VertexInstanceIDA(VertexIndexToID[a]);
		const FVertexInstanceID VertexInstanceIDB(VertexIndexToID[b]);
		const FVertexID VertexIDA = MeshDescription.GetVertexInstanceVertex(VertexInstanceIDA);
		const FVertexID VertexIDB = MeshDescription.GetVertexInstanceVertex(VertexInstanceIDB);

		const TVertexAttributeArray<FVector>& VertexPositions = MeshDescription.VertexAttributes().GetAttributes<FVector>(MeshAttribute::Vertex::Position);
		return VertexPositions[VertexIDA].Equals(VertexPositions[VertexIDB], THRESH_POINTS_ARE_SAME);
	}

	inline bool FLayoutUV::NormalsMatch(uint32 a, uint32 b) const
	{
		// If current SrcChannel is out of range of the number of UVs defined by the mesh description, just return true
		// @todo: hopefully remove this check entirely and just ensure that the mesh description matches the inputs
		const uint32 NumUVs = MeshDescription.VertexInstanceAttributes().GetAttributeIndexCount<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);
		if (SrcChannel >= NumUVs)
		{
			ensure(false);	// not expecting it to get here
			return true;
		}

		const FVertexInstanceID VertexInstanceIDA(VertexIndexToID[a]);
		const FVertexInstanceID VertexInstanceIDB(VertexIndexToID[b]);

		const TVertexInstanceAttributeArray<FVector>& VertexNormals = MeshDescription.VertexInstanceAttributes().GetAttributes<FVector>(MeshAttribute::VertexInstance::Normal);
		return VertexNormals[VertexInstanceIDA].Equals(VertexNormals[VertexInstanceIDB], THRESH_NORMALS_ARE_SAME);
	}

	inline bool FLayoutUV::UVsMatch(uint32 a, uint32 b) const
	{
		// If current SrcChannel is out of range of the number of UVs defined by the mesh description, just return true
		const uint32 NumUVs = MeshDescription.VertexInstanceAttributes().GetAttributeIndexCount<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);
		if (SrcChannel >= NumUVs)
		{
			ensure(false);	// not expecting it to get here
			return true;
		}

		const FVertexInstanceID VertexInstanceIDA(VertexIndexToID[a]);
		const FVertexInstanceID VertexInstanceIDB(VertexIndexToID[b]);

		const TVertexInstanceAttributeArray<FVector2D>& VertexUVs = MeshDescription.VertexInstanceAttributes().GetAttributes<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate, SrcChannel);
		return VertexUVs[VertexInstanceIDA].Equals(VertexUVs[VertexInstanceIDB], GetUVEqualityThreshold());
	}

	inline bool FLayoutUV::VertsMatch(uint32 a, uint32 b) const
	{
		return PositionsMatch(a, b) && UVsMatch(a, b);
	}

	// Signed UV area
	inline float FLayoutUV::TriangleUVArea(uint32 Tri) const
	{
		const TVertexInstanceAttributeArray<FVector2D>& VertexUVs = MeshDescription.VertexInstanceAttributes().GetAttributes<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate, SrcChannel);

		FVector2D UVs[3];
		for (int k = 0; k < 3; k++)
		{
			UVs[k] = VertexUVs[FVertexInstanceID(VertexIndexToID[(3 * Tri) + k])];
		}

		FVector2D EdgeUV1 = UVs[1] - UVs[0];
		FVector2D EdgeUV2 = UVs[2] - UVs[0];
		return 0.5f * (EdgeUV1.X * EdgeUV2.Y - EdgeUV1.Y * EdgeUV2.X);
	}

	inline void FLayoutUV::DisconnectChart(FMeshChart& Chart, uint32 Side)
	{
		if (Chart.Join[Side] != -1)
		{
			Charts[Chart.Join[Side]].Join[Side ^ 1] = -1;
			Chart.Join[Side] = -1;
		}
	}
}