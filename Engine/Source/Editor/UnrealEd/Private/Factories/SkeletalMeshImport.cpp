// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalMeshImport.cpp: Skeletal mesh import code.
=============================================================================*/

#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include "Misc/FeedbackContext.h"
#include "Modules/ModuleManager.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Materials/MaterialInterface.h"
#include "GPUSkinPublicDefs.h"
#include "ReferenceSkeleton.h"
#include "Engine/SkeletalMesh.h"
#include "EditorFramework/ThumbnailInfo.h"
#include "SkelImport.h"
#include "RawIndexBuffer.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Logging/TokenizedMessage.h"
#include "FbxImporter.h"
#include "Misc/FbxErrors.h"
#include "Engine/SkeletalMeshSocket.h"
#include "LODUtilities.h"
#include "UObject/Package.h"
#include "MeshUtilities.h"
#include "ClothingAssetInterface.h"
#include "Factories/FbxSkeletalMeshImportData.h"
#include "IMeshReductionManagerModule.h"
#include "Rendering/SkeletalMeshModel.h"

DEFINE_LOG_CATEGORY_STATIC(LogSkeletalMeshImport, Log, All);

#define LOCTEXT_NAMESPACE "SkeletalMeshImport"

/** Check that root bone is the same, and that any bones that are common have the correct parent. */
bool SkeletonsAreCompatible( const FReferenceSkeleton& NewSkel, const FReferenceSkeleton& ExistSkel, bool bFailNoError)
{
	if(NewSkel.GetBoneName(0) != ExistSkel.GetBoneName(0))
	{
		if (!bFailNoError)
		{
			UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
			FFbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("MeshHasDifferentRoot", "Root Bone is '{0}' instead of '{1}'.\nDiscarding existing LODs."),
				FText::FromName(NewSkel.GetBoneName(0)), FText::FromName(ExistSkel.GetBoneName(0)))), FFbxErrors::SkeletalMesh_DifferentRoots);
		}
		return false;
	}

	for(int32 i=1; i<NewSkel.GetRawBoneNum(); i++)
	{
		// See if bone is in both skeletons.
		int32 NewBoneIndex = i;
		FName NewBoneName = NewSkel.GetBoneName(NewBoneIndex);
		int32 BBoneIndex = ExistSkel.FindBoneIndex(NewBoneName);

		// If it is, check parents are the same.
		if(BBoneIndex != INDEX_NONE)
		{
			FName NewParentName = NewSkel.GetBoneName( NewSkel.GetParentIndex(NewBoneIndex) );
			FName ExistParentName = ExistSkel.GetBoneName( ExistSkel.GetParentIndex(BBoneIndex) );

			if(NewParentName != ExistParentName)
			{
				if (!bFailNoError)
				{
					UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
					FFbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("MeshHasDifferentRoot", "Root Bone is '{0}' instead of '{1}'.\nDiscarding existing LODs."),
						FText::FromName(NewBoneName), FText::FromName(NewParentName))), FFbxErrors::SkeletalMesh_DifferentRoots);
				}
				return false;
			}
		}
	}

	return true;
}

/**
* Process and fill in the mesh Materials using the raw binary import data
* 
* @param Materials - [out] array of materials to update
* @param ImportData - raw binary import data to process
*/
void ProcessImportMeshMaterials(TArray<FSkeletalMaterial>& Materials, FSkeletalMeshImportData& ImportData )
{
	TArray <SkeletalMeshImportData::FMaterial>&	ImportedMaterials = ImportData.Materials;

	// If direct linkup of materials is requested, try to find them here - to get a texture name from a 
	// material name, cut off anything in front of the dot (beyond are special flags).
	Materials.Empty();
	int32 SkinOffset = INDEX_NONE;
	for( int32 MatIndex=0; MatIndex < ImportedMaterials.Num(); ++MatIndex)
	{			
		const SkeletalMeshImportData::FMaterial& ImportedMaterial = ImportedMaterials[MatIndex];

		UMaterialInterface* Material = NULL;
		FString MaterialNameNoSkin = ImportedMaterial.MaterialImportName;
		if( ImportedMaterial.Material.IsValid() )
		{
			Material = ImportedMaterial.Material.Get();
		}
		else
		{
			const FString& MaterialName = ImportedMaterial.MaterialImportName;
			MaterialNameNoSkin = MaterialName;
			Material = FindObject<UMaterialInterface>(ANY_PACKAGE, *MaterialName);
			if (Material == nullptr)
			{
				SkinOffset = MaterialName.Find(TEXT("_skin"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
				if (SkinOffset != INDEX_NONE)
				{
					FString SkinXXNumber = MaterialName.Right(MaterialName.Len() - (SkinOffset + 1)).RightChop(4);
					if (SkinXXNumber.IsNumeric())
					{
						MaterialNameNoSkin = MaterialName.LeftChop(MaterialName.Len() - SkinOffset);
						Material = FindObject<UMaterialInterface>(ANY_PACKAGE, *MaterialNameNoSkin);
					}
				}
			}
		}

		const bool bEnableShadowCasting = true;
		Materials.Add( FSkeletalMaterial( Material, bEnableShadowCasting, false, Material != nullptr ? Material->GetFName() : FName(*MaterialNameNoSkin), FName(*(ImportedMaterial.MaterialImportName)) ) );
	}

	int32 NumMaterialsToAdd = FMath::Max<int32>( ImportedMaterials.Num(), ImportData.MaxMaterialIndex + 1 );

	// Pad the material pointers
	while( NumMaterialsToAdd > Materials.Num() )
	{
		Materials.Add( FSkeletalMaterial( NULL, true, false, NAME_None, NAME_None ) );
	}
}

/**
* Process and fill in the mesh ref skeleton bone hierarchy using the raw binary import data
* 
* @param RefSkeleton - [out] reference skeleton hierarchy to update
* @param SkeletalDepth - [out] depth of the reference skeleton hierarchy
* @param ImportData - raw binary import data to process
* @return true if the operation completed successfully
*/
bool ProcessImportMeshSkeleton(const USkeleton* SkeletonAsset, FReferenceSkeleton& RefSkeleton, int32& SkeletalDepth, FSkeletalMeshImportData& ImportData)
{
	TArray <SkeletalMeshImportData::FBone>&	RefBonesBinary = ImportData.RefBonesBinary;

	// Setup skeletal hierarchy + names structure.
	RefSkeleton.Empty();

	FReferenceSkeletonModifier RefSkelModifier(RefSkeleton, SkeletonAsset);

	// Digest bones to the serializable format.
	for( int32 b=0; b<RefBonesBinary.Num(); b++ )
	{
		const SkeletalMeshImportData::FBone & BinaryBone = RefBonesBinary[ b ];
		const FString BoneName = FSkeletalMeshImportData::FixupBoneName( BinaryBone.Name );
		const FMeshBoneInfo BoneInfo(FName(*BoneName, FNAME_Add), BinaryBone.Name, BinaryBone.ParentIndex);
		const FTransform BoneTransform(BinaryBone.BonePos.Transform);

		if(RefSkeleton.FindRawBoneIndex(BoneInfo.Name) != INDEX_NONE)
		{
			UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
			FFbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("SkeletonHasDuplicateBones", "Skeleton has non-unique bone names.\nBone named '{0}' encountered more than once."), FText::FromName(BoneInfo.Name))), FFbxErrors::SkeletalMesh_DuplicateBones);
			return false;
		}

		RefSkelModifier.Add(BoneInfo, BoneTransform);
	}

	// Add hierarchy index to each bone and detect max depth.
	SkeletalDepth = 0;

	TArray<int32> SkeletalDepths;
	SkeletalDepths.Empty( RefBonesBinary.Num() );
	SkeletalDepths.AddZeroed( RefBonesBinary.Num() );
	for( int32 b=0; b < RefSkeleton.GetRawBoneNum(); b++ )
	{
		int32 Parent	= RefSkeleton.GetRawParentIndex(b);
		int32 Depth	= 1.0f;

		SkeletalDepths[b]	= 1.0f;
		if( Parent != INDEX_NONE )
		{
			Depth += SkeletalDepths[Parent];
		}
		if( SkeletalDepth < Depth )
		{
			SkeletalDepth = Depth;
		}
		SkeletalDepths[b] = Depth;
	}

	return true;
}

/**
* Process and update the vertex Influences using the raw binary import data
* 
* @param ImportData - raw binary import data to process
*/
void ProcessImportMeshInfluences(FSkeletalMeshImportData& ImportData)
{
	TArray <FVector>& Points = ImportData.Points;
	TArray <SkeletalMeshImportData::FVertex>& Wedges = ImportData.Wedges;
	TArray <SkeletalMeshImportData::FRawBoneInfluence>& Influences = ImportData.Influences;

	// Sort influences by vertex index.
	struct FCompareVertexIndex
	{
		bool operator()( const SkeletalMeshImportData::FRawBoneInfluence& A, const SkeletalMeshImportData::FRawBoneInfluence& B ) const
		{
			if		( A.VertexIndex > B.VertexIndex	) return false;
			else if ( A.VertexIndex < B.VertexIndex	) return true;
			else if ( A.Weight      < B.Weight		) return false;
			else if ( A.Weight      > B.Weight		) return true;
			else if ( A.BoneIndex   > B.BoneIndex	) return false;
			else if ( A.BoneIndex   < B.BoneIndex	) return true;
			else									  return  false;	
		}
	};
	Influences.Sort( FCompareVertexIndex() );

	TArray <SkeletalMeshImportData::FRawBoneInfluence> NewInfluences;
	int32	LastNewInfluenceIndex=0;
	int32	LastVertexIndex		= INDEX_NONE;
	int32	InfluenceCount			= 0;

	float TotalWeight		= 0.f;
	const float MINWEIGHT   = 0.01f;

	int MaxVertexInfluence = 0;
	float MaxIgnoredWeight = 0.0f;

	//We have to normalize the data before filtering influences
	//Because influence filtering is base on the normalize value.
	//Some DCC like Daz studio don't have normalized weight
	for (int32 i = 0; i < Influences.Num(); i++)
	{
		// if less than min weight, or it's more than 8, then we clear it to use weight
		InfluenceCount++;
		TotalWeight += Influences[i].Weight;
		// we have all influence for the same vertex, normalize it now
		if (i + 1 >= Influences.Num() || Influences[i].VertexIndex != Influences[i+1].VertexIndex)
		{
			// Normalize the last set of influences.
			if (InfluenceCount && (TotalWeight != 1.0f))
			{
				float OneOverTotalWeight = 1.f / TotalWeight;
				for (int r = 0; r < InfluenceCount; r++)
				{
					Influences[i - r].Weight *= OneOverTotalWeight;
				}
			}
			
			if (MaxVertexInfluence < InfluenceCount)
			{
				MaxVertexInfluence = InfluenceCount;
			}

			// clear to count next one
			InfluenceCount = 0;
			TotalWeight = 0.f;
		}

		if (InfluenceCount > MAX_TOTAL_INFLUENCES &&  Influences[i].Weight > MaxIgnoredWeight)
		{
			MaxIgnoredWeight = Influences[i].Weight;
		}
	}
 
 	// warn about too many influences
	if (MaxVertexInfluence > MAX_TOTAL_INFLUENCES)
	{
		UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
		FFbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("WarningTooManySkelInfluences", "Warning skeletal mesh influence count of {0} exceeds max count of {1}. Influence truncation will occur. Maximum Ignored Weight {2}"), MaxVertexInfluence, MAX_TOTAL_INFLUENCES, MaxIgnoredWeight)), FFbxErrors::SkeletalMesh_TooManyInfluences);
	}

	for( int32 i=0; i<Influences.Num(); i++ )
	{
		// we found next verts, normalize it now
		if (LastVertexIndex != Influences[i].VertexIndex )
		{
			// Normalize the last set of influences.
			if (InfluenceCount && (TotalWeight != 1.0f))
			{
				float OneOverTotalWeight = 1.f / TotalWeight;
				for (int r = 0; r < InfluenceCount; r++)
				{
					NewInfluences[LastNewInfluenceIndex - r].Weight *= OneOverTotalWeight;
				}
			}

			// now we insert missing verts
			if (LastVertexIndex != INDEX_NONE)
			{
				int32 CurrentVertexIndex = Influences[i].VertexIndex;
				for(int32 j=LastVertexIndex+1; j<CurrentVertexIndex; j++)
				{
					// Add a 0-bone weight if none other present (known to happen with certain MAX skeletal setups).
					LastNewInfluenceIndex = NewInfluences.AddUninitialized();
					NewInfluences[LastNewInfluenceIndex].VertexIndex	= j;
					NewInfluences[LastNewInfluenceIndex].BoneIndex		= 0;
					NewInfluences[LastNewInfluenceIndex].Weight		= 1.f;
				}
			}

			// clear to count next one
			InfluenceCount = 0;
			TotalWeight = 0.f;
			LastVertexIndex = Influences[i].VertexIndex;
		}
		
		// if less than min weight, or it's more than 8, then we clear it to use weight
		if (Influences[i].Weight > MINWEIGHT && InfluenceCount < MAX_TOTAL_INFLUENCES)
		{
			LastNewInfluenceIndex = NewInfluences.Add(Influences[i]);
			InfluenceCount++;
			TotalWeight	+= Influences[i].Weight;
		}
	}

	Influences = NewInfluences;

	// Ensure that each vertex has at least one influence as e.g. CreateSkinningStream relies on it.
	// The below code relies on influences being sorted by vertex index.
	if( Influences.Num() == 0 )
	{
		UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
		// warn about no influences
		FFbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("WarningNoSkelInfluences", "Warning skeletal mesh is has no vertex influences")), FFbxErrors::SkeletalMesh_NoInfluences);
		// add one for each wedge entry
		Influences.AddUninitialized(Wedges.Num());
		for( int32 WedgeIdx=0; WedgeIdx<Wedges.Num(); WedgeIdx++ )
		{	
			Influences[WedgeIdx].VertexIndex = WedgeIdx;
			Influences[WedgeIdx].BoneIndex = 0;
			Influences[WedgeIdx].Weight = 1.0f;
		}		
		for(int32 i=0; i<Influences.Num(); i++)
		{
			int32 CurrentVertexIndex = Influences[i].VertexIndex;

			if(LastVertexIndex != CurrentVertexIndex)
			{
				for(int32 j=LastVertexIndex+1; j<CurrentVertexIndex; j++)
				{
					// Add a 0-bone weight if none other present (known to happen with certain MAX skeletal setups).
					Influences.InsertUninitialized(i, 1);
					Influences[i].VertexIndex	= j;
					Influences[i].BoneIndex		= 0;
					Influences[i].Weight		= 1.f;
				}
				LastVertexIndex = CurrentVertexIndex;
			}
		}
	}
}

bool SkeletalMeshIsUsingMaterialSlotNameWorkflow(UAssetImportData* AssetImportData)
{
	UFbxSkeletalMeshImportData* ImportData = Cast<UFbxSkeletalMeshImportData>(AssetImportData);
	if (ImportData == nullptr || ImportData->ImportMaterialOriginalNameData.Num() <= 0)
	{
		return false;
	}
	bool AllNameAreNone = true;
	for (FName ImportMaterialName : ImportData->ImportMaterialOriginalNameData)
	{
		if (ImportMaterialName != NAME_None)
		{
			AllNameAreNone = false;
			break;
		}
	}
	return !AllNameAreNone;
}

ExistingSkelMeshData* SaveExistingSkelMeshData(USkeletalMesh* ExistingSkelMesh, bool bSaveMaterials, int32 ReimportLODIndex)
{
	struct ExistingSkelMeshData* ExistingMeshDataPtr = NULL;
	if(ExistingSkelMesh)
	{
		bool ReimportSpecificLOD = (ReimportLODIndex > 0) && ExistingSkelMesh->GetLODNum() > ReimportLODIndex;

		ExistingMeshDataPtr = new ExistingSkelMeshData();
		
		ExistingMeshDataPtr->UseMaterialNameSlotWorkflow = SkeletalMeshIsUsingMaterialSlotNameWorkflow(ExistingSkelMesh->AssetImportData);
		ExistingMeshDataPtr->MinLOD = ExistingSkelMesh->MinLod;

		FSkeletalMeshModel* ImportedResource = ExistingSkelMesh->GetImportedModel();

		//Add the existing Material slot name data
		for (int32 MaterialIndex = 0; MaterialIndex < ExistingSkelMesh->Materials.Num(); ++MaterialIndex)
		{
			ExistingMeshDataPtr->ExistingImportMaterialOriginalNameData.Add(ExistingSkelMesh->Materials[MaterialIndex].ImportedMaterialSlotName);
		}

		for (int32 LodIndex = 0; LodIndex < ImportedResource->LODModels.Num(); ++LodIndex)
		{
			ExistingMeshDataPtr->ExistingImportMeshLodSectionMaterialData.AddZeroed();
			for (int32 SectionIndex = 0; SectionIndex < ImportedResource->LODModels[LodIndex].Sections.Num(); ++SectionIndex)
			{
				int32 SectionMaterialIndex = ImportedResource->LODModels[LodIndex].Sections[SectionIndex].MaterialIndex;
				bool SectionCastShadow = ImportedResource->LODModels[LodIndex].Sections[SectionIndex].bCastShadow;
				bool SectionRecomputeTangents = ImportedResource->LODModels[LodIndex].Sections[SectionIndex].bRecomputeTangent;
				int32 GenerateUpTo = ImportedResource->LODModels[LodIndex].Sections[SectionIndex].GenerateUpToLodIndex;
				if (ExistingMeshDataPtr->ExistingImportMaterialOriginalNameData.IsValidIndex(SectionMaterialIndex))
				{
					ExistingMeshDataPtr->ExistingImportMeshLodSectionMaterialData[LodIndex].Add(ExistingMeshLodSectionData(ExistingMeshDataPtr->ExistingImportMaterialOriginalNameData[SectionMaterialIndex], SectionCastShadow, SectionRecomputeTangents, GenerateUpTo));
				}
			}
		}

		ExistingMeshDataPtr->ExistingSockets = ExistingSkelMesh->GetMeshOnlySocketList();
		ExistingMeshDataPtr->bSaveRestoreMaterials = bSaveMaterials;
		if (ExistingMeshDataPtr->bSaveRestoreMaterials)
		{
			ExistingMeshDataPtr->ExistingMaterials = ExistingSkelMesh->Materials;
		}
		ExistingMeshDataPtr->ExistingRetargetBasePose = ExistingSkelMesh->RetargetBasePose;

		if( ImportedResource->LODModels.Num() > 0 &&
			ExistingSkelMesh->GetLODNum() == ImportedResource->LODModels.Num() )
		{
			// Remove the zero'th LOD (ie: the LOD being reimported).
			if (!ReimportSpecificLOD)
			{
				ImportedResource->LODModels.RemoveAt(0);
				ExistingSkelMesh->RemoveLODInfo(0);
			}

			// Copy off the remaining LODs.
			for ( int32 LODModelIndex = 0 ; LODModelIndex < ImportedResource->LODModels.Num() ; ++LODModelIndex )
			{
				FSkeletalMeshLODModel& LODModel = ImportedResource->LODModels[LODModelIndex];
				LODModel.RawPointIndices.Lock( LOCK_READ_ONLY );
				LODModel.LegacyRawPointIndices.Lock( LOCK_READ_ONLY );
				LODModel.RawSkeletalMeshBulkData.GetBulkData().Lock( LOCK_READ_ONLY );
			}
			ExistingMeshDataPtr->ExistingLODModels = ImportedResource->LODModels;
			for ( auto& LODModel : ImportedResource->LODModels )
			{
				LODModel.RawPointIndices.Unlock();
				LODModel.LegacyRawPointIndices.Unlock();
				LODModel.RawSkeletalMeshBulkData.GetBulkData().Unlock();
			}

			ExistingMeshDataPtr->ExistingLODInfo = ExistingSkelMesh->GetLODInfoArray();
			ExistingMeshDataPtr->ExistingRefSkeleton = ExistingSkelMesh->RefSkeleton;
		
		}

		// First asset should be the one that the skeletal mesh should point too
		ExistingMeshDataPtr->ExistingPhysicsAssets.Empty();
		ExistingMeshDataPtr->ExistingPhysicsAssets.Add( ExistingSkelMesh->PhysicsAsset );
		for (TObjectIterator<UPhysicsAsset> It; It; ++It)
		{
			UPhysicsAsset* PhysicsAsset = *It;
			if ( PhysicsAsset->PreviewSkeletalMesh == ExistingSkelMesh && ExistingSkelMesh->PhysicsAsset != PhysicsAsset )
			{
				ExistingMeshDataPtr->ExistingPhysicsAssets.Add( PhysicsAsset );
			}
		}

		ExistingMeshDataPtr->ExistingShadowPhysicsAsset = ExistingSkelMesh->ShadowPhysicsAsset;

		ExistingMeshDataPtr->ExistingSkeleton = ExistingSkelMesh->Skeleton;
		// since copying back original skeleton, this shoudl be safe to do
		ExistingMeshDataPtr->ExistingPostProcessAnimBlueprint = ExistingSkelMesh->PostProcessAnimBlueprint;

		ExistingMeshDataPtr->ExistingLODSettings = ExistingSkelMesh->LODSettings;

		ExistingSkelMesh->ExportMirrorTable(ExistingMeshDataPtr->ExistingMirrorTable);

		ExistingMeshDataPtr->ExistingMorphTargets.Empty(ExistingSkelMesh->MorphTargets.Num());
		ExistingMeshDataPtr->ExistingMorphTargets.Append(ExistingSkelMesh->MorphTargets);
	
		ExistingMeshDataPtr->bExistingUseFullPrecisionUVs = ExistingSkelMesh->bUseFullPrecisionUVs;
		ExistingMeshDataPtr->bExistingUseHighPrecisionTangentBasis = ExistingSkelMesh->bUseHighPrecisionTangentBasis;

		ExistingMeshDataPtr->ExistingAssetImportData = ExistingSkelMesh->AssetImportData;
		ExistingMeshDataPtr->ExistingThumbnailInfo = ExistingSkelMesh->ThumbnailInfo;

		ExistingMeshDataPtr->ExistingClothingAssets = ExistingSkelMesh->MeshClothingAssets;

		ExistingMeshDataPtr->ExistingSamplingInfo = ExistingSkelMesh->GetSamplingInfo();

		//Add the last fbx import data
		UFbxSkeletalMeshImportData* ImportData = Cast<UFbxSkeletalMeshImportData>(ExistingSkelMesh->AssetImportData);
		if (ImportData && ExistingMeshDataPtr->UseMaterialNameSlotWorkflow)
		{
			for (int32 ImportMaterialOriginalNameDataIndex = 0; ImportMaterialOriginalNameDataIndex < ImportData->ImportMaterialOriginalNameData.Num(); ++ImportMaterialOriginalNameDataIndex)
			{
				FName MaterialName = ImportData->ImportMaterialOriginalNameData[ImportMaterialOriginalNameDataIndex];
				ExistingMeshDataPtr->LastImportMaterialOriginalNameData.Add(MaterialName);
			}
			for (int32 LodIndex = 0; LodIndex < ImportData->ImportMeshLodData.Num(); ++LodIndex)
			{
				ExistingMeshDataPtr->LastImportMeshLodSectionMaterialData.AddZeroed();
				const FImportMeshLodSectionsData &ImportMeshLodSectionsData = ImportData->ImportMeshLodData[LodIndex];
				for (int32 SectionIndex = 0; SectionIndex < ImportMeshLodSectionsData.SectionOriginalMaterialName.Num(); ++SectionIndex)
				{
					FName MaterialName = ImportMeshLodSectionsData.SectionOriginalMaterialName[SectionIndex];
					ExistingMeshDataPtr->LastImportMeshLodSectionMaterialData[LodIndex].Add(MaterialName);
				}
			}
		}
	}

	return ExistingMeshDataPtr;
}

void TryRegenerateLODs(ExistingSkelMeshData* MeshData, USkeletalMesh* SkeletalMesh)
{
	int32 TotalLOD = MeshData->ExistingLODModels.Num();
	// see if mesh reduction util is available
	IMeshReductionManagerModule& Module = FModuleManager::Get().LoadModuleChecked<IMeshReductionManagerModule>("MeshReductionInterface");
	static bool bAutoMeshReductionAvailable = Module.GetSkeletalMeshReductionInterface() != NULL;

	if (bAutoMeshReductionAvailable)
	{
		GWarn->BeginSlowTask(LOCTEXT("RegenLODs", "Generating new LODs"), true);
		FSkeletalMeshUpdateContext UpdateContext;
		UpdateContext.SkeletalMesh = SkeletalMesh;
		TArray<bool> Dependencies;
		Dependencies.AddZeroed(TotalLOD+1);
		Dependencies[0] = true;
		for (int32 Index = 0; Index < TotalLOD; ++Index)
		{
			int32 LODIndex = Index + 1;
			if (LODIndex >= SkeletalMesh->GetLODInfoArray().Num())
			{
				FSkeletalMeshLODInfo& ExistLODInfo = MeshData->ExistingLODInfo[Index];
				// reset material maps, it won't work anyway. 
				ExistLODInfo.LODMaterialMap.Empty();
				// add LOD info back
				SkeletalMesh->AddLODInfo(ExistLODInfo);
				check(LODIndex < SkeletalMesh->GetLODInfoArray().Num());
			}
			const FSkeletalMeshLODInfo* LODInfo = SkeletalMesh->GetLODInfo(LODIndex);
			if (LODInfo && LODInfo->bHasBeenSimplified && Dependencies[LODInfo->ReductionSettings.BaseLOD])
			{
				Dependencies[LODIndex] = true;
				// force it to regenerate
				FLODUtilities::SimplifySkeletalMeshLOD(UpdateContext, LODIndex, false);
			}
		}
		GWarn->EndSlowTask();
	}
	else
	{
		UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
		FFbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("NoCompatibleSkeleton", "New base mesh is not compatible with previous LODs. LOD will be removed.")), FFbxErrors::SkeletalMesh_LOD_MissingBone);
	}
}

void FindNearestWedgeIndexes(const TWedgeInfoPosOctree& WedgePosOctree, const FVector& SearchPosition, TArray<FWedgeInfo>& OutNearestWedges)
{
	OutNearestWedges.Empty();
	TWedgeInfoPosOctree::TConstIterator<> OctreeIter(WedgePosOctree);
	// Iterate through the octree attempting to find the vertices closest to the current new point
	while (OctreeIter.HasPendingNodes())
	{
		const TWedgeInfoPosOctree::FNode& CurNode = OctreeIter.GetCurrentNode();
		const FOctreeNodeContext& CurContext = OctreeIter.GetCurrentContext();

		// Find the child of the current node, if any, that contains the current new point
		FOctreeChildNodeRef ChildRef = CurContext.GetContainingChild(FBoxCenterAndExtent(SearchPosition, FVector::ZeroVector));

		if (!ChildRef.IsNULL())
		{
			const TWedgeInfoPosOctree::FNode* ChildNode = CurNode.GetChild(ChildRef);

			// If the specified child node exists and contains any of the old vertices, push it to the iterator for future consideration
			if (ChildNode && ChildNode->GetInclusiveElementCount() > 0)
			{
				OctreeIter.PushChild(ChildRef);
			}
			// If the child node doesn't have any of the old vertices in it, it's not worth pursuing any further. In an attempt to find
			// anything to match vs. the new point, add all of the children of the current octree node that have old points in them to the
			// iterator for future consideration.
			else
			{
				FOREACH_OCTREE_CHILD_NODE(OctreeChildRef)
				{
					if (CurNode.HasChild(OctreeChildRef))
					{
						OctreeIter.PushChild(OctreeChildRef);
					}
				}
			}
		}

		// Add all of the elements in the current node to the list of points to consider for closest point calculations
		OutNearestWedges.Append(CurNode.GetElements());
		OctreeIter.Advance();
	}
}

namespace SkeletalMeshHelper
{
	void ApplySkinning(USkeletalMesh* SkeletalMesh, FSkeletalMeshLODModel& SrcLODModel, FSkeletalMeshLODModel& DestLODModel)
	{
		TArray<FSoftSkinVertex> SrcVertices;
		SrcLODModel.GetVertices(SrcVertices);

		FBox OldBounds(EForceInit::ForceInit);
		for (int32 SrcIndex = 0; SrcIndex < SrcVertices.Num(); ++SrcIndex)
		{
			const FSoftSkinVertex& SrcVertex = SrcVertices[SrcIndex];
			OldBounds += SrcVertex.Position;
		}

		TWedgeInfoPosOctree SrcWedgePosOctree(OldBounds.GetCenter(), OldBounds.GetExtent().GetMax());

		// Add each old vertex to the octree
		for (int32 SrcIndex = 0; SrcIndex < SrcVertices.Num(); ++SrcIndex)
		{
			FWedgeInfo WedgeInfo;
			WedgeInfo.WedgeIndex = SrcIndex;
			WedgeInfo.Position = SrcVertices[SrcIndex].Position;
			SrcWedgePosOctree.AddElement(WedgeInfo);
		}
		TArray<FBoneIndexType> RequiredActiveBones;

		bool bUseBone = false;
		for (int32 SectionIndex = 0; SectionIndex < DestLODModel.Sections.Num(); SectionIndex++)
		{
			FSkelMeshSection& Section = DestLODModel.Sections[SectionIndex];
			Section.BoneMap.Reset();
			for (FSoftSkinVertex& DestVertex : Section.SoftVertices)
			{
				//Find the nearest wedges in the src model
				TArray<FWedgeInfo> NearestSrcWedges;
				FindNearestWedgeIndexes(SrcWedgePosOctree, DestVertex.Position, NearestSrcWedges);
				if (NearestSrcWedges.Num() < 1)
				{
					//Should we check???
					continue;
				}
				//Find the matching wedges in the src model
				int32 MatchingSrcWedge = INDEX_NONE;
				for (FWedgeInfo& SrcWedgeInfo : NearestSrcWedges)
				{
					int32 SrcIndex = SrcWedgeInfo.WedgeIndex;
					const FSoftSkinVertex& SrcVertex = SrcVertices[SrcIndex];
					if (SrcVertex.Position.Equals(DestVertex.Position, THRESH_POINTS_ARE_SAME) &&
						SrcVertex.UVs[0].Equals(DestVertex.UVs[0], THRESH_UVS_ARE_SAME) &&
						(SrcVertex.TangentX == DestVertex.TangentX) &&
						(SrcVertex.TangentY == DestVertex.TangentY) &&
						(SrcVertex.TangentZ == DestVertex.TangentZ))
					{
						MatchingSrcWedge = SrcIndex;
						break;
					}
				}
				if (MatchingSrcWedge == INDEX_NONE)
				{
					//We have to find the nearest wedges, then find the most similar normal
					float MinDistance = MAX_FLT;
					float MinNormalAngle = MAX_FLT;
					for (FWedgeInfo& SrcWedgeInfo : NearestSrcWedges)
					{
						int32 SrcIndex = SrcWedgeInfo.WedgeIndex;
						const FSoftSkinVertex& SrcVertex = SrcVertices[SrcIndex];
						float VectorDelta = FVector::DistSquared(SrcVertex.Position, DestVertex.Position);
						if (VectorDelta <= MinDistance)
						{
							if (VectorDelta < MinDistance)
							{
								MinDistance = VectorDelta;
								MinNormalAngle = MAX_FLT;
							}
							FVector DestTangentZ = DestVertex.TangentZ;
							DestTangentZ.Normalize();
							FVector SrcTangentZ = SrcVertex.TangentZ;
							SrcTangentZ.Normalize();
							float AngleDiff = FMath::Abs(FMath::Acos(FVector::DotProduct(DestTangentZ, SrcTangentZ)));
							if (AngleDiff < MinNormalAngle)
							{
								MinNormalAngle = AngleDiff;
								MatchingSrcWedge = SrcIndex;
							}
						}
					}
				}
				check(SrcVertices.IsValidIndex(MatchingSrcWedge));
				const FSoftSkinVertex& SrcVertex = SrcVertices[MatchingSrcWedge];

				//Find the src section to assign the correct remapped bone
				int32 SrcSectionIndex = INDEX_NONE;
				int32 SrcSectionWedgeIndex = INDEX_NONE;
				SrcLODModel.GetSectionFromVertexIndex(MatchingSrcWedge, SrcSectionIndex, SrcSectionWedgeIndex);
				check(SrcSectionIndex != INDEX_NONE);

				for (int32 InfluenceIndex = 0; InfluenceIndex < MAX_TOTAL_INFLUENCES; ++InfluenceIndex)
				{
					if (SrcVertex.InfluenceWeights[InfluenceIndex] > 0.0f)
					{
						Section.MaxBoneInfluences = FMath::Max(Section.MaxBoneInfluences, InfluenceIndex+1);
						//Copy the weight
						DestVertex.InfluenceWeights[InfluenceIndex] = SrcVertex.InfluenceWeights[InfluenceIndex];
						//Copy the bone ID
						FBoneIndexType OriginalBoneIndex = SrcLODModel.Sections[SrcSectionIndex].BoneMap[SrcVertex.InfluenceBones[InfluenceIndex]];
						int32 OverrideIndex;
						if (Section.BoneMap.Find(OriginalBoneIndex, OverrideIndex))
						{
							DestVertex.InfluenceBones[InfluenceIndex] = OverrideIndex;
						}
						else
						{
							DestVertex.InfluenceBones[InfluenceIndex] = Section.BoneMap.Add(OriginalBoneIndex);
							DestLODModel.ActiveBoneIndices.AddUnique(OriginalBoneIndex);
						}
						bUseBone = true;
					}
				}
			}
		}

		if (bUseBone)
		{
			//Set the required/active bones
			DestLODModel.RequiredBones = SrcLODModel.RequiredBones;
			DestLODModel.RequiredBones.Sort();
			SkeletalMesh->RefSkeleton.EnsureParentsExistAndSort(DestLODModel.ActiveBoneIndices);
		}
	}
} //namespace SkeletalMeshHelper

void RestoreExistingSkelMeshData(ExistingSkelMeshData* MeshData, USkeletalMesh* SkeletalMesh, int32 ReimportLODIndex, bool bCanShowDialog, bool bImportSkinningOnly)
{
	if (!MeshData || !SkeletalMesh)
	{
		return;
	}

	SkeletalMesh->MinLod = MeshData->MinLOD;

	//Create a remap material Index use to find the matching section later
	TArray<int32> RemapMaterial;
	RemapMaterial.AddZeroed(SkeletalMesh->Materials.Num());
	TArray<FName> RemapMaterialName;
	RemapMaterialName.AddZeroed(SkeletalMesh->Materials.Num());
	
	bool bMaterialReset = false;
	if (MeshData->bSaveRestoreMaterials)
	{
		UnFbx::EFBXReimportDialogReturnOption ReturnOption;
		//Ask the user to match the materials conflict
		UnFbx::FFbxImporter::PrepareAndShowMaterialConflictDialog<FSkeletalMaterial>(MeshData->ExistingMaterials, SkeletalMesh->Materials, RemapMaterial, RemapMaterialName, bCanShowDialog, false, ReturnOption);
		
		if (ReturnOption != UnFbx::EFBXReimportDialogReturnOption::FBXRDRO_ResetToFbx)
		{
			//Build a ordered material list that try to keep intact the existing material list
			TArray<FSkeletalMaterial> MaterialOrdered;
			TArray<bool> MatchedNewMaterial;
			MatchedNewMaterial.AddZeroed(SkeletalMesh->Materials.Num());
			for (int32 ExistMaterialIndex = 0; ExistMaterialIndex < MeshData->ExistingMaterials.Num(); ++ExistMaterialIndex)
			{
				int32 MaterialIndexOrdered = MaterialOrdered.Add(MeshData->ExistingMaterials[ExistMaterialIndex]);
				FSkeletalMaterial& OrderedMaterial = MaterialOrdered[MaterialIndexOrdered];
				int32 NewMaterialIndex = INDEX_NONE;
				if (RemapMaterial.Find(ExistMaterialIndex, NewMaterialIndex))
				{
					MatchedNewMaterial[NewMaterialIndex] = true;
					RemapMaterial[NewMaterialIndex] = MaterialIndexOrdered;
					OrderedMaterial.ImportedMaterialSlotName = SkeletalMesh->Materials[NewMaterialIndex].ImportedMaterialSlotName;
				}
				else
				{
					//Unmatched material must be conserve
				}
			}

			//Add the new material entries (the one that do not match with any existing material)
			for (int32 NewMaterialIndex = 0; NewMaterialIndex < MatchedNewMaterial.Num(); ++NewMaterialIndex)
			{
				if (MatchedNewMaterial[NewMaterialIndex] == false)
				{
					int32 NewMeshIndex = MaterialOrdered.Add(SkeletalMesh->Materials[NewMaterialIndex]);
					RemapMaterial[NewMaterialIndex] = NewMeshIndex;
				}
			}

			//Set the RemapMaterialName array helper
			for (int32 MaterialIndex = 0; MaterialIndex < RemapMaterial.Num(); ++MaterialIndex)
			{
				int32 SourceMaterialMatch = RemapMaterial[MaterialIndex];
				if (MeshData->ExistingMaterials.IsValidIndex(SourceMaterialMatch))
				{
					RemapMaterialName[MaterialIndex] = MeshData->ExistingMaterials[SourceMaterialMatch].ImportedMaterialSlotName;
				}
			}

			//Copy the re ordered materials (this ensure the material array do not change when we re-import)
			SkeletalMesh->Materials = MaterialOrdered;
		}
	}

	SkeletalMesh->LODSettings = MeshData->ExistingLODSettings;
	// ensure LOD 0 contains correct setting 
	if (SkeletalMesh->LODSettings && SkeletalMesh->GetLODInfoArray().Num() > 0)
	{
		SkeletalMesh->LODSettings->SetLODSettingsToMesh(SkeletalMesh, 0);
	}

	//Do everything we need for base LOD re-import
	if (ReimportLODIndex < 1)
	{
		// this is not ideal. Ideally we'll have to save only diff with indicating which joints, 
		// but for now, we allow them to keep the previous pose IF the element count is same
		if (MeshData->ExistingRetargetBasePose.Num() == SkeletalMesh->RefSkeleton.GetRawBoneNum())
		{
			SkeletalMesh->RetargetBasePose = MeshData->ExistingRetargetBasePose;
		}

		// Assign sockets from old version of this SkeletalMesh.
		// Only copy ones for bones that exist in the new mesh.
		for (int32 i = 0; i < MeshData->ExistingSockets.Num(); i++)
		{
			const int32 BoneIndex = SkeletalMesh->RefSkeleton.FindBoneIndex(MeshData->ExistingSockets[i]->BoneName);
			if (BoneIndex != INDEX_NONE)
			{
				SkeletalMesh->GetMeshOnlySocketList().Add(MeshData->ExistingSockets[i]);
			}
		}

		// We copy back and fix-up the LODs that still work with this skeleton.
		if (MeshData->ExistingLODModels.Num() > 0)
		{
			// see if we have reduction avail
			IMeshReductionManagerModule& Module = FModuleManager::Get().LoadModuleChecked<IMeshReductionManagerModule>("MeshReductionInterface");
			static bool bAutoMeshReductionAvailable = Module.GetSkeletalMeshReductionInterface() != NULL;

			bool bRegenLODs = true;
			
			if (SkeletonsAreCompatible(SkeletalMesh->RefSkeleton, MeshData->ExistingRefSkeleton, bImportSkinningOnly))
			{
				bRegenLODs = false;
				// First create mapping table from old skeleton to new skeleton.
				TArray<int32> OldToNewMap;
				OldToNewMap.AddUninitialized(MeshData->ExistingRefSkeleton.GetRawBoneNum());
				for (int32 i = 0; i < MeshData->ExistingRefSkeleton.GetRawBoneNum(); i++)
				{
					OldToNewMap[i] = SkeletalMesh->RefSkeleton.FindBoneIndex(MeshData->ExistingRefSkeleton.GetBoneName(i));
				}

				for (int32 i = 0; i < MeshData->ExistingLODModels.Num(); i++)
				{
					FSkeletalMeshLODModel& LODModel = MeshData->ExistingLODModels[i];
					FSkeletalMeshLODInfo& LODInfo = MeshData->ExistingLODInfo[i];

					// Fix ActiveBoneIndices array.
					bool bMissingBone = false;
					FName MissingBoneName = NAME_None;
					for (int32 j = 0; j < LODModel.ActiveBoneIndices.Num() && !bMissingBone; j++)
					{
						int32 OldActiveBoneIndex = LODModel.ActiveBoneIndices[j];
						if (OldToNewMap.IsValidIndex(OldActiveBoneIndex))
						{
							int32 NewBoneIndex = OldToNewMap[OldActiveBoneIndex];
							if (NewBoneIndex == INDEX_NONE)
							{
								bMissingBone = true;
								MissingBoneName = MeshData->ExistingRefSkeleton.GetBoneName(LODModel.ActiveBoneIndices[j]);
							}
							else
							{
								LODModel.ActiveBoneIndices[j] = NewBoneIndex;
							}
						}
						else
						{
							LODModel.ActiveBoneIndices.RemoveAt(j, 1, false);
							--j;
						}
					}

					// Fix RequiredBones array.
					for (int32 j = 0; j < LODModel.RequiredBones.Num() && !bMissingBone; j++)
					{
						const int32 OldBoneIndex = LODModel.RequiredBones[j];

						if (OldToNewMap.IsValidIndex(OldBoneIndex))	//Previously virtual bones could end up in this array
																	// Must validate against this
						{
							const int32 NewBoneIndex = OldToNewMap[OldBoneIndex];
							if (NewBoneIndex == INDEX_NONE)
							{
								bMissingBone = true;
								MissingBoneName = MeshData->ExistingRefSkeleton.GetBoneName(OldBoneIndex);
							}
							else
							{
								LODModel.RequiredBones[j] = NewBoneIndex;
							}
						}
						else
						{
							//Bone didn't exist in our required bones, clean up. 
							LODModel.RequiredBones.RemoveAt(j, 1, false);
							--j;
						}
					}

					// Sort ascending for parent child relationship
					LODModel.RequiredBones.Sort();
					SkeletalMesh->RefSkeleton.EnsureParentsExistAndSort(LODModel.ActiveBoneIndices);

					// Fix the sections' BoneMaps.
					for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
					{
						FSkelMeshSection& Section = LODModel.Sections[SectionIndex];
						for (int32 BoneIndex = 0; BoneIndex < Section.BoneMap.Num(); BoneIndex++)
						{
							int32 NewBoneIndex = OldToNewMap[Section.BoneMap[BoneIndex]];
							if (NewBoneIndex == INDEX_NONE)
							{
								bMissingBone = true;
								MissingBoneName = MeshData->ExistingRefSkeleton.GetBoneName(Section.BoneMap[BoneIndex]);
								break;
							}
							else
							{
								Section.BoneMap[BoneIndex] = NewBoneIndex;
							}
						}
						if (bMissingBone)
						{
							break;
						}
					}

					if (bMissingBone)
					{
						UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
						FFbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("NewMeshMissingBoneFromLOD", "New mesh is missing bone '{0}' required by an LOD."), FText::FromName(MissingBoneName))), FFbxErrors::SkeletalMesh_LOD_MissingBone);
						bRegenLODs = true;
						break;
					}
					else
					{
						// if it has been regenerated, it try to regen and if we have reduction available 
						if (bAutoMeshReductionAvailable && LODInfo.bHasBeenSimplified && LODInfo.ReductionSettings.BaseLOD == 0)
						{
							bRegenLODs = true;
						}
						else
						{
							FSkeletalMeshLODModel* NewLODModel = new(SkeletalMesh->GetImportedModel()->LODModels) FSkeletalMeshLODModel(LODModel);
							SkeletalMesh->AddLODInfo(LODInfo);
						}
					}
				}
			}
			else if (bImportSkinningOnly)
			{
				bRegenLODs = false;
				FSkeletalMeshLODModel& BaseLodModel = SkeletalMesh->GetImportedModel()->LODModels[0];
				//Apply the new skinning on all existing LOD
				for (int32 Index = 0; Index < MeshData->ExistingLODModels.Num(); ++Index)
				{
					int32 LODIndex = Index + 1;
					FSkeletalMeshLODModel& LODModel = MeshData->ExistingLODModels[Index];
					FSkeletalMeshLODInfo& LODInfo = MeshData->ExistingLODInfo[Index];
					FSkeletalMeshLODModel* NewLODModel = new(SkeletalMesh->GetImportedModel()->LODModels) FSkeletalMeshLODModel(LODModel);
					// add LOD info back
					SkeletalMesh->AddLODInfo(LODInfo);
					//Apply the new skinning to the existing LOD geometry
					SkeletalMeshHelper::ApplySkinning(SkeletalMesh, BaseLodModel, *NewLODModel);
				}
			}

			if (bRegenLODs)
			{
				TryRegenerateLODs(MeshData, SkeletalMesh);
			}
		}

		for (int32 AssetIndex = 0; AssetIndex < MeshData->ExistingPhysicsAssets.Num(); ++AssetIndex)
		{
			UPhysicsAsset* PhysicsAsset = MeshData->ExistingPhysicsAssets[AssetIndex];
			if (AssetIndex == 0)
			{
				// First asset is the one that the skeletal mesh should point too
				SkeletalMesh->PhysicsAsset = PhysicsAsset;
			}
			// No need to mark as modified here, because the asset hasn't actually changed
			if (PhysicsAsset)
			{
				PhysicsAsset->PreviewSkeletalMesh = SkeletalMesh;
			}
		}

		SkeletalMesh->ShadowPhysicsAsset = MeshData->ExistingShadowPhysicsAsset;

		SkeletalMesh->Skeleton = MeshData->ExistingSkeleton;
		SkeletalMesh->PostProcessAnimBlueprint = MeshData->ExistingPostProcessAnimBlueprint;
		
		// Copy mirror table.
		SkeletalMesh->ImportMirrorTable(MeshData->ExistingMirrorTable);

		SkeletalMesh->MorphTargets.Empty(MeshData->ExistingMorphTargets.Num());
		SkeletalMesh->MorphTargets.Append(MeshData->ExistingMorphTargets);
		SkeletalMesh->InitMorphTargets();

		SkeletalMesh->bUseFullPrecisionUVs = MeshData->bExistingUseFullPrecisionUVs;
		SkeletalMesh->bUseHighPrecisionTangentBasis= MeshData->bExistingUseHighPrecisionTangentBasis;

		SkeletalMesh->AssetImportData = MeshData->ExistingAssetImportData.Get();
		SkeletalMesh->ThumbnailInfo = MeshData->ExistingThumbnailInfo.Get();

		SkeletalMesh->MeshClothingAssets = MeshData->ExistingClothingAssets;

		for (UClothingAssetBase* ClothingAsset : SkeletalMesh->MeshClothingAssets)
		{
			ClothingAsset->RefreshBoneMapping(SkeletalMesh);
		}

		SkeletalMesh->SetSamplingInfo(MeshData->ExistingSamplingInfo);
	}

	//Restore the section change only for the reimport LOD, other LOD are not affected since the material array can only grow.
	if (MeshData->UseMaterialNameSlotWorkflow)
	{
		FSkeletalMeshLODModel &NewSkelMeshLodModel = SkeletalMesh->GetImportedModel()->LODModels[ReimportLODIndex];
		//Restore the section changes from the old import data
		for (int32 SectionIndex = 0; SectionIndex < NewSkelMeshLodModel.Sections.Num(); SectionIndex++)
		{
			int32 NewMeshSectionMaterialIndex = NewSkelMeshLodModel.Sections[SectionIndex].MaterialIndex;
			//Get the new skelmesh section slot import name
			FName NewMeshSectionSlotName = SkeletalMesh->Materials[NewMeshSectionMaterialIndex].ImportedMaterialSlotName;

			if (RemapMaterial.IsValidIndex(NewMeshSectionMaterialIndex))
			{
				if (SkeletalMesh->Materials.IsValidIndex(RemapMaterial[NewMeshSectionMaterialIndex]))
				{
					NewSkelMeshLodModel.Sections[SectionIndex].MaterialIndex = RemapMaterial[NewMeshSectionMaterialIndex];
					if (MeshData->ExistingImportMeshLodSectionMaterialData[ReimportLODIndex].IsValidIndex(RemapMaterial[NewMeshSectionMaterialIndex]))
					{
						NewSkelMeshLodModel.Sections[SectionIndex].bCastShadow = MeshData->ExistingImportMeshLodSectionMaterialData[ReimportLODIndex][RemapMaterial[NewMeshSectionMaterialIndex]].bCastShadow;
						NewSkelMeshLodModel.Sections[SectionIndex].bRecomputeTangent = MeshData->ExistingImportMeshLodSectionMaterialData[ReimportLODIndex][RemapMaterial[NewMeshSectionMaterialIndex]].bRecomputeTangents;
						NewSkelMeshLodModel.Sections[SectionIndex].GenerateUpToLodIndex = MeshData->ExistingImportMeshLodSectionMaterialData[ReimportLODIndex][RemapMaterial[NewMeshSectionMaterialIndex]].GenerateUpTo;
					}
				}
			}

			if (MeshData->LastImportMeshLodSectionMaterialData.Num() < 1 || MeshData->LastImportMeshLodSectionMaterialData[ReimportLODIndex].Num() <= SectionIndex ||
				MeshData->ExistingImportMeshLodSectionMaterialData.Num() < 1 || MeshData->ExistingImportMeshLodSectionMaterialData[ReimportLODIndex].Num() <= SectionIndex)
			{
				break;
			}

			FName CurrentSectionImportedMaterialName = SkeletalMesh->Materials[NewSkelMeshLodModel.Sections[SectionIndex].MaterialIndex].ImportedMaterialSlotName;
			for (int32 ExistSectionIndex = 0; ExistSectionIndex < MeshData->ExistingImportMeshLodSectionMaterialData[ReimportLODIndex].Num(); ++ExistSectionIndex)
			{
				if (!MeshData->LastImportMeshLodSectionMaterialData[ReimportLODIndex].IsValidIndex(ExistSectionIndex) || !MeshData->ExistingImportMeshLodSectionMaterialData[ReimportLODIndex].IsValidIndex(ExistSectionIndex))
				{
					continue;
				}
				//Get the Last imported skelmesh section slot import name
				FName OriginalImportMeshSectionSlotName = MeshData->LastImportMeshLodSectionMaterialData[ReimportLODIndex][ExistSectionIndex];
				if (OriginalImportMeshSectionSlotName != CurrentSectionImportedMaterialName)
				{
					continue;
				}

				//Get the current skelmesh section slot import name
				FName ExistMeshSectionSlotName = MeshData->ExistingImportMeshLodSectionMaterialData[ReimportLODIndex][ExistSectionIndex].ImportedMaterialSlotName;
				if (ExistMeshSectionSlotName != OriginalImportMeshSectionSlotName)
				{
					//The last import slot name match the New import slot name, but the Exist slot name is different then the last import slot name.
					//This mean the user has change the section assign slot and the fbx file did not change it
					//Override the new section material index to use the one that the user set
					for (int32 RemapMaterialIndex = 0; RemapMaterialIndex < SkeletalMesh->Materials.Num(); ++RemapMaterialIndex)
					{
						const FSkeletalMaterial &NewSectionMaterial = SkeletalMesh->Materials[RemapMaterialIndex];
						if (NewSectionMaterial.ImportedMaterialSlotName == ExistMeshSectionSlotName)
						{
							NewSkelMeshLodModel.Sections[SectionIndex].MaterialIndex = RemapMaterialIndex;
							break;
						}
					}
				}
				break;
			}
		}
	}
}
#undef LOCTEXT_NAMESPACE
