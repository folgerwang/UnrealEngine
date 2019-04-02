// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCacheMeshData.h"
#include "GeometryCacheModule.h"


DECLARE_CYCLE_STAT(TEXT("Deserialize Vertices"), STAT_DeserializeVertices, STATGROUP_GeometryCache);
DECLARE_CYCLE_STAT(TEXT("Deserialize Indices"), STAT_DeserializeIndices, STATGROUP_GeometryCache);
DECLARE_CYCLE_STAT(TEXT("Deserialize Schnabbels"), STAT_DeserializeSchnabbels, STATGROUP_GeometryCache);

FArchive& operator<<(FArchive& Ar, FGeometryCacheMeshData& Mesh)
{
	Ar.UsingCustomVersion(FGeometryObjectVersion::GUID);

	int32 NumVertices = 0;
		
	if (Ar.IsSaving())
	{
		NumVertices = Mesh.Positions.Num();		
		checkf(Mesh.VertexInfo.bHasMotionVectors == false || Mesh.MotionVectors.Num() == Mesh.Positions.Num(),
			TEXT("Mesh is flagged as having motion vectors but the number of motion vectors does not match the number of vertices"));
	}

	// Serialize metadata first so we can use it later on
	{
		SCOPE_CYCLE_COUNTER(STAT_DeserializeSchnabbels);
		Ar << Mesh.BoundingBox;
		Ar << Mesh.BatchesInfo;
		Ar << Mesh.VertexInfo;
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_DeserializeVertices);

		Ar << NumVertices;
		if (Ar.IsLoading())
		{
			Mesh.Positions.SetNumUninitialized(NumVertices);
			Mesh.TextureCoordinates.SetNumUninitialized(NumVertices);
			Mesh.TangentsX.SetNumUninitialized(NumVertices);
			Mesh.TangentsZ.SetNumUninitialized(NumVertices);
			Mesh.Colors.SetNumUninitialized(NumVertices);

			if (Mesh.VertexInfo.bHasMotionVectors)
			{
				Mesh.MotionVectors.SetNumUninitialized(NumVertices);
			}
			else
			{
				Mesh.MotionVectors.Empty();
			}
		}

		if (Mesh.Positions.Num() > 0)
		{				
			Ar.Serialize(&Mesh.Positions[0], Mesh.Positions.Num() * Mesh.Positions.GetTypeSize());
			Ar.Serialize(&Mesh.TextureCoordinates[0], Mesh.TextureCoordinates.Num() * Mesh.TextureCoordinates.GetTypeSize());
			Ar.Serialize(&Mesh.TangentsX[0], Mesh.TangentsX.Num() * Mesh.TangentsX.GetTypeSize());
			Ar.Serialize(&Mesh.TangentsZ[0], Mesh.TangentsZ.Num() * Mesh.TangentsZ.GetTypeSize());
			Ar.Serialize(&Mesh.Colors[0], Mesh.Colors.Num() * Mesh.Colors.GetTypeSize());/**/
		  
			if (Mesh.VertexInfo.bHasMotionVectors)
			{
				Ar.Serialize(&Mesh.MotionVectors[0], Mesh.MotionVectors.Num()*Mesh.MotionVectors.GetTypeSize());
			}			
		}
	}

	{
		// Serializing explicitly here instead of doing Ar << Mesh.Indices
		// makes it about 8 times faster halving the deserialization time of the test mesh
		// so I guess it's worth it here for the little effort it takes
		SCOPE_CYCLE_COUNTER(STAT_DeserializeIndices);
		int32 NumIndices = Mesh.Indices.Num();
		Ar << NumIndices;

		if (Ar.IsLoading())
		{
			Mesh.Indices.Empty(NumIndices);
			Mesh.Indices.AddUninitialized(NumIndices);
		}

		if (Mesh.Indices.Num() > 0)
		{
			Ar.Serialize(&Mesh.Indices[0], Mesh.Indices.Num() * sizeof(uint32));
		}
	}

	return Ar;
}
