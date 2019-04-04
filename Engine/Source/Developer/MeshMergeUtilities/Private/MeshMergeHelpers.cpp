// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MeshMergeHelpers.h"

#include "Engine/MapBuildDataRegistry.h"
#include "Engine/MeshMerging.h"

#include "MaterialOptions.h"
#include "MeshDescription.h"
#include "MeshAttributes.h"
#include "MeshAttributeArray.h"
#include "MeshDescriptionOperations.h"

#include "Misc/PackageName.h"
#include "MaterialUtilities.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Rendering/SkeletalMeshModel.h"

#include "SkeletalMeshTypes.h"
#include "SkeletalRenderPublic.h"

#include "UObject/UObjectBaseUtility.h"
#include "UObject/Package.h"
#include "Materials/Material.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "HierarchicalLODUtilitiesModule.h"
#include "MeshMergeData.h"
#include "IHierarchicalLODUtilities.h"
#include "Engine/MeshMergeCullingVolume.h"

#include "Landscape.h"
#include "LandscapeProxy.h"

#include "Editor.h"

#include "Engine/StaticMesh.h"
#include "PhysicsEngine/ConvexElem.h"
#include "PhysicsEngine/BodySetup.h"
#include "MeshUtilities.h"
#include "ImageUtils.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "IMeshReductionManagerModule.h"
#include "LayoutUV.h"
#include "Components/InstancedStaticMeshComponent.h"

//DECLARE_LOG_CATEGORY_CLASS(LogMeshMerging, Verbose, All);

void FMeshMergeHelpers::ExtractSections(const UStaticMeshComponent* Component, int32 LODIndex, TArray<FSectionInfo>& OutSections)
{
	static UMaterialInterface* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);

	const UStaticMesh* StaticMesh = Component->GetStaticMesh();

	TArray<FName> MaterialSlotNames;
	for (const FStaticMaterial& StaticMaterial : StaticMesh->StaticMaterials)
	{
#if WITH_EDITOR
		MaterialSlotNames.Add(StaticMaterial.ImportedMaterialSlotName);
#else
		MaterialSlotNames.Add(StaticMaterial.MaterialSlotName);
#endif
	}

	const bool bMirrored = Component->GetComponentToWorld().GetDeterminant() < 0.f;
	for (const FStaticMeshSection& MeshSection : StaticMesh->RenderData->LODResources[LODIndex].Sections)
	{
		// Retrieve material for this section
		UMaterialInterface* StoredMaterial = Component->GetMaterial(MeshSection.MaterialIndex);

		// Make sure the resource actual exists, otherwise use default material
		StoredMaterial = (StoredMaterial != nullptr) && StoredMaterial->GetMaterialResource(GMaxRHIFeatureLevel) ? StoredMaterial : DefaultMaterial;

		// Populate section data
		FSectionInfo SectionInfo;
		SectionInfo.Material = StoredMaterial;
		SectionInfo.MaterialIndex = MeshSection.MaterialIndex;
		SectionInfo.MaterialSlotName = MaterialSlotNames.IsValidIndex(MeshSection.MaterialIndex) ? MaterialSlotNames[MeshSection.MaterialIndex] : NAME_None;
		SectionInfo.StartIndex = MeshSection.FirstIndex / 3;
		SectionInfo.EndIndex = SectionInfo.StartIndex + MeshSection.NumTriangles;

		// In case the object is mirrored the material indices/vertex data will be reversed in place, so we need to adjust the sections accordingly
		if (bMirrored)
		{
			const uint32 NumTriangles = StaticMesh->RenderData->LODResources[LODIndex].GetNumTriangles();
			SectionInfo.StartIndex = NumTriangles - SectionInfo.EndIndex;
			SectionInfo.EndIndex = SectionInfo.StartIndex + MeshSection.NumTriangles;
		}

		if (MeshSection.bEnableCollision)
		{
			SectionInfo.EnabledProperties.Add(GET_MEMBER_NAME_CHECKED(FStaticMeshSection, bEnableCollision));
		}

		if (MeshSection.bCastShadow && Component->CastShadow)
		{
			SectionInfo.EnabledProperties.Add(GET_MEMBER_NAME_CHECKED(FStaticMeshSection, bCastShadow));
		}

		OutSections.Add(SectionInfo);
	}
}

void FMeshMergeHelpers::ExtractSections(const USkeletalMeshComponent* Component, int32 LODIndex, TArray<FSectionInfo>& OutSections)
{
	static UMaterialInterface* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	FSkeletalMeshModel* Resource = Component->SkeletalMesh->GetImportedModel();

	checkf(Resource->LODModels.IsValidIndex(LODIndex), TEXT("Invalid LOD Index"));

	TArray<FName> MaterialSlotNames = Component->GetMaterialSlotNames();

	const FSkeletalMeshLODModel& Model = Resource->LODModels[LODIndex];
	for (const FSkelMeshSection& MeshSection : Model.Sections)
	{
		// Retrieve material for this section
		UMaterialInterface* StoredMaterial = Component->GetMaterial(MeshSection.MaterialIndex);
		// Make sure the resource actual exists, otherwise use default material
		StoredMaterial = (StoredMaterial != nullptr) && StoredMaterial->GetMaterialResource(GMaxRHIFeatureLevel) ? StoredMaterial : DefaultMaterial;

		FSectionInfo SectionInfo;
		SectionInfo.Material = StoredMaterial;
		SectionInfo.MaterialSlotName = MaterialSlotNames.IsValidIndex(MeshSection.MaterialIndex) ? MaterialSlotNames[MeshSection.MaterialIndex] : NAME_None;

		if (MeshSection.bCastShadow && Component->CastShadow)
		{
			SectionInfo.EnabledProperties.Add(GET_MEMBER_NAME_CHECKED(FSkelMeshSection, bCastShadow));
		}

		if (MeshSection.bRecomputeTangent)
		{
			SectionInfo.EnabledProperties.Add(GET_MEMBER_NAME_CHECKED(FSkelMeshSection, bRecomputeTangent));
		}

		OutSections.Add(SectionInfo);
	}
}

void FMeshMergeHelpers::ExtractSections(const UStaticMesh* StaticMesh, int32 LODIndex, TArray<FSectionInfo>& OutSections)
{
	static UMaterialInterface* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);

	for (const FStaticMeshSection& MeshSection : StaticMesh->RenderData->LODResources[LODIndex].Sections)
	{
		// Retrieve material for this section
		UMaterialInterface* StoredMaterial = StaticMesh->GetMaterial(MeshSection.MaterialIndex);

		// Make sure the resource actual exists, otherwise use default material
		StoredMaterial = (StoredMaterial != nullptr) && StoredMaterial->GetMaterialResource(GMaxRHIFeatureLevel) ? StoredMaterial : DefaultMaterial;

		// Populate section data
		FSectionInfo SectionInfo;
		SectionInfo.Material = StoredMaterial;
		SectionInfo.MaterialIndex = MeshSection.MaterialIndex;
#if WITH_EDITOR
		SectionInfo.MaterialSlotName = StaticMesh->StaticMaterials.IsValidIndex(MeshSection.MaterialIndex) ? StaticMesh->StaticMaterials[MeshSection.MaterialIndex].ImportedMaterialSlotName : NAME_None;
#else
		SectionInfo.MaterialSlotName = StaticMesh->StaticMaterials.IsValidIndex(MeshSection.MaterialIndex) ? StaticMesh->StaticMaterials[MeshSection.MaterialIndex].MaterialSlotName : NAME_None;
#endif
		

		if (MeshSection.bEnableCollision)
		{
			SectionInfo.EnabledProperties.Add(GET_MEMBER_NAME_CHECKED(FStaticMeshSection, bEnableCollision));
		}

		if (MeshSection.bCastShadow)
		{
			SectionInfo.EnabledProperties.Add(GET_MEMBER_NAME_CHECKED(FStaticMeshSection, bCastShadow));
		}

		OutSections.Add(SectionInfo);
	}
}

void FMeshMergeHelpers::ExpandInstances(const UInstancedStaticMeshComponent* InInstancedStaticMeshComponent, FMeshDescription& InOutRawMesh, TArray<FSectionInfo>& InOutSections)
{
	FMeshDescription CombinedRawMesh;

	for(const FInstancedStaticMeshInstanceData& InstanceData : InInstancedStaticMeshComponent->PerInstanceSMData)
	{
		FMeshDescription InstanceRawMesh = InOutRawMesh;
		FMeshMergeHelpers::TransformRawMeshVertexData(FTransform(InstanceData.Transform), InstanceRawMesh);
		FMeshMergeHelpers::AppendRawMesh(CombinedRawMesh, InstanceRawMesh);
	}

	InOutRawMesh = CombinedRawMesh;
}

void FMeshMergeHelpers::RetrieveMesh(const UStaticMeshComponent* StaticMeshComponent, int32 LODIndex, FMeshDescription& RawMesh, bool bPropagateVertexColours)
{
	const UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
	const FStaticMeshSourceModel& StaticMeshModel = StaticMesh->SourceModels[LODIndex];

	const bool bIsSplineMeshComponent = StaticMeshComponent->IsA<USplineMeshComponent>();

	// Imported meshes will have a valid mesh description
	const bool bImportedMesh = StaticMesh->IsMeshDescriptionValid(LODIndex);
		
	// Export the raw mesh data using static mesh render data
	ExportStaticMeshLOD(StaticMesh->RenderData->LODResources[LODIndex], RawMesh, StaticMesh->StaticMaterials);

	// Make sure the raw mesh is not irreparably malformed.
	if (RawMesh.VertexInstances().Num() <= 0)
	{
		return;
	}

	// Use build settings from base mesh for LOD entries that was generated inside Editor.
	const FMeshBuildSettings& BuildSettings = bImportedMesh ? StaticMeshModel.BuildSettings : StaticMesh->SourceModels[0].BuildSettings;

	// Transform raw mesh to world space
	FTransform ComponentToWorldTransform = StaticMeshComponent->GetComponentTransform();

	// Handle spline mesh deformation
	if (bIsSplineMeshComponent)
	{
		const USplineMeshComponent* SplineMeshComponent = Cast<USplineMeshComponent>(StaticMeshComponent);
		// Deform raw mesh data according to the Spline Mesh Component's data
		PropagateSplineDeformationToRawMesh(SplineMeshComponent, RawMesh);
	}

	// If specified propagate painted vertex colors into our raw mesh
	if (bPropagateVertexColours)
	{
		PropagatePaintedColorsToRawMesh(StaticMeshComponent, LODIndex, RawMesh);
	}

	// Transform raw mesh vertex data by the Static Mesh Component's component to world transformation	
	TransformRawMeshVertexData(ComponentToWorldTransform, RawMesh);

	if (RawMesh.VertexInstances().Num() <= 0)
	{
		return;
	}

	// Figure out if we should recompute normals and tangents. By default generated LODs should not recompute normals	
	uint32 TangentOptions = FMeshDescriptionOperations::ETangentOptions::BlendOverlappingNormals;
	if (BuildSettings.bRemoveDegenerates)
	{
		// If removing degenerate triangles, ignore them when computing tangents.
		TangentOptions |= FMeshDescriptionOperations::ETangentOptions::IgnoreDegenerateTriangles;
	}
	FMeshDescriptionOperations::CreatePolygonNTB(RawMesh, 0.0f);
	FMeshDescriptionOperations::RecomputeNormalsAndTangentsIfNeeded(RawMesh, (FMeshDescriptionOperations::ETangentOptions)TangentOptions, BuildSettings.bUseMikkTSpace);
}

void FMeshMergeHelpers::RetrieveMesh(USkeletalMeshComponent* SkeletalMeshComponent, int32 LODIndex, FMeshDescription& RawMesh, bool bPropagateVertexColours)
{
	FSkeletalMeshModel* Resource = SkeletalMeshComponent->SkeletalMesh->GetImportedModel();
	if (Resource->LODModels.IsValidIndex(LODIndex))
	{
		FSkeletalMeshLODInfo& SrcLODInfo = *(SkeletalMeshComponent->SkeletalMesh->GetLODInfo(LODIndex));

		// Get the CPU skinned verts for this LOD
		TArray<FFinalSkinVertex> FinalVertices;
		SkeletalMeshComponent->GetCPUSkinnedVertices(FinalVertices, LODIndex);

		FSkeletalMeshLODModel& LODModel = Resource->LODModels[LODIndex];
		
		const int32 NumSections = LODModel.Sections.Num();
		
		//Empty the raw mesh
		RawMesh.Empty();

		TVertexAttributesRef<FVector> VertexPositions = RawMesh.VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);
		TEdgeAttributesRef<bool> EdgeHardnesses = RawMesh.EdgeAttributes().GetAttributesRef<bool>(MeshAttribute::Edge::IsHard);
		TEdgeAttributesRef<float> EdgeCreaseSharpnesses = RawMesh.EdgeAttributes().GetAttributesRef<float>(MeshAttribute::Edge::CreaseSharpness);
		TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames = RawMesh.PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);
		TVertexInstanceAttributesRef<FVector> VertexInstanceNormals = RawMesh.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
		TVertexInstanceAttributesRef<FVector> VertexInstanceTangents = RawMesh.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
		TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = RawMesh.VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);
		TVertexInstanceAttributesRef<FVector4> VertexInstanceColors = RawMesh.VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color);
		TVertexInstanceAttributesRef<FVector2D> VertexInstanceUVs = RawMesh.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);

		int32 TotalTriangles = 0;
		int32 TotalCorners = 0;
		for (int32 SectionIndex = 0; SectionIndex < NumSections; SectionIndex++)
		{
			const FSkelMeshSection& SkelMeshSection = LODModel.Sections[SectionIndex];
			TotalTriangles += SkelMeshSection.NumTriangles;
		}
		TotalCorners = TotalTriangles * 3;
		RawMesh.ReserveNewVertices(FinalVertices.Num());
		RawMesh.ReserveNewPolygons(TotalTriangles);
		RawMesh.ReserveNewVertexInstances(TotalCorners);
		RawMesh.ReserveNewEdges(TotalCorners);

		// Copy skinned vertex positions
		for (int32 VertIndex = 0; VertIndex < FinalVertices.Num(); ++VertIndex)
		{
			const FVertexID VertexID = RawMesh.CreateVertex();
			VertexPositions[VertexID] = FinalVertices[VertIndex].Position;
		}

		VertexInstanceUVs.SetNumIndices(MAX_TEXCOORDS);

		
		for (int32 SectionIndex = 0; SectionIndex < NumSections; SectionIndex++)
		{
			const FSkelMeshSection& SkelMeshSection = LODModel.Sections[SectionIndex];
			const int32 NumWedges = SkelMeshSection.NumTriangles * 3;

			//Create the polygon group ID
			int32 SectionMaterialIndex = SkelMeshSection.MaterialIndex;
			int32 MaterialIndex = SectionMaterialIndex;
			// use the remapping of material indices for all LODs besides the base LOD 
			if (LODIndex > 0 && SrcLODInfo.LODMaterialMap.IsValidIndex(SkelMeshSection.MaterialIndex))
			{
				MaterialIndex = FMath::Clamp<int32>(SrcLODInfo.LODMaterialMap[SkelMeshSection.MaterialIndex], 0, SkeletalMeshComponent->SkeletalMesh->Materials.Num());
			}

			FName ImportedMaterialSlotName = SkeletalMeshComponent->SkeletalMesh->Materials[MaterialIndex].ImportedMaterialSlotName;
			const FPolygonGroupID SectionPolygonGroupID(SectionMaterialIndex);
			if (!RawMesh.IsPolygonGroupValid(SectionPolygonGroupID))
			{
				RawMesh.CreatePolygonGroupWithID(SectionPolygonGroupID);
				PolygonGroupImportedMaterialSlotNames[SectionPolygonGroupID] = ImportedMaterialSlotName;
			}
			int32 WedgeIndex = 0;
			for (uint32 SectionTriangleIndex = 0; SectionTriangleIndex < SkelMeshSection.NumTriangles; ++SectionTriangleIndex)
			{
				FVertexID VertexIndexes[3];
				TArray<FVertexInstanceID> VertexInstanceIDs;
				VertexInstanceIDs.SetNum(3);
				for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex, ++WedgeIndex)
				{
					const int32 VertexIndexForWedge = LODModel.IndexBuffer[SkelMeshSection.BaseIndex + WedgeIndex];
					VertexIndexes[CornerIndex] = FVertexID(VertexIndexForWedge);
					FVertexInstanceID VertexInstanceID = RawMesh.CreateVertexInstance(VertexIndexes[CornerIndex]);
					VertexInstanceIDs[CornerIndex] = VertexInstanceID;
					
					const FSoftSkinVertex& SoftVertex = SkelMeshSection.SoftVertices[VertexIndexForWedge - SkelMeshSection.BaseVertexIndex];
					const FFinalSkinVertex& SkinnedVertex = FinalVertices[VertexIndexForWedge];

					//Set NTBs
					const FVector TangentX = SkinnedVertex.TangentX.ToFVector();
					const FVector TangentZ = SkinnedVertex.TangentZ.ToFVector();
					//@todo: do we need to inverse the sign between skeletalmesh and staticmesh, the old code was doing so.
					const float TangentYSign = SkinnedVertex.TangentZ.ToFVector4().W;
					
					VertexInstanceTangents[VertexInstanceID] = TangentX;
					VertexInstanceBinormalSigns[VertexInstanceID] = TangentYSign;
					VertexInstanceNormals[VertexInstanceID] = TangentZ;

					for (uint32 TexCoordIndex = 0; TexCoordIndex < MAX_TEXCOORDS; TexCoordIndex++)
					{
						//Add this vertex instance tex coord
						VertexInstanceUVs.Set(VertexInstanceID, TexCoordIndex, SoftVertex.UVs[TexCoordIndex]);
					}

					//Add this vertex instance color
					VertexInstanceColors[VertexInstanceID] = bPropagateVertexColours ? FVector4(FLinearColor(SoftVertex.Color)) : FVector4(1.0f, 1.0f, 1.0f);
				}
				//Create a polygon from this triangle
				const FPolygonID NewPolygonID = RawMesh.CreatePolygon(SectionPolygonGroupID, VertexInstanceIDs);
				//Triangulate the polygon
				FMeshPolygon& Polygon = RawMesh.GetPolygon(NewPolygonID);
				RawMesh.ComputePolygonTriangulation(NewPolygonID, Polygon.Triangles);
			}
		}
	}
}

void FMeshMergeHelpers::RetrieveMesh(const UStaticMesh* StaticMesh, int32 LODIndex, FMeshDescription& RawMesh)
{
	const FStaticMeshSourceModel& StaticMeshModel = StaticMesh->SourceModels[LODIndex];

	// Imported meshes will have a valid mesh description
	const bool bImportedMesh = StaticMesh->IsMeshDescriptionValid(LODIndex);
	
	// Check whether or not this mesh has been reduced in-engine
	const bool bReducedMesh = StaticMesh->IsReductionActive(LODIndex);
	// Trying to retrieve rawmesh from SourceStaticMeshModel was giving issues, which causes a mismatch			
	const bool bRenderDataMismatch = (LODIndex > 0) || StaticMeshModel.BuildSettings.bGenerateLightmapUVs;

	if (bImportedMesh && !bReducedMesh && !bRenderDataMismatch)
	{
		RawMesh = *StaticMesh->GetMeshDescription(LODIndex);
	}
	else
	{
		ExportStaticMeshLOD(StaticMesh->RenderData->LODResources[LODIndex], RawMesh, StaticMesh->StaticMaterials);
	}

	// Make sure the raw mesh is not irreparably malformed.
	if (RawMesh.VertexInstances().Num() <= 0)
	{
		// wrong
		bool check = true;
	}

	// Use build settings from base mesh for LOD entries that was generated inside Editor.
	const FMeshBuildSettings& BuildSettings = bImportedMesh ? StaticMeshModel.BuildSettings : StaticMesh->SourceModels[0].BuildSettings;

	// Figure out if we should recompute normals and tangents. By default generated LODs should not recompute normals	
	uint32 TangentOptions = FMeshDescriptionOperations::ETangentOptions::BlendOverlappingNormals;
	if (BuildSettings.bRemoveDegenerates)
	{
		// If removing degenerate triangles, ignore them when computing tangents.
		TangentOptions |= FMeshDescriptionOperations::ETangentOptions::IgnoreDegenerateTriangles;
	}
	FMeshDescriptionOperations::CreatePolygonNTB(RawMesh, 0.0f);
	FMeshDescriptionOperations::RecomputeNormalsAndTangentsIfNeeded(RawMesh, (FMeshDescriptionOperations::ETangentOptions)TangentOptions, BuildSettings.bUseMikkTSpace, (bImportedMesh && BuildSettings.bRecomputeNormals), (bImportedMesh && BuildSettings.bRecomputeTangents));
}

void FMeshMergeHelpers::ExportStaticMeshLOD(const FStaticMeshLODResources& StaticMeshLOD, FMeshDescription& OutRawMesh, const TArray<FStaticMaterial>& Materials)
{
	const int32 NumWedges = StaticMeshLOD.IndexBuffer.GetNumIndices();
	const int32 NumVertexPositions = StaticMeshLOD.VertexBuffers.PositionVertexBuffer.GetNumVertices();
	const int32 NumFaces = NumWedges / 3;

	OutRawMesh.Empty();

	if (NumVertexPositions <= 0 || StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices() <= 0)
	{
		return;
	}

	TVertexAttributesRef<FVector> VertexPositions = OutRawMesh.VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);
	TEdgeAttributesRef<bool> EdgeHardnesses = OutRawMesh.EdgeAttributes().GetAttributesRef<bool>(MeshAttribute::Edge::IsHard);
	TEdgeAttributesRef<float> EdgeCreaseSharpnesses = OutRawMesh.EdgeAttributes().GetAttributesRef<float>(MeshAttribute::Edge::CreaseSharpness);
	TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames = OutRawMesh.PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);
	TVertexInstanceAttributesRef<FVector> VertexInstanceNormals = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
	TVertexInstanceAttributesRef<FVector> VertexInstanceTangents = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
	TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);
	TVertexInstanceAttributesRef<FVector4> VertexInstanceColors = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color);
	TVertexInstanceAttributesRef<FVector2D> VertexInstanceUVs = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);

	OutRawMesh.ReserveNewVertices(NumVertexPositions);
	OutRawMesh.ReserveNewVertexInstances(NumWedges);
	OutRawMesh.ReserveNewPolygons(NumFaces);
	OutRawMesh.ReserveNewEdges(NumWedges);

	const int32 NumTexCoords = StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();
	VertexInstanceUVs.SetNumIndices(NumTexCoords);

	
	for (int32 SectionIndex = 0; SectionIndex < StaticMeshLOD.Sections.Num(); ++SectionIndex)
	{
		const FStaticMeshSection& Section = StaticMeshLOD.Sections[SectionIndex];
		FPolygonGroupID CurrentPolygonGroupID = OutRawMesh.CreatePolygonGroup();
		check(CurrentPolygonGroupID.GetValue() == SectionIndex);
		if (Materials.IsValidIndex(Section.MaterialIndex))
		{
			PolygonGroupImportedMaterialSlotNames[CurrentPolygonGroupID] = Materials[Section.MaterialIndex].ImportedMaterialSlotName;
		}
		else
		{
			PolygonGroupImportedMaterialSlotNames[CurrentPolygonGroupID] = FName(*(TEXT("MeshMergeMaterial_") + FString::FromInt(SectionIndex)));
		}
	}

	//Create the vertex
	for (int32 VertexIndex = 0; VertexIndex < NumVertexPositions; ++VertexIndex)
	{
		FVertexID VertexID = OutRawMesh.CreateVertex();
		VertexPositions[VertexID] = StaticMeshLOD.VertexBuffers.PositionVertexBuffer.VertexPosition(VertexIndex);
	}

	//Create the vertex instances
	for (int32 TriangleIndex = 0; TriangleIndex < NumFaces; ++TriangleIndex)
	{
		FPolygonGroupID CurrentPolygonGroupID = FPolygonGroupID::Invalid;
		for (int32 SectionIndex = 0; SectionIndex < StaticMeshLOD.Sections.Num(); ++SectionIndex)
		{
			const FStaticMeshSection& Section = StaticMeshLOD.Sections[SectionIndex];
			uint32 FirstTriangle = Section.FirstIndex / 3;
			uint32 LastTriangle = FirstTriangle + Section.NumTriangles - 1;
			if ((uint32)TriangleIndex >= FirstTriangle && (uint32)TriangleIndex <= LastTriangle)
			{
				CurrentPolygonGroupID = FPolygonGroupID(SectionIndex);
				break;
			}
		}
		check(CurrentPolygonGroupID != FPolygonGroupID::Invalid);

		FVertexID VertexIDs[3];
		TArray<FVertexInstanceID> VertexInstanceIDs;
		VertexInstanceIDs.SetNum(3);

		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			int32 WedgeIndex = StaticMeshLOD.IndexBuffer.GetIndex(TriangleIndex * 3 + Corner);
			FVertexID VertexID(WedgeIndex);
			FVertexInstanceID VertexInstanceID = OutRawMesh.CreateVertexInstance(VertexID);
			VertexIDs[Corner] = VertexID;
			VertexInstanceIDs[Corner] = VertexInstanceID;

			//NTBs
			FVector TangentX = StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentX(WedgeIndex);
			FVector TangentY = StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentY(WedgeIndex);
			FVector TangentZ = StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(WedgeIndex);
			VertexInstanceTangents[VertexInstanceID] = TangentX;
			VertexInstanceBinormalSigns[VertexInstanceID] = GetBasisDeterminantSign(TangentX, TangentY, TangentZ);
			VertexInstanceNormals[VertexInstanceID] = TangentZ;

			// Vertex colors
			if (StaticMeshLOD.VertexBuffers.ColorVertexBuffer.GetNumVertices() > 0)
			{
				VertexInstanceColors[VertexInstanceID] = FLinearColor(StaticMeshLOD.VertexBuffers.ColorVertexBuffer.VertexColor(WedgeIndex));
			}
			else
			{
				VertexInstanceColors[VertexInstanceID] = FLinearColor::White;
			}

			//Tex coord
			for (int32 TexCoodIdx = 0; TexCoodIdx < NumTexCoords; ++TexCoodIdx)
			{
				VertexInstanceUVs.Set(VertexInstanceID, TexCoodIdx, StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(WedgeIndex, TexCoodIdx));
			}
		}
		//Create a polygon from this triangle
		const FPolygonID NewPolygonID = OutRawMesh.CreatePolygon(CurrentPolygonGroupID, VertexInstanceIDs);
		//Triangulate the polygon
		FMeshPolygon& Polygon = OutRawMesh.GetPolygon(NewPolygonID);
		OutRawMesh.ComputePolygonTriangulation(NewPolygonID, Polygon.Triangles);
	}
}

bool FMeshMergeHelpers::CheckWrappingUVs(const TArray<FVector2D>& UVs)
{	
	bool bResult = false;

	FVector2D Min(FLT_MAX, FLT_MAX);
	FVector2D Max(-FLT_MAX, -FLT_MAX);
	for (const FVector2D& Coordinate : UVs)
	{
		if ((FMath::IsNegativeFloat(Coordinate.X) || FMath::IsNegativeFloat(Coordinate.Y)) || (Coordinate.X > (1.0f + KINDA_SMALL_NUMBER) || Coordinate.Y > (1.0f + KINDA_SMALL_NUMBER)))
		{
			bResult = true;
			break;
		}
	}

	return bResult;
}

bool FMeshMergeHelpers::CheckWrappingUVs(const FMeshDescription& MeshDescription, int32 UVChannelIndex)
{
	TVertexInstanceAttributesConstRef<FVector2D> VertexInstanceUVs = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);
	bool bResult = false;

	//Validate the channel, return false if there is an invalid channel index
	if (UVChannelIndex < 0 || UVChannelIndex >= VertexInstanceUVs.GetNumIndices())
	{
		return bResult;
	}

	for (const FVertexInstanceID& VertexInstanceID : MeshDescription.VertexInstances().GetElementIDs())
	{
		const FVector2D& Coordinate = VertexInstanceUVs.Get(VertexInstanceID, UVChannelIndex);
		if ((FMath::IsNegativeFloat(Coordinate.X) || FMath::IsNegativeFloat(Coordinate.Y)) || (Coordinate.X > (1.0f + KINDA_SMALL_NUMBER) || Coordinate.Y > (1.0f + KINDA_SMALL_NUMBER)))
		{
			bResult = true;
			break;
		}
	}

	return bResult;
}

void FMeshMergeHelpers::CullTrianglesFromVolumesAndUnderLandscapes(const UWorld* World, const FBoxSphereBounds& Bounds, FMeshDescription& InOutRawMesh)
{
	TArray<ALandscapeProxy*> Landscapes;
	TArray<AMeshMergeCullingVolume*> CullVolumes;

	FBox BoxBounds = Bounds.GetBox();

	for (ULevel* Level : World->GetLevels())
	{
		for (AActor* Actor : Level->Actors)
		{
			ALandscape* Proxy = Cast<ALandscape>(Actor);
			if (Proxy && Proxy->bUseLandscapeForCullingInvisibleHLODVertices)
			{
				FVector Origin, Extent;
				Proxy->GetActorBounds(false, Origin, Extent);
				FBox LandscapeBox(Origin - Extent, Origin + Extent);

				// Ignore Z axis for 2d bounds check
				if (LandscapeBox.IntersectXY(BoxBounds))
				{
					Landscapes.Add(Proxy->GetLandscapeActor());
				}
			}

			// Check for culling volumes
			AMeshMergeCullingVolume* Volume = Cast<AMeshMergeCullingVolume>(Actor);
			if (Volume)
			{
				// If the mesh's bounds intersect with the volume there is a possibility of culling
				const bool bIntersecting = Volume->EncompassesPoint(Bounds.Origin, Bounds.SphereRadius, nullptr);
				if (bIntersecting)
				{
					CullVolumes.Add(Volume);
				}
			}
		}
	}

	TVertexAttributesConstRef<FVector> VertexPositions = InOutRawMesh.VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);

	TMap<FVertexID, bool> VertexVisible;
	VertexVisible.Reserve(InOutRawMesh.Vertices().Num());
	int32 Index = 0;
	for(const FVertexID& VertexID : InOutRawMesh.Vertices().GetElementIDs())
	{
		const FVector& Position = VertexPositions[VertexID];
		// Start with setting visibility to true on all vertices
		VertexVisible.Add(VertexID, true);

		// Check if this vertex is culled due to being underneath a landscape
		if (Landscapes.Num() > 0)
		{
			bool bVertexWithinLandscapeBounds = false;

			for (ALandscapeProxy* Proxy : Landscapes)
			{
				FVector Origin, Extent;
				Proxy->GetActorBounds(false, Origin, Extent);
				FBox LandscapeBox(Origin - Extent, Origin + Extent);
				bVertexWithinLandscapeBounds |= LandscapeBox.IsInsideXY(Position);
			}

			if (bVertexWithinLandscapeBounds)
			{
				const FVector Start = Position;
				FVector End = Position - (WORLD_MAX * FVector::UpVector);
				FVector OutHit;
				const bool IsAboveLandscape = IsLandscapeHit(Start, End, World, Landscapes, OutHit);

				End = Position + (WORLD_MAX * FVector::UpVector);
				const bool IsUnderneathLandscape = IsLandscapeHit(Start, End, World, Landscapes, OutHit);

				// Vertex is visible when above landscape (with actual landscape underneath) or if there is no landscape beneath or above the vertex (falls outside of landscape bounds)
				VertexVisible[VertexID] = (IsAboveLandscape && !IsUnderneathLandscape);// || (!IsAboveLandscape && !IsUnderneathLandscape);
			}
		}

		// Volume culling	
		for (AMeshMergeCullingVolume* Volume : CullVolumes)
		{
			const bool bVertexIsInsideVolume = Volume->EncompassesPoint(Position, 0.0f, nullptr);
			if (bVertexIsInsideVolume)
			{
				// Inside a culling volume so invisible
				VertexVisible[VertexID] = false;
			}
		}

		Index++;
	}


	// We now know which vertices are below the landscape
	TArray<FPolygonID> PolygonToRemove;
	for(const FPolygonID& PolygonID : InOutRawMesh.Polygons().GetElementIDs())
	{
		bool AboveLandscape = false;
		for (FMeshTriangle& Triangle : InOutRawMesh.GetPolygonTriangles(PolygonID))
		{
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				AboveLandscape |= VertexVisible[InOutRawMesh.GetVertexInstanceVertex(Triangle.GetVertexInstanceID(Corner))];
			}
		}
		if (!AboveLandscape)
		{
			PolygonToRemove.Add(PolygonID);
		}
	}

	// Delete the polygons that are not visible
	{
		TArray<FEdgeID> OrphanedEdges;
		TArray<FVertexInstanceID> OrphanedVertexInstances;
		TArray<FPolygonGroupID> OrphanedPolygonGroups;
		TArray<FVertexID> OrphanedVertices;
		for (FPolygonID PolygonID : PolygonToRemove)
		{
			InOutRawMesh.DeletePolygon(PolygonID, &OrphanedEdges, &OrphanedVertexInstances, &OrphanedPolygonGroups);
		}
		//Do not remove the polygongroup since its indexed with the mesh material array
		/*for (FPolygonGroupID PolygonGroupID : OrphanedPolygonGroups)
		{
			InOutRawMesh.DeletePolygonGroup(PolygonGroupID);
		}*/
		for (FVertexInstanceID VertexInstanceID : OrphanedVertexInstances)
		{
			InOutRawMesh.DeleteVertexInstance(VertexInstanceID, &OrphanedVertices);
		}
		for (FEdgeID EdgeID : OrphanedEdges)
		{
			InOutRawMesh.DeleteEdge(EdgeID, &OrphanedVertices);
		}
		for (FVertexID VertexID : OrphanedVertices)
		{
			InOutRawMesh.DeleteVertex(VertexID);
		}
		//Compact and Remap IDs so we have clean ID from 0 to n since we just erase some polygons
		//The render build need to have compact ID
		FElementIDRemappings OutRemappings;
		InOutRawMesh.Compact(OutRemappings);
	}
}

void FMeshMergeHelpers::PropagateSplineDeformationToRawMesh(const USplineMeshComponent* InSplineMeshComponent, FMeshDescription &OutRawMesh)
{
	TVertexAttributesRef<FVector> VertexPositions = OutRawMesh.VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);
	TVertexInstanceAttributesRef<FVector> VertexInstanceNormals = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
	TVertexInstanceAttributesRef<FVector> VertexInstanceTangents = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
	TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);

	// Apply spline deformation for each vertex's tangents
	int32 WedgeIndex = 0;
	for (const FPolygonID& PolygonID : OutRawMesh.Polygons().GetElementIDs())
	{
		for (FMeshTriangle& Triangle : OutRawMesh.GetPolygonTriangles(PolygonID))
		{
			for (int32 Corner = 0; Corner < 3; ++Corner, ++WedgeIndex)
			{
				const FVertexInstanceID VertexInstanceID = Triangle.GetVertexInstanceID(Corner);
				const FVertexID VertexID = OutRawMesh.GetVertexInstanceVertex(VertexInstanceID);
				const float& AxisValue = USplineMeshComponent::GetAxisValue(VertexPositions[VertexID], InSplineMeshComponent->ForwardAxis);
				FTransform SliceTransform = InSplineMeshComponent->CalcSliceTransform(AxisValue);
				FVector TangentY = FVector::CrossProduct(VertexInstanceNormals[VertexInstanceID], VertexInstanceTangents[VertexInstanceID]).GetSafeNormal() * VertexInstanceBinormalSigns[VertexInstanceID];
				VertexInstanceTangents[VertexInstanceID] = SliceTransform.TransformVector(VertexInstanceTangents[VertexInstanceID]);
				TangentY = SliceTransform.TransformVector(TangentY);
				VertexInstanceNormals[VertexInstanceID] = SliceTransform.TransformVector(VertexInstanceNormals[VertexInstanceID]);
				VertexInstanceBinormalSigns[VertexInstanceID] = GetBasisDeterminantSign(VertexInstanceTangents[VertexInstanceID], TangentY, VertexInstanceNormals[VertexInstanceID]);
			}
		}
	}

	// Apply spline deformation for each vertex position
	for (const FVertexID& VertexID : OutRawMesh.Vertices().GetElementIDs())
	{
		float& AxisValue = USplineMeshComponent::GetAxisValue(VertexPositions[VertexID], InSplineMeshComponent->ForwardAxis);
		FTransform SliceTransform = InSplineMeshComponent->CalcSliceTransform(AxisValue);
		AxisValue = 0.0f;
		VertexPositions[VertexID] = SliceTransform.TransformPosition(VertexPositions[VertexID]);
	}
}

void FMeshMergeHelpers::PropagateSplineDeformationToPhysicsGeometry(USplineMeshComponent* SplineMeshComponent, FKAggregateGeom& InOutPhysicsGeometry)
{
	const FVector Mask = USplineMeshComponent::GetAxisMask(SplineMeshComponent->GetForwardAxis());

	for (FKConvexElem& Elem : InOutPhysicsGeometry.ConvexElems)
	{
		for (FVector& Position : Elem.VertexData)
		{
			const float& AxisValue = USplineMeshComponent::GetAxisValue(Position, SplineMeshComponent->ForwardAxis);
			FTransform SliceTransform = SplineMeshComponent->CalcSliceTransform(AxisValue);
			Position = SliceTransform.TransformPosition(Position * Mask);
		}

		Elem.UpdateElemBox();
	}

	for (FKSphereElem& Elem : InOutPhysicsGeometry.SphereElems)
	{
		const FVector WorldSpaceCenter = Elem.GetTransform().TransformPosition(Elem.Center);
		Elem.Center = SplineMeshComponent->CalcSliceTransform(USplineMeshComponent::GetAxisValue(WorldSpaceCenter, SplineMeshComponent->ForwardAxis)).TransformPosition(Elem.Center * Mask);
	}

	for (FKSphylElem& Elem : InOutPhysicsGeometry.SphylElems)
	{
		const FVector WorldSpaceCenter = Elem.GetTransform().TransformPosition(Elem.Center);
		Elem.Center = SplineMeshComponent->CalcSliceTransform(USplineMeshComponent::GetAxisValue(WorldSpaceCenter, SplineMeshComponent->ForwardAxis)).TransformPosition(Elem.Center * Mask);
	}
}

void FMeshMergeHelpers::TransformRawMeshVertexData(const FTransform& InTransform, FMeshDescription &OutRawMesh)
{
	TVertexAttributesRef<FVector> VertexPositions = OutRawMesh.VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);
	TEdgeAttributesRef<bool> EdgeHardnesses = OutRawMesh.EdgeAttributes().GetAttributesRef<bool>(MeshAttribute::Edge::IsHard);
	TEdgeAttributesRef<float> EdgeCreaseSharpnesses = OutRawMesh.EdgeAttributes().GetAttributesRef<float>(MeshAttribute::Edge::CreaseSharpness);
	TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames = OutRawMesh.PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);
	TVertexInstanceAttributesRef<FVector> VertexInstanceNormals = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
	TVertexInstanceAttributesRef<FVector> VertexInstanceTangents = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
	TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);
	TVertexInstanceAttributesRef<FVector4> VertexInstanceColors = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color);
	TVertexInstanceAttributesRef<FVector2D> VertexInstanceUVs = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);

	for(const FVertexID& VertexID : OutRawMesh.Vertices().GetElementIDs())
	{
		VertexPositions[VertexID] = InTransform.TransformPosition(VertexPositions[VertexID]);
	}
	
	auto TransformNormal = [&](FVector& Normal)
	{
		FMatrix Matrix = InTransform.ToMatrixWithScale();
		const float DetM = Matrix.Determinant();
		FMatrix AdjointT = Matrix.TransposeAdjoint();
		AdjointT.RemoveScaling();

		Normal = AdjointT.TransformVector(Normal);
		if (DetM < 0.f)
		{
			Normal *= -1.0f;
		}
	};	

	for(const FVertexInstanceID& VertexInstanceID : OutRawMesh.VertexInstances().GetElementIDs())
	{
		FVector TangentY = FVector::CrossProduct(VertexInstanceNormals[VertexInstanceID], VertexInstanceTangents[VertexInstanceID]).GetSafeNormal() * VertexInstanceBinormalSigns[VertexInstanceID];
		TransformNormal(VertexInstanceTangents[VertexInstanceID]);
		TransformNormal(TangentY);
		TransformNormal(VertexInstanceNormals[VertexInstanceID]);
		VertexInstanceBinormalSigns[VertexInstanceID] = GetBasisDeterminantSign(VertexInstanceTangents[VertexInstanceID], TangentY, VertexInstanceNormals[VertexInstanceID]);
	}

	const bool bIsMirrored = InTransform.GetDeterminant() < 0.f;
	if (bIsMirrored)
	{
		//Reverse the vertexinstance
		OutRawMesh.ReverseAllPolygonFacing();
	}
}

void FMeshMergeHelpers::RetrieveCullingLandscapeAndVolumes(UWorld* InWorld, const FBoxSphereBounds& EstimatedMeshProxyBounds, const TEnumAsByte<ELandscapeCullingPrecision::Type> PrecisionType, TArray<FMeshDescription*>& CullingRawMeshes)
{
	// Extract landscape proxies and cull volumes from the world
	TArray<ALandscapeProxy*> LandscapeActors;
	TArray<AMeshMergeCullingVolume*> CullVolumes;

	uint32 MaxLandscapeExportLOD = 0;
	if (InWorld->IsValidLowLevel())
	{
		for (FConstLevelIterator Iterator = InWorld->GetLevelIterator(); Iterator; ++Iterator)
		{
			for (AActor* Actor : (*Iterator)->Actors)
			{
				if (Actor)
				{
					ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(Actor);
					if (LandscapeProxy && LandscapeProxy->bUseLandscapeForCullingInvisibleHLODVertices)
					{
						// Retrieve highest landscape LOD level possible
						MaxLandscapeExportLOD = FMath::Max(MaxLandscapeExportLOD, FMath::CeilLogTwo(LandscapeProxy->SubsectionSizeQuads + 1) - 1);
						LandscapeActors.Add(LandscapeProxy);
					}
					// Check for culling volumes
					AMeshMergeCullingVolume* Volume = Cast<AMeshMergeCullingVolume>(Actor);
					if (Volume)
					{
						// If the mesh's bounds intersect with the volume there is a possibility of culling
						const bool bIntersecting = Volume->EncompassesPoint(EstimatedMeshProxyBounds.Origin, EstimatedMeshProxyBounds.SphereRadius, nullptr);
						if (bIntersecting)
						{
							CullVolumes.Add(Volume);
						}
					}
				}
			}
		}
	}

	// Setting determines the precision at which we should export the landscape for culling (highest, half or lowest)
	const uint32 LandscapeExportLOD = ((float)MaxLandscapeExportLOD * (0.5f * (float)PrecisionType));
	for (ALandscapeProxy* Landscape : LandscapeActors)
	{
		// Export the landscape to raw mesh format
		FMeshDescription* MeshDescription = new FMeshDescription();
		UStaticMesh::RegisterMeshAttributes(*MeshDescription);
		FBoxSphereBounds LandscapeBounds = EstimatedMeshProxyBounds;
		Landscape->ExportToRawMesh(LandscapeExportLOD, *MeshDescription, LandscapeBounds);
		if (MeshDescription->Vertices().Num())
		{
			CullingRawMeshes.Add(MeshDescription);
		}
	}

	// Also add volume mesh data as culling meshes
	for (AMeshMergeCullingVolume* Volume : CullVolumes)
	{
		// Export the landscape to raw mesh format
		FMeshDescription* VolumeMesh = new FMeshDescription();
		UStaticMesh::RegisterMeshAttributes(*VolumeMesh);

		TArray<FStaticMaterial>	VolumeMaterials;
		GetBrushMesh(Volume, Volume->Brush, *VolumeMesh, VolumeMaterials);

		// Offset vertices to correct world position;
		FVector VolumeLocation = Volume->GetActorLocation();
		TVertexAttributesRef<FVector> VertexPositions = VolumeMesh->VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);
		for(const FVertexID& VertexID : VolumeMesh->Vertices().GetElementIDs())
		{
			VertexPositions[VertexID] += VolumeLocation;
		}

		CullingRawMeshes.Add(VolumeMesh);
	}
}

void FMeshMergeHelpers::TransformPhysicsGeometry(const FTransform& InTransform, const bool bBakeConvexTransform, struct FKAggregateGeom& AggGeom)
{
	FTransform NoScaleInTransform = InTransform;
	NoScaleInTransform.SetScale3D(FVector(1, 1, 1));

	// Pre-scale all non-convex geometry		
	const FVector Scale3D = InTransform.GetScale3D();
	if (!Scale3D.Equals(FVector(1.f)))
	{
		const float MinPrimSize = KINDA_SMALL_NUMBER;

		for (FKSphereElem& Elem : AggGeom.SphereElems)
		{
			Elem = Elem.GetFinalScaled(Scale3D, FTransform::Identity);
		}

		for (FKBoxElem& Elem : AggGeom.BoxElems)
		{
			Elem = Elem.GetFinalScaled(Scale3D, FTransform::Identity);
		}

		for (FKSphylElem& Elem : AggGeom.SphylElems)
		{
			Elem = Elem.GetFinalScaled(Scale3D, FTransform::Identity);
		}
	}
	
	// Multiply out merge transform (excluding scale) with original transforms for non-convex geometry
	for (FKSphereElem& Elem : AggGeom.SphereElems)
	{
		FTransform ElemTM = Elem.GetTransform();
		Elem.SetTransform(ElemTM*NoScaleInTransform);
	}

	for (FKBoxElem& Elem : AggGeom.BoxElems)
	{
		FTransform ElemTM = Elem.GetTransform();
		Elem.SetTransform(ElemTM*NoScaleInTransform);
	}

	for (FKSphylElem& Elem : AggGeom.SphylElems)
	{
		FTransform ElemTM = Elem.GetTransform();
		Elem.SetTransform(ElemTM*NoScaleInTransform);
	}

	for (FKConvexElem& Elem : AggGeom.ConvexElems)
	{
		FTransform ElemTM = Elem.GetTransform();
        if (bBakeConvexTransform)
        {
            for (FVector& Position : Elem.VertexData)
            {
                Position = ElemTM.TransformPosition(Position);
            }
		    Elem.SetTransform(InTransform);
        }
        else
        {
            Elem.SetTransform(ElemTM*InTransform);
        }
	}
}

void FMeshMergeHelpers::ExtractPhysicsGeometry(UBodySetup* InBodySetup, const FTransform& ComponentToWorld, const bool bBakeConvexTransform, struct FKAggregateGeom& OutAggGeom)
{
	if (InBodySetup == nullptr)
	{
		return;
	}


	OutAggGeom = InBodySetup->AggGeom;

	// Convert boxes to convex, so they can be sheared 
	for (int32 BoxIdx = 0; BoxIdx < OutAggGeom.BoxElems.Num(); BoxIdx++)
	{
		FKConvexElem* NewConvexColl = new(OutAggGeom.ConvexElems) FKConvexElem();
		NewConvexColl->ConvexFromBoxElem(OutAggGeom.BoxElems[BoxIdx]);
	}
	OutAggGeom.BoxElems.Empty();

	// we are not owner of this stuff
	OutAggGeom.RenderInfo = nullptr;
	for (FKConvexElem& Elem : OutAggGeom.ConvexElems)
	{
		Elem.SetConvexMesh(nullptr);
		Elem.SetMirroredConvexMesh(nullptr);
	}

	// Transform geometry to world space
	TransformPhysicsGeometry(ComponentToWorld, bBakeConvexTransform, OutAggGeom);
}

FVector2D FMeshMergeHelpers::GetValidUV(const FVector2D& UV)
{
	FVector2D NewUV = UV;
	// first make sure they're positive
	if (UV.X < 0.0f)
	{
		NewUV.X = UV.X + FMath::CeilToInt(FMath::Abs(UV.X));
	}

	if (UV.Y < 0.0f)
	{
		NewUV.Y = UV.Y + FMath::CeilToInt(FMath::Abs(UV.Y));
	}

	// now make sure they're within [0, 1]
	if (UV.X > 1.0f)
	{
		NewUV.X = FMath::Fmod(NewUV.X, 1.0f);
	}

	if (UV.Y > 1.0f)
	{
		NewUV.Y = FMath::Fmod(NewUV.Y, 1.0f);
	}

	return NewUV;
}

void FMeshMergeHelpers::CalculateTextureCoordinateBoundsForRawMesh(const FMeshDescription& InRawMesh, TArray<FBox2D>& OutBounds)
{
	TVertexInstanceAttributesConstRef<FVector2D> VertexInstanceUVs = InRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);
	OutBounds.Empty();
	for (const FPolygonID& PolygonID : InRawMesh.Polygons().GetElementIDs())
	{
		int32 MaterialIndex = InRawMesh.GetPolygonPolygonGroup(PolygonID).GetValue();
		if (OutBounds.Num() <= MaterialIndex)
			OutBounds.SetNumZeroed(MaterialIndex + 1);
		{
			TArray<FVertexInstanceID> PolygonVertexInstances = InRawMesh.GetPolygonPerimeterVertexInstances(PolygonID);
			for (const FVertexInstanceID& VertexInstanceID : PolygonVertexInstances)
			{
				for (int32 UVIndex = 0; UVIndex < VertexInstanceUVs.GetNumIndices(); ++UVIndex)
				{
					OutBounds[MaterialIndex] += VertexInstanceUVs.Get(VertexInstanceID, UVIndex);
				}
			}
		}
	}
}

bool FMeshMergeHelpers::PropagatePaintedColorsToRawMesh(const UStaticMeshComponent* StaticMeshComponent, int32 LODIndex, FMeshDescription& RawMesh)
{
	UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();

	if (StaticMesh->SourceModels.IsValidIndex(LODIndex) &&
		StaticMeshComponent->LODData.IsValidIndex(LODIndex) &&
		StaticMeshComponent->LODData[LODIndex].OverrideVertexColors != nullptr)
	{
		FColorVertexBuffer& ColorVertexBuffer = *StaticMeshComponent->LODData[LODIndex].OverrideVertexColors;
		FStaticMeshLODResources& RenderModel = StaticMesh->RenderData->LODResources[LODIndex];

		if (ColorVertexBuffer.GetNumVertices() == RenderModel.GetNumVertices())
		{	
			const int32 NumWedges = RawMesh.VertexInstances().Num();
			const int32 NumRenderWedges = RenderModel.IndexBuffer.GetNumIndices();
			const bool bUseRenderWedges = NumWedges == NumRenderWedges;

			TVertexInstanceAttributesRef<FVector4> VertexInstanceColors = RawMesh.VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color);

			if (bUseRenderWedges)
			{
				//Create a map index
				TMap<int32, FVertexInstanceID> IndexToVertexInstanceID;
				IndexToVertexInstanceID.Reserve(NumWedges);
				int32 CurrentWedgeIndex = 0;
				for (const FPolygonID& PolygonID : RawMesh.Polygons().GetElementIDs())
				{
					const TArray<FMeshTriangle>& Triangles = RawMesh.GetPolygonTriangles(PolygonID);
					for (const FMeshTriangle& Triangle : Triangles)
					{
						for (int32 Corner = 0; Corner < 3; ++Corner, ++CurrentWedgeIndex)
						{
							IndexToVertexInstanceID.Add(CurrentWedgeIndex, Triangle.GetVertexInstanceID(Corner));
						}
					}
				}

				const FIndexArrayView ArrayView = RenderModel.IndexBuffer.GetArrayView();
				for (int32 WedgeIndex = 0; WedgeIndex < NumRenderWedges; WedgeIndex++)
				{
					const int32 Index = ArrayView[WedgeIndex];
					FColor WedgeColor = FColor::White;
					if (Index != INDEX_NONE)
					{
						WedgeColor = ColorVertexBuffer.VertexColor(Index);
					}
					VertexInstanceColors[IndexToVertexInstanceID[WedgeIndex]] = FLinearColor(WedgeColor);
				}

				return true;				
			}
			// No wedge map (this can happen when we poly reduce the LOD for example)
			// Use index buffer directly. Not sure this will happen with FMeshDescription
			else
			{
				if (RawMesh.Vertices().Num() == ColorVertexBuffer.GetNumVertices())
				{
					//Create a map index
					TMap<FVertexID, int32> VertexIDToVertexIndex;
					VertexIDToVertexIndex.Reserve(RawMesh.Vertices().Num());
					int32 CurrentVertexIndex = 0;
					for (const FVertexID& VertexID : RawMesh.Vertices().GetElementIDs())
					{
						VertexIDToVertexIndex.Add(VertexID, CurrentVertexIndex++);
					}

					for (const FVertexID& VertexID : RawMesh.Vertices().GetElementIDs())
					{
						FColor WedgeColor = FColor::White;
						uint32 VertIndex = VertexIDToVertexIndex[VertexID];

						if (VertIndex < ColorVertexBuffer.GetNumVertices())
						{
							WedgeColor = ColorVertexBuffer.VertexColor(VertIndex);
						}
						const TArray<FVertexInstanceID>& VertexInstances = RawMesh.GetVertexVertexInstances(VertexID);
						for (const FVertexInstanceID& VertexInstanceID : VertexInstances)
						{
							VertexInstanceColors[VertexInstanceID] = FLinearColor(WedgeColor);
						}
					}
					return true;
				}
			}
		}
	}

	return false;
}

bool FMeshMergeHelpers::IsLandscapeHit(const FVector& RayOrigin, const FVector& RayEndPoint, const UWorld* World, const TArray<ALandscapeProxy*>& LandscapeProxies, FVector& OutHitLocation)
{
	TArray<FHitResult> Results;
	// Each landscape component has 2 collision shapes, 1 of them is specific to landscape editor
	// Trace only ECC_Visibility channel, so we do hit only Editor specific shape
	World->LineTraceMultiByObjectType(Results, RayOrigin, RayEndPoint, FCollisionObjectQueryParams(ECollisionChannel::ECC_WorldStatic), FCollisionQueryParams(SCENE_QUERY_STAT(LandscapeTrace), true));

	bool bHitLandscape = false;

	for (const FHitResult& HitResult : Results)
	{
		ULandscapeHeightfieldCollisionComponent* CollisionComponent = Cast<ULandscapeHeightfieldCollisionComponent>(HitResult.Component.Get());
		if (CollisionComponent)
		{
			ALandscapeProxy* HitLandscape = CollisionComponent->GetLandscapeProxy();
			if (HitLandscape && LandscapeProxies.Contains(HitLandscape))
			{
				// Could write a correct clipping algorithm, that clips the triangle to hit location
				OutHitLocation = HitLandscape->LandscapeActorToWorld().InverseTransformPosition(HitResult.Location);
				// Above landscape so visible
				bHitLandscape = true;
			}
		}
	}

	return bHitLandscape;
}

void FMeshMergeHelpers::AppendRawMesh(FMeshDescription& InTarget, const FMeshDescription& InSource)
{
	TVertexAttributesConstRef<FVector> SourceVertexPositions = InSource.VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);
	TEdgeAttributesConstRef<bool> SourceEdgeHardnesses = InSource.EdgeAttributes().GetAttributesRef<bool>(MeshAttribute::Edge::IsHard);
	TEdgeAttributesConstRef<float> SourceEdgeCreaseSharpnesses = InSource.EdgeAttributes().GetAttributesRef<float>(MeshAttribute::Edge::CreaseSharpness);
	TPolygonGroupAttributesConstRef<FName> SourcePolygonGroupImportedMaterialSlotNames = InSource.PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);
	TVertexInstanceAttributesConstRef<FVector> SourceVertexInstanceNormals = InSource.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
	TVertexInstanceAttributesConstRef<FVector> SourceVertexInstanceTangents = InSource.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
	TVertexInstanceAttributesConstRef<float> SourceVertexInstanceBinormalSigns = InSource.VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);
	TVertexInstanceAttributesConstRef<FVector4> SourceVertexInstanceColors = InSource.VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color);
	TVertexInstanceAttributesConstRef<FVector2D> SourceVertexInstanceUVs = InSource.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);

	TVertexAttributesRef<FVector> TargetVertexPositions = InTarget.VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);
	TEdgeAttributesRef<bool> TargetEdgeHardnesses = InTarget.EdgeAttributes().GetAttributesRef<bool>(MeshAttribute::Edge::IsHard);
	TEdgeAttributesRef<float> TargetEdgeCreaseSharpnesses = InTarget.EdgeAttributes().GetAttributesRef<float>(MeshAttribute::Edge::CreaseSharpness);
	TPolygonGroupAttributesRef<FName> TargetPolygonGroupImportedMaterialSlotNames = InTarget.PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);
	TVertexInstanceAttributesRef<FVector> TargetVertexInstanceNormals = InTarget.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
	TVertexInstanceAttributesRef<FVector> TargetVertexInstanceTangents = InTarget.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
	TVertexInstanceAttributesRef<float> TargetVertexInstanceBinormalSigns = InTarget.VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);
	TVertexInstanceAttributesRef<FVector4> TargetVertexInstanceColors = InTarget.VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color);
	TVertexInstanceAttributesRef<FVector2D> TargetVertexInstanceUVs = InTarget.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);

	InTarget.ReserveNewVertices(InSource.Vertices().Num());
	InTarget.ReserveNewVertexInstances(InSource.VertexInstances().Num());
	InTarget.ReserveNewEdges(InSource.Edges().Num());
	InTarget.ReserveNewPolygons(InSource.Vertices().Num());

	//Append PolygonGroup
	for (const FPolygonGroupID& SourcePolygonGroupID : InSource.PolygonGroups().GetElementIDs())
	{
		if (!InTarget.IsPolygonGroupValid(SourcePolygonGroupID))
		{
			InTarget.CreatePolygonGroupWithID(SourcePolygonGroupID);
			const FName BaseName = SourcePolygonGroupImportedMaterialSlotNames[SourcePolygonGroupID];
			FName CurrentTestName = BaseName;
			int32 UniqueID = 1;
			bool bUnique = true;
			do 
			{
				for (const FPolygonGroupID PolygonGroupID : InTarget.PolygonGroups().GetElementIDs())
				{
					if (TargetPolygonGroupImportedMaterialSlotNames[PolygonGroupID] == CurrentTestName)
					{
						CurrentTestName = FName(*(BaseName.ToString() + FString::FromInt(UniqueID++)));
						bUnique = false;
					}
				}
			} while (!bUnique);
			TargetPolygonGroupImportedMaterialSlotNames[SourcePolygonGroupID] = CurrentTestName;
		}
	}

	//Append the Vertexs
	TMap<FVertexID, FVertexID> SourceToTargetVertexID;
	SourceToTargetVertexID.Reserve(InSource.Vertices().Num());
	for (const FVertexID& SourceVertexID : InSource.Vertices().GetElementIDs())
	{
		const FVertexID TargetVertexID = InTarget.CreateVertex();
		SourceToTargetVertexID.Add(SourceVertexID, TargetVertexID);
		TargetVertexPositions[TargetVertexID] = SourceVertexPositions[SourceVertexID];
	}

	//Append VertexInstances
	if (SourceVertexInstanceUVs.GetNumIndices() > TargetVertexInstanceUVs.GetNumIndices())
	{
		TargetVertexInstanceUVs.SetNumIndices(SourceVertexInstanceUVs.GetNumIndices());
	}
	TMap<FVertexInstanceID, FVertexInstanceID> SourceToTargetVertexInstanceID;
	SourceToTargetVertexInstanceID.Reserve(InSource.VertexInstances().Num());
	for (const FVertexInstanceID& SourceVertexInstanceID : InSource.VertexInstances().GetElementIDs())
	{
		const FVertexID SourceVertexID = InSource.GetVertexInstanceVertex(SourceVertexInstanceID);
		const FVertexInstanceID TargetVertexInstanceID = InTarget.CreateVertexInstance(SourceToTargetVertexID[SourceVertexID]);
		TargetVertexInstanceTangents[TargetVertexInstanceID] = SourceVertexInstanceTangents[SourceVertexInstanceID];
		TargetVertexInstanceBinormalSigns[TargetVertexInstanceID] = SourceVertexInstanceBinormalSigns[SourceVertexInstanceID];
		TargetVertexInstanceNormals[TargetVertexInstanceID] = SourceVertexInstanceNormals[SourceVertexInstanceID];
		TargetVertexInstanceColors[TargetVertexInstanceID] = SourceVertexInstanceColors[SourceVertexInstanceID];
		for (int32 UVIndex = 0; UVIndex < TargetVertexInstanceUVs.GetNumIndices(); ++UVIndex)
		{
			FVector2D SourceUV = SourceVertexInstanceUVs.GetNumIndices() > UVIndex ? SourceVertexInstanceUVs.Get(SourceVertexInstanceID, UVIndex) : FVector2D(0.0f, 0.0f);
			TargetVertexInstanceUVs.Set(TargetVertexInstanceID, UVIndex, SourceUV);
		}
		SourceToTargetVertexInstanceID.Add(SourceVertexInstanceID, TargetVertexInstanceID);
	}

	//Append Edges
	TMap<FEdgeID, FEdgeID> SourceToTargetEdgeID;
	SourceToTargetEdgeID.Reserve(InSource.Edges().Num());
	for (const FEdgeID& SourceEdgeID : InSource.Edges().GetElementIDs())
	{
		const FMeshEdge& SourceEdge = InSource.GetEdge(SourceEdgeID);
		const FEdgeID TargetEdgeID = InTarget.CreateEdge(SourceToTargetVertexID[SourceEdge.VertexIDs[0]], SourceToTargetVertexID[SourceEdge.VertexIDs[1]]);
		TargetEdgeHardnesses[TargetEdgeID] = SourceEdgeHardnesses[SourceEdgeID];
		TargetEdgeCreaseSharpnesses[TargetEdgeID] = SourceEdgeCreaseSharpnesses[SourceEdgeID];
		SourceToTargetEdgeID.Add(SourceEdgeID, TargetEdgeID);
	}

	auto CreateContour = [&InSource, &SourceToTargetVertexInstanceID, &SourceToTargetEdgeID](const TArray<FVertexInstanceID>& SourceVertexInstanceIDs, TArray<FVertexInstanceID>& DestVertexInstanceIDs)
	{
		const int32 ContourCount = SourceVertexInstanceIDs.Num();
		for (int32 ContourIndex = 0; ContourIndex < ContourCount; ++ContourIndex)
		{
			FVertexInstanceID SourceVertexInstanceID = SourceVertexInstanceIDs[ContourIndex];
			DestVertexInstanceIDs.Add(SourceToTargetVertexInstanceID[SourceVertexInstanceID]);
		}
	};

	//Append Polygons
	for (const FPolygonID& SourcePolygonID : InSource.Polygons().GetElementIDs())
	{
		const FMeshPolygon& SourcePolygon = InSource.GetPolygon(SourcePolygonID);
		const TArray<FVertexInstanceID>& SourceVertexInstanceIDs = InSource.GetPolygonPerimeterVertexInstances(SourcePolygonID);


		TArray<FVertexInstanceID> ContourVertexInstances;
		CreateContour(SourceVertexInstanceIDs, ContourVertexInstances);

		// Insert a polygon into the mesh
		const FPolygonID TargetPolygonID = InTarget.CreatePolygon(SourcePolygon.PolygonGroupID, ContourVertexInstances);
		//Triangulate the polygon
		FMeshPolygon& Polygon = InTarget.GetPolygon(TargetPolygonID);
		InTarget.ComputePolygonTriangulation(TargetPolygonID, Polygon.Triangles);
	}
}


void FMeshMergeHelpers::MergeImpostersToRawMesh(TArray<const UStaticMeshComponent*> ImposterComponents, FMeshDescription& InRawMesh, const FVector& InPivot, int32 InBaseMaterialIndex, TArray<UMaterialInterface*>& OutImposterMaterials)
{
	// TODO decide whether we want this to be user specified or derived from the RawMesh
	/*const int32 UVOneIndex = [RawMesh, Data]() -> int32
	{
		int32 ChannelIndex = 0;
		for (; ChannelIndex < MAX_MESH_TEXTURE_COORDS; ++ChannelIndex)
		{
			if (RawMesh.WedgeTexCoords[ChannelIndex].Num() == 0)
			{
				break;
			}
		}

		int32 MaxUVChannel = ChannelIndex;
		for (const UStaticMeshComponent* Component : ImposterComponents)
		{
			MaxUVChannel = FMath::Max(MaxUVChannel, Component->GetStaticMesh()->RenderData->LODResources[Component->GetStaticMesh()->GetNumLODs() - 1].GetNumTexCoords());
		}

		return MaxUVChannel;
	}();*/

	const int32 UVOneIndex = 2; // if this is changed back to being dynamic, renable the if statement below

	// Ensure there are enough UV channels available to store the imposter data
	//if (UVOneIndex != INDEX_NONE && UVOneIndex < (MAX_MESH_TEXTURE_COORDS - 2))
	{
		TMap<UMaterialInterface*, FPolygonGroupID> ImposterMaterialToPolygonGroupID;
		for (const UStaticMeshComponent* Component : ImposterComponents)
		{
			// Retrieve imposter LOD mesh and material			
			const int32 LODIndex = Component->GetStaticMesh()->GetNumLODs() - 1;

			// Retrieve mesh data in FMeshDescription form
			FMeshDescription ImposterMesh;
			UStaticMesh::RegisterMeshAttributes(ImposterMesh);
			FMeshMergeHelpers::RetrieveMesh(Component, LODIndex, ImposterMesh, false);

			// Retrieve the sections, we're expect 1 for imposter meshes
			TArray<FSectionInfo> Sections;
			FMeshMergeHelpers::ExtractSections(Component, LODIndex, Sections);

			TArray<int32> SectionImposterUniqueMaterialIndex;
			for (FSectionInfo& Info : Sections)
			{
				SectionImposterUniqueMaterialIndex.Add(OutImposterMaterials.AddUnique(Info.Material));
			}

			// Imposter magic, we're storing the actor world position and X scale spread across two UV channels
			const int32 UVTwoIndex = UVOneIndex + 1;
			TVertexInstanceAttributesRef<FVector2D> VertexInstanceUVs = ImposterMesh.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);
			VertexInstanceUVs.SetNumIndices(UVTwoIndex + 1);
			const int32 NumIndices = ImposterMesh.VertexInstances().Num();
			const FTransform& ActorToWorld = Component->GetOwner()->GetActorTransform();
			const FVector ActorPosition = ActorToWorld.TransformPosition(FVector::ZeroVector) - InPivot;
			for(const FVertexInstanceID& VertexInstanceID : ImposterMesh.VertexInstances().GetElementIDs())
			{
				FVector2D UVOne;
				FVector2D UVTwo;
					
				UVOne.X = ActorPosition.X;
				UVOne.Y = ActorPosition.Y;
				VertexInstanceUVs.Set(VertexInstanceID, UVOneIndex, UVOne);

				UVTwo.X = ActorPosition.Z;
				UVTwo.Y = FMath::Abs(ActorToWorld.GetScale3D().X);
				VertexInstanceUVs.Set(VertexInstanceID, UVTwoIndex, UVTwo);
			}

			TPolygonGroupAttributesRef<FName> SourcePolygonGroupImportedMaterialSlotNames = ImposterMesh.PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);
			TPolygonGroupAttributesRef<FName> TargetPolygonGroupImportedMaterialSlotNames = InRawMesh.PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);

			//Add the missing polygon group ID to the target(InRawMesh)
			//Remap the source mesh(ImposterMesh) polygongroup to fit with the target polygon groups
			TMap<FPolygonGroupID, FPolygonGroupID> RemapSourcePolygonGroup;
			RemapSourcePolygonGroup.Reserve(ImposterMesh.PolygonGroups().Num());
			int32 SectionIndex = 0;
			for (const FPolygonGroupID& SourcePolygonGroupID : ImposterMesh.PolygonGroups().GetElementIDs())
			{
				UMaterialInterface* MaterialUseBySection = OutImposterMaterials[SectionImposterUniqueMaterialIndex[SectionIndex++]];
				FPolygonGroupID* ExistTargetPolygonGroupID = ImposterMaterialToPolygonGroupID.Find(MaterialUseBySection);
				FPolygonGroupID MatchTargetPolygonGroupID = ExistTargetPolygonGroupID == nullptr ? FPolygonGroupID::Invalid : *ExistTargetPolygonGroupID;
				if (MatchTargetPolygonGroupID == FPolygonGroupID::Invalid)
				{
					MatchTargetPolygonGroupID = InRawMesh.CreatePolygonGroup();
					//use the material name to fill the imported material name. Material name will be unique
					TargetPolygonGroupImportedMaterialSlotNames[MatchTargetPolygonGroupID] = MaterialUseBySection->GetFName();
					ImposterMaterialToPolygonGroupID.Add(MaterialUseBySection, MatchTargetPolygonGroupID);
				}
				RemapSourcePolygonGroup.Add(SourcePolygonGroupID, MatchTargetPolygonGroupID);
			}
			FMeshDescriptionOperations::RemapPolygonGroups(ImposterMesh, RemapSourcePolygonGroup);

			FMeshMergeHelpers::AppendRawMesh(InRawMesh, ImposterMesh);
		}
	}
}
