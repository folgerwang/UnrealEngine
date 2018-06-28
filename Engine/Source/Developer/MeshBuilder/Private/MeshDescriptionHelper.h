// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMeshBuilder, Log, All);

class UObject;
class FMeshDescription;
struct FMeshBuildSettings;
struct FVertexInstanceID;
struct FMeshReductionSettings;

enum
{
	//Remove the _MD when FRawMesh will be remove
	MAX_MESH_TEXTURE_COORDS_MD = 8,
};

class FMeshDescriptionHelper
{
public:

	FMeshDescriptionHelper(FMeshBuildSettings* InBuildSettings);

	//Build a render mesh description with the BuildSettings. This will update the InRenderMeshDescription ptr content
	void GetRenderMeshDescription(UObject* Owner, const FMeshDescription& InOriginalMeshDescription, FMeshDescription& OutRenderMeshDescription);

	void ReduceLOD(const FMeshDescription& BaseMesh, FMeshDescription& DestMesh, const struct FMeshReductionSettings& ReductionSettings, const TMultiMap<int32, int32>& InOverlappingCorners, float &OutMaxDeviation);

	void FindOverlappingCorners(const FMeshDescription& MeshDescription, float ComparisonThreshold);

	const TMultiMap<int32, int32>& GetOverlappingCorners() const { return OverlappingCorners; }

private:

	//////////////////////////////////////////////////////////////////////////
	//PRIVATE function declarations

	//////////////////////////////////////////////////////////////////////////
	//PRIVATE class members

	FMeshBuildSettings* BuildSettings;
	TMultiMap<int32, int32> OverlappingCorners;

	
	//////////////////////////////////////////////////////////////////////////
	//INLINE small helper use to optimize search and compare

	/**
	* Smoothing group interpretation helper structure.
	*/
	struct FFanFace
	{
		int32 FaceIndex;
		int32 LinkedVertexIndex;
		bool bFilled;
		bool bBlendTangents;
		bool bBlendNormals;
	};

};
