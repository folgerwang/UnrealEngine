// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "EditableMesh.h"

namespace Nv
{
	namespace Blast
	{
		class Mesh;
		class FractureTool;
	}
}

namespace physx
{
	class PxVec3;
}


DECLARE_LOG_CATEGORY_EXTERN(LogBlastMeshUtility, Log, All);

class FMeshUtility
{
public:
#if PLATFORM_WINDOWS
	/** Converts entire Editable Mesh to a Blast Mesh format ready for fracturing algorithms */
	static void EditableMeshToBlastMesh(const UEditableMesh* SourceMesh, Nv::Blast::Mesh*& OutBlastMesh);

	/** Converts one Polygon Group of an Editable Mesh to a Blast Mesh format ready for fracturing algorithms, OutBlastMesh could be nullptr if no geometry found */
	static void EditableMeshToBlastMesh(const UEditableMesh* SourceMesh, int32 PolygonGroup, Nv::Blast::Mesh*& OutBlastMesh);
	
	/** Add Blast Mesh data to the provided Geometry Collection - indexed vertices version */
	static void AddBlastMeshToGeometryCollection(Nv::Blast::FractureTool* BlastFractureTool, int32 FracturedChunkIndex, const FString& ParentName, const FTransform& ParentTransform, class UGeometryCollection* FracturedGeometryCollection, TArray<struct FGeneratedFracturedChunk>& GeneratedChunksOut, TArray<int32>& DeletedChunksOut);
#endif

	/** Write fracture hierarchy details to log file */
	static void LogHierarchy(const class UGeometryCollection* GeometryCollection);
	
	static void ValidateGeometryCollectionState(const UGeometryCollection* GeometryCollection);
private:
	static void AddAdditionalAttributesIfRequired(UGeometryCollection& OutGeometryCollection);

#if PLATFORM_WINDOWS
	static FVector CalcChunkDelta(Nv::Blast::Mesh* ChunkMesh, physx::PxVec3 Origin);

	static FVector GetChunkCenter(Nv::Blast::Mesh* ChunkMesh, physx::PxVec3 Origin);
	
    static void GenerateGeometryCollectionFromBlastChunk(Nv::Blast::FractureTool* BlastFractureTool, int32 ChunkIndex, UGeometryCollection* FracturedGeometryCollectionObject, bool IsVisible, struct FGeneratedFracturedChunk& ChunkOut);
#endif

	static int GetMaterialForIndex(const UGeometryCollection* GeometryCollection, int TriangleIndex);

};
