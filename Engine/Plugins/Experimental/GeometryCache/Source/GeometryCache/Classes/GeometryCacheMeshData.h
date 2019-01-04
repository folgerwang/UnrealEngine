// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ProfilingDebugging/ResourceSize.h"
#include "DynamicMeshBuilder.h"
#include "UObject/GeometryObjectVersion.h"
#include "GeometryCacheMeshData.generated.h"

/** Stores per-batch data used for rendering */
USTRUCT()
struct FGeometryCacheMeshBatchInfo
{
	GENERATED_USTRUCT_BODY()

	/** Starting index into IndexBuffer to draw from */
	uint32 StartIndex;
	/** Total number of Triangles to draw */
	uint32 NumTriangles;
	/** Index to Material used to draw this batch */
	uint32 MaterialIndex;

	friend FArchive& operator<<(FArchive& Ar, FGeometryCacheMeshBatchInfo& Mesh)
	{
		Ar << Mesh.StartIndex;
		Ar << Mesh.NumTriangles;
		Ar << Mesh.MaterialIndex;
		// Empty batches?!?
		check(Mesh.NumTriangles > 0);
		return Ar;
	}	
};

/** Stores info on the attributes of a vertex in a mesh */
USTRUCT()
struct FGeometryCacheVertexInfo
{
	GENERATED_USTRUCT_BODY()

	/** Info on which attributes are present or valid */
	bool bHasTangentX;
	bool bHasTangentZ;
	bool bHasUV0;
	bool bHasColor0;
	bool bHasMotionVectors;

	bool bConstantUV0;
	bool bConstantColor0;
	bool bConstantIndices;

	FGeometryCacheVertexInfo()
	{
		bHasTangentX = false;
		bHasTangentZ = false;
		bHasUV0 = false;
		bHasColor0 = false;
		bHasMotionVectors = false;
		bConstantUV0 = false;
		bConstantColor0 = false;
		bConstantIndices = false;
	}

	FGeometryCacheVertexInfo(
		bool bSetTangentX,
		bool bSetTangentZ,
		bool bSetUV0,
		bool bSetColor0,
		bool bSetMotionVectors0
	) : bHasTangentX(bSetTangentX),
		bHasTangentZ(bSetTangentZ),
		bHasUV0(bSetUV0),
		bHasColor0(bSetColor0),
		bHasMotionVectors(bSetMotionVectors0)
	{}

	friend FArchive& operator<<(FArchive& Ar, FGeometryCacheVertexInfo& Mesh)
	{
		Ar << Mesh.bHasTangentX;
		Ar << Mesh.bHasTangentZ;
		Ar << Mesh.bHasUV0;
		Ar << Mesh.bHasColor0;
		Ar << Mesh.bHasMotionVectors;

		Ar << Mesh.bConstantUV0;
		Ar << Mesh.bConstantColor0;
		Ar << Mesh.bConstantIndices;
		
		return Ar;
	}
};

/** Stores per Track/Mesh data used for rendering*/
USTRUCT()
struct GEOMETRYCACHE_API FGeometryCacheMeshData
{
	GENERATED_USTRUCT_BODY()

	FGeometryCacheMeshData() {}
	~FGeometryCacheMeshData()
	{
		Positions.Empty();
		TextureCoordinates.Empty();
		TangentsX.Empty();
		TangentsZ.Empty();
		Colors.Empty();

		MotionVectors.Empty();
		BatchesInfo.Empty();
		BoundingBox.Init();
	}
	
	/** Draw-able vertex data */
	TArray<FVector> Positions;
	TArray<FVector2D> TextureCoordinates;
	TArray<FPackedNormal> TangentsX;
	TArray<FPackedNormal> TangentsZ;
	TArray<FColor> Colors;

	/** Motion vector for each vertex. The number of motion vectors should be zero (= no motion vectors) or identical to the number of vertices. */
	TArray<FVector> MotionVectors;
	/** Array of per-batch info structs*/
	TArray<FGeometryCacheMeshBatchInfo> BatchesInfo;
	/** Bounding box for this sample in the track */
	FBox BoundingBox;
	/** Indices for this sample, used for drawing the mesh */
	TArray<uint32> Indices;
	/** Info on the vertex attributes */
	FGeometryCacheVertexInfo VertexInfo;
		
	/** Serialization for FVertexAnimationSample. */
	friend FArchive& operator<<(FArchive& Ar, FGeometryCacheMeshData& Mesh);

	/** Serialization for const FVertexAnimationSample. */
	friend FArchive& operator<<(FArchive& Ar, const FGeometryCacheMeshData& Mesh)
		{
		check(Ar.IsSaving());
		return (Ar << const_cast<FGeometryCacheMeshData&>(Mesh));
	}

	void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) const
	{
		// Calculate resource size according to what is actually serialized
		CumulativeResourceSize.AddUnknownMemoryBytes(Positions.Num() * sizeof(FVector));
		CumulativeResourceSize.AddUnknownMemoryBytes(TextureCoordinates.Num() * sizeof(FVector2D));
		CumulativeResourceSize.AddUnknownMemoryBytes(TangentsX.Num() * sizeof(FPackedNormal));
		CumulativeResourceSize.AddUnknownMemoryBytes(TangentsZ.Num() * sizeof(FPackedNormal));
		CumulativeResourceSize.AddUnknownMemoryBytes(Colors.Num() * sizeof(FColor));


		CumulativeResourceSize.AddUnknownMemoryBytes(MotionVectors.Num() * sizeof(FVector));
		CumulativeResourceSize.AddUnknownMemoryBytes(BatchesInfo.Num() * sizeof(FGeometryCacheMeshBatchInfo));

		CumulativeResourceSize.AddUnknownMemoryBytes(sizeof(Positions));
		CumulativeResourceSize.AddUnknownMemoryBytes(sizeof(TextureCoordinates));
		CumulativeResourceSize.AddUnknownMemoryBytes(sizeof(TangentsX));
		CumulativeResourceSize.AddUnknownMemoryBytes(sizeof(TangentsZ));
		CumulativeResourceSize.AddUnknownMemoryBytes(sizeof(Colors));

		CumulativeResourceSize.AddUnknownMemoryBytes(sizeof(BatchesInfo));
		CumulativeResourceSize.AddUnknownMemoryBytes(sizeof(BoundingBox));
		CumulativeResourceSize.AddUnknownMemoryBytes(Indices.Num() * sizeof(uint32));
		CumulativeResourceSize.AddUnknownMemoryBytes(sizeof(Indices));
		CumulativeResourceSize.AddUnknownMemoryBytes(sizeof(VertexInfo));
	}
};
