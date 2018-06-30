// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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

FMeshDescriptionHelper::FMeshDescriptionHelper(FMeshBuildSettings* InBuildSettings)
	: BuildSettings(InBuildSettings)
{
}

void FMeshDescriptionHelper::GetRenderMeshDescription(UObject* Owner, const FMeshDescription& InOriginalMeshDescription, FMeshDescription& OutRenderMeshDescription)
{
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(Owner);
	check(StaticMesh);

	//Copy the Original Mesh Description in the render mesh description
	OutRenderMeshDescription = InOriginalMeshDescription;
	float ComparisonThreshold = BuildSettings->bRemoveDegenerates ? THRESH_POINTS_ARE_SAME : 0.0f;
	
	//This function make sure the Polygon NTB are compute and also remove degenerated triangle from the render mesh description.
	FMeshDescriptionOperations::CreatePolygonNTB(OutRenderMeshDescription, ComparisonThreshold);
	//OutRenderMeshDescription->ComputePolygonTangentsAndNormals(BuildSettings->bRemoveDegenerates ? SMALL_NUMBER : 0.0f);

	FVertexInstanceArray& VertexInstanceArray = OutRenderMeshDescription.VertexInstances();
	TVertexInstanceAttributeArray<FVector>& Normals = OutRenderMeshDescription.VertexInstanceAttributes().GetAttributes<FVector>( MeshAttribute::VertexInstance::Normal );
	TVertexInstanceAttributeArray<FVector>& Tangents = OutRenderMeshDescription.VertexInstanceAttributes().GetAttributes<FVector>( MeshAttribute::VertexInstance::Tangent );
	TVertexInstanceAttributeArray<float>& BinormalSigns = OutRenderMeshDescription.VertexInstanceAttributes().GetAttributes<float>( MeshAttribute::VertexInstance::BinormalSign );

	// Find overlapping corners to accelerate adjacency.
	FMeshDescriptionOperations::FindOverlappingCorners(OverlappingCorners, OutRenderMeshDescription, ComparisonThreshold);

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
				FMeshDescriptionOperations::CreateNormals(OutRenderMeshDescription, (FMeshDescriptionOperations::ETangentOptions)TangentOptions, false);

				//EComputeNTBsOptions ComputeNTBsOptions = EComputeNTBsOptions::Normals;
				//OutRenderMeshDescription.ComputeTangentsAndNormals(ComputeNTBsOptions);
			}
			FMeshDescriptionOperations::CreateMikktTangents(OutRenderMeshDescription, (FMeshDescriptionOperations::ETangentOptions)TangentOptions);
		}
		else
		{
			FMeshDescriptionOperations::CreateNormals(OutRenderMeshDescription, (FMeshDescriptionOperations::ETangentOptions)TangentOptions, true);
			//EComputeNTBsOptions ComputeNTBsOptions = (bHasAllNormals ? EComputeNTBsOptions::None : EComputeNTBsOptions::Normals) | (bHasAllTangents ? EComputeNTBsOptions::None : EComputeNTBsOptions::Tangents);
			//OutRenderMeshDescription.ComputeTangentsAndNormals(ComputeNTBsOptions);
		}
	}

	if (BuildSettings->bGenerateLightmapUVs && VertexInstanceArray.Num() > 0)
	{
		TVertexInstanceAttributeIndicesArray<FVector2D>& VertexInstanceUVs = OutRenderMeshDescription.VertexInstanceAttributes().GetAttributesSet<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);
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
		FMeshDescriptionOperations::CreateLightMapUVLayout(OutRenderMeshDescription,
			BuildSettings->SrcLightmapIndex,
			BuildSettings->DstLightmapIndex,
			BuildSettings->MinLightmapResolution,
			(FMeshDescriptionOperations::ELightmapUVVersion)(StaticMesh->LightmapUVVersion),
			OverlappingCorners);
	}
}

void FMeshDescriptionHelper::ReduceLOD(const FMeshDescription& BaseMesh, FMeshDescription& DestMesh, const FMeshReductionSettings& ReductionSettings, const TMultiMap<int32, int32>& InOverlappingCorners, float &OutMaxDeviation)
{
	IMeshReductionManagerModule& MeshReductionModule = FModuleManager::Get().LoadModuleChecked<IMeshReductionManagerModule>("MeshReductionInterface");
	IMeshReduction* MeshReduction = MeshReductionModule.GetStaticMeshReductionInterface();
	// Reduce this LOD mesh according to its reduction settings.

	if (!MeshReduction || (ReductionSettings.PercentTriangles >= 1.0f && ReductionSettings.MaxDeviation <= 0.0f))
	{
		return;
	}
	OutMaxDeviation = ReductionSettings.MaxDeviation;
	MeshReduction->ReduceMeshDescription(DestMesh, OutMaxDeviation, BaseMesh, InOverlappingCorners, ReductionSettings);
}

void FMeshDescriptionHelper::FindOverlappingCorners(const FMeshDescription& MeshDescription, float ComparisonThreshold)
{
	FMeshDescriptionOperations::FindOverlappingCorners(OverlappingCorners, MeshDescription, ComparisonThreshold);
}

