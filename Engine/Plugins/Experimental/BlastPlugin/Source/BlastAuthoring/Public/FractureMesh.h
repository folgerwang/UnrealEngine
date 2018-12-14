// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "EditableMesh.h"
#include "MeshFractureSettings.h"
#include "NvBlastExtAuthoringFractureTool.h"
#include "GeneratedFracturedChunk.h"
#include "FractureMesh.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogFractureMesh, Log, All);

/** Random Generator class implementation required by Blast, based on Nv::Blast::RandomGeneratorBase */
class FractureRandomGenerator : public Nv::Blast::RandomGeneratorBase
{
public:
	FractureRandomGenerator(int32_t RandomSeed)
	{
		seed(RandomSeed);
	};

	virtual ~FractureRandomGenerator() {};

	virtual float getRandomValue() override
	{
		return RandStream.GetFraction();
	}
	virtual void seed(int32_t RandomSeed) override
	{
		RandStream.Initialize(RandomSeed);
	}

private:
	FRandomStream RandStream;

};

/** Performs Voronoi or Slicing fracture of the currently selected mesh */
UCLASS()
class BLASTAUTHORING_API UFractureMesh : public UObject
{
	GENERATED_BODY()
		
public:
	/** Performs fracturing of an Editable Mesh */
	void FractureMesh(const UEditableMesh* SourceMesh, const FString& ParentName, const UMeshFractureSettings& FractureSettings, int32 FracturedChunkIndex, const FTransform& Transform, int RandomSeed, UGeometryCollection* FracturedGeometryCollection, TArray<FGeneratedFracturedChunk>& GeneratedChunksOut, TArray<int32>& DeletedChunksOut);

	/** ensure node hierarchy is setup appropriately */
	void FixupHierarchy(int32 FracturedChunkIndex, class UGeometryCollection* GeometryCollectionObject, FGeneratedFracturedChunk& GeneratedChunk, const FString& Name);

private:
	const float MagicScaling = 100.0f;

#if PLATFORM_WINDOWS
	/** Generate geometry for all the bones of the geometry collection */
	void GenerateChunkMeshes(Nv::Blast::FractureTool* BlastFractureTool, const UMeshFractureSettings& FractureSettings, int32 FracturedChunkIndex, const FString& ParentName, const FTransform& ParentTransform, Nv::Blast::Mesh* BlastMesh, UGeometryCollection* FracturedGeometryCollection, TArray<FGeneratedFracturedChunk>& GeneratedChunksOut, TArray<int32>& DeletedChunksOut);

	/** Log some stats */
	void LogStatsAndTimings(const Nv::Blast::Mesh* BlastMesh, const Nv::Blast::FractureTool* BlastFractureTool, const FTransform& Transform, const UMeshFractureSettings& FractureSettings, float ProcessingTime);
#endif

	/** Get raw bitmap data from texture */
	void ExtractDataFromTexture(const TWeakObjectPtr<UTexture> SourceTexture, TArray<uint8>& RawDataOut, int32& WidthOut, int32& HeightOut);

#if PLATFORM_WINDOWS
	/** Draw debug render of exploded shape, i.e. all fracture chunks */
	void RenderDebugGraphics(Nv::Blast::FractureTool* BlastFractureTool, const UMeshFractureSettings& FractureSettings, const FTransform& Transform);

	/** Draws all edges of Blast Mesh as debug lines */
	void DrawDebugBlastMesh(const Nv::Blast::Mesh* ChunkMesh, int ChunkIndex, float ExplodedViewAmount, const FTransform& Transform);
#endif

};
