// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FMeshDescription;
struct FRawMesh;
struct FUVMapParameters;
struct FOverlappingCorners;
enum class ELightmapUVVersion : int32;

DECLARE_LOG_CATEGORY_EXTERN(LogMeshDescriptionOperations, Log, All);

//////////////////////////////////////////////////////////////////////////
// Any operations on the mesh description that do not depend on engine module
// should be implement here.

class MESHDESCRIPTIONOPERATIONS_API FMeshDescriptionOperations
{
public:
	enum ETangentOptions
	{
		None = 0,
		BlendOverlappingNormals = 0x1,
		IgnoreDegenerateTriangles = 0x2,
		UseMikkTSpace = 0x4,
	};

	/** Convert this mesh description into the old FRawMesh format. */
	static void ConvertToRawMesh(const FMeshDescription& SourceMeshDescription, FRawMesh& DestinationRawMesh, const TMap<FName, int32>& MaterialMap);

	/** Convert old FRawMesh format to MeshDescription. */
	static void ConvertFromRawMesh(const FRawMesh& SourceRawMesh, FMeshDescription& DestinationMeshDescription, const TMap<int32, FName>& MaterialMap);

	/**
	 * Compute normal, tangent and Bi-Normal for every polygon in the mesh description. (this do not compute Vertex NTBs)
	 * It also remove the degenerated polygon from the mesh description
	 */
	static void CreatePolygonNTB(FMeshDescription& MeshDescription, float ComparisonThreshold);
	
	/** Compute normal, tangent and Bi-Normal(only if bComputeTangent is true) for every vertex in the mesh description. */
	static void CreateNormals(FMeshDescription& MeshDescription, ETangentOptions TangentOptions, bool bComputeTangent);
	
	/** Compute tangent and Bi-Normal using mikkt space for every vertex in the mesh description. */
	static void CreateMikktTangents(FMeshDescription& MeshDescription, ETangentOptions TangentOptions);

	/** Find all overlapping vertex using the threshold in the mesh description. */
	static void FindOverlappingCorners(FOverlappingCorners& OverlappingCorners, const FMeshDescription& MeshDescription, float ComparisonThreshold);

	static void CreateLightMapUVLayout(FMeshDescription& MeshDescription,
		int32 SrcLightmapIndex,
		int32 DstLightmapIndex,
		int32 MinLightmapResolution,
		ELightmapUVVersion LightmapUVVersion,
		const FOverlappingCorners& OverlappingCorners);

	/** Create some UVs from the specified mesh description data. */
	static bool GenerateUniqueUVsForStaticMesh(const FMeshDescription& MeshDescription, int32 TextureResolution, TArray<FVector2D>& OutTexCoords);

	/** Add a UV channel to the MeshDescription. */
	static bool AddUVChannel(FMeshDescription& MeshDescription);

	/** Insert a UV channel at the given index to the MeshDescription. */
	static bool InsertUVChannel(FMeshDescription& MeshDescription, int32 UVChannelIndex);

	/** Remove the UV channel at the given index from the MeshDescription. */
	static bool RemoveUVChannel(FMeshDescription& MeshDescription, int32 UVChannelIndex);

	/** Generate planar UV mapping for the MeshDescription */
	static void GeneratePlanarUV(const FMeshDescription& MeshDescription, const FUVMapParameters& Params, TArray<FVector2D>& OutTexCoords);

	/** Generate cylindrical UV mapping for the MeshDescription */
	static void GenerateCylindricalUV(FMeshDescription& MeshDescription, const FUVMapParameters& Params, TArray<FVector2D>& OutTexCoords);

	/** Generate box UV mapping for the MeshDescription */
	static void GenerateBoxUV(const FMeshDescription& MeshDescription, const FUVMapParameters& Params, TArray<FVector2D>& OutTexCoords);

	static void ConvertHardEdgesToSmoothGroup(const FMeshDescription& SourceMeshDescription, FRawMesh& DestinationRawMesh);

	static void ConvertSmoothGroupToHardEdges(const TArray<uint32>& FaceSmoothingMasks, FMeshDescription& DestinationMeshDescription);
};