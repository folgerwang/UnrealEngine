// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/MaterialMerging.h"
#include "GameFramework/Actor.h"
#include "MeshMerging.generated.h"

/** The importance of a mesh feature when automatically generating mesh LODs. */
UENUM()
namespace EMeshFeatureImportance
{
	enum Type
	{
		Off,
		Lowest,
		Low,
		Normal,
		High,
		Highest
	};
}


/** Enum specifying the reduction type to use when simplifying static meshes with the engines internal tool */
UENUM()
enum class EStaticMeshReductionTerimationCriterion : uint8
{
	Triangles,
	Vertices,
	Any
};

/** Settings used to reduce a mesh. */
USTRUCT(Blueprintable)
struct FMeshReductionSettings
{
	GENERATED_USTRUCT_BODY()

	/** Percentage of triangles to keep. 1.0 = no reduction, 0.0 = no triangles. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	float PercentTriangles;

	/** Percentage of vertices to keep. 1.0 = no reduction, 0.0 = no vertices. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	float PercentVertices;

	/** The maximum distance in object space by which the reduced mesh may deviate from the original mesh. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	float MaxDeviation;

	/** The amount of error in pixels allowed for this LOD. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	float PixelError;

	/** Threshold in object space at which vertices are welded together. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	float WeldingThreshold;

	/** Angle at which a hard edge is introduced between faces. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	float HardAngleThreshold;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	int32 BaseLODModel;

	/** Higher values minimize change to border edges. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	TEnumAsByte<EMeshFeatureImportance::Type> SilhouetteImportance;

	/** Higher values reduce texture stretching. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	TEnumAsByte<EMeshFeatureImportance::Type> TextureImportance;

	/** Higher values try to preserve normals better. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	TEnumAsByte<EMeshFeatureImportance::Type> ShadingImportance;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	uint8 bRecalculateNormals:1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	uint8 bGenerateUniqueLightmapUVs:1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	uint8 bKeepSymmetry:1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	uint8 bVisibilityAided:1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	uint8 bCullOccluded:1;

	/** The method to use when optimizing static mesh LODs */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	EStaticMeshReductionTerimationCriterion TerminationCriterion;

	/** Higher values generates fewer samples*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	TEnumAsByte<EMeshFeatureImportance::Type> VisibilityAggressiveness;

	/** Higher values minimize change to vertex color data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	TEnumAsByte<EMeshFeatureImportance::Type> VertexColorImportance;

	/** Default settings. */
	FMeshReductionSettings()
		: PercentTriangles(1.0f)
		, PercentVertices(1.0f)
		, MaxDeviation(0.0f)
		, PixelError(8.0f)
		, WeldingThreshold(0.0f)
		, HardAngleThreshold(80.0f)
		, BaseLODModel(0)
		, SilhouetteImportance(EMeshFeatureImportance::Normal)
		, TextureImportance(EMeshFeatureImportance::Normal)
		, ShadingImportance(EMeshFeatureImportance::Normal)
		, bRecalculateNormals(false)
		, bGenerateUniqueLightmapUVs(false)
		, bKeepSymmetry(false)
		, bVisibilityAided(false)
		, bCullOccluded(false)
		, TerminationCriterion(EStaticMeshReductionTerimationCriterion::Triangles)
		, VisibilityAggressiveness(EMeshFeatureImportance::Lowest)
		, VertexColorImportance(EMeshFeatureImportance::Off)
	{
	}

	FMeshReductionSettings(const FMeshReductionSettings& Other)
		: PercentTriangles(Other.PercentTriangles)
		, PercentVertices(Other.PercentVertices)
		, MaxDeviation(Other.MaxDeviation)
		, PixelError(Other.PixelError)
		, WeldingThreshold(Other.WeldingThreshold)
		, HardAngleThreshold(Other.HardAngleThreshold)
		, BaseLODModel(Other.BaseLODModel)
		, SilhouetteImportance(Other.SilhouetteImportance)
		, TextureImportance(Other.TextureImportance)
		, ShadingImportance(Other.ShadingImportance)
		, bRecalculateNormals(Other.bRecalculateNormals)
		, bGenerateUniqueLightmapUVs(Other.bGenerateUniqueLightmapUVs)
		, bKeepSymmetry(Other.bKeepSymmetry)
		, bVisibilityAided(Other.bVisibilityAided)
		, bCullOccluded(Other.bCullOccluded)
		, TerminationCriterion(Other.TerminationCriterion)
		, VisibilityAggressiveness(Other.VisibilityAggressiveness)
		, VertexColorImportance(Other.VertexColorImportance)
	{
	}

	/** Equality operator. */
	bool operator==(const FMeshReductionSettings& Other) const
	{
		return
			TerminationCriterion == Other.TerminationCriterion
			&& PercentVertices == Other.PercentVertices
			&& PercentTriangles == Other.PercentTriangles
			&& MaxDeviation == Other.MaxDeviation
			&& PixelError == Other.PixelError
			&& WeldingThreshold == Other.WeldingThreshold
			&& HardAngleThreshold == Other.HardAngleThreshold
			&& SilhouetteImportance == Other.SilhouetteImportance
			&& TextureImportance == Other.TextureImportance
			&& ShadingImportance == Other.ShadingImportance
			&& bRecalculateNormals == Other.bRecalculateNormals
			&& BaseLODModel == Other.BaseLODModel
			&& bGenerateUniqueLightmapUVs == Other.bGenerateUniqueLightmapUVs
			&& bKeepSymmetry == Other.bKeepSymmetry
			&& bVisibilityAided == Other.bVisibilityAided
			&& bCullOccluded == Other.bCullOccluded
			&& VisibilityAggressiveness == Other.VisibilityAggressiveness
			&& VertexColorImportance == Other.VertexColorImportance;
	}

	/** Inequality. */
	bool operator!=(const FMeshReductionSettings& Other) const
	{
		return !(*this == Other);
	}
};

UENUM()
namespace ELandscapeCullingPrecision
{
	enum Type
	{
		High = 0 UMETA(DisplayName = "High memory intensity and computation time"),
		Medium = 1 UMETA(DisplayName = "Medium memory intensity and computation time"),
		Low = 2 UMETA(DisplayName = "Low memory intensity and computation time")
	};
}

UENUM()
namespace EProxyNormalComputationMethod
{
	enum Type
	{
		AngleWeighted = 0 UMETA(DisplayName = "Angle Weighted"),
		AreaWeighted = 1 UMETA(DisplayName = "Area  Weighted"),
		EqualWeighted = 2 UMETA(DisplayName = "Equal Weighted")
	};
}


USTRUCT(Blueprintable)
struct FMeshProxySettings
{
	GENERATED_USTRUCT_BODY()
	/** Screen size of the resulting proxy mesh in pixels*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings, meta = (ClampMin = "1", ClampMax = "1200", UIMin = "1", UIMax = "1200"))
	int32 ScreenSize;

	/** Override when converting multiple meshes for proxy LOD merging. Warning, large geometry with small sampling has very high memory costs*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, AdvancedDisplay, Category = ProxySettings, meta = (EditCondition = "bOverrideVoxelSize", ClampMin = "0.1", DisplayName = "Overide Spatial Sampling Distance"))
	float VoxelSize;

	/** Material simplification */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	FMaterialProxySettings MaterialSettings;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	int32 TextureWidth_DEPRECATED;
	UPROPERTY()
	int32 TextureHeight_DEPRECATED;

	UPROPERTY()
	uint8 bExportNormalMap_DEPRECATED:1;

	UPROPERTY()
	uint8 bExportMetallicMap_DEPRECATED:1;

	UPROPERTY()
	uint8 bExportRoughnessMap_DEPRECATED:1;

	UPROPERTY()
	uint8 bExportSpecularMap_DEPRECATED:1;

	UPROPERTY()
	uint8 bBakeVertexData_DEPRECATED:1;
#endif

	/** Distance at which meshes should be merged together, this can close gaps like doors and windows in distant geometry */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	float MergeDistance;

	/** Base color assigned to LOD geometry that can't be associated with the source geometry: e.g. doors and windows that have been closed by the Merge Distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings, meta = (DisplayName = "Unresolved Geometry Color"))
	FColor UnresolvedGeometryColor;

	/** Override search distance used when discovering texture values for simplified geometry. Useful when non-zero Merge Distance setting generates new geometry in concave corners.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings, meta = (EditCondition = "bOverrideTransferDistance", DisplayName = "Transfer Distance Override", ClampMin = 0))
	float MaxRayCastDist;

	/** Angle at which a hard edge is introduced between faces */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings, meta = (EditCondition = "bUseHardAngleThreshold", DisplayName = "Hard Edge Angle", ClampMin = 0, ClampMax = 180))
	float HardAngleThreshold;

	/** Lightmap resolution */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings, meta = (ClampMin = 32, ClampMax = 4096, EditCondition = "!bComputeLightMapResolution", DisplayAfter="NormalCalculationMethod"))
	int32 LightMapResolution;

	/** Controls the method used to calculate the normal for the simplified geometry */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings, meta = (DisplayName = "Normal Calculation Method"))
	TEnumAsByte<EProxyNormalComputationMethod::Type> NormalCalculationMethod;

	/** Level of detail of the landscape that should be used for the culling */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = LandscapeCulling, meta = (EditCondition="bUseLandscapeCulling", DisplayAfter="bUseLandscapeCulling"))
	TEnumAsByte<ELandscapeCullingPrecision::Type> LandscapeCullingPrecision;

	/** Determines whether or not the correct LOD models should be calculated given the source meshes and transition size */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings, meta=(DisplayAfter="ScreenSize"))
	uint8 bCalculateCorrectLODModel:1;

	/** If true, Spatial Sampling Distance will not be automatically computed based on geometry and you must set it directly */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, AdvancedDisplay, Category = ProxySettings, meta = (InlineEditConditionToggle))
	uint8 bOverrideVoxelSize : 1;

	/** Enable an override for material transfer distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MaxRayCastDist, meta = (InlineEditConditionToggle))
	uint8 bOverrideTransferDistance:1;

	/** Enable the use of hard angle based vertex splitting */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = HardAngleThreshold, meta = (InlineEditConditionToggle))
	uint8 bUseHardAngleThreshold:1;

	/** If ticked will compute the lightmap resolution by summing the dimensions for each mesh included for merging */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	uint8 bComputeLightMapResolution:1;

	/** Whether Simplygon should recalculate normals, otherwise the normals channel will be sampled from the original mesh */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	uint8 bRecalculateNormals:1;

	/** Whether or not to use available landscape geometry to cull away invisible triangles */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = LandscapeCulling)
	uint8 bUseLandscapeCulling:1;

	/** Whether to allow adjacency buffers for tessellation in the merged mesh */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	uint8 bAllowAdjacency:1;

	/** Whether to allow distance field to be computed for this mesh. Disable this to save memory if the merged mesh will only be rendered in the distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	uint8 bAllowDistanceField:1;

	/** Whether to attempt to re-use the source mesh's lightmap UVs when baking the material or always generate a new set. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	uint8 bReuseMeshLightmapUVs:1;

	/** Whether to generate collision for the merged mesh */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	uint8 bCreateCollision:1;

	/** Whether to allow vertex colors saved in the merged mesh */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	uint8 bAllowVertexColors:1;

	/** Whether to generate lightmap uvs for the merged mesh */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	uint8 bGenerateLightmapUVs:1;

	/** Default settings. */
	FMeshProxySettings()
		: ScreenSize(300)
		, VoxelSize(3.f)
#if WITH_EDITORONLY_DATA
		, TextureWidth_DEPRECATED(512)
		, TextureHeight_DEPRECATED(512)
		, bExportNormalMap_DEPRECATED(true)
		, bExportMetallicMap_DEPRECATED(false)
		, bExportRoughnessMap_DEPRECATED(false)
		, bExportSpecularMap_DEPRECATED(false)
		, bBakeVertexData_DEPRECATED(false)
#endif
		, MergeDistance(0)
		, UnresolvedGeometryColor(FColor::Black)
		, MaxRayCastDist(20)
		, HardAngleThreshold(130.f)
		, LightMapResolution(256)
		, NormalCalculationMethod(EProxyNormalComputationMethod::AngleWeighted)
		, LandscapeCullingPrecision(ELandscapeCullingPrecision::Medium)
		, bCalculateCorrectLODModel(false)
		, bOverrideVoxelSize(false)
		, bOverrideTransferDistance(false)
		, bUseHardAngleThreshold(false)
		, bComputeLightMapResolution(false)
		, bRecalculateNormals(true)
		, bUseLandscapeCulling(false)
		, bAllowAdjacency(false)
		, bAllowDistanceField(false)
		, bReuseMeshLightmapUVs(true)
		, bCreateCollision(true)
		, bAllowVertexColors(false)
		, bGenerateLightmapUVs(false)
	{
		MaterialSettings.MaterialMergeType = EMaterialMergeType::MaterialMergeType_Simplygon;
	}

	/** Equality operator. */
	bool operator==(const FMeshProxySettings& Other) const
	{
		return ScreenSize == Other.ScreenSize
			&& MaterialSettings == Other.MaterialSettings
			&& bRecalculateNormals == Other.bRecalculateNormals
			&& bOverrideTransferDistance == Other.bOverrideTransferDistance
			&& MaxRayCastDist == Other.MaxRayCastDist
			&& bUseHardAngleThreshold == Other.bUseHardAngleThreshold
			&& HardAngleThreshold == Other.HardAngleThreshold
			&& NormalCalculationMethod == Other.NormalCalculationMethod
			&& MergeDistance == Other.MergeDistance
			&& UnresolvedGeometryColor == Other.UnresolvedGeometryColor
			&& bOverrideVoxelSize == Other.bOverrideVoxelSize
			&& VoxelSize == Other.VoxelSize;
	}

	/** Inequality. */
	bool operator!=(const FMeshProxySettings& Other) const
	{
		return !(*this == Other);
	}

#if WITH_EDITORONLY_DATA
	/** Handles deprecated properties */
	void PostLoadDeprecated();
#endif
};


UENUM()
enum class EMeshLODSelectionType : uint8
{
	// Whether or not to export all of the LODs found in the source meshes
	AllLODs = 0 UMETA(DisplayName = "Use all LOD levels"),
	// Whether or not to export all of the LODs found in the source meshes
	SpecificLOD = 1 UMETA(DisplayName = "Use specific LOD level"),
	// Whether or not to calculate the appropriate LOD model for the given screen size
	CalculateLOD = 2 UMETA(DisplayName = "Calculate correct LOD level"),
	// Whether or not to use the lowest-detail LOD
	LowestDetailLOD = 3 UMETA(DisplayName = "Always use the lowest-detail LOD (i.e. the highest LOD index)")
};

UENUM()
enum class EMeshMergeType : uint8
{
	MeshMergeType_Default,
	MeshMergeType_MergeActor
};

/** As UHT doesnt allow arrays of bools, we need this binary enum :( */
UENUM()
enum class EUVOutput : uint8
{
	DoNotOutputChannel,
	OutputChannel
};

/**
* Mesh merging settings
*/
USTRUCT(Blueprintable)
struct FMeshMergingSettings
{
	GENERATED_USTRUCT_BODY()

	/** Target lightmap resolution */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = MeshSettings, meta=(ClampMax = 4096, EditCondition = "!bComputedLightMapResolution", DisplayAfter="bGenerateLightMapUV"))
	int32 TargetLightMapResolution;

	/** Whether to output the specified UV channels into the merged mesh (only if the source meshes contain valid UVs for the specified channel) */
	UPROPERTY(EditAnywhere, Category = MeshSettings, meta=(DisplayAfter="bBakeVertexDataToMesh"))
	EUVOutput OutputUVs[8];	// Should be MAX_MESH_TEXTURE_COORDS but as this is an engine module we cant include RawMesh

	/** Material simplification */
	UPROPERTY(EditAnywhere, Category = MaterialSettings, BlueprintReadWrite, meta = (EditCondition = "bMergeMaterials", DisplayAfter="bMergeMaterials"))
	FMaterialProxySettings MaterialSettings;

	/** The gutter (in texels) to add to each sub-chart for our baked-out material for the top mip level */
	UPROPERTY(EditAnywhere, Category = MaterialSettings, meta=(DisplayAfter="MaterialSettings"))
	int32 GutterSize;

	// A given LOD level to export from the source meshes
	UPROPERTY(EditAnywhere, Category = MeshSettings, BlueprintReadWrite, meta = (DisplayAfter="LODSelectionType", ClampMin = "0", ClampMax = "7", UIMin = "0", UIMax = "7", EnumCondition = 1))
	int32 SpecificLOD;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshSettings, meta = (DisplayAfter="bBakeVertexDataToMesh"))
	EMeshLODSelectionType LODSelectionType;

	/** Whether to generate lightmap UVs for a merged mesh*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = MeshSettings)
	uint8 bGenerateLightMapUV:1;

	/** Whether or not the lightmap resolution should be computed by summing the lightmap resolutions for the input Mesh Components */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = MeshSettings)
	uint8 bComputedLightMapResolution:1;

	/** Whether merged mesh should have pivot at world origin, or at first merged component otherwise */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshSettings)
	uint8 bPivotPointAtZero:1;

	/** Whether to merge physics data (collision primitives)*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshSettings)
	uint8 bMergePhysicsData:1;

	/** Whether to merge source materials into one flat material, ONLY available when merging a single LOD level, see LODSelectionType */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MaterialSettings)
	uint8 bMergeMaterials:1;

	/** Whether or not vertex data such as vertex colours should be baked into the resulting mesh */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshSettings)
	uint8 bBakeVertexDataToMesh:1;

	/** Whether or not vertex data such as vertex colours should be used when baking out materials */
	UPROPERTY(EditAnywhere, Category = MaterialSettings, BlueprintReadWrite, meta = (EditCondition = "bMergeMaterials"))
	uint8 bUseVertexDataForBakingMaterial:1;

	// Whether or not to calculate varying output texture sizes according to their importance in the final atlas texture
	UPROPERTY(Category = MaterialSettings, EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bMergeMaterials"))
	uint8 bUseTextureBinning:1;

	/** Whether to attempt to re-use the source mesh's lightmap UVs when baking the material or always generate a new set. */
	UPROPERTY(EditAnywhere, Category = MaterialSettings)
	uint8 bReuseMeshLightmapUVs:1;

	/** Whether to attempt to merge materials that are deemed equivalent. This can cause artifacts in the merged mesh if world position/actor position etc. is used to determine output color. */
	UPROPERTY(EditAnywhere, Category = MaterialSettings)
	uint8 bMergeEquivalentMaterials:1;

	/** Whether or not to use available landscape geometry to cull away invisible triangles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LandscapeCulling)
	uint8 bUseLandscapeCulling:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshSettings)
	uint8 bIncludeImposters:1;

	/** Whether to allow distance field to be computed for this mesh. Disable this to save memory if the merged mesh will only be rendered in the distance. */
	UPROPERTY(EditAnywhere, Category = MeshSettings)
	uint8 bAllowDistanceField:1;

#if WITH_EDITORONLY_DATA
	/** Whether we should import vertex colors into merged mesh */
	UPROPERTY()
	uint8 bImportVertexColors_DEPRECATED:1;
	UPROPERTY()
	uint8 bCalculateCorrectLODModel_DEPRECATED:1;
	/** Whether to export normal maps for material merging */
	UPROPERTY()
	uint8 bExportNormalMap_DEPRECATED:1;
	/** Whether to export metallic maps for material merging */
	UPROPERTY()
	uint8 bExportMetallicMap_DEPRECATED:1;
	/** Whether to export roughness maps for material merging */
	UPROPERTY()
	uint8 bExportRoughnessMap_DEPRECATED:1;
	/** Whether to export specular maps for material merging */
	UPROPERTY()
	uint8 bExportSpecularMap_DEPRECATED:1;
	/** Merged material texture atlas resolution */
	UPROPERTY()
	int32 MergedMaterialAtlasResolution_DEPRECATED;
	UPROPERTY()
	int32 ExportSpecificLOD_DEPRECATED;
#endif

	EMeshMergeType MergeType;

	/** Default settings. */
	FMeshMergingSettings()
		: TargetLightMapResolution(256)
		, GutterSize(2)
		, SpecificLOD(0)
		, LODSelectionType(EMeshLODSelectionType::CalculateLOD)
		, bGenerateLightMapUV(true)
		, bComputedLightMapResolution(false)
		, bPivotPointAtZero(false)
		, bMergePhysicsData(false)
		, bMergeMaterials(false)
		, bBakeVertexDataToMesh(false)
		, bUseVertexDataForBakingMaterial(true)
		, bUseTextureBinning(false)
		, bReuseMeshLightmapUVs(true)
		, bMergeEquivalentMaterials(true)
		, bUseLandscapeCulling(false)
		, bIncludeImposters(true)
		, bAllowDistanceField(false)
#if WITH_EDITORONLY_DATA
		, bImportVertexColors_DEPRECATED(false)
		, bCalculateCorrectLODModel_DEPRECATED(false)
		, bExportNormalMap_DEPRECATED(true)
		, bExportMetallicMap_DEPRECATED(false)
		, bExportRoughnessMap_DEPRECATED(false)
		, bExportSpecularMap_DEPRECATED(false)
		, MergedMaterialAtlasResolution_DEPRECATED(1024)
		, ExportSpecificLOD_DEPRECATED(0)
#endif
		, MergeType(EMeshMergeType::MeshMergeType_Default)
	{
		for(EUVOutput& OutputUV : OutputUVs)
		{
			OutputUV = EUVOutput::OutputChannel;
		}
	}

#if WITH_EDITORONLY_DATA
	/** Handles deprecated properties */
	void PostLoadDeprecated();
#endif
};

/** Struct to store per section info used to populate data after (multiple) meshes are merged together */
struct FSectionInfo
{
	FSectionInfo() : Material(nullptr), MaterialSlotName(NAME_None), MaterialIndex(INDEX_NONE), StartIndex(INDEX_NONE), EndIndex(INDEX_NONE), bProcessed(false)	{}

	/** Material used by the section */
	class UMaterialInterface* Material;
	/** Name value for the section */
	FName MaterialSlotName;
	/** List of properties enabled for the section (collision, cast shadow etc) */
	TArray<FName> EnabledProperties;
	/** Original index of Material in the source data */
	int32 MaterialIndex;
	/** Index pointing to the start set of mesh indices that belong to this section */
	int32 StartIndex;
	/** Index pointing to the end set of mesh indices that belong to this section */
	int32 EndIndex;
	/** Used while baking out materials, to check which sections are and aren't being baked out */
	bool bProcessed;

	bool operator==(const FSectionInfo& Other) const
	{
		return Material == Other.Material && EnabledProperties == Other.EnabledProperties;
	}
};

/** How to replace instanced */
UENUM()
enum class EMeshInstancingReplacementMethod : uint8
{
	/** Destructive workflow: remove the original actors when replacing with instanced static meshes */
	RemoveOriginalActors,

	/** Non-destructive workflow: keep the original actors but hide them and set them to be editor-only */
	KeepOriginalActorsAsEditorOnly
};

/** Mesh instance-replacement settings */
USTRUCT(Blueprintable)
struct FMeshInstancingSettings
{
	GENERATED_BODY()

	FMeshInstancingSettings()
		: ActorClassToUse(AActor::StaticClass())
		, InstanceReplacementThreshold(2)
		, MeshReplacementMethod(EMeshInstancingReplacementMethod::KeepOriginalActorsAsEditorOnly)
		, bSkipMeshesWithVertexColors(true)
		, bUseHLODVolumes(true)
	{}

	/** The actor class to attach new instance static mesh components to */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, NoClear, Category="Instancing")
	TSubclassOf<AActor> ActorClassToUse;

	/** The number of static mesh instances needed before a mesh is replaced with an instanced version */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Instancing", meta=(ClampMin=1))
	int32 InstanceReplacementThreshold;

	/** How to replace the original actors when instancing */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Instancing")
	EMeshInstancingReplacementMethod MeshReplacementMethod;

	/**
	 * Whether to skip the conversion to an instanced static mesh for meshes with vertex colors.
	 * Instanced static meshes do not support vertex colors per-instance, so conversion will lose
	 * this data.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Instancing")
	bool bSkipMeshesWithVertexColors;

	/**
	 * Whether split up instanced static mesh components based on their intersection with HLOD volumes
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Instancing", meta=(DisplayName="Use HLOD Volumes"))
	bool bUseHLODVolumes;
};