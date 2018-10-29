// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*-----------------------------------------------------------------------------
	Data structures only used for importing skeletal meshes and animations.
-----------------------------------------------------------------------------*/

#pragma once

#include "CoreMinimal.h"
#include "Containers/IndirectArray.h"
#include "ReferenceSkeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshLODImporterData.h"

class UAssetImportData;
class UMorphTarget;
class UPhysicsAsset;
class USkeletalMeshSocket;
class USkeleton;
class UThumbnailInfo;
class FSkeletalMeshLODModel;

struct ExistingMeshLodSectionData
{
	ExistingMeshLodSectionData(FName InImportedMaterialSlotName, bool InbCastShadow, bool InbRecomputeTangents, int32 InGenerateUpTo)
	: ImportedMaterialSlotName(InImportedMaterialSlotName)
	, bCastShadow(InbCastShadow)
	, bRecomputeTangents(InbRecomputeTangents)
	, GenerateUpTo(InGenerateUpTo)
	{}
	FName ImportedMaterialSlotName;
	bool bCastShadow;
	bool bRecomputeTangents;
	int32 GenerateUpTo;
};

struct ExistingSkelMeshData
{
	TArray<USkeletalMeshSocket*>			ExistingSockets;
	TIndirectArray<FSkeletalMeshLODModel>	ExistingLODModels;
	TArray<FSkeletalMeshLODInfo>			ExistingLODInfo;
	FReferenceSkeleton						ExistingRefSkeleton;
	TArray<FSkeletalMaterial>				ExistingMaterials;
	bool									bSaveRestoreMaterials;
	TArray<UMorphTarget*>					ExistingMorphTargets;
	TArray<UPhysicsAsset*>					ExistingPhysicsAssets;
	UPhysicsAsset*							ExistingShadowPhysicsAsset;
	USkeleton*								ExistingSkeleton;
	TArray<FTransform>						ExistingRetargetBasePose;
	USkeletalMeshLODSettings*				ExistingLODSettings;
	TSubclassOf<UAnimInstance>				ExistingPostProcessAnimBlueprint;

	bool									bExistingUseFullPrecisionUVs;
	bool									bExistingUseHighPrecisionTangentBasis;

	TArray<FBoneMirrorExport>				ExistingMirrorTable;

	TWeakObjectPtr<UAssetImportData>		ExistingAssetImportData;
	TWeakObjectPtr<UThumbnailInfo>			ExistingThumbnailInfo;

	TArray<UClothingAssetBase*>				ExistingClothingAssets;

	bool UseMaterialNameSlotWorkflow;
	//The existing import material data (the state of sections before the reimport)
	TArray<FName> ExistingImportMaterialOriginalNameData;
	TArray<TArray<ExistingMeshLodSectionData>> ExistingImportMeshLodSectionMaterialData;
	//The last import material data (fbx original data before user changes)
	TArray<FName> LastImportMaterialOriginalNameData;
	TArray<TArray<FName>> LastImportMeshLodSectionMaterialData;

	FSkeletalMeshSamplingInfo				ExistingSamplingInfo;
	FPerPlatformInt							MinLOD;
};

/** 
 * Optional data passed in when importing a skeletal mesh LDO
 */
class FSkelMeshOptionalImportData
{
public:
	FSkelMeshOptionalImportData() {}

	/** extra data used for importing extra weight/bone influences */
	FSkeletalMeshImportData RawMeshInfluencesData;
	int32 MaxBoneCountPerChunk;
};

/**
* Data needed for importing an extra set of vertex influences
*/
struct FSkelMeshExtraInfluenceImportData
{
	FReferenceSkeleton		RefSkeleton;
	TArray<SkeletalMeshImportData::FVertInfluence> Influences;
	TArray<SkeletalMeshImportData::FMeshWedge> Wedges;
	TArray<SkeletalMeshImportData::FMeshFace> Faces;
	TArray<FVector> Points;
	int32 MaxBoneCountPerChunk;
};