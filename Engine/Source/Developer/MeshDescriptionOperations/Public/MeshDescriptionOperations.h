// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FMeshDescription;
struct FRawMesh;
struct FUVMapParameters;
struct FOverlappingCorners;
enum class ELightmapUVVersion : int32;
struct FPolygonGroupID;
struct FVertexID;

DECLARE_LOG_CATEGORY_EXTERN(LogMeshDescriptionOperations, Log, All);

typedef TMap<FPolygonGroupID, FPolygonGroupID> PolygonGroupMap;

DECLARE_DELEGATE_ThreeParams(FAppendPolygonGroupsDelegate, const FMeshDescription& /*SourceMesh*/, FMeshDescription& /*TargetMesh*/, PolygonGroupMap& /*RemapPolygonGroup*/)

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

	struct FAppendSettings
	{
		FAppendSettings()
			: bMergeVertexColor(true)
			, MergedAssetPivot(0.0f, 0.0f, 0.0f)
		{}
		FAppendPolygonGroupsDelegate PolygonGroupsDelegate;
		bool bMergeVertexColor;
		FVector MergedAssetPivot;
		TOptional<FTransform> MeshTransform; // Apply a transformation on source mesh (see MeshTransform)
	};

	static void AppendMeshDescription(const FMeshDescription& SourceMesh, FMeshDescription& TargetMesh, FAppendSettings& AppendSettings);

	/*
	 * Check if all normals and tangents are valid, if not recompute them
	 */
	static void RecomputeNormalsAndTangentsIfNeeded(FMeshDescription& MeshDescription, ETangentOptions TangentOptions, bool bUseMikkTSpace, bool bForceRecomputeNormals = false, bool bForceRecomputeTangents = false);

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
	
	/** Find all charts in the mesh description. */
	static int32 GetUVChartCount(FMeshDescription& MeshDescription, int32 SrcLightmapIndex, ELightmapUVVersion LightmapUVVersion, const FOverlappingCorners& OverlappingCorners);

	/**
	 * Find and pack UV charts for lightmap.
	 * The packing algorithm uses a rasterization method, hence the resolution parameter.
	 *
	 * If the given minimum resolution is not enough to handle all the charts, generation will fail.
	 *
	 * @param MeshDescription        Edited mesh
	 * @param SrcLightmapIndex       index of the source UV channel
	 * @param DstLightmapIndex       index of the destination UV channel
	 * @param MinLightmapResolution  Minimum resolution used for the packing
	 * @param LightmapUVVersion      Algorithm version
	 * @param OverlappingCorners     Overlapping corners of the given mesh
	 * @return                       UV layout correctly generated
	 */
	static bool CreateLightMapUVLayout(FMeshDescription& MeshDescription,
		int32 SrcLightmapIndex,
		int32 DstLightmapIndex,
		int32 MinLightmapResolution,
		ELightmapUVVersion LightmapUVVersion,
		const FOverlappingCorners& OverlappingCorners);

	/** Create some UVs from the specified mesh description data. */
	static bool GenerateUniqueUVsForStaticMesh(const FMeshDescription& MeshDescription, int32 TextureResolution, bool bMergeIdenticalMaterials, TArray<FVector2D>& OutTexCoords);

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

	static void RemapPolygonGroups(FMeshDescription& MeshDescription, TMap<FPolygonGroupID, FPolygonGroupID>& Remap);

	/*
	 * Move some polygon to a new PolygonGroup(section)
	 * SectionIndex: The target section we want to assign the polygon. See bRemoveEmptyPolygonGroup to know how its used
	 * TriangleIndexStart: The triangle index is compute as follow: foreach polygon {TriangleIndex += Polygon->NumberTriangles}
	 * TriangleIndexEnd: The triangle index is compute as follow: foreach polygon {TriangleIndex += Polygon->NumberTriangles}
	 * bRemoveEmptyPolygonGroup: If true, any polygonGroup that is empty after moving a polygon will be delete.
	 *                           This parameter impact how SectionIndex is use
	 *                           If param is true  : PolygonGroupTargetID.GetValue() do not necessary equal SectionIndex in case there is less sections then SectionIndex
	 *                           If param is false : PolygonGroupTargetID.GetValue() equal SectionIndex, we will add all necessary missing PolygonGroupID (this can generate empty PolygonGroupID)
	 */
	static void SwapPolygonPolygonGroup(FMeshDescription& MeshDescription, int32 SectionIndex, int32 TriangleIndexStart, int32 TriangleIndexEnd, bool bRemoveEmptyPolygonGroup);

	static void ConvertHardEdgesToSmoothGroup(const FMeshDescription& SourceMeshDescription, TArray<uint32>& FaceSmoothingMasks);

	static void ConvertSmoothGroupToHardEdges(const TArray<uint32>& FaceSmoothingMasks, FMeshDescription& DestinationMeshDescription);

	static bool HasVertexColor(const FMeshDescription& MeshDescription);

	static void BuildWeldedVertexIDRemap(const FMeshDescription& MeshDescription, const float WeldingThreshold, TMap<FVertexID, FVertexID>& OutVertexIDRemap);
};