// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StaticMeshBuilder.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "PhysicsEngine/BodySetup.h"
#include "MeshDescription.h"

FStaticMeshBuilder::FStaticMeshBuilder()
{

}

int32 FStaticMeshBuilder::GetPolygonGroupTriangles(const UMeshDescription *MeshDescription, TArray<FMeshTriangle>& OutTriangles, const FPolygonGroupID& PolygonGroupID)
{
	int32 TriangleCount = 0;
	for (const FPolygonID& PolygonID : MeshDescription->Polygons().GetElementIDs())
	{
		FMeshPolygon MeshPolygon = MeshDescription->GetPolygon(PolygonID);
		if (MeshPolygon.PolygonGroupID == PolygonGroupID)
		{
			for (const FMeshTriangle& MeshTriangle : MeshPolygon.Triangles)
			{
				OutTriangles.Add(MeshTriangle);
				TriangleCount++;
			}
		}
	}
	return TriangleCount;
}

bool FStaticMeshBuilder::Build(UStaticMesh* StaticMesh)
{
	int32 MeshDescriptionCount = StaticMesh->GetMeshDescriptionCount();
	if (MeshDescriptionCount <= 0)
	{
		//TODO: Warn the user that there is no mesh description data
		return false;
	}

	OnBuildRenderMeshStart(StaticMesh, false);

	FStaticMeshRenderData& StaticMeshRenderData = *StaticMesh->RenderData;

	for (int32 LodIndex = 0; LodIndex < MeshDescriptionCount; ++LodIndex)
	{
		const UMeshDescription* MeshDescription = StaticMesh->GetMeshDescription(LodIndex);
		check(MeshDescription != nullptr);

		FStaticMeshLODResources& StaticMeshLOD = StaticMeshRenderData.LODResources[LodIndex];

		// Build new vertex buffers
		static TArray< FStaticMeshBuildVertex > StaticMeshBuildVertices;
		StaticMeshBuildVertices.Reset();

		static TArray< uint32 > IndexBuffer;
		IndexBuffer.Reset();

		const FPolygonGroupArray& PolygonGroups = MeshDescription->PolygonGroups();

		StaticMeshLOD.Sections.Empty(PolygonGroups.Num());

		bool bHasColor = false;

		const FVertexArray& Vertices = MeshDescription->Vertices();
		const FVertexInstanceArray& VertexInstances = MeshDescription->VertexInstances();

		// set up vertex buffer elements
		StaticMeshBuildVertices.SetNum(VertexInstances.GetArraySize());

		for (const FVertexInstanceID VertexInstanceID : VertexInstances.GetElementIDs())
		{
			const FMeshVertexInstance& VertexInstance = VertexInstances[VertexInstanceID];

			if (VertexInstance.Color != FColor::White)
			{
				bHasColor = true;
			}

			FStaticMeshBuildVertex& StaticMeshVertex = StaticMeshBuildVertices[VertexInstanceID.GetValue()];

			StaticMeshVertex.Position = Vertices[VertexInstance.VertexID].VertexPosition;
			StaticMeshVertex.TangentX = VertexInstance.Tangent;
			StaticMeshVertex.TangentY = FVector::CrossProduct(VertexInstance.Normal, VertexInstance.Tangent).GetSafeNormal() * VertexInstance.BinormalSign;
			StaticMeshVertex.TangentZ = VertexInstance.Normal;
			StaticMeshVertex.Color = VertexInstance.Color.ToFColor(true);
			for (int32 UVIndex = 0; UVIndex < VertexInstance.VertexUVs.Num(); ++UVIndex)
			{
				StaticMeshVertex.UVs[UVIndex] = VertexInstance.VertexUVs[UVIndex];
			}
		}

		// Set up index buffer
		for (const FPolygonGroupID& PolygonGroupID : PolygonGroups.GetElementIDs())
		{
			TArray<FMeshTriangle> Triangles;
			int32 SectionTriangleCount = GetPolygonGroupTriangles(MeshDescription, Triangles, PolygonGroupID);

			const FMeshPolygonGroup& PolygonGroup = PolygonGroups[PolygonGroupID];

			// Create new rendering section
			StaticMeshLOD.Sections.Add(FStaticMeshSection());
			FStaticMeshSection& StaticMeshSection = StaticMeshLOD.Sections.Last();

			StaticMeshSection.FirstIndex = IndexBuffer.Num();
			StaticMeshSection.NumTriangles = SectionTriangleCount;

			const int32 MaterialIndex = StaticMesh->GetMaterialIndex(PolygonGroup.ImportedMaterialSlotName);
			check(MaterialIndex != INDEX_NONE);
			check(StaticMesh->StaticMaterials[MaterialIndex].MaterialInterface->GetPathName() == PolygonGroup.MaterialAsset.ToString());
			StaticMeshSection.MaterialIndex = MaterialIndex;
			StaticMeshSection.bEnableCollision = PolygonGroup.bEnableCollision;
			StaticMeshSection.bCastShadow = PolygonGroup.bCastShadow;

			if (Triangles.Num() > 0)
			{
				IndexBuffer.Reserve(IndexBuffer.Num() + Triangles.Num() * 3);
				uint32 MinIndex = TNumericLimits< uint32 >::Max();
				uint32 MaxIndex = TNumericLimits< uint32 >::Min();
				for (int32 TriangleIndex = 0; TriangleIndex < Triangles.Num(); ++TriangleIndex)
				{
					const FMeshTriangle& Triangle = Triangles[TriangleIndex];
					for (int32 TriVert = 0; TriVert < 3; ++TriVert)
					{
						const uint32 RenderingVertexIndex = Triangle.GetVertexInstanceID(TriVert).GetValue();
						IndexBuffer.Add(RenderingVertexIndex);
						MinIndex = FMath::Min(MinIndex, RenderingVertexIndex);
						MaxIndex = FMath::Max(MaxIndex, RenderingVertexIndex);
					}
				}
				StaticMeshSection.MinVertexIndex = MinIndex;
				StaticMeshSection.MaxVertexIndex = MaxIndex;
			}
			else
			{
				// No triangles in this section
				StaticMeshSection.MinVertexIndex = 0;
				StaticMeshSection.MaxVertexIndex = 0;
			}
		}

		// Figure out which index buffer stride we need
		bool bNeeds32BitIndices = false;
		for (const FStaticMeshSection& StaticMeshSection : StaticMeshLOD.Sections)
		{
			if (StaticMeshSection.MaxVertexIndex > TNumericLimits<uint16>::Max())
			{
				bNeeds32BitIndices = true;
			}
		}
		const EIndexBufferStride::Type IndexBufferStride = bNeeds32BitIndices ? EIndexBufferStride::Force32Bit : EIndexBufferStride::Force16Bit;

		StaticMeshLOD.PositionVertexBuffer.Init(StaticMeshBuildVertices);
		StaticMeshLOD.VertexBuffer.Init(StaticMeshBuildVertices, 1); //TODO: use the real texture coordinnate count

		if (bHasColor)
		{
			StaticMeshLOD.ColorVertexBuffer.Init(StaticMeshBuildVertices);
		}
		else
		{
			StaticMeshLOD.ColorVertexBuffer.InitFromSingleColor(FColor::White, StaticMeshBuildVertices.Num());
		}

		StaticMeshLOD.IndexBuffer.SetIndices(IndexBuffer, IndexBufferStride);

		// @todo mesheditor: support the other index buffer types
		StaticMeshLOD.ReversedIndexBuffer.SetIndices(TArray< uint32 >(), IndexBufferStride);
		StaticMeshLOD.DepthOnlyIndexBuffer.SetIndices(TArray< uint32 >(), IndexBufferStride);
		StaticMeshLOD.ReversedDepthOnlyIndexBuffer.SetIndices(TArray< uint32 >(), IndexBufferStride);
		StaticMeshLOD.WireframeIndexBuffer.SetIndices(TArray< uint32 >(), IndexBufferStride);
		StaticMeshLOD.AdjacencyIndexBuffer.SetIndices(TArray< uint32 >(), IndexBufferStride);

		StaticMeshLOD.bHasAdjacencyInfo = false;
		StaticMeshLOD.bHasDepthOnlyIndices = false;
		StaticMeshLOD.bHasReversedIndices = false;
		StaticMeshLOD.bHasReversedDepthOnlyIndices = false;
		StaticMeshLOD.DepthOnlyNumTriangles = 0;
	} //End of LOD for loop

	OnBuildRenderMeshFinish(StaticMesh, true);
	return true;
}


void FStaticMeshBuilder::OnBuildRenderMeshStart(UStaticMesh* StaticMesh, const bool bInvalidateLighting)
{
	// We may already have a lock on the rendering resources, if it wasn't released the last time we called EndModification()
	// on the mesh.  This is only the case when rolling back preview changes for a mesh, because we're guaranteed to apply another
	// modification to the same mesh in the very same frame.  So we can avoid having to update the GPU resources twice in one frame.
	if (!RecreateRenderStateContext.IsValid())
	{
		// We're changing the mesh itself, so ALL static mesh components in the scene will need
		// to be unregistered for this (and reregistered afterwards.)
		const bool bRefreshBounds = true;
		RecreateRenderStateContext = MakeShareable(new FStaticMeshComponentRecreateRenderStateContext(StaticMesh, bInvalidateLighting, bRefreshBounds));

		// Release the static mesh's resources.
		StaticMesh->ReleaseResources();

		// Flush the resource release commands to the rendering thread to ensure that the build doesn't occur while a resource is still
		// allocated, and potentially accessing the UStaticMesh.
		StaticMesh->ReleaseResourcesFence.Wait();
	}
}

void FStaticMeshBuilder::OnBuildRenderMeshFinish(UStaticMesh* StaticMesh, const bool bRebuildBoundsAndCollision)
{
	

	if (bRebuildBoundsAndCollision)
	{
		UpdateBounds(StaticMesh);
		UpdateCollision(StaticMesh);
	}

	StaticMesh->InitResources();

	// NOTE: This can call InvalidateLightingCache() on all components using this mesh, causing Modify() to be 
	// called on those components!  Just something to be aware of when EndModification() is called within
	// an undo transaction.
	RecreateRenderStateContext.Reset();
}

bool FStaticMeshBuilder::IsOrphanedVertex(const UMeshDescription *MeshDescription, const FVertexID VertexID) const
{
	const FVertexInstanceArray& VertexInstances = MeshDescription->VertexInstances();
	for (const FVertexInstanceID VertexInstanceID : MeshDescription->GetVertex(VertexID).VertexInstanceIDs)
	{
		if (VertexInstances[VertexInstanceID].ConnectedPolygons.Num() > 0)
		{
			return false;
		}
	}

	return true;
}

void FStaticMeshBuilder::UpdateBounds(UStaticMesh* StaticMesh)
{
	const UMeshDescription* MeshDescription = StaticMesh->GetMeshDescription();
	check(MeshDescription != nullptr);

	// Compute a new bounding box
	// @todo mesheditor perf: During the final modification, only do this if the bounds may have changed (need hinting)
	FBoxSphereBounds BoundingBoxAndSphere;

	FBox BoundingBox;
	BoundingBox.Init();

	// Could improve performance here if necessary:
	// 1) cache polygon IDs per vertex (in order to quickly reject orphans) and just iterate vertex array; or
	// 2) cache bounding box per polygon
	// There are other cases where having polygon adjacency information (1) might be useful, so it's maybe worth considering.

	const FVertexArray& Vertices = MeshDescription->Vertices();
	const FVertexInstanceArray& VertexInstances = MeshDescription->VertexInstances();

	for (const FVertexID VertexID : Vertices.GetElementIDs())
	{
		if (!IsOrphanedVertex(MeshDescription, VertexID))
		{
			BoundingBox += Vertices[VertexID].VertexPosition;
		}
	}

	BoundingBox.GetCenterAndExtents( /* Out */ BoundingBoxAndSphere.Origin, /* Out */ BoundingBoxAndSphere.BoxExtent);

	// Calculate the bounding sphere, using the center of the bounding box as the origin.
	BoundingBoxAndSphere.SphereRadius = 0.0f;

	for (const FVertexID VertexID : Vertices.GetElementIDs())
	{
		if (!IsOrphanedVertex(MeshDescription, VertexID))
		{
			const FVector VertexPosition = Vertices[VertexID].VertexPosition;
			BoundingBoxAndSphere.SphereRadius = FMath::Max((VertexPosition - BoundingBoxAndSphere.Origin).Size(), BoundingBoxAndSphere.SphereRadius);
		}
	}

	FStaticMeshRenderData& StaticMeshRenderData = *StaticMesh->RenderData;
	StaticMeshRenderData.Bounds = BoundingBoxAndSphere;
	StaticMesh->CalculateExtendedBounds();
}


void FStaticMeshBuilder::UpdateCollision(UStaticMesh *StaticMesh)
{
	// @todo mesheditor collision: We're wiping the existing simplified collision and generating a simple bounding
	// box collision, since that's the best we can do without impacting performance.  We always using visibility (complex)
	// collision for traces while mesh editing (for hover/selection), so simplified collision isn't really important.
	const bool bRecreateSimplifiedCollision = true;

	if (StaticMesh->BodySetup == nullptr)
	{
		StaticMesh->CreateBodySetup();
	}

	UBodySetup* BodySetup = StaticMesh->BodySetup;

	// NOTE: We don't bother calling Modify() on the BodySetup as EndModification() will rebuild this guy after every undo
	// BodySetup->Modify();

	if (bRecreateSimplifiedCollision)
	{
		if (BodySetup->AggGeom.GetElementCount() > 0)
		{
			BodySetup->RemoveSimpleCollision();
		}
	}

	BodySetup->InvalidatePhysicsData();

	if (bRecreateSimplifiedCollision)
	{
		const FBoxSphereBounds Bounds = StaticMesh->GetBounds();

		FKBoxElem BoxElem;
		BoxElem.Center = Bounds.Origin;
		BoxElem.X = Bounds.BoxExtent.X * 2.0f;
		BoxElem.Y = Bounds.BoxExtent.Y * 2.0f;
		BoxElem.Z = Bounds.BoxExtent.Z * 2.0f;
		BodySetup->AggGeom.BoxElems.Add(BoxElem);
	}

	// Update all static mesh components that are using this mesh
	// @todo mesheditor perf: This is a pretty heavy operation, and overlaps with what we're already doing in RecreateRenderStateContext
	// a little bit.  Ideally we do everything in a single pass.  Furthermore, if this could be updated lazily it would be faster.
	{
		for (FObjectIterator Iter(UStaticMeshComponent::StaticClass()); Iter; ++Iter)
		{
			UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(*Iter);
			if (StaticMeshComponent->GetStaticMesh() == StaticMesh)
			{
				// it needs to recreate IF it already has been created
				if (StaticMeshComponent->IsPhysicsStateCreated())
				{
					StaticMeshComponent->RecreatePhysicsState();
				}
			}
		}
	}
}
