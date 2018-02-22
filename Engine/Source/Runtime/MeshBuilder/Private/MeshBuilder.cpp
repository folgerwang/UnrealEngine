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

bool FMeshBuilderOperations::GenerateUniqueUVsForStaticMesh(const UMeshDescription* MeshDescription, int32 TextureResolution, TArray<FVector2D>& OutTexCoords)
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