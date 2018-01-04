// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInterface.h"

/**
 * Abstract class which is the base class of all builder.
 * All share code to build some render data should be found inside this class
 */
class MESHBUILDER_API FMeshBuilder
{
public:
	FMeshBuilder();
	
	/**
	 * Build function should be override and is the starting point for all mesh builders
	 */
	virtual bool Build(class UStaticMesh* StaticMesh, const class FStaticMeshLODGroup& LODGroup) =0;

private:

};

class MESHBUILDER_API FMeshDescriptionOperations
{
public:
	/** Compute the normals tangents and binormals for the mesh description. */
	static void ComputeMeshNTBs(class UMeshDescription* MeshDescription, const struct FMeshBuildSettings& BuildSettings);

	/** Convert this mesh description into the old FRawMesh format. */
	static void ConverToRawMesh(const class UMeshDescription* SourceMeshDescription, struct FRawMesh &DestinationRawMesh);

	/** Convert old FRawMesh format to MeshDescription. */
	static void ConverFromRawMesh(const struct FRawMesh &SourceRawMesh, class UMeshDescription* DestinationMeshDescription);

	/** Create some UVs from the specified mesh description data. */
	static bool GenerateUniqueUVsForStaticMesh(const class UMeshDescription* MeshDescription, int32 TextureResolution, TArray<FVector2D>& OutTexCoords);
};

