// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "Logging/LogMacros.h"
#include "OverlappingCorners.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMeshBuilder, Log, All);

class UObject;
struct FMeshDescription;
struct FMeshBuildSettings;
struct FVertexInstanceID;
struct FMeshReductionSettings;

class FMeshDescriptionHelper
{
public:

	FMeshDescriptionHelper(FMeshBuildSettings* InBuildSettings);

	//Build a render mesh description with the BuildSettings. This will update the InRenderMeshDescription ptr content
	void GetRenderMeshDescription(UObject* Owner, const FMeshDescription& InOriginalMeshDescription, FMeshDescription& OutRenderMeshDescription);

	void ReduceLOD(const FMeshDescription& BaseMesh, FMeshDescription& DestMesh, const struct FMeshReductionSettings& ReductionSettings, const FOverlappingCorners& InOverlappingCorners, float &OutMaxDeviation);

	void FindOverlappingCorners(const FMeshDescription& MeshDescription, float ComparisonThreshold);

	const FOverlappingCorners& GetOverlappingCorners() const { return OverlappingCorners; }

private:

	//////////////////////////////////////////////////////////////////////////
	//PRIVATE function declarations

	//////////////////////////////////////////////////////////////////////////
	//PRIVATE class members

	FMeshBuildSettings* BuildSettings;
	FOverlappingCorners OverlappingCorners;

	
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
