// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "MeshBuilder.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "MeshDescriptionHelper.h"
#include "MeshDescription.h"
#include "MeshAttributes.h"
#include "RawMesh.h"
#include "LayoutUV.h"


FMeshBuilder::FMeshBuilder()
{

}

void FMeshDescriptionOperations::ComputeMeshNTBs(class UMeshDescription* MeshDescription, const struct FMeshBuildSettings& BuildSettings)
{
	FVertexInstanceArray& VertexInstanceArray = MeshDescription->VertexInstances();
	TVertexInstanceAttributeArray<FVector>& Normals = MeshDescription->VertexInstanceAttributes().GetAttributes<FVector>(MeshAttribute::VertexInstance::Normal);
	TVertexInstanceAttributeArray<FVector>& Tangents = MeshDescription->VertexInstanceAttributes().GetAttributes<FVector>(MeshAttribute::VertexInstance::Tangent);
	TVertexInstanceAttributeArray<float>& BinormalSigns = MeshDescription->VertexInstanceAttributes().GetAttributes<float>(MeshAttribute::VertexInstance::BinormalSign);

	// Static meshes always blend normals of overlapping corners.
	uint32 TangentOptions = FMeshDescriptionHelper::ETangentOptions::BlendOverlappingNormals;
	if (BuildSettings.bRemoveDegenerates)
	{
		// If removing degenerate triangles, ignore them when computing tangents.
		TangentOptions |= FMeshDescriptionHelper::ETangentOptions::IgnoreDegenerateTriangles;
	}

	//This function make sure the Polygon NTB are compute and also remove degenerated triangle from the render mesh description.
	FMeshDescriptionHelper::CreatePolygonNTB(MeshDescription, BuildSettings.bRemoveDegenerates ? SMALL_NUMBER : 0.0f);

	//Keep the original mesh description NTBs if we do not rebuild the normals or tangents.
	bool bComputeTangentLegacy = !BuildSettings.bUseMikkTSpace && (BuildSettings.bRecomputeNormals || BuildSettings.bRecomputeTangents);
	bool bHasAllNormals = true;
	bool bHasAllTangents = true;
	for (const FVertexInstanceID VertexInstanceID : VertexInstanceArray.GetElementIDs())
	{
		// Dump normals and tangents if we are recomputing them.
		if (BuildSettings.bRecomputeTangents)
		{
			//Dump the tangents
			BinormalSigns[VertexInstanceID] = 0.0f;
			Tangents[VertexInstanceID] = FVector(0.0f);
		}
		if (BuildSettings.bRecomputeNormals)
		{
			//Dump the normals
			Normals[VertexInstanceID] = FVector(0.0f);
		}
		bHasAllNormals &= !Normals[VertexInstanceID].IsNearlyZero();
		bHasAllTangents &= !Tangents[VertexInstanceID].IsNearlyZero();
	}


	//MikkTSpace should be use only when the user want to recompute the normals or tangents otherwise should always fallback on builtin
	//We cannot use mikkt space with degenerated normals fallback on buitin.
	if (BuildSettings.bUseMikkTSpace && (BuildSettings.bRecomputeNormals || BuildSettings.bRecomputeTangents))
	{
		if (!bHasAllNormals)
		{
			FMeshDescriptionHelper::CreateNormals(MeshDescription, (FMeshDescriptionHelper::ETangentOptions)TangentOptions, false);
		}
		FMeshDescriptionHelper::CreateMikktTangents(MeshDescription, (FMeshDescriptionHelper::ETangentOptions)TangentOptions);
	}
	else if (!bHasAllNormals || !bHasAllTangents)
	{
		//Set the compute tangent to true when we do not build using mikkt space
		FMeshDescriptionHelper::CreateNormals(MeshDescription, (FMeshDescriptionHelper::ETangentOptions)TangentOptions, true);
	}
}

/** Convert this mesh description into the old FRawMesh format*/
void FMeshDescriptionOperations::ConverToRawMesh(const UMeshDescription* SourceMeshDescription, FRawMesh &DestinationRawMesh)
{
	FMeshDescriptionHelper::ConverToRawMesh(SourceMeshDescription, DestinationRawMesh);
}

/** Convert old FRawMesh format to MeshDescription*/
void FMeshDescriptionOperations::ConverFromRawMesh(const FRawMesh &SourceRawMesh, UMeshDescription* DestinationMeshDescription)
{
	FMeshDescriptionHelper::ConverFromRawMesh(SourceRawMesh, DestinationMeshDescription);
}

bool FMeshDescriptionOperations::GenerateUniqueUVsForStaticMesh(const UMeshDescription* MeshDescription, int32 TextureResolution, TArray<FVector2D>& OutTexCoords)
{
	// Create a copy of original mesh (only copy necessary data)
	UMeshDescription* DuplicateMeshDescription = Cast<UMeshDescription>(StaticDuplicateObject(MeshDescription, GetTransientPackage(), NAME_None, RF_NoFlags));

	// Find overlapping corners for UV generator. Allow some threshold - this should not produce any error in a case if resulting
	// mesh will not merge these vertices.
	TMultiMap<int32, int32> OverlappingCorners;
	FMeshDescriptionHelper::FindOverlappingCorners(OverlappingCorners, DuplicateMeshDescription, THRESH_POINTS_ARE_SAME);

	// Generate new UVs
	FLayoutUV Packer(DuplicateMeshDescription, 0, 1, FMath::Clamp(TextureResolution / 4, 32, 512));
	Packer.FindCharts(OverlappingCorners);

	bool bPackSuccess = Packer.FindBestPacking();
	if (bPackSuccess)
	{
		Packer.CommitPackedUVs();
		TVertexInstanceAttributeIndicesArray<FVector2D>& VertexInstanceUVs = DuplicateMeshDescription->VertexInstanceAttributes().GetAttributesSet<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);
		// Save generated UVs
		check(VertexInstanceUVs.GetNumIndices() > 1);
		auto& UniqueUVsArray = VertexInstanceUVs.GetArrayForIndex(1);
		OutTexCoords.AddZeroed(UniqueUVsArray.Num());
		int32 TextureCoordIndex = 0;
		for(const FVertexInstanceID& VertexInstanceID : DuplicateMeshDescription->VertexInstances().GetElementIDs())
		{
			OutTexCoords[TextureCoordIndex++] = UniqueUVsArray[VertexInstanceID];
		}
	}
	//Make sure the transient duplicate will be GC
	DuplicateMeshDescription->MarkPendingKill();

	return bPackSuccess;
}