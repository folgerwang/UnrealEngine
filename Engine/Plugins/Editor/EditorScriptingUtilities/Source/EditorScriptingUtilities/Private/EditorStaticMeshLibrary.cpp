// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "EditorStaticMeshLibrary.h"

#include "EditorScriptingUtils.h"

#include "ActorEditorUtils.h"
#include "AssetRegistryModule.h"
#include "Components/MeshComponent.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "EngineUtils.h"
#include "Engine/Brush.h"
#include "Engine/Selection.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "FileHelpers.h"
#include "GameFramework/Actor.h"
#include "IContentBrowserSingleton.h"
#include "IMeshMergeUtilities.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/ComponentEditorUtils.h"
#include "Layers/ILayers.h"
#include "LevelEditorViewport.h"
#include "Engine/MapBuildDataRegistry.h"
#include "MeshAttributes.h"
#include "MeshAttributeArray.h"
#include "MeshDescriptionOperations.h"
#include "MeshMergeModule.h"
#include "PhysicsEngine/BodySetup.h"
#include "MeshDescription.h"
#include "ScopedTransaction.h"
#include "Toolkits/AssetEditorManager.h"
#include "UnrealEdGlobals.h"
#include "UnrealEd/Private/GeomFitUtils.h"
#include "UnrealEd/Private/ConvexDecompTool.h"

#define LOCTEXT_NAMESPACE "EditorStaticMeshLibrary"

/**
 *
 * Editor Scripting | DataPrep
 *
 **/

namespace InternalEditorMeshLibrary
{
	/** Note: This method is a replicate of FStaticMeshEditor::DoDecomp */
	bool GenerateConvexCollision(UStaticMesh* StaticMesh, uint32 HullCount, int32 MaxHullVerts, uint32 HullPrecision)
	{
		// Check we have a valid StaticMesh
		if (!StaticMesh || !StaticMesh->IsMeshDescriptionValid(0))
		{
			return false;
		}

		// If RenderData has not been computed yet, do it
		if (!StaticMesh->RenderData)
		{
			StaticMesh->CacheDerivedData();
		}

		const FStaticMeshLODResources& LODModel = StaticMesh->RenderData->LODResources[0];

		// Make vertex buffer
		int32 NumVerts = LODModel.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices();
		TArray<FVector> Verts;
		Verts.Reserve(NumVerts);
		for(int32 i=0; i<NumVerts; i++)
		{
			Verts.Add(LODModel.VertexBuffers.PositionVertexBuffer.VertexPosition(i));
		}

		// Grab all indices
		TArray<uint32> AllIndices;
		LODModel.IndexBuffer.GetCopy(AllIndices);

		// Only copy indices that have collision enabled
		TArray<uint32> CollidingIndices;
		for(const FStaticMeshSection& Section : LODModel.Sections)
		{
			if(Section.bEnableCollision)
			{
				for (uint32 IndexIdx = Section.FirstIndex; IndexIdx < Section.FirstIndex + (Section.NumTriangles * 3); IndexIdx++)
				{
					CollidingIndices.Add(AllIndices[IndexIdx]);
				}
			}
		}

		// Do not perform any action if we have invalid input
		if(Verts.Num() < 3 || CollidingIndices.Num() < 3)
		{
			return false;
		}

		// Get the BodySetup we are going to put the collision into
		UBodySetup* BodySetup = StaticMesh->BodySetup;
		if(BodySetup)
		{
			BodySetup->RemoveSimpleCollision();
		}
		else
		{
			// Otherwise, create one here.
			StaticMesh->CreateBodySetup();
			BodySetup = StaticMesh->BodySetup;
		}

		// Run actual util to do the work (if we have some valid input)
		DecomposeMeshToHulls(BodySetup, Verts, CollidingIndices, HullCount, MaxHullVerts, HullPrecision);

		// refresh collision change back to static mesh components
		RefreshCollisionChange(*StaticMesh);

		// Mark mesh as dirty
		StaticMesh->MarkPackageDirty();

		StaticMesh->bCustomizedCollision = true;	//mark the static mesh for collision customization

		return true;
	}

	bool IsUVChannelValid(UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannelIndex)
	{
		if (StaticMesh == nullptr)
		{
			UE_LOG(LogEditorScripting, Error, TEXT("The StaticMesh is null."));
			return false;
		}

		if (LODIndex >= StaticMesh->GetNumLODs() || LODIndex < 0)
		{
			UE_LOG(LogEditorScripting, Error, TEXT("The StaticMesh doesn't have LOD %d."), LODIndex);
			return false;
		}

		if (!StaticMesh->IsMeshDescriptionValid(LODIndex))
		{
			UE_LOG(LogEditorScripting, Error, TEXT("No mesh description for LOD %d."), LODIndex);
			return false;
		}

		int32 NumUVChannels = StaticMesh->GetNumUVChannels(LODIndex);
		if (UVChannelIndex < 0 || UVChannelIndex >= NumUVChannels)
		{
			UE_LOG(LogEditorScripting, Error, TEXT("The given UV channel index %d is out of bounds."), UVChannelIndex);
			return false;
		}

		return true;
	}
}

int32 UEditorStaticMeshLibrary::SetLods(UStaticMesh* StaticMesh, const FEditorScriptingMeshReductionOptions& ReductionOptions)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return -1;
	}

	if (StaticMesh == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("SetLODs: The StaticMesh is null."));
		return -1;
	}

	// If LOD 0 does not exist, warn and return
	if (!StaticMesh->SourceModels.IsValidIndex(0))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("SetLODs: This StaticMesh does not have LOD 0."));
		return -1;
	}

	if(ReductionOptions.ReductionSettings.Num() == 0)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("SetLODs: Nothing done as no LOD settings were provided."));
		return -1;
	}

	// Close the mesh editor to prevent crashing. Reopen it after the mesh has been built.
	FAssetEditorManager& AssetEditorManager = FAssetEditorManager::Get();
	bool bStaticMeshIsEdited = false;
	if (AssetEditorManager.FindEditorForAsset(StaticMesh, false))
	{
		AssetEditorManager.CloseAllEditorsForAsset(StaticMesh);
		bStaticMeshIsEdited = true;
	}

	// Resize array of LODs to only keep LOD 0
	StaticMesh->Modify();
	StaticMesh->SetNumSourceModels(1);

	// Set up LOD 0
	StaticMesh->SourceModels[0].ReductionSettings.PercentTriangles = ReductionOptions.ReductionSettings[0].PercentTriangles;
	StaticMesh->SourceModels[0].ScreenSize = ReductionOptions.ReductionSettings[0].ScreenSize;

	int32 LODIndex = 1;
	for (; LODIndex < ReductionOptions.ReductionSettings.Num(); ++LODIndex)
	{
		// Create new SourceModel for new LOD
		FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();

		// Copy settings from previous LOD
		SrcModel.BuildSettings = StaticMesh->SourceModels[LODIndex-1].BuildSettings;
		SrcModel.ReductionSettings = StaticMesh->SourceModels[LODIndex-1].ReductionSettings;

		// Modify reduction settings based on user's requirements
		SrcModel.ReductionSettings.PercentTriangles = ReductionOptions.ReductionSettings[LODIndex].PercentTriangles;
		SrcModel.ScreenSize = ReductionOptions.ReductionSettings[LODIndex].ScreenSize;

		// Stop when reaching maximum of supported LODs
		if (StaticMesh->SourceModels.Num() == MAX_STATIC_MESH_LODS)
		{
			break;
		}
	}

	StaticMesh->bAutoComputeLODScreenSize = ReductionOptions.bAutoComputeLODScreenSize ? 1 : 0;

	// Request re-building of mesh with new LODs
	StaticMesh->PostEditChange();

	// Reopen MeshEditor on this mesh if the MeshEditor was previously opened in it
	if (bStaticMeshIsEdited)
	{
		AssetEditorManager.OpenEditorForAsset(StaticMesh);
	}

	return LODIndex;
}

int32 UEditorStaticMeshLibrary::SetLodFromStaticMesh(UStaticMesh* DestinationStaticMesh, int32 DestinationLodIndex, UStaticMesh* SourceStaticMesh, int32 SourceLodIndex, bool bReuseExistingMaterialSlots)
{
	TGuardValue<bool> UnattendedScriptGuard( GIsRunningUnattendedScript, true );

	if ( !EditorScriptingUtils::CheckIfInEditorAndPIE() )
	{
		return -1;
	}

	if ( DestinationStaticMesh == nullptr )
	{
		UE_LOG(LogEditorScripting, Error, TEXT("SetLodFromStaticMesh: The DestinationStaticMesh is null."));
		return -1;
	}

	if ( SourceStaticMesh == nullptr )
	{
		UE_LOG(LogEditorScripting, Error, TEXT("SetLodFromStaticMesh: The SourceStaticMesh is null."));
		return -1;
	}

	if ( !SourceStaticMesh->SourceModels.IsValidIndex( SourceLodIndex ) )
	{
		UE_LOG(LogEditorScripting, Error, TEXT("SetLodFromStaticMesh: SourceLodIndex is invalid."));
		return -1;
	}

	// Close the mesh editor to prevent crashing. Reopen it after the mesh has been built.
	FAssetEditorManager& AssetEditorManager = FAssetEditorManager::Get();
	bool bStaticMeshIsEdited = false;
	if ( AssetEditorManager.FindEditorForAsset( DestinationStaticMesh, false ) )
	{
		AssetEditorManager.CloseAllEditorsForAsset( DestinationStaticMesh );
		bStaticMeshIsEdited = true;
	}

	DestinationStaticMesh->Modify();

	if ( DestinationStaticMesh->SourceModels.Num() < DestinationLodIndex + 1 )
	{
		// Add one LOD 
		DestinationStaticMesh->AddSourceModel();

		DestinationLodIndex = DestinationStaticMesh->SourceModels.Num() - 1;

		// The newly added SourceModel won't have a MeshDescription so create it explicitly
		DestinationStaticMesh->CreateMeshDescription(DestinationLodIndex);
	}

	// Transfers the build settings and the reduction settings.
	const FStaticMeshSourceModel& SourceMeshSourceModel = SourceStaticMesh->SourceModels[SourceLodIndex];
	FStaticMeshSourceModel& DestinationMeshSourceModel = DestinationStaticMesh->SourceModels[DestinationLodIndex];
	DestinationMeshSourceModel.BuildSettings = SourceMeshSourceModel.BuildSettings;
	DestinationMeshSourceModel.ReductionSettings = SourceMeshSourceModel.ReductionSettings;
	// Base the reduction on the new lod
	DestinationMeshSourceModel.ReductionSettings.BaseLODModel = DestinationLodIndex;

	// Fragile. If a public function emerge to determine if a reduction will be used please consider using it and remove this code.
	bool bDoesSourceLodUseReduction = false;
	switch (SourceMeshSourceModel.ReductionSettings.TerminationCriterion)
	{
	case EStaticMeshReductionTerimationCriterion::Triangles:
		bDoesSourceLodUseReduction = !FMath::IsNearlyEqual(SourceMeshSourceModel.ReductionSettings.PercentTriangles, 100.f);
		break;
	case EStaticMeshReductionTerimationCriterion::Vertices:
		bDoesSourceLodUseReduction = !FMath::IsNearlyEqual(SourceMeshSourceModel.ReductionSettings.PercentVertices, 100.f);
		break;
	case EStaticMeshReductionTerimationCriterion::Any:
		bDoesSourceLodUseReduction = !(FMath::IsNearlyEqual(SourceMeshSourceModel.ReductionSettings.PercentTriangles, 100.f) && FMath::IsNearlyEqual(SourceMeshSourceModel.ReductionSettings.PercentVertices, 100.f));
		break;
	default:
		break;
	}
	bDoesSourceLodUseReduction |= SourceMeshSourceModel.ReductionSettings.MaxDeviation > 0.f;


	int32 BaseSourceLodIndex  = bDoesSourceLodUseReduction ? SourceMeshSourceModel.ReductionSettings.BaseLODModel : SourceLodIndex;
	bool bIsReductionSettingAproximated = false;

	// Find the original mesh description for this LOD
	while (!SourceStaticMesh->IsMeshDescriptionValid(BaseSourceLodIndex ))
	{
		if (!SourceStaticMesh->SourceModels.IsValidIndex(BaseSourceLodIndex ))
		{
			UE_LOG(LogEditorScripting, Error, TEXT("SetLodFromStaticMesh: The SourceStaticMesh is in a invalid state."));
			return -1;
		}

		const FMeshReductionSettings& PossibleSourceMeshReductionSetting = SourceStaticMesh->SourceModels[BaseSourceLodIndex ].ReductionSettings;
		DestinationMeshSourceModel.ReductionSettings.PercentTriangles *= PossibleSourceMeshReductionSetting.PercentTriangles;
		DestinationMeshSourceModel.ReductionSettings.PercentVertices *= PossibleSourceMeshReductionSetting.PercentVertices;
		BaseSourceLodIndex  = SourceStaticMesh->SourceModels[BaseSourceLodIndex ].ReductionSettings.BaseLODModel;

		bIsReductionSettingAproximated = true;
	}

	if (bIsReductionSettingAproximated)
	{
		TArray<FStringFormatArg> InOrderedArguments;
		InOrderedArguments.Reserve(4);
		InOrderedArguments.Add(SourceStaticMesh->GetName());
		InOrderedArguments.Add(SourceLodIndex);
		InOrderedArguments.Add(DestinationLodIndex);
		InOrderedArguments.Add(DestinationStaticMesh->GetName());

		UE_LOG(LogEditorScripting, Warning, TEXT("%s"), *FString::Format(TEXT("SetLodFromStaticMesh: The reduction settings from the SourceStaticMesh {0} LOD {1} were approximated."
			" The LOD {2} from {3} might not be identical."), InOrderedArguments));
	}

	// Copy the source import file.
	DestinationMeshSourceModel.SourceImportFilename = SourceStaticMesh->SourceModels[BaseSourceLodIndex ].SourceImportFilename;

	// Copy the mesh description
	const FMeshDescription& SourceMeshDescription = *SourceStaticMesh->GetMeshDescription(BaseSourceLodIndex );
	FMeshDescription& DestinationMeshDescription = *DestinationStaticMesh->GetMeshDescription(DestinationLodIndex);
	DestinationMeshDescription = SourceMeshDescription;
	DestinationStaticMesh->CommitMeshDescription(DestinationLodIndex);

	// Assign materials for the destination LOD
	{
		auto FindMaterialIndex = []( UStaticMesh* StaticMesh, const UMaterialInterface* Material ) -> int32
		{
			for ( int32 MaterialIndex = 0; MaterialIndex < StaticMesh->StaticMaterials.Num(); ++MaterialIndex )
			{
				if ( StaticMesh->GetMaterial( MaterialIndex ) == Material )
				{
					return MaterialIndex;
				}
			}

			return INDEX_NONE;
		};

		TMap< int32, int32 > LodSectionMaterialMapping; // LOD section index -> destination material index

		int32 NumDestinationMaterial = DestinationStaticMesh->StaticMaterials.Num();

		const int32 SourceLodNumSections = SourceStaticMesh->SectionInfoMap.GetSectionNumber( SourceLodIndex );

		for ( int32 SourceLodSectionIndex = 0; SourceLodSectionIndex < SourceLodNumSections; ++SourceLodSectionIndex )
		{
			const FMeshSectionInfo& SourceMeshSectionInfo = SourceStaticMesh->SectionInfoMap.Get( SourceLodIndex, SourceLodSectionIndex );

			const UMaterialInterface* SourceMaterial = SourceStaticMesh->GetMaterial( SourceMeshSectionInfo.MaterialIndex );

			int32 DestinationMaterialIndex = INDEX_NONE;
			
			if ( bReuseExistingMaterialSlots )
			{
				DestinationMaterialIndex = FindMaterialIndex( DestinationStaticMesh, SourceMaterial );
			}
			
			if ( DestinationMaterialIndex == INDEX_NONE )
			{
				DestinationMaterialIndex = NumDestinationMaterial++;
			}
			
			LodSectionMaterialMapping.Add( SourceLodSectionIndex, DestinationMaterialIndex );
		}

		for ( TMap< int32, int32 >::TConstIterator It = LodSectionMaterialMapping.CreateConstIterator(); It; ++It )
		{
			const int32 SectionIndex = It->Key;
		
			const FMeshSectionInfo& SourceSectionInfo = SourceStaticMesh->SectionInfoMap.Get( SourceLodIndex, SectionIndex );

			UMaterialInterface* SourceMaterial = SourceStaticMesh->GetMaterial( SourceSectionInfo.MaterialIndex );

			const int32 SourceMaterialIndex = SourceSectionInfo.MaterialIndex;
			const int32 DestinationMaterialIndex = It->Value;

			if ( !DestinationStaticMesh->StaticMaterials.IsValidIndex( DestinationMaterialIndex ) )
			{
				DestinationStaticMesh->StaticMaterials.Add( SourceStaticMesh->StaticMaterials[ SourceSectionInfo.MaterialIndex ] );

				ensure( DestinationStaticMesh->StaticMaterials.Num() == DestinationMaterialIndex + 1 ); // We assume that we are not creating holes in StaticMaterials
			}

			FMeshSectionInfo DestinationSectionInfo = SourceSectionInfo;
			DestinationSectionInfo.MaterialIndex = DestinationMaterialIndex;

			DestinationStaticMesh->SectionInfoMap.Set( DestinationLodIndex, SectionIndex, MoveTemp( DestinationSectionInfo ) );
		}
	}

	DestinationStaticMesh->PostEditChange();

	// Reopen MeshEditor on this mesh if the MeshEditor was previously opened in it
	if ( bStaticMeshIsEdited )
	{
		AssetEditorManager.OpenEditorForAsset( DestinationStaticMesh );
	}

	return DestinationLodIndex;
}

int32 UEditorStaticMeshLibrary::GetLodCount(UStaticMesh* StaticMesh)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (StaticMesh == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("GetLODCount: The StaticMesh is null."));
		return -1;
	}

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return -1;
	}

	return StaticMesh->SourceModels.Num();
}

bool UEditorStaticMeshLibrary::RemoveLods(UStaticMesh* StaticMesh)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (StaticMesh == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("RemoveLODs: The StaticMesh is null."));
		return false;
	}

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return false;
	}

	// No main LOD, skip
	if (!StaticMesh->SourceModels.IsValidIndex(0))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("RemoveLODs: This StaticMesh does not have LOD 0."));
		return false;
	}

	// Close the mesh editor to prevent crashing. Reopen it after the mesh has been built.
	FAssetEditorManager& AssetEditorManager = FAssetEditorManager::Get();
	bool bStaticMeshIsEdited = false;
	if (AssetEditorManager.FindEditorForAsset(StaticMesh, false))
	{
		AssetEditorManager.CloseAllEditorsForAsset(StaticMesh);
		bStaticMeshIsEdited = true;
	}

	// Reduce array of source models to 1
	StaticMesh->Modify();
	StaticMesh->SetNumSourceModels(1);

	// Request re-building of mesh with new LODs
	StaticMesh->PostEditChange();

	// Reopen MeshEditor on this mesh if the MeshEditor was previously opened in it
	if (bStaticMeshIsEdited)
	{
		AssetEditorManager.OpenEditorForAsset(StaticMesh);
	}

	return true;
}

TArray<float> UEditorStaticMeshLibrary::GetLodScreenSizes(UStaticMesh* StaticMesh)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	TArray<float> ScreenSizes;
	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return ScreenSizes;
	}

	if (StaticMesh == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("GetLodScreenSizes: The StaticMesh is null."));
		return ScreenSizes;
	}

	for (int i = 0; i < StaticMesh->GetNumLODs(); i++)
	{
		if (StaticMesh->RenderData.IsValid())
		{
			float CurScreenSize = StaticMesh->RenderData->ScreenSize[i].Default;
			ScreenSizes.Add(CurScreenSize);
		}
		else
		{
			UE_LOG(LogEditorScripting, Warning, TEXT("GetLodScreenSizes: The RenderData is invalid for LOD %d."), i);
		}
	}

	return ScreenSizes;

}

int32 UEditorStaticMeshLibrary::AddSimpleCollisions(UStaticMesh* StaticMesh, const EScriptingCollisionShapeType ShapeType)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (StaticMesh == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("AddSimpleCollisions: The StaticMesh is null."));
		return INDEX_NONE;
	}

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return INDEX_NONE;
	}

	// Close the mesh editor to prevent crashing. Reopen it after the mesh has been built.
	FAssetEditorManager& AssetEditorManager = FAssetEditorManager::Get();
	bool bStaticMeshIsEdited = false;
	if (AssetEditorManager.FindEditorForAsset(StaticMesh, false))
	{
		AssetEditorManager.CloseAllEditorsForAsset(StaticMesh);
		bStaticMeshIsEdited = true;
	}

	int32 PrimIndex = INDEX_NONE;

	switch (ShapeType)
	{
		case EScriptingCollisionShapeType::Box:
		{
			PrimIndex = GenerateBoxAsSimpleCollision(StaticMesh);
			break;
		}
		case EScriptingCollisionShapeType::Sphere:
		{
			PrimIndex = GenerateSphereAsSimpleCollision(StaticMesh);
			break;
		}
		case EScriptingCollisionShapeType::Capsule:
		{
			PrimIndex = GenerateSphylAsSimpleCollision(StaticMesh);
			break;
		}
		case EScriptingCollisionShapeType::NDOP10_X:
		{
			TArray<FVector>	DirArray(KDopDir10X, 10);
			PrimIndex = GenerateKDopAsSimpleCollision(StaticMesh, DirArray);
			break;
		}
		case EScriptingCollisionShapeType::NDOP10_Y:
		{
			TArray<FVector>	DirArray(KDopDir10Y, 10);
			PrimIndex = GenerateKDopAsSimpleCollision(StaticMesh, DirArray);
			break;
		}
		case EScriptingCollisionShapeType::NDOP10_Z:
		{
			TArray<FVector>	DirArray(KDopDir10Z, 10);
			PrimIndex = GenerateKDopAsSimpleCollision(StaticMesh, DirArray);
			break;
		}
		case EScriptingCollisionShapeType::NDOP18:
		{
			TArray<FVector>	DirArray(KDopDir18, 18);
			PrimIndex = GenerateKDopAsSimpleCollision(StaticMesh, DirArray);
			break;
		}
		case EScriptingCollisionShapeType::NDOP26:
		{
			TArray<FVector>	DirArray(KDopDir26, 26);
			PrimIndex = GenerateKDopAsSimpleCollision(StaticMesh, DirArray);
			break;
		}
	}

	// Request re-building of mesh with new collision shapes
	StaticMesh->PostEditChange();

	// Reopen MeshEditor on this mesh if the MeshEditor was previously opened in it
	if (bStaticMeshIsEdited)
	{
		AssetEditorManager.OpenEditorForAsset(StaticMesh);
	}

	return PrimIndex;
}

int32 UEditorStaticMeshLibrary::GetSimpleCollisionCount(UStaticMesh* StaticMesh)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (StaticMesh == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("GetSimpleCollisionCount: The StaticMesh is null."));
		return -1;
	}

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return -1;
	}

	UBodySetup* BodySetup = StaticMesh->BodySetup;
	if (BodySetup == nullptr)
	{
		return 0;
	}

	int32 Count = BodySetup->AggGeom.BoxElems.Num();
	Count += BodySetup->AggGeom.SphereElems.Num();
	Count += BodySetup->AggGeom.SphylElems.Num();

	return Count;
}

TEnumAsByte<ECollisionTraceFlag> UEditorStaticMeshLibrary::GetCollisionComplexity(UStaticMesh* StaticMesh)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (StaticMesh == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("GetCollisionComplexity: The StaticMesh is null."));
		return ECollisionTraceFlag::CTF_UseDefault;
	}

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return ECollisionTraceFlag::CTF_UseDefault;
	}

	if (StaticMesh->BodySetup)
	{
		return StaticMesh->BodySetup->CollisionTraceFlag;
	}

	return ECollisionTraceFlag::CTF_UseDefault;
}

int32 UEditorStaticMeshLibrary::GetConvexCollisionCount(UStaticMesh* StaticMesh)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (StaticMesh == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("GetConvexCollisionCount: The StaticMesh is null."));
		return -1;
	}

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return -1;
	}

	UBodySetup* BodySetup = StaticMesh->BodySetup;
	if (BodySetup == nullptr)
	{
		return 0;
	}

	return BodySetup->AggGeom.ConvexElems.Num();
}

bool UEditorStaticMeshLibrary::SetConvexDecompositionCollisions(UStaticMesh* StaticMesh, int32 HullCount, int32 MaxHullVerts, int32 HullPrecision)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return false;
	}

	if (StaticMesh == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("SetConvexDecompositionCollisions: The StaticMesh is null."));
		return false;
	}

	if (HullCount < 0 || HullPrecision < 0)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("SetConvexDecompositionCollisions: Parameters HullCount and HullPrecision must be positive."));
		return false;
	}

	// Close the mesh editor to prevent crashing. Reopen it after the mesh has been built.
	FAssetEditorManager& AssetEditorManager = FAssetEditorManager::Get();
	bool bStaticMeshIsEdited = false;
	if (AssetEditorManager.FindEditorForAsset(StaticMesh, false))
	{
		AssetEditorManager.CloseAllEditorsForAsset(StaticMesh);
		bStaticMeshIsEdited = true;
	}

	if (StaticMesh->BodySetup)
	{
		// Remove simple collisions
		StaticMesh->BodySetup->Modify();

		StaticMesh->BodySetup->RemoveSimpleCollision();

		// refresh collision change back to static mesh components
		RefreshCollisionChange(*StaticMesh);
	}

	// Generate convex collision on mesh
	bool bResult = InternalEditorMeshLibrary::GenerateConvexCollision(StaticMesh, HullCount, MaxHullVerts, HullPrecision);

	// Request re-building of mesh following collision changes
	StaticMesh->PostEditChange();

	// Reopen MeshEditor on this mesh if the MeshEditor was previously opened in it
	if (bStaticMeshIsEdited)
	{
		AssetEditorManager.OpenEditorForAsset(StaticMesh);
	}

	return bResult;
}

bool UEditorStaticMeshLibrary::RemoveCollisions(UStaticMesh* StaticMesh)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return false;
	}

	if (StaticMesh == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("RemoveCollisions: The StaticMesh is null."));
		return false;
	}

	if (StaticMesh->BodySetup == nullptr)
	{
		UE_LOG(LogEditorScripting, Log, TEXT("RemoveCollisions: No collision set up. Nothing to do."));
		return true;
	}

	// Close the mesh editor to prevent crashing. Reopen it after the mesh has been built.
	FAssetEditorManager& AssetEditorManager = FAssetEditorManager::Get();
	bool bStaticMeshIsEdited = false;
	if (AssetEditorManager.FindEditorForAsset(StaticMesh, false))
	{
		AssetEditorManager.CloseAllEditorsForAsset(StaticMesh);
		bStaticMeshIsEdited = true;
	}

	// Remove simple collisions
	StaticMesh->BodySetup->Modify();

	StaticMesh->BodySetup->RemoveSimpleCollision();

	// refresh collision change back to static mesh components
	RefreshCollisionChange(*StaticMesh);

	// Request re-building of mesh with new collision shapes
	StaticMesh->PostEditChange();

	// Reopen MeshEditor on this mesh if the MeshEditor was previously opened in it
	if (bStaticMeshIsEdited)
	{
		AssetEditorManager.OpenEditorForAsset(StaticMesh);
	}

	return true;
}

void UEditorStaticMeshLibrary::EnableSectionCollision(UStaticMesh* StaticMesh, bool bCollisionEnabled, int32 LODIndex, int32 SectionIndex)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return;
	}

	if (StaticMesh == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("EnableSectionCollision: The StaticMesh is null."));
		return;
	}

	if (LODIndex >= StaticMesh->GetNumLODs())
	{
		UE_LOG(LogEditorScripting, Error, TEXT("EnableSectionCollision: Invalid LOD index %d (of %d)."), LODIndex, StaticMesh->GetNumLODs());
		return;
	}

	if (SectionIndex >= StaticMesh->GetNumSections(LODIndex))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("EnableSectionCollision: Invalid section index %d (of %d)."), SectionIndex, StaticMesh->GetNumSections(LODIndex));
		return;
	}

	StaticMesh->Modify();

	FMeshSectionInfo SectionInfo = StaticMesh->SectionInfoMap.Get(LODIndex, SectionIndex);

	SectionInfo.bEnableCollision = bCollisionEnabled;

	StaticMesh->SectionInfoMap.Set(LODIndex, SectionIndex, SectionInfo);

	StaticMesh->PostEditChange();
}

bool UEditorStaticMeshLibrary::IsSectionCollisionEnabled(UStaticMesh* StaticMesh, int32 LODIndex, int32 SectionIndex)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return false;
	}

	if (StaticMesh == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("IsSectionCollisionEnabled: The StaticMesh is null."));
		return false;
	}

	if (LODIndex >= StaticMesh->GetNumLODs())
	{
		UE_LOG(LogEditorScripting, Error, TEXT("IsSectionCollisionEnabled: Invalid LOD index %d (of %d)."), LODIndex, StaticMesh->GetNumLODs());
		return false;
	}

	if (SectionIndex >= StaticMesh->GetNumSections(LODIndex))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("IsSectionCollisionEnabled: Invalid section index %d (of %d)."), SectionIndex, StaticMesh->GetNumSections(LODIndex));
		return false;
	}

	FMeshSectionInfo SectionInfo = StaticMesh->SectionInfoMap.Get(LODIndex, SectionIndex);
	return SectionInfo.bEnableCollision;
}

void UEditorStaticMeshLibrary::EnableSectionCastShadow(UStaticMesh* StaticMesh, bool bCastShadow, int32 LODIndex, int32 SectionIndex)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return;
	}

	if (StaticMesh == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("EnableSectionCastShadow: The StaticMesh is null."));
		return;
	}

	if (LODIndex >= StaticMesh->GetNumLODs())
	{
		UE_LOG(LogEditorScripting, Error, TEXT("EnableSectionCastShadow: Invalid LOD index %d (of %d)."), LODIndex, StaticMesh->GetNumLODs());
		return;
	}

	if (SectionIndex >= StaticMesh->GetNumSections(LODIndex))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("EnableSectionCastShadow: Invalid section index %d (of %d)."), SectionIndex, StaticMesh->GetNumSections(LODIndex));
		return;
	}

	StaticMesh->Modify();

	FMeshSectionInfo SectionInfo = StaticMesh->SectionInfoMap.Get(LODIndex, SectionIndex);

	SectionInfo.bCastShadow = bCastShadow;

	StaticMesh->SectionInfoMap.Set(LODIndex, SectionIndex, SectionInfo);

	StaticMesh->PostEditChange();
}

bool UEditorStaticMeshLibrary::HasVertexColors(UStaticMesh* StaticMesh)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return false;
	}

	if (StaticMesh == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("HasVertexColors: The StaticMesh is null."));
		return false;
	}

	for (int32 LodIndex = 0; LodIndex < StaticMesh->SourceModels.Num(); ++LodIndex)
	{
		const FMeshDescription* MeshDescription = StaticMesh->GetMeshDescription(LodIndex);
		if (!MeshDescription->VertexInstanceAttributes().HasAttribute(MeshAttribute::VertexInstance::Color))
		{
			continue;
		}
		TVertexInstanceAttributesConstRef<FVector4> VertexInstanceColors = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color);
		for (const FVertexInstanceID VertexInstanceID : MeshDescription->VertexInstances().GetElementIDs())
		{
			FLinearColor VertexInstanceColor(VertexInstanceColors[VertexInstanceID]);
			if (VertexInstanceColor != FLinearColor::White)
			{
				return true;
			}
		}
	}
	return false;
}

bool UEditorStaticMeshLibrary::HasInstanceVertexColors(UStaticMeshComponent* StaticMeshComponent)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return false;
	}

	if (StaticMeshComponent == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("HasInstanceVertexColors: The StaticMeshComponent is null."));
		return false;
	}

	for (const FStaticMeshComponentLODInfo& CurrentLODInfo : StaticMeshComponent->LODData)
	{
		if (CurrentLODInfo.OverrideVertexColors != nullptr || CurrentLODInfo.PaintedVertices.Num() > 0)
		{
			return true;
		}
	}

	return false;
}

bool UEditorStaticMeshLibrary::SetGenerateLightmapUVs(UStaticMesh* StaticMesh, bool bGenerateLightmapUVs)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return false;
	}

	if (StaticMesh == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("SetGenerateLightmapUVs: The StaticMesh is null."));
		return false;
	}

	bool AnySettingsToChange = false;
	for (int32 LodIndex = 0; LodIndex < StaticMesh->SourceModels.Num(); ++LodIndex)
	{
		FStaticMeshSourceModel& SourceModel = StaticMesh->SourceModels[LodIndex];
		//Make sure LOD is not a reduction before considering its BuildSettings
		if (StaticMesh->IsMeshDescriptionValid(LodIndex))
		{
			AnySettingsToChange = (SourceModel.BuildSettings.bGenerateLightmapUVs != bGenerateLightmapUVs);

			if (AnySettingsToChange)
			{
				break;
			}
		}
	}

	if (AnySettingsToChange)
	{
		StaticMesh->Modify();
		for (FStaticMeshSourceModel& SourceModel : StaticMesh->SourceModels)
		{
			SourceModel.BuildSettings.bGenerateLightmapUVs = bGenerateLightmapUVs;

		}

		StaticMesh->Build();
		StaticMesh->PostEditChange();
		return true;
	}

	return false;
}

int32 UEditorStaticMeshLibrary::GetNumberVerts(UStaticMesh* StaticMesh, int32 LODIndex)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return 0;
	}

	if (StaticMesh == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("GetNumberVerts: The StaticMesh is null."));
		return 0;
	}

	return StaticMesh->GetNumVertices(LODIndex);
}

void UEditorStaticMeshLibrary::SetAllowCPUAccess(UStaticMesh* StaticMesh, bool bAllowCPUAccess)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return;
	}

	if (StaticMesh == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("SetAllowCPUAccess: The StaticMesh is null."));
		return;
	}

	StaticMesh->Modify();
	StaticMesh->bAllowCPUAccess = bAllowCPUAccess;
	StaticMesh->PostEditChange();
}

int32 UEditorStaticMeshLibrary::GetNumUVChannels(UStaticMesh* StaticMesh, int32 LODIndex)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return 0;
	}

	if (StaticMesh == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("GetNumUVChannels: The StaticMesh is null."));
		return 0;
	}

	if (LODIndex >= StaticMesh->GetNumLODs() || LODIndex < 0)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("GetNumUVChannels: The StaticMesh doesn't have LOD %d."), LODIndex);
		return 0;
	}

	return StaticMesh->GetNumUVChannels(LODIndex);
}

bool UEditorStaticMeshLibrary::AddUVChannel(UStaticMesh* StaticMesh, int32 LODIndex)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return false;
	}

	if (StaticMesh == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("AddUVChannel: The StaticMesh is null."));
		return false;
	}

	if (LODIndex >= StaticMesh->GetNumLODs() || LODIndex < 0)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("AddUVChannel: The StaticMesh doesn't have LOD %d."), LODIndex);
		return false;
	}

	if (StaticMesh->GetNumUVChannels(LODIndex) >= MAX_MESH_TEXTURE_COORDS_MD)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("AddUVChannel: Cannot add UV channel. Maximum number of UV channels reached (%d)."), MAX_MESH_TEXTURE_COORDS_MD);
		return false;
	}

	return StaticMesh->AddUVChannel(LODIndex);
}

bool UEditorStaticMeshLibrary::InsertUVChannel(UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannelIndex)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return false;
	}

	if (StaticMesh == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("InsertUVChannel: The StaticMesh is null."));
		return false;
	}

	if (LODIndex >= StaticMesh->GetNumLODs() || LODIndex < 0)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("InsertUVChannel: The StaticMesh doesn't have LOD %d."), LODIndex);
		return false;
	}

	int32 NumUVChannels = StaticMesh->GetNumUVChannels(LODIndex);
	if (UVChannelIndex < 0 || UVChannelIndex > NumUVChannels)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("InsertUVChannel: Cannot insert UV channel. Given UV channel index %d is out of bounds."), UVChannelIndex);
		return false;
	}

	if (NumUVChannels >= MAX_MESH_TEXTURE_COORDS_MD)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("InsertUVChannel: Cannot add UV channel. Maximum number of UV channels reached (%d)."), MAX_MESH_TEXTURE_COORDS_MD);
		return false;
	}

	return StaticMesh->InsertUVChannel(LODIndex, UVChannelIndex);
}

bool UEditorStaticMeshLibrary::RemoveUVChannel(UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannelIndex)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return false;
	}

	if (StaticMesh == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("RemoveUVChannel: The StaticMesh is null."));
		return false;
	}

	if (LODIndex >= StaticMesh->GetNumLODs() || LODIndex < 0)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("RemoveUVChannel: The StaticMesh doesn't have LOD %d."), LODIndex);
		return false;
	}

	int32 NumUVChannels = StaticMesh->GetNumUVChannels(LODIndex);
	if (NumUVChannels == 1)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("RemoveUVChannel: Cannot remove UV channel. There must be at least one channel."));
		return false;
	}

	if (UVChannelIndex < 0 || UVChannelIndex >= NumUVChannels)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("RemoveUVChannel: Cannot remove UV channel. Given UV channel index %d is out of bounds."), UVChannelIndex);
		return false;
	}

	return StaticMesh->RemoveUVChannel(LODIndex, UVChannelIndex);
}

bool UEditorStaticMeshLibrary::GeneratePlanarUVChannel(UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannelIndex, const FVector& Position, const FRotator& Orientation, const FVector2D& Tiling)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return false;
	}

	if (!InternalEditorMeshLibrary::IsUVChannelValid(StaticMesh, LODIndex, UVChannelIndex))
	{
		return false;
	}

	FMeshDescription* MeshDescription = StaticMesh->GetMeshDescription(LODIndex);

	FUVMapParameters UVParameters(Position, Orientation.Quaternion(), StaticMesh->GetBoundingBox().GetSize(), FVector::OneVector, Tiling );

	TArray<FVector2D> TexCoords;
	FMeshDescriptionOperations::GeneratePlanarUV(*MeshDescription, UVParameters, TexCoords);

	return StaticMesh->SetUVChannel(LODIndex, UVChannelIndex, TexCoords);
}

bool UEditorStaticMeshLibrary::GenerateCylindricalUVChannel(UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannelIndex, const FVector& Position, const FRotator& Orientation, const FVector2D& Tiling)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return false;
	}

	if (!InternalEditorMeshLibrary::IsUVChannelValid(StaticMesh, LODIndex, UVChannelIndex))
	{
		return false;
	}

	FMeshDescription* MeshDescription = StaticMesh->GetMeshDescription(LODIndex);

	FUVMapParameters UVParameters(Position, Orientation.Quaternion(), StaticMesh->GetBoundingBox().GetSize(), FVector::OneVector, Tiling);

	TArray<FVector2D> TexCoords;
	FMeshDescriptionOperations::GenerateCylindricalUV(*MeshDescription, UVParameters, TexCoords);

	return StaticMesh->SetUVChannel(LODIndex, UVChannelIndex, TexCoords);
}

bool UEditorStaticMeshLibrary::GenerateBoxUVChannel(UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannelIndex, const FVector& Position, const FRotator& Orientation, const FVector& Size)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return false;
	}

	if (!InternalEditorMeshLibrary::IsUVChannelValid(StaticMesh, LODIndex, UVChannelIndex))
	{
		return false;
	}

	FMeshDescription* MeshDescription = StaticMesh->GetMeshDescription(LODIndex);

	FUVMapParameters UVParameters(Position, Orientation.Quaternion(), Size, FVector::OneVector, FVector2D::UnitVector);

	TArray<FVector2D> TexCoords;
	FMeshDescriptionOperations::GenerateBoxUV(*MeshDescription, UVParameters, TexCoords);

	return StaticMesh->SetUVChannel(LODIndex, UVChannelIndex, TexCoords);
}

#undef LOCTEXT_NAMESPACE
