// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalMeshReductionSettings.h: Skeletal Mesh Reduction Settings
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BoneContainer.h"
#include "SkeletalMeshReductionSettings.generated.h"

/** Enum specifying the reduction type to use when simplifying skeletal meshes. */
UENUM()
enum SkeletalMeshOptimizationType
{
	SMOT_NumOfTriangles UMETA(DisplayName = "Triangles", ToolTip = "Triangle requirement will be used for simplification."),
	SMOT_MaxDeviation UMETA(DisplayName = "Accuracy", ToolTip = "Accuracy requirement will be used for simplification."),
	SMOT_TriangleOrDeviation UMETA(DisplayName = "Any", ToolTip = "Simplification will continue until either Triangle or Accuracy requirement is met."),
	SMOT_MAX UMETA(Hidden),
};

/** Enum specifying the importance of properties when simplifying skeletal meshes. */
UENUM()
enum SkeletalMeshOptimizationImportance
{
	SMOI_Off UMETA(DisplayName = "Off"),
	SMOI_Lowest UMETA(DisplayName = "Lowest"),
	SMOI_Low UMETA(DisplayName = "Low"),
	SMOI_Normal UMETA(DisplayName = "Normal"),
	SMOI_High UMETA(DisplayName = "High"),
	SMOI_Highest UMETA(DisplayName = "Highest"),
	SMOI_MAX UMETA(Hidden)
};

/**
* FSkeletalMeshOptimizationSettings - The settings used to optimize a skeletal mesh LOD.
*/
USTRUCT()
struct FSkeletalMeshOptimizationSettings
{
	GENERATED_USTRUCT_BODY()

	/** The method to use when optimizing the skeletal mesh LOD */
	UPROPERTY(EditAnywhere, Category = ReductionMethod)
	TEnumAsByte<enum SkeletalMeshOptimizationType> ReductionMethod;

	/** If ReductionMethod equals NumOfTriangles this value is the ratio of triangles percentage to remove from the mesh.
	 * In code, it ranges from [0, 1]. In the editor UI, it ranges from [0, 100]
	 */
	UPROPERTY(EditAnywhere, Category = ReductionMethod)
	float NumOfTrianglesPercentage;

	/**If ReductionMethod equals MaxDeviation this value is the maximum deviation from the base mesh as a percentage of the bounding sphere. 
	 * In code, it ranges from [0, 1]. In the editor UI, it ranges from [0, 100]
	 */
	UPROPERTY(EditAnywhere, Category = ReductionMethod)
	float MaxDeviationPercentage;

	/* Remap the morph targets from the base LOD onto the reduce LOD. */
	UPROPERTY(EditAnywhere, Category = FSkeletalMeshOptimizationSettings)
	bool bRemapMorphTargets;

	/** How important the shape of the geometry is. */
	UPROPERTY(EditAnywhere, Category = FSkeletalMeshOptimizationSettings, meta = (DisplayName = "Silhouette"))
	TEnumAsByte<enum SkeletalMeshOptimizationImportance> SilhouetteImportance;

	/** How important texture density is. */
	UPROPERTY(EditAnywhere, Category = FSkeletalMeshOptimizationSettings, meta = (DisplayName = "Texture"))
	TEnumAsByte<enum SkeletalMeshOptimizationImportance> TextureImportance;

	/** How important shading quality is. */
	UPROPERTY(EditAnywhere, Category = FSkeletalMeshOptimizationSettings, meta = (DisplayName = "Shading"))
	TEnumAsByte<enum SkeletalMeshOptimizationImportance> ShadingImportance;

	/** How important skinning quality is. */
	UPROPERTY(EditAnywhere, Category = FSkeletalMeshOptimizationSettings, meta = (DisplayName = "Skinning"))
	TEnumAsByte<enum SkeletalMeshOptimizationImportance> SkinningImportance;

	/** The welding threshold distance. Vertices under this distance will be welded. */
	UPROPERTY(EditAnywhere, Category = FSkeletalMeshOptimizationSettings)
	float WeldingThreshold;

	/** Whether Normal smoothing groups should be preserved. If true then Hard Edge Angle (NormalsThreshold) is used **/
	UPROPERTY(EditAnywhere, Category = FSkeletalMeshOptimizationSettings, meta = (DisplayName = "Recompute Normal"))
	bool bRecalcNormals;

	/** If the angle between two triangles are above this value, the normals will not be
	smooth over the edge between those two triangles. Set in degrees. This is only used when bRecalcNormals is set to true*/
	UPROPERTY(EditAnywhere, Category = FSkeletalMeshOptimizationSettings, meta = (DisplayName = "Hard Edge Angle", EditCondition = "bRecalcNormals"))
	float NormalsThreshold;

	/** Maximum number of bones that can be assigned to each vertex. */
	UPROPERTY(EditAnywhere, Category = FSkeletalMeshOptimizationSettings, meta = (DisplayName = "Max Bones Influence"))
	int32 MaxBonesPerVertex;

	UPROPERTY()
	TArray<FBoneReference> BonesToRemove_DEPRECATED;

	/** Base LOD index to generate this LOD. By default, we generate from LOD 0 */
	UPROPERTY(EditAnywhere, Category = FSkeletalMeshOptimizationSettings)
	int32 BaseLOD;

	UPROPERTY()
	class UAnimSequence* BakePose_DEPRECATED;

	FSkeletalMeshOptimizationSettings()
		: ReductionMethod(SMOT_NumOfTriangles)
		, NumOfTrianglesPercentage(0.5f)
		, MaxDeviationPercentage(0.5f)
		, bRemapMorphTargets(false)
		, SilhouetteImportance(SMOI_Normal)
		, TextureImportance(SMOI_Normal)
		, ShadingImportance(SMOI_Normal)
		, SkinningImportance(SMOI_Normal)
		, WeldingThreshold(0.1f)
		, bRecalcNormals(true)
		, NormalsThreshold(60.0f)
		, MaxBonesPerVertex(4)
		, BaseLOD(0)
		, BakePose_DEPRECATED(nullptr)
	{
	}
};

