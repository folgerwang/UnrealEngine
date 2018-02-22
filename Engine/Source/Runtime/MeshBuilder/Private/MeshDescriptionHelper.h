// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMeshBuilder, Log, All);

class UObject;
class UMeshDescription;
struct FMeshBuildSettings;
struct FVertexInstanceID;

enum
{
	//Remove the _MD when FRawMesh will be remove
	MAX_MESH_TEXTURE_COORDS_MD = 8,
};

class FMeshDescriptionHelper
{
public:

	FMeshDescriptionHelper(FMeshBuildSettings* InBuildSettings, const UMeshDescription* InOriginalMeshDescription);

	//Build a render mesh description with the BuildSettings. This will update the InRenderMeshDescription ptr content
	UMeshDescription* GetRenderMeshDescription(UObject* Owner);

	void ReduceLOD(const UMeshDescription* BaseMesh, UMeshDescription* DestMesh, const struct FMeshReductionSettings& ReductionSettings, const TMultiMap<int32, int32>& InOverlappingCorners);

	//Return true if there is a valid original mesh description, false otherwise(Auto generate LOD).
	bool IsValidOriginalMeshDescription();

	void FindOverlappingCorners(const UMeshDescription* MeshDescription, float ComparisonThreshold);

	const TMultiMap<int32, int32>& GetOverlappingCorners() const { return OverlappingCorners; }

private:

	//////////////////////////////////////////////////////////////////////////
	//PRIVATE function declarations

	//////////////////////////////////////////////////////////////////////////
	//PRIVATE class members

	const UMeshDescription* OriginalMeshDescription;
	FMeshBuildSettings *BuildSettings;
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
