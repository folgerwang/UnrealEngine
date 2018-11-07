// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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

/** Settings used to reduce a mesh. */
USTRUCT(Blueprintable)
struct FMeshReductionSettings
{
	GENERATED_USTRUCT_BODY()

	/** Percentage of triangles to keep. 1.0 = no reduction, 0.0 = no triangles. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	float PercentTriangles;

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
	bool bRecalculateNormals;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	int32 BaseLODModel;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	bool bGenerateUniqueLightmapUVs;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	bool bKeepSymmetry;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	bool bVisibilityAided;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	bool bCullOccluded;

	/** Higher values generates fewer samples*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	TEnumAsByte<EMeshFeatureImportance::Type> VisibilityAggressiveness;

	/** Higher values minimize change to vertex color data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	TEnumAsByte<EMeshFeatureImportance::Type> VertexColorImportance;

	/** Default settings. */
	FMeshReductionSettings()
		: PercentTriangles(1.0f)
		, MaxDeviation(0.0f)
		, PixelError(8.0f)
		, WeldingThreshold(0.0f)
		, HardAngleThreshold(80.0f)
		, SilhouetteImportance(EMeshFeatureImportance::Normal)
		, TextureImportance(EMeshFeatureImportance::Normal)
		, ShadingImportance(EMeshFeatureImportance::Normal)
		, bRecalculateNormals(false)
		, BaseLODModel(0)
		, bGenerateUniqueLightmapUVs(false)
		, bKeepSymmetry(false)
		, bVisibilityAided(false)
		, bCullOccluded(false)
		, VisibilityAggressiveness(EMeshFeatureImportance::Lowest)
		, VertexColorImportance(EMeshFeatureImportance::Off)
	{
	}

	FMeshReductionSettings(const FMeshReductionSettings& Other)
		: PercentTriangles(Other.PercentTriangles)
		, MaxDeviation(Other.MaxDeviation)
		, PixelError(Other.PixelError)
		, WeldingThreshold(Other.WeldingThreshold)
		, HardAngleThreshold(Other.HardAngleThreshold)
		, SilhouetteImportance(Other.SilhouetteImportance)
		, TextureImportance(Other.TextureImportance)
		, ShadingImportance(Other.ShadingImportance)
		, bRecalculateNormals(Other.bRecalculateNormals)
		, BaseLODModel(Other.BaseLODModel)
		, bGenerateUniqueLightmapUVs(Other.bGenerateUniqueLightmapUVs)
		, bKeepSymmetry(Other.bKeepSymmetry)
		, bVisibilityAided(Other.bVisibilityAided)
		, bCullOccluded(Other.bCullOccluded)
		, VisibilityAggressiveness(Other.VisibilityAggressiveness)
		, VertexColorImportance(Other.VertexColorImportance)
	{
	}

	/** Equality operator. */
	bool operator==(const FMeshReductionSettings& Other) const
	{
		return PercentTriangles == Other.PercentTriangles
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

	/** If true, Spatial Sampling Distance will not be automatically computed based on geometry and you must set it directly */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, AdvancedDisplay, Category = ProxySettings, meta = (InlineEditConditionToggle))
	uint8 bOverrideVoxelSize : 1;

	/** Override when converting multiple meshes for proxy LOD merging. Warning, large geometry with small sampling has very high memory costs*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, AdvancedDisplay, Category = ProxySettings, meta = (EditCondition = "bOverrideVoxelSize", ClampMin = "0.1", DisplayName = "Overide Spatial Sampling Distance"))
	float VoxelSize;

	/** Material simplification */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	FMaterialProxySettings MaterialSettings;

	UPROPERTY()
	int32 TextureWidth_DEPRECATED;

	UPROPERTY()
	int32 TextureHeight_DEPRECATED;

	UPROPERTY()
	bool bExportNormalMap_DEPRECATED;

	UPROPERTY()
	bool bExportMetallicMap_DEPRECATED;

	UPROPERTY()
	bool bExportRoughnessMap_DEPRECATED;

	UPROPERTY()
	bool bExportSpecularMap_DEPRECATED;

	/** Determines whether or not the correct LOD models should be calculated given the source meshes and transition size */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	bool bCalculateCorrectLODModel;

	/** Distance at which meshes should be merged together, this can close gaps like doors and windows in distant geometry */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	float MergeDistance;

	/** Base color assigned to LOD geometry that can't be associated with the source geometry: e.g. doors and windows that have been closed by the Merge Distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings, meta = (DisplayName = "Unresolved Geometry Color"))
	FColor UnresolvedGeometryColor;

	/** Enable an override for material transfer distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MaxRayCastDist, meta = (InlineEditConditionToggle))
	bool bOverrideTransferDistance;

	/** Override search distance used when discovering texture values for simplified geometry. Useful when non-zero Merge Distance setting generates new geometry in concave corners.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings, meta = (EditCondition = "bOverrideTransferDistance", DisplayName = "Transfer Distance Override", ClampMin = 0))
	float MaxRayCastDist;

	/** Enable the use of hard angle based vertex splitting */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = HardAngleThreshold, meta = (InlineEditConditionToggle))
	bool bUseHardAngleThreshold;

	/** Angle at which a hard edge is introduced between faces */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings, meta = (EditCondition = "bUseHardAngleThreshold", DisplayName = "Hard Edge Angle", ClampMin = 0, ClampMax = 180))
	float HardAngleThreshold;

	/** Controls the method used to calculate the normal for the simplified geometry */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings, meta = (DisplayName = "Normal Calculation Method"))
	TEnumAsByte<EProxyNormalComputationMethod::Type> NormalCalculationMethod;

	/** Lightmap resolution */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings, meta = (ClampMin = 32, ClampMax = 4096, EditCondition = "!bComputeLightMapResolution"))
	int32 LightMapResolution;

	/** If ticked will compute the lightmap resolution by summing the dimensions for each mesh included for merging */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	bool bComputeLightMapResolution;

	/** Whether Simplygon should recalculate normals, otherwise the normals channel will be sampled from the original mesh */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	bool bRecalculateNormals;

	UPROPERTY()
	bool bBakeVertexData_DEPRECATED;

	/** Whether or not to use available landscape geometry to cull away invisible triangles */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = LandscapeCulling)
	bool bUseLandscapeCulling;

	/** Level of detail of the landscape that should be used for the culling */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = LandscapeCulling, meta = (EditCondition="bUseLandscapeCulling"))
	TEnumAsByte<ELandscapeCullingPrecision::Type> LandscapeCullingPrecision;

	/** Whether to allow adjacency buffers for tessellation in the merged mesh */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	bool bAllowAdjacency;

	/** Whether to allow distance field to be computed for this mesh. Disable this to save memory if the merged mesh will only be rendered in the distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	bool bAllowDistanceField;

	/** Whether to attempt to re-use the source mesh's lightmap UVs when baking the material or always generate a new set. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	bool bReuseMeshLightmapUVs;

	/** Whether to generate collision for the merged mesh */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	bool bCreateCollision;

	/** Whether to allow vertex colors saved in the merged mesh */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	bool bAllowVertexColors;

	/** Whether to generate lightmap uvs for the merged mesh */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	bool bGenerateLightmapUVs;

	/** Default settings. */
	FMeshProxySettings()
		: ScreenSize(300)
		, bOverrideVoxelSize(false)
		, VoxelSize(3.f)
		, TextureWidth_DEPRECATED(512)
		, TextureHeight_DEPRECATED(512)
		, bExportNormalMap_DEPRECATED(true)
		, bExportMetallicMap_DEPRECATED(false)
		, bExportRoughnessMap_DEPRECATED(false)
		, bExportSpecularMap_DEPRECATED(false)
		, bCalculateCorrectLODModel(false)
		, MergeDistance(0)
		, UnresolvedGeometryColor(FColor::Black)
		, bOverrideTransferDistance(false)
		, MaxRayCastDist(20)
		, bUseHardAngleThreshold(false)
		, HardAngleThreshold(130.f)
		, NormalCalculationMethod(EProxyNormalComputationMethod::AngleWeighted)
		, LightMapResolution(256)
		, bComputeLightMapResolution(false)
		, bRecalculateNormals(true)
		, bBakeVertexData_DEPRECATED(false)
		, bUseLandscapeCulling(false)
		, LandscapeCullingPrecision(ELandscapeCullingPrecision::Medium)
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

	/** Handles deprecated properties */
	void PostLoadDeprecated();
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

	/** Whether to generate lightmap UVs for a merged mesh*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = MeshSettings)
	bool bGenerateLightMapUV;

	/** Target lightmap resolution */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = MeshSettings, meta=(ClampMax = 4096, EditCondition = "!bComputedLightMapResolution"))
	int32 TargetLightMapResolution;

	/** Whether or not the lightmap resolution should be computed by summing the lightmap resolutions for the input Mesh Components */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = MeshSettings)
	bool bComputedLightMapResolution;

	/** Whether we should import vertex colors into merged mesh */
	UPROPERTY()
	bool bImportVertexColors_DEPRECATED;

	/** Whether merged mesh should have pivot at world origin, or at first merged component otherwise */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshSettings)
	bool bPivotPointAtZero;

	/** Whether to merge physics data (collision primitives)*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshSettings)
	bool bMergePhysicsData;

	/** Whether to merge source materials into one flat material, ONLY available when merging a single LOD level, see LODSelectionType */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MaterialSettings)
	bool bMergeMaterials;

	/** Material simplification */
	UPROPERTY(EditAnywhere, Category = MaterialSettings, BlueprintReadWrite, meta = (EditCondition = "bMergeMaterials"))
	FMaterialProxySettings MaterialSettings;

	/** Whether or not vertex data such as vertex colours should be baked into the resulting mesh */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshSettings)
	bool bBakeVertexDataToMesh;

	/** Whether or not vertex data such as vertex colours should be used when baking out materials */
	UPROPERTY(EditAnywhere, Category = MaterialSettings, BlueprintReadWrite, meta = (EditCondition = "bMergeMaterials"))
	bool bUseVertexDataForBakingMaterial;

	// Whether or not to calculate varying output texture sizes according to their importance in the final atlas texture
	UPROPERTY(Category = MaterialSettings, EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bMergeMaterials"))
	bool bUseTextureBinning;

	/** Whether to attempt to re-use the source mesh's lightmap UVs when baking the material or always generate a new set. */
	UPROPERTY(EditAnywhere, Category = MaterialSettings)
	bool bReuseMeshLightmapUVs;

	/** Whether to attempt to merge materials that are deemed equivalent. This can cause artifacts in the merged mesh if world position/actor position etc. is used to determine output color. */
	UPROPERTY(EditAnywhere, Category = MaterialSettings)
	bool bMergeEquivalentMaterials;

	/** Whether to output the specified UV channels into the merged mesh (only if the source meshes contain valid UVs for the specified channel) */
	UPROPERTY(EditAnywhere, Category = MeshSettings)
	EUVOutput OutputUVs[8];	// Should be MAX_MESH_TEXTURE_COORDS but as this is an engine module we cant include RawMesh

	/** The gutter (in texels) to add to each sub-chart for our baked-out material for the top mip level */
	UPROPERTY(EditAnywhere, Category = MaterialSettings)
	int32 GutterSize;

	UPROPERTY()
	bool bCalculateCorrectLODModel_DEPRECATED;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshSettings)
	EMeshLODSelectionType LODSelectionType;

	UPROPERTY()
	int32 ExportSpecificLOD_DEPRECATED;

	// A given LOD level to export from the source meshes
	UPROPERTY(EditAnywhere, Category = MeshSettings, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "7", UIMin = "0", UIMax = "7", EnumCondition = 1))
	int32 SpecificLOD;

	/** Whether or not to use available landscape geometry to cull away invisible triangles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LandscapeCulling)
	bool bUseLandscapeCulling;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshSettings)
	bool bIncludeImposters;

	/** Whether to allow distance field to be computed for this mesh. Disable this to save memory if the merged mesh will only be rendered in the distance. */
	UPROPERTY(EditAnywhere, Category = MeshSettings)
	bool bAllowDistanceField;

	/** Whether to export normal maps for material merging */
	UPROPERTY()
	bool bExportNormalMap_DEPRECATED;
	/** Whether to export metallic maps for material merging */
	UPROPERTY()
	bool bExportMetallicMap_DEPRECATED;
	/** Whether to export roughness maps for material merging */
	UPROPERTY()
	bool bExportRoughnessMap_DEPRECATED;
	/** Whether to export specular maps for material merging */
	UPROPERTY()
	bool bExportSpecularMap_DEPRECATED;
	/** Merged material texture atlas resolution */
	UPROPERTY()
	int32 MergedMaterialAtlasResolution_DEPRECATED;

	EMeshMergeType MergeType;

	/** Default settings. */
	FMeshMergingSettings()
		: bGenerateLightMapUV(true)
		, TargetLightMapResolution(256)
		, bComputedLightMapResolution(false)
		, bImportVertexColors_DEPRECATED(false)
		, bPivotPointAtZero(false)
		, bMergePhysicsData(false)
		, bMergeMaterials(false)
		, bBakeVertexDataToMesh(false)
		, bUseVertexDataForBakingMaterial(true)
		, bUseTextureBinning(false)
		, bReuseMeshLightmapUVs(true)
		, bMergeEquivalentMaterials(true)
		, GutterSize(2)
		, bCalculateCorrectLODModel_DEPRECATED(false)
		, LODSelectionType(EMeshLODSelectionType::CalculateLOD)
		, ExportSpecificLOD_DEPRECATED(0)
		, SpecificLOD(0)
		, bUseLandscapeCulling(false)
		, bIncludeImposters(true)
		, bAllowDistanceField(false)
		, bExportNormalMap_DEPRECATED(true)
		, bExportMetallicMap_DEPRECATED(false)
		, bExportRoughnessMap_DEPRECATED(false)
		, bExportSpecularMap_DEPRECATED(false)
		, MergedMaterialAtlasResolution_DEPRECATED(1024)
		, MergeType(EMeshMergeType::MeshMergeType_Default)
	{
		for(EUVOutput& OutputUV : OutputUVs)
		{
			OutputUV = EUVOutput::OutputChannel;
		}
	}

	/** Handles deprecated properties */
	void PostLoadDeprecated();
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
enum class EMeshInstancingReplacementMethod
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