// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshDescriptionHelper.h"

#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "Engine/EngineTypes.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "MeshAttributes.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "RenderUtils.h"
#include "IMeshReductionInterfaces.h"
#include "IMeshReductionManagerModule.h"
#include "Materials/Material.h"
#include "RawMesh.h"
#include "BuildStatisticManager.h"
#include "MeshDescriptionOperations.h"
#include "Modules/ModuleManager.h"

//Enable all check
//#define ENABLE_NTB_CHECK

DEFINE_LOG_CATEGORY(LogMeshDescriptionBuildStatistic);

FMeshDescriptionHelper::FMeshDescriptionHelper(FMeshBuildSettings* InBuildSettings, const UMeshDescription* InOriginalMeshDescription)
	: OriginalMeshDescription(InOriginalMeshDescription)
	, BuildSettings(InBuildSettings)
{
}

UMeshDescription* FMeshDescriptionHelper::GetRenderMeshDescription(UObject* Owner)
{
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(Owner);
	check(StaticMesh);
	//Use the build settings to create the RenderMeshDescription
	UMeshDescription *RenderMeshDescription = nullptr;

	if (OriginalMeshDescription == nullptr)
	{
		//oups, we do not have a valid original mesh to create the render from
		return RenderMeshDescription;
	}
	//Copy The Original Mesh Description in the render mesh description
	RenderMeshDescription = Cast<UMeshDescription>(StaticDuplicateObject(OriginalMeshDescription, Owner, NAME_None));
	float ComparisonThreshold = BuildSettings->bRemoveDegenerates ? THRESH_POINTS_ARE_SAME : 0.0f;
	
	//This function make sure the Polygon NTB are compute and also remove degenerated triangle from the render mesh description.
	FMeshDescriptionOperations::CreatePolygonNTB(RenderMeshDescription, ComparisonThreshold);
	//RenderMeshDescription->ComputePolygonTangentsAndNormals(BuildSettings->bRemoveDegenerates ? SMALL_NUMBER : 0.0f);

	FVertexInstanceArray& VertexInstanceArray = RenderMeshDescription->VertexInstances();
	TVertexInstanceAttributeArray<FVector>& Normals = RenderMeshDescription->VertexInstanceAttributes().GetAttributes<FVector>( MeshAttribute::VertexInstance::Normal );
	TVertexInstanceAttributeArray<FVector>& Tangents = RenderMeshDescription->VertexInstanceAttributes().GetAttributes<FVector>( MeshAttribute::VertexInstance::Tangent );
	TVertexInstanceAttributeArray<float>& BinormalSigns = RenderMeshDescription->VertexInstanceAttributes().GetAttributes<float>( MeshAttribute::VertexInstance::BinormalSign );

	// Find overlapping corners to accelerate adjacency.
	FMeshDescriptionOperations::FindOverlappingCorners(OverlappingCorners, RenderMeshDescription, ComparisonThreshold);

	// Compute any missing normals or tangents.
	{
		// Static meshes always blend normals of overlapping corners.
		uint32 TangentOptions = FMeshDescriptionOperations::ETangentOptions::BlendOverlappingNormals;
		if (BuildSettings->bRemoveDegenerates)
		{
			// If removing degenerate triangles, ignore them when computing tangents.
			TangentOptions |= FMeshDescriptionOperations::ETangentOptions::IgnoreDegenerateTriangles;
		}
		
		//Keep the original mesh description NTBs if we do not rebuild the normals or tangents.
		bool bComputeTangentLegacy = !BuildSettings->bUseMikkTSpace && (BuildSettings->bRecomputeNormals || BuildSettings->bRecomputeTangents);
		bool bHasAllNormals = true;
		bool bHasAllTangents = true;
		for (const FVertexInstanceID VertexInstanceID : VertexInstanceArray.GetElementIDs())
		{
			// Dump normals and tangents if we are recomputing them.
			if (BuildSettings->bRecomputeTangents)
			{
				//Dump the tangents
				BinormalSigns[VertexInstanceID] = 0.0f;
				Tangents[VertexInstanceID] = FVector(0.0f);
			}
			if (BuildSettings->bRecomputeNormals)
			{
				//Dump the normals
				Normals[VertexInstanceID] = FVector(0.0f);
			}
			bHasAllNormals &= !Normals[VertexInstanceID].IsNearlyZero();
			bHasAllTangents &= !Tangents[VertexInstanceID].IsNearlyZero();
		}


		//MikkTSpace should be use only when the user want to recompute the normals or tangents otherwise should always fallback on builtin
		//We cannot use mikkt space with degenerated normals fallback on buitin.
		if (BuildSettings->bUseMikkTSpace && (BuildSettings->bRecomputeNormals || BuildSettings->bRecomputeTangents))
		{
			if (!bHasAllNormals)
			{
				FMeshDescriptionOperations::CreateNormals(RenderMeshDescription, (FMeshDescriptionOperations::ETangentOptions)TangentOptions, false);

				//EComputeNTBsOptions ComputeNTBsOptions = EComputeNTBsOptions::Normals;
				//RenderMeshDescription->ComputeTangentsAndNormals(ComputeNTBsOptions);
			}
			FMeshDescriptionOperations::CreateMikktTangents(RenderMeshDescription, (FMeshDescriptionOperations::ETangentOptions)TangentOptions);
		}
		else
		{
			FMeshDescriptionOperations::CreateNormals(RenderMeshDescription, (FMeshDescriptionOperations::ETangentOptions)TangentOptions, true);
			//EComputeNTBsOptions ComputeNTBsOptions = (bHasAllNormals ? EComputeNTBsOptions::None : EComputeNTBsOptions::Normals) | (bHasAllTangents ? EComputeNTBsOptions::None : EComputeNTBsOptions::Tangents);
			//RenderMeshDescription->ComputeTangentsAndNormals(ComputeNTBsOptions);
		}
	}

	if (BuildSettings->bGenerateLightmapUVs && VertexInstanceArray.Num() > 0)
	{
		TVertexInstanceAttributeIndicesArray<FVector2D>& VertexInstanceUVs = RenderMeshDescription->VertexInstanceAttributes().GetAttributesSet<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);
		int32 NumIndices = VertexInstanceUVs.GetNumIndices();
		//Verify the src light map channel
		if (BuildSettings->SrcLightmapIndex >= NumIndices)
		{
			BuildSettings->SrcLightmapIndex = 0;
		}
		//Verify the destination light map channel
		if (BuildSettings->DstLightmapIndex >= NumIndices)
		{
			//Make sure we do not add illegal UV Channel index
			if (BuildSettings->DstLightmapIndex >= MAX_MESH_TEXTURE_COORDS_MD)
			{
				BuildSettings->DstLightmapIndex = MAX_MESH_TEXTURE_COORDS_MD - 1;
			}

			//Add some unused UVChannel to the mesh description for the lightmapUVs
			VertexInstanceUVs.SetNumIndices(BuildSettings->DstLightmapIndex + 1);
			BuildSettings->DstLightmapIndex = NumIndices;
		}
		FMeshDescriptionOperations::CreateLightMapUVLayout(RenderMeshDescription,
			BuildSettings->SrcLightmapIndex,
			BuildSettings->DstLightmapIndex,
			BuildSettings->MinLightmapResolution,
			(FMeshDescriptionOperations::ELightmapUVVersion)(StaticMesh->LightmapUVVersion),
			OverlappingCorners);
	}

	return RenderMeshDescription;
}

void FMeshDescriptionHelper::ReduceLOD(const UMeshDescription* BaseMesh, UMeshDescription* DestMesh, const FMeshReductionSettings& ReductionSettings, const TMultiMap<int32, int32>& InOverlappingCorners)
{
	if (BaseMesh == nullptr || DestMesh == nullptr)
	{
		//UE_LOG(LogMeshUtilities, Error, TEXT("Mesh contains zero source models."));
		return;
	}

	IMeshReductionManagerModule& MeshReductionModule = FModuleManager::Get().LoadModuleChecked<IMeshReductionManagerModule>("MeshReductionInterface");
	IMeshReduction* MeshReduction = MeshReductionModule.GetStaticMeshReductionInterface();
	// Reduce this LOD mesh according to its reduction settings.

	if (!MeshReduction || (ReductionSettings.PercentTriangles >= 1.0f && ReductionSettings.MaxDeviation <= 0.0f))
	{
		return;
	}
	float MaxDeviation = ReductionSettings.MaxDeviation;
	MeshReduction->ReduceMeshDescription(DestMesh, MaxDeviation, BaseMesh, InOverlappingCorners, ReductionSettings);
}

bool FMeshDescriptionHelper::IsValidOriginalMeshDescription()
{
	return OriginalMeshDescription != nullptr;
}

void FMeshDescriptionHelper::FindOverlappingCorners(const UMeshDescription* MeshDescription, float ComparisonThreshold)
{
	FMeshDescriptionOperations::FindOverlappingCorners(OverlappingCorners, MeshDescription, ComparisonThreshold);
}

