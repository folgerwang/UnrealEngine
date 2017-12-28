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
	enum ETangentOptions
	{
		None = 0,
		BlendOverlappingNormals = 0x1,
		IgnoreDegenerateTriangles = 0x2,
		UseMikkTSpace = 0x4,
	};

	enum class ELightmapUVVersion : int32
	{
		BitByBit = 0,
		Segments = 1,
		SmallChartPacking = 2,
		Latest = SmallChartPacking
	};

	FMeshDescriptionHelper(FMeshBuildSettings* InBuildSettings, const UMeshDescription* InOriginalMeshDescription);

	//Build a render mesh description with the BuildSettings. This will update the InRenderMeshDescription ptr content
	UMeshDescription* GetRenderMeshDescription(UObject* Owner);

	void ReduceLOD(const UMeshDescription* BaseMesh, UMeshDescription* DestMesh, const struct FMeshReductionSettings& ReductionSettings, const TMultiMap<int32, int32>& InOverlappingCorners);

	//Return true if there is a valid original mesh description, false otherwise(Auto generate LOD).
	bool IsValidOriginalMeshDescription();

	static void FindOverlappingCorners(TMultiMap<int32, int32>& OverlappingCorners, const UMeshDescription* MeshDescription, float ComparisonThreshold);
	void FindOverlappingCorners(const UMeshDescription* MeshDescription, float ComparisonThreshold);

	const TMultiMap<int32, int32>& GetOverlappingCorners() const { return OverlappingCorners; }

	/** Convert this mesh description into the old FRawMesh format*/
	static void ConverToRawMesh(const UMeshDescription* SourceMeshDescription, struct FRawMesh &DestinationRawMesh);
	/** Convert old FRawMesh format to MeshDescription*/
	static void ConverFromRawMesh(const struct FRawMesh &SourceRawMesh, UMeshDescription* DestinationMeshDescription);

private:

	//////////////////////////////////////////////////////////////////////////
	//PRIVATE function declarations

	static void ConvertHardEdgesToSmoothGroup(const UMeshDescription* SourceMeshDescription, struct FRawMesh &DestinationRawMesh);
	static void ConvertSmoothGroupToHardEdges(const struct FRawMesh &SourceRawMesh, UMeshDescription* DestinationMeshDescription);

	static void CreateNormals(UMeshDescription* MeshDescription, ETangentOptions TangentOptions, bool bComputeTangent);
	static void CreateMikktTangents(UMeshDescription* MeshDescription, ETangentOptions TangentOptions);
	static void CreatePolygonNTB(UMeshDescription* MeshDescription, float ComparisonThreshold);

	//////////////////////////////////////////////////////////////////////////
	//PRIVATE class members

	const UMeshDescription* OriginalMeshDescription;
	FMeshBuildSettings *BuildSettings;
	TMultiMap<int32, int32> OverlappingCorners;

	
	//////////////////////////////////////////////////////////////////////////
	//INLINE small helper use to optimize search and compare

	/** Helper struct for building acceleration structures. */
	struct FIndexAndZ
	{
		float Z;
		int32 Index;
		const FVector *OriginalVector;

		/** Default constructor. */
		FIndexAndZ() {}

		/** Initialization constructor. */
		FIndexAndZ(int32 InIndex, const FVector& V)
		{
			Z = 0.30f * V.X + 0.33f * V.Y + 0.37f * V.Z;
			Index = InIndex;
			OriginalVector = &V;
		}
	};

	/** Sorting function for vertex Z/index pairs. */
	struct FCompareIndexAndZ
	{
		FORCEINLINE bool operator()(FIndexAndZ const& A, FIndexAndZ const& B) const { return A.Z < B.Z; }
	};

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
