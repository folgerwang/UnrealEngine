// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/FbxMeshImportData.h"
#include "MeshBuild.h"
#include "FbxSkeletalMeshImportData.generated.h"

class USkeletalMesh;
class FSkeletalMeshLODModel;
struct FSkeletalMaterial;

UENUM()
enum EFBXImportContentType
{
	FBXICT_All UMETA(DisplayName = "Geometry and Skinning Weights.", ToolTip = "Import all fbx content: geometry, skinning and weights."),
	FBXICT_Geometry UMETA(DisplayName = "Geometry Only", ToolTip = "Import the skeletal mesh geometry only (will create a default skeleton, or map the geometry to the existing one). Morph and LOD can be imported with it."),
	FBXICT_SkinningWeights UMETA(DisplayName = "Skinning Weights Only", ToolTip = "Import the skeletal mesh skinning and weights only (no geometry will be imported). Morph and LOD will not be imported with this settings."),
	FBXICT_MAX,
};

namespace NSSkeletalMeshSourceFileLabels
{
	static FText GeoAndSkinningText()
	{
		static FText GeoAndSkinningText = (NSLOCTEXT("FBXReimport", "ImportContentTypeAll", "Geometry and Skinning Weights"));
		return GeoAndSkinningText;
	}

	static FText GeometryText()
	{
		static FText GeometryText = (NSLOCTEXT("FBXReimport", "ImportContentTypeGeometry", "Geometry"));
		return GeometryText;
	}
	static FText SkinningText()
	{
		static FText SkinningText = (NSLOCTEXT("FBXReimport", "ImportContentTypeSkinning", "Skinning Weights"));
		return SkinningText;
	}
}

/**
 * Import data and options used when importing a static mesh from fbx
 */
UCLASS(MinimalAPI)
class UFbxSkeletalMeshImportData : public UFbxMeshImportData
{
	GENERATED_UCLASS_BODY()
public:
	/** Filter the content we want to import from the incoming FBX skeletal mesh.*/
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ImportType = "SkeletalMesh", DisplayName = "Import Content Type", OBJRestrict = "true"))
	TEnumAsByte<enum EFBXImportContentType> ImportContentType;
	
	/** The value of the content type during the last import. This cannot be edited and is set only on successful import or re-import*/
	UPROPERTY()
	TEnumAsByte<enum EFBXImportContentType> LastImportContentType;

	/** Specify how vertex colors should be imported */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, config, Category = Mesh, meta = (OBJRestrict = "true", ImportType = "SkeletalMesh"))
	TEnumAsByte<EVertexColorImportOption::Type> VertexColorImportOption;

	/** Specify override color in the case that VertexColorImportOption is set to Override */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, config, Category = Mesh, meta = (OBJRestrict = "true", ImportType = "SkeletalMesh"))
	FColor VertexOverrideColor;

	/** Enable this option to update Skeleton (of the mesh)'s reference pose. Mesh's reference pose is always updated.  */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Mesh, meta=(ImportType="SkeletalMesh|RigOnly", ToolTip="If enabled, update the Skeleton (of the mesh being imported)'s reference pose."))
	uint32 bUpdateSkeletonReferencePose:1;

	/** Enable this option to use frame 0 as reference pose */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category= Mesh, meta=(ImportType="SkeletalMesh|RigOnly", DisplayName="Use T0 As Ref Pose"))
	uint32 bUseT0AsRefPose:1;

	/** If checked, triangles with non-matching smoothing groups will be physically split. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category=Mesh, meta=(ImportType="SkeletalMesh|GeoOnly"))
	uint32 bPreserveSmoothingGroups:1;

	/** If checked, meshes nested in bone hierarchies will be imported instead of being converted to bones. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category=Mesh, meta=(ImportType="SkeletalMesh"))
	uint32 bImportMeshesInBoneHierarchy:1;

	/** True to import morph target meshes from the FBX file */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category=Mesh, meta=(ImportType="SkeletalMesh|GeoOnly", ToolTip="If enabled, creates Unreal morph objects for the imported meshes"))
	uint32 bImportMorphTargets:1;

	/** Threshold to compare vertex position equality. */
	UPROPERTY(EditAnywhere, config, Category="Mesh", meta = (ImportType = "SkeletalMesh|GeoOnly", SubCategory = "Thresholds", NoSpinbox = "true", ClampMin = "0.0"))
	float ThresholdPosition;
	
	/** Threshold to compare normal, tangent or bi-normal equality. */
	UPROPERTY(EditAnywhere, config, Category="Mesh", meta = (ImportType = "SkeletalMesh|GeoOnly", SubCategory = "Thresholds", NoSpinbox = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float ThresholdTangentNormal;
	
	/** Threshold to compare UV equality. */
	UPROPERTY(EditAnywhere, config, Category="Mesh", meta = (ImportType = "SkeletalMesh|GeoOnly", SubCategory = "Thresholds", NoSpinbox = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float ThresholdUV;

	/** Gets or creates fbx import data for the specified skeletal mesh */
	static UFbxSkeletalMeshImportData* GetImportDataForSkeletalMesh(USkeletalMesh* SkeletalMesh, UFbxSkeletalMeshImportData* TemplateForCreation);

	bool CanEditChange( const UProperty* InProperty ) const override;

	bool GetImportContentFilename(FString& OutFilename, FString& OutFilenameLabel) const;

	/** This function add the last import content type to the asset registry which is use by the thumbnail overlay of the skeletal mesh */
	virtual void AppendAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags);
};

class FSkeletalMeshImportData;
struct ExistingSkelMeshData;
class USkeleton;
struct FReferenceSkeleton;

extern UNREALED_API ExistingSkelMeshData* SaveExistingSkelMeshData(USkeletalMesh* ExistingSkelMesh, bool bSaveMaterials, int32 ReimportLODIndex);
extern UNREALED_API void RestoreExistingSkelMeshData(ExistingSkelMeshData* MeshData, USkeletalMesh* SkeletalMesh, int32 ReimportLODIndex, bool bCanShowDialog, bool bImportSkinningOnly);
extern UNREALED_API void ProcessImportMeshInfluences(FSkeletalMeshImportData& ImportData);
extern UNREALED_API void ProcessImportMeshMaterials(TArray<FSkeletalMaterial>& Materials, FSkeletalMeshImportData& ImportData);
extern UNREALED_API bool ProcessImportMeshSkeleton(const USkeleton* SkeletonAsset, FReferenceSkeleton& RefSkeleton, int32& SkeletalDepth, FSkeletalMeshImportData& ImportData);
namespace SkeletalMeshHelper
{
	extern UNREALED_API void ApplySkinning(USkeletalMesh* SkeletalMesh, FSkeletalMeshLODModel& SrcLODModel, FSkeletalMeshLODModel& DestLODModel);
}