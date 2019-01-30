// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "EditableGeometryCollectionAdapter.h"
#include "EditableMesh.h"
#include "EditableMeshChanges.h"
#include "Engine/StaticMesh.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "EditableMeshFactory.h"
#include "MeshAttributes.h"
#include "EditorSupportDelegates.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"

DEFINE_LOG_CATEGORY(LogGeometryCollectionAdapter);

// #todo: still lots to implement in here and some it might be in common with the Static Mesh variant of this class

const FAdaptorTriangleID FAdaptorTriangleID::Invalid( TNumericLimits<uint32>::Max() );


UEditableGeometryCollectionAdapter::UEditableGeometryCollectionAdapter()
	: GeometryCollection( nullptr ),
	  GeometryCollectionLODIndex( 0 ),
	  CachedBoundingBoxAndSphere( FVector::ZeroVector, FVector::ZeroVector, 0.0f )
{
}

void UEditableGeometryCollectionAdapter::InitEditableGeometryCollection( UEditableMesh* EditableMesh, class UPrimitiveComponent& Component, const FEditableMeshSubMeshAddress& InitSubMeshAddress )
{
	EditableMesh->SetSubMeshAddress( InitSubMeshAddress );
	GeometryCollectionLODIndex = InitSubMeshAddress.LODIndex;

	RenderingPolygons.Reset();
	RenderingPolygonGroups.Reset();

	// We're partial to geometry collection components, here
	GeometryCollectionComponent = Cast<UGeometryCollectionComponent>( &Component );
	if( GeometryCollectionComponent != nullptr )
	{
		FGeometryCollectionEdit GeometryCollectionEdit = GeometryCollectionComponent->EditRestCollection();
		if( UGeometryCollection* GeometryCollectionObject = GeometryCollectionEdit.GetRestCollection() )
		{
			TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
			if (FGeometryCollection* GeometryCollectionSource = GeometryCollectionPtr.Get())
			{
				//LogGeometryCollectionStats(FString(TEXT("Source Geometry Collection")));
				this->GeometryCollection = GeometryCollectionObject;
				this->OriginalGeometryCollection = GeometryCollectionObject;

				FMeshDescription* MeshDescription = EditableMesh->GetMeshDescription();

				// The Editable Mesh mesh attributes that are going to be initialised
				TVertexAttributesRef<FVector> VertexPositions = MeshDescription->VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);
				TVertexInstanceAttributesRef<FVector> VertexInstanceNormals = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
				TVertexInstanceAttributesRef<FVector> VertexInstanceTangents = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
				TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = MeshDescription->VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);
				TVertexInstanceAttributesRef<FVector4> VertexInstanceColors = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color);
				TVertexInstanceAttributesRef<FVector2D> VertexInstanceUVs = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);

				// The source Geometry Collection
				TSharedRef<TManagedArray<FVector> > GCVerticesArray = GeometryCollectionSource->GetAttribute<FVector>("Vertex", FGeometryCollection::VerticesGroup);
				TSharedRef<TManagedArray<FVector> > GCNormalsArray = GeometryCollectionSource->GetAttribute<FVector>("Normal", FGeometryCollection::VerticesGroup);
				TSharedRef<TManagedArray<FVector> > GCTangentsArray = GeometryCollectionSource->GetAttribute<FVector>("TangentU", FGeometryCollection::VerticesGroup);
				TSharedRef<TManagedArray<FVector2D> > GCUVsArray = GeometryCollectionSource->GetAttribute<FVector2D>("UV", FGeometryCollection::VerticesGroup);
				TSharedRef<TManagedArray<FLinearColor> > GCColorsArray = GeometryCollectionSource->GetAttribute<FLinearColor>("Color", FGeometryCollection::VerticesGroup);
				TSharedRef<TManagedArray<int32> > GCBoneMapArray = GeometryCollectionSource->GetAttribute<int32>("BoneMap", FGeometryCollection::VerticesGroup);
				TSharedRef<TManagedArray<FIntVector> > GCIndicesArray = GeometryCollectionSource->GetAttribute<FIntVector>("Indices", FGeometryCollection::FacesGroup);
				TSharedRef<TManagedArray<bool> > GCVisibleArray = GeometryCollectionSource->GetAttribute<bool>("Visible", FGeometryCollection::FacesGroup);


				TManagedArray<FVector>& GCVertices = *GCVerticesArray;
				TManagedArray<FVector>& GCNormals = *GCNormalsArray;
				TManagedArray<FVector>& GCTangents = *GCTangentsArray;
				TManagedArray<FVector2D>& GCUVs = *GCUVsArray;
				TManagedArray<FLinearColor>& GCColors = *GCColorsArray;
				TManagedArray<int32>& GCBoneMap = *GCBoneMapArray;
				TManagedArray<FIntVector>&  GCIndices = *GCIndicesArray;
				TManagedArray<bool>&  GCVisible = *GCVisibleArray;

				TArray<FTransform> GCTransforms;
				GeometryCollectionAlgo::GlobalMatrices(GeometryCollectionSource, GCTransforms);
				checkSlow(GeometryCollectionSource->Transform->Num() == GCTransforms.Num());


				TArray<int32> BoneTriangleCount;
				BoneTriangleCount.Init(0, GCTransforms.Num());
				for (int Index = 0; Index < GCBoneMap.Num(); Index++)
				{
					int32 BoneIndex = GCBoneMap[Index];
					check(BoneIndex < BoneTriangleCount.Num());
					BoneTriangleCount[BoneIndex]++;
				}

				// Store off the number of texture coordinates in this mesh
				const int32 NumUVs = 1;
				VertexInstanceUVs.SetNumIndices(NumUVs);
				EditableMesh->TextureCoordinateCount = NumUVs;

				// Vertex Positions
				const int32 NumRenderingVertices = GCVertices.Num();
				MeshDescription->ReserveNewVertices(NumRenderingVertices);
				MeshDescription->ReserveNewVertexInstances(NumRenderingVertices);

				// Vertex Colors
				const int32 NumColorVertices = GCColors.Num();
				const bool bHasColor = NumColorVertices > 0;
				check(!bHasColor || NumColorVertices == NumRenderingVertices);

				const double CacheStartLoopTime = FPlatformTime::Seconds();
				int32 CallCount = 0;
				int32 NewVertexIndex = 0;
				for (int32 RenderingVertexIndex = 0; RenderingVertexIndex < NumRenderingVertices; ++RenderingVertexIndex)
				{
					const FVector VertexPosition = GCVertices[RenderingVertexIndex];
					const FVertexInstanceID VertexInstanceID(RenderingVertexIndex);

					const FVertexID NewVertexID = MeshDescription->CreateVertex();
					VertexPositions[NewVertexID] = VertexPosition;

					MeshDescription->CreateVertexInstanceWithID(VertexInstanceID, NewVertexID);
					CallCount++;

					// Populate the vertex instance attributes
					{
						const FVector Normal = GCNormals[RenderingVertexIndex];
						const FVector Tangent = GCTangents[RenderingVertexIndex];
						const FVector2D UV = GCUVs[RenderingVertexIndex];
						const FLinearColor Color = bHasColor ? FLinearColor(GCColors[RenderingVertexIndex]) : FLinearColor::White;

						VertexInstanceNormals[VertexInstanceID] = Normal;
						VertexInstanceTangents[VertexInstanceID] = Tangent;
						VertexInstanceColors[VertexInstanceID] = Color;
						for (int32 UVIndex = 0; UVIndex < 1; ++UVIndex)
						{
							VertexInstanceUVs.Set(VertexInstanceID, UVIndex, GCUVs[RenderingVertexIndex]);
						}
					}

				}

				// one group per bone in the Geometry Collection
				const uint32 NumBones = BoneTriangleCount.Num();
				const uint32 NumTotalTriangles = GCIndices.Num();

				// Polygon Groups
				MeshDescription->ReserveNewPolygonGroups(NumBones);
				uint32 NumSections = NumBones;

				// Add all polygon groups from the mesh sections
				for (uint32 RenderingSectionIndex = 0; RenderingSectionIndex < NumSections; ++RenderingSectionIndex)
				{
					// Create a new polygon group
					const FPolygonGroupID NewPolygonGroupID = MeshDescription->CreatePolygonGroup();

					// Create a rendering polygon group for holding the triangulated data and references to the static mesh rendering section.
					// This is indexed by the same FPolygonGroupID as the PolygonGroups.
					RenderingPolygonGroups.Insert(NewPolygonGroupID);
					FAdaptorPolygon2Group& NewRenderingPolygonGroup = RenderingPolygonGroups[NewPolygonGroupID];

					const uint32 NumSectionTriangles = BoneTriangleCount[RenderingSectionIndex];
					NewRenderingPolygonGroup.Triangles.Reserve(NumSectionTriangles);
					NewRenderingPolygonGroup.MaxTriangles = NumSectionTriangles;
					NewRenderingPolygonGroup.RenderingSectionIndex = RenderingSectionIndex;
					NewRenderingPolygonGroup.MaterialIndex = 0;		// #todo: support multiple materials

					MeshDescription->ReserveNewPolygons(NumSectionTriangles);
					MeshDescription->ReserveNewEdges(NumSectionTriangles * 3);	// more than required, but not a problem

					int32 TriangleGroupIndex = 0;
					for (uint32 TriangleIndex = 0; TriangleIndex < NumTotalTriangles; ++TriangleIndex)
					{
						FIntVector Indices = GCIndices[TriangleIndex];

						check(GCBoneMap[Indices[0]] == GCBoneMap[Indices[1]]);
						check(GCBoneMap[Indices[0]] == GCBoneMap[Indices[2]]);
						// only select those triangles that are associated with the currently selected MeshIndex/BoneIndex
						if ((GCBoneMap[Indices[0]] != RenderingSectionIndex) || GCVisible[TriangleIndex] == false)
							continue;

						TArray<FVertexInstanceID> TriangleVertexInstanceIDs;
						TriangleVertexInstanceIDs.SetNum(3);

						FVertexID TriangleVertexIDs[3];
						for (uint32 TriangleVertexIndex = 0; TriangleVertexIndex < 3; ++TriangleVertexIndex)
						{
							TriangleVertexInstanceIDs[TriangleVertexIndex] = FVertexInstanceID(Indices[TriangleVertexIndex]);
							TriangleVertexIDs[TriangleVertexIndex] = MeshDescription->GetVertexInstanceVertex(TriangleVertexInstanceIDs[TriangleVertexIndex]);
						}

						// Make sure we have a valid triangle.  The triangle can be invalid because at least two if it's vertex indices 
						// point to the exact same vertex.  The triangle is degenerate.  This can happen due to us welding the overlapping 
						// vertices because they were either extremely close to each other (or exactly overlapping.)  We'll ignore this triangle.
						const bool bIsValidTriangle =
							TriangleVertexIDs[0] != TriangleVertexIDs[1] &&
							TriangleVertexIDs[1] != TriangleVertexIDs[2] &&
							TriangleVertexIDs[2] != TriangleVertexIDs[0];

						if (bIsValidTriangle)
						{
							// Geometry Collections only support triangles, so there's no need to triangulate anything yet.  We'll make both
							// a triangle and a polygon here.
							const FAdaptorTriangleID NewTriangleID = FAdaptorTriangleID(TriangleGroupIndex++);

							NewRenderingPolygonGroup.Triangles.Insert(NewTriangleID);
							FMeshTriangle& NewTriangle = NewRenderingPolygonGroup.Triangles[NewTriangleID];
							for (uint32 TriangleVertexIndex = 0; TriangleVertexIndex < 3; ++TriangleVertexIndex)
							{
								NewTriangle.SetVertexInstanceID(TriangleVertexIndex, TriangleVertexInstanceIDs[TriangleVertexIndex]);
							}

							// Insert a polygon into the mesh
							const FPolygonID NewPolygonID(TriangleIndex);
							MeshDescription->CreatePolygonWithID(NewPolygonID, NewPolygonGroupID, TriangleVertexInstanceIDs);

							// Create a rendering polygon mirror, indexed by the same ID
							RenderingPolygons.Insert(NewPolygonID);
							FAdaptorPolygon& NewRenderingPolygon = RenderingPolygons[NewPolygonID];
							NewRenderingPolygon.PolygonGroupID = NewPolygonGroupID;
							NewRenderingPolygon.TriangulatedPolygonTriangleIndices.Add(NewTriangleID);

							// Add triangle to polygon triangulation array
							MeshDescription->GetPolygonTriangles(NewPolygonID).Add(NewTriangle);

						}
						else
						{
							// Triangle was not valid.  This will result in an empty entry in our Triangles sparse array.  Luckily,
							// the triangle is already degenerate so we don't need to change anything.  This triangle index will be
							// re-used if a new triangle needs to be created during editing
							// @todo mesheditor: This can cause vertex instances to be orphaned.  Should we delete them?
						}
					}

					// Determine edge hardnesses
					MeshDescription->DetermineEdgeHardnessesFromVertexInstanceNormals();

					// Determine UV seams
					CA_SUPPRESS(6326);
					if (NumUVs > 0)
					{
						MeshDescription->DetermineUVSeamsFromUVs(0);
					}

					// Cache polygon tangent bases
					static TArray<FPolygonID> PolygonIDs;
					PolygonIDs.Reset();
					for (const FPolygonID PolygonID : EditableMesh->GetMeshDescription()->Polygons().GetElementIDs())
					{
						PolygonIDs.Add(PolygonID);
					}

					EditableMesh->GeneratePolygonTangentsAndNormals(PolygonIDs);

				}
			}
		}
		
		FTransform LocalToWorld = FTransform::Identity;
		this->CachedBoundingBoxAndSphere = GeometryCollectionComponent->CalcBounds(LocalToWorld);
	}


#if EDITABLE_MESH_USE_OPENSUBDIV
	EditableMesh->RefreshOpenSubdiv();
#endif
	EditableMesh->RebuildOctree();
}


void UEditableGeometryCollectionAdapter::InitFromBlankGeometryCollection( UEditableMesh* EditableMesh, UGeometryCollection& InGeometryCollection)
{
	GeometryCollection = &InGeometryCollection;
}


void UEditableGeometryCollectionAdapter::InitializeFromEditableMesh( const UEditableMesh* EditableMesh )
{
	// Get the Geometry Collection from the editable mesh submesh address
	const FEditableMeshSubMeshAddress& SubMeshAddress = EditableMesh->GetSubMeshAddress();
	GeometryCollection = static_cast<UGeometryCollection*>( SubMeshAddress.MeshObjectPtr );

	// @todo mesheditor instancing: sort this out
	OriginalGeometryCollection = nullptr;

	// Always targets LOD0 at the moment
	GeometryCollectionLODIndex = 0;

	RenderingPolygons.Reset();
	RenderingPolygonGroups.Reset();

	const FMeshDescription* MeshDescription = EditableMesh->GetMeshDescription();

	// Create all the required rendering polygon groups (initialized to 'empty', each with a unique rendering section index)
	int32 RenderingSectionIndex = 0;
	for( const FPolygonGroupID PolygonGroupID : MeshDescription->PolygonGroups().GetElementIDs() )
	{
		RenderingPolygonGroups.Insert( PolygonGroupID );
		FAdaptorPolygon2Group& RenderingPolygonGroup = RenderingPolygonGroups[ PolygonGroupID ];
		RenderingPolygonGroup.RenderingSectionIndex = RenderingSectionIndex;

		//const FName SlotName = MeshDescription->PolygonGroupAttributes().GetAttribute<FName>( PolygonGroupID, MeshAttribute::PolygonGroup::ImportedMaterialSlotName );
		//RenderingPolygonGroup.MaterialIndex = GeometryCollection->StaticMaterials.IndexOfByPredicate(
		//	[ &SlotName ]( const FStaticMaterial& StaticMaterial )
		//	{
		//		return StaticMaterial.ImportedMaterialSlotName == SlotName;
		//	}
		//);
		RenderingPolygonGroup.MaxTriangles = 0;

		RenderingSectionIndex++;
	}

	// Go through all the polygons, adding their triangles to the rendering polygon group
	for( const FPolygonID PolygonID : MeshDescription->Polygons().GetElementIDs() )
	{
		const FPolygonGroupID PolygonGroupID = MeshDescription->GetPolygonPolygonGroup( PolygonID );
		FAdaptorPolygon2Group& RenderingPolygonGroup = RenderingPolygonGroups[ PolygonGroupID ];

		RenderingPolygons.Insert( PolygonID );
		FAdaptorPolygon& RenderingPolygon = RenderingPolygons[ PolygonID ];
		RenderingPolygon.PolygonGroupID = PolygonGroupID;

		const TArray<FMeshTriangle>& Triangles = MeshDescription->GetPolygonTriangles( PolygonID );
		for( const FMeshTriangle& Triangle : Triangles )
		{
			const FAdaptorTriangleID TriangleID = RenderingPolygonGroup.Triangles.Add( Triangle );
			RenderingPolygon.TriangulatedPolygonTriangleIndices.Add( TriangleID );
		}

		RenderingPolygonGroup.MaxTriangles += Triangles.Num();
	}

}


void UEditableGeometryCollectionAdapter::OnRebuildRenderMesh(const UEditableMesh* EditableMesh)
{
	return;
	const FMeshDescription* MeshDescription = EditableMesh->GetMeshDescription();

	// Source is the Editable Mesh Data
	TVertexAttributesConstRef<FVector> VertexPositions = MeshDescription->VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);
	TVertexInstanceAttributesConstRef<FVector> VertexInstanceNormals = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
	TVertexInstanceAttributesConstRef<FVector> VertexInstanceTangents = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
	TVertexInstanceAttributesConstRef<float> VertexInstanceBinormalSigns = MeshDescription->VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);
	TVertexInstanceAttributesConstRef<FVector4> VertexInstanceColors = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color);
	TVertexInstanceAttributesConstRef<FVector2D> VertexInstanceUVs = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);

	// Clear Geometry Collection
	if (GeometryCollection)
	{
		TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollection->GetGeometryCollection();
		if (FGeometryCollection* Collection = GeometryCollectionPtr.Get())
		{
			GeometryCollection->Modify();

			Collection->Resize(0, FGeometryCollection::VerticesGroup);
			Collection->Resize(0, FGeometryCollection::FacesGroup);
			//GeometryCollection->Resize(0, FGeometryCollection::TransformGroup);

			// Destination is the Geometry Collection
			TSharedRef<TManagedArray<FVector> > GCVerticesArray = Collection->GetAttribute<FVector>("Vertex", FGeometryCollection::VerticesGroup);
			TSharedRef<TManagedArray<FVector> >  GCNormalsArray = Collection->GetAttribute<FVector>("Normal", FGeometryCollection::VerticesGroup);
			TSharedRef<TManagedArray<FVector> >  GCTangentsArray = Collection->GetAttribute<FVector>("TangentU", FGeometryCollection::VerticesGroup);
			TSharedRef<TManagedArray<FVector2D> >  GCUVsArray = Collection->GetAttribute<FVector2D>("UV", FGeometryCollection::VerticesGroup);
			TSharedRef<TManagedArray<FLinearColor> >  GCColorsArray = Collection->GetAttribute<FLinearColor>("Color", FGeometryCollection::VerticesGroup);
			TSharedRef<TManagedArray<int32> >  GCBoneMapArray = Collection->GetAttribute<int32>("BoneMap", FGeometryCollection::VerticesGroup);

			TSharedRef<TManagedArray<FIntVector> > GCIndicesArray = Collection->GetAttribute<FIntVector>("Indices", FGeometryCollection::FacesGroup);
			TSharedRef<TManagedArray<bool> > GCVisibleArray = Collection->GetAttribute<bool>("Visible", FGeometryCollection::FacesGroup);

			TSharedRef<TManagedArray<FTransform> > GCTransformsArray = Collection->GetAttribute<FTransform>("Transform", FGeometryCollection::TransformGroup);

			TManagedArray<FVector>& GCVertices = *GCVerticesArray;
			TManagedArray<FVector>& GCNormals = *GCNormalsArray;
			TManagedArray<FVector>& GCTangents = *GCTangentsArray;
			TManagedArray<FVector2D>& GCUVs = *GCUVsArray;
			TManagedArray<FLinearColor>& GCColors = *GCColorsArray;
			TManagedArray<int32>& GCBoneMap = *GCBoneMapArray;

			TManagedArray<FIntVector>&  GCIndices = *GCIndicesArray;
			TManagedArray<bool>&  GCVisible = *GCVisibleArray;

			TManagedArray<FTransform> & GCTransforms = *GCTransformsArray;


			Collection->AddElements(VertexPositions.GetNumElements(), FGeometryCollection::VerticesGroup);

			// fill vertex buffer elements
			for (const FVertexID& VertexID : MeshDescription->Vertices().GetElementIDs())
			{
				int32 VertexIDValue = VertexID.GetValue();

				FVector VertexPosition = VertexPositions[VertexID];
				GCVertices[VertexIDValue] = VertexPosition;
			}

			for (const FVertexInstanceID& VertexInstanceID : MeshDescription->VertexInstances().GetElementIDs())
			{
				int32 VertexInstanceIDValue = VertexInstanceID.GetValue();
				GCNormals[VertexInstanceIDValue] = VertexInstanceNormals[VertexInstanceID];
				GCUVs[VertexInstanceIDValue] = VertexInstanceUVs.Get(VertexInstanceID, 0);
				GCTangents[VertexInstanceIDValue] = VertexInstanceTangents[VertexInstanceID];
				FVector4 Color = VertexInstanceColors[VertexInstanceID];
				GCColors[VertexInstanceIDValue] = FLinearColor(Color[0], Color[1], Color[2], Color[3]);
			}

			for (const FPolygonID& PolygonID : MeshDescription->Polygons().GetElementIDs())
			{
				const FPolygonGroupID& PolygonGroupID = MeshDescription->GetPolygonPolygonGroup(PolygonID);
				int32 PolygonGroupIDValue = PolygonGroupID.GetValue();
				int32 PolygonIDValue = PolygonID.GetValue();
				const TArray<FMeshTriangle>& Triangles = MeshDescription->GetPolygonTriangles(PolygonID);
				for (const FMeshTriangle& MeshTriangle : Triangles)
				{
					uint32 ElementIndex = Collection->AddElements(1, FGeometryCollection::FacesGroup);

					// might need a lookup here VertexInstanceID to GCVector array index
					GCIndices[ElementIndex] = FIntVector(MeshTriangle.VertexInstanceID0.GetValue(), MeshTriangle.VertexInstanceID1.GetValue(), MeshTriangle.VertexInstanceID2.GetValue());
					GCVisible[ElementIndex] = true;

					GCBoneMap[MeshTriangle.VertexInstanceID0.GetValue()] = PolygonGroupIDValue;
					GCBoneMap[MeshTriangle.VertexInstanceID1.GetValue()] = PolygonGroupIDValue;
					GCBoneMap[MeshTriangle.VertexInstanceID2.GetValue()] = PolygonGroupIDValue;

				}
			}
		}
	}

	LogGeometryCollectionStats(FString(TEXT("Generated Geometry Collection")));
}


void UEditableGeometryCollectionAdapter::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	Ar.UsingCustomVersion( FEditableMeshCustomVersion::GUID );

	Ar << RenderingPolygons;
	Ar << RenderingPolygonGroups;
}


void UEditableGeometryCollectionAdapter::OnStartModification( const UEditableMesh* EditableMesh, const EMeshModificationType MeshModificationType, const EMeshTopologyChange MeshTopologyChange )
{
	// @todo mesheditor undo: We're not using traditional transactions to undo mesh changes yet, but we still want to dirty the mesh package
	// Also, should we even need the Initializing type? Should we not wait for the first modification before dirtying the package?

	GeometryCollection->Modify();
}


void UEditableGeometryCollectionAdapter::OnRebuildRenderMeshStart( const UEditableMesh* EditableMesh, const bool bInvalidateLighting )
{
	// #todo : should there be some work to do here to avoid threading issues, see OnRebuildRenderMeshStart in EditableStaticMeshAdaptor.cpp 
}


void UEditableGeometryCollectionAdapter::OnEndModification( const UEditableMesh* EditableMesh )
{
	// nothing to do here
}


void UEditableGeometryCollectionAdapter::OnRebuildRenderMeshFinish( const UEditableMesh* EditableMesh, const bool bRebuildBoundsAndCollision, const bool bIsPreviewRollback )
{
	UpdateBounds(EditableMesh, bRebuildBoundsAndCollision);

	if (bRebuildBoundsAndCollision)
	{
		UpdateCollision();
	}

	if (GeometryCollectionComponent)
	{
		GeometryCollectionComponent->MarkRenderStateDirty();
	}
#if WITH_EDITOR
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
#endif // WITH_EDITOR
}


void UEditableGeometryCollectionAdapter::OnReindexElements( const UEditableMesh* EditableMesh, const FElementIDRemappings& Remappings )
{
	// #todo: implement this
	check(false);
}


bool UEditableGeometryCollectionAdapter::IsCommitted( const UEditableMesh* EditableMesh ) const
{
	return GeometryCollection->EditableMesh == EditableMesh;
}


bool UEditableGeometryCollectionAdapter::IsCommittedAsInstance( const UEditableMesh* EditableMesh ) const
{
	return GeometryCollection != OriginalGeometryCollection;
}


void UEditableGeometryCollectionAdapter::OnCommit( UEditableMesh* EditableMesh )
{
	if( !IsCommitted( EditableMesh ) )
	{
		// Move the editable mesh to an inner of the static mesh, and set the static mesh's EditableMesh property.
		EditableMesh->Rename( nullptr, GeometryCollection, REN_DontCreateRedirectors );
		GeometryCollection->EditableMesh = EditableMesh;
	}
}


UEditableMesh* UEditableGeometryCollectionAdapter::OnCommitInstance( UEditableMesh* EditableMesh, UPrimitiveComponent* ComponentToInstanceTo )
{
	check(false);

	GeometryCollectionComponent = Cast<UGeometryCollectionComponent>( ComponentToInstanceTo );

	if( GeometryCollectionComponent )
	{
		// Duplicate the static mesh, putting it as an *inner* of the static mesh component.
		// This is no longer a persistent asset, so clear the appropriate flags.
		UGeometryCollection* NewGeometryCollection = DuplicateObject( OriginalGeometryCollection, GeometryCollectionComponent );
		NewGeometryCollection->ClearFlags( RF_Public | RF_Standalone );

		// Point the static mesh component to the new static mesh instance we just made for it
		GeometryCollectionComponent->SetRestCollection( NewGeometryCollection );

		// Duplicate this editable mesh to a new instance inside the new static mesh instance, and set the static mesh's EditableMesh property.
		UEditableMesh* NewEditableMesh = DuplicateObject( EditableMesh, NewGeometryCollection );

		// Look for the corresponding adapter instance in the duplicated mesh.
		const int32 AdapterIndex = EditableMesh->Adapters.Find( this );
		check( AdapterIndex != INDEX_NONE );
		UEditableGeometryCollectionAdapter* NewAdapter = Cast<UEditableGeometryCollectionAdapter>( NewEditableMesh->Adapters[ AdapterIndex ] );

		NewGeometryCollection->EditableMesh = NewEditableMesh;
		NewAdapter->GeometryCollection = NewGeometryCollection;

		// Update the submesh address which will have changed now it's been instanced
		NewEditableMesh->SetSubMeshAddress( UEditableMeshFactory::MakeSubmeshAddress( GeometryCollectionComponent, EditableMesh->SubMeshAddress.LODIndex ) );
		NewEditableMesh->RebuildRenderMesh();

		return NewEditableMesh;
	}

	return nullptr;
}


void UEditableGeometryCollectionAdapter::OnRevert( UEditableMesh* EditableMesh )
{
	// #todo: implement this?
	check(false);

}


UEditableMesh* UEditableGeometryCollectionAdapter::OnRevertInstance( UEditableMesh* EditableMesh )
{
	// #todo: implement this
	return nullptr;
}


void UEditableGeometryCollectionAdapter::OnPropagateInstanceChanges( UEditableMesh* EditableMesh )
{
	// #todo: implement this
	check( false );
}


void UEditableGeometryCollectionAdapter::UpdateBounds( const UEditableMesh* EditableMesh, const bool bShouldRecompute )
{
	if( bShouldRecompute )
	{
		// Compute a new bounding box
		// @todo mesheditor perf: During the final modification, only do this if the bounds may have changed (need hinting)
		FBoxSphereBounds BoundingBoxAndSphere;

		// @todo mesheditor LODs: Really we should store the bounds of LOD0 inside the static mesh.  Our editable mesh might be for a different LOD.

		// If we're in subdivision preview mode, use the bounds of the base cage mesh, so that simple collision
		// queries will always include the base cage, even though the actual mesh geometry might be quite a bit smaller.
		// This also relies on us specifically querying against the simple collision, which we do in a second pass after
		// looking for meshes using a complex collision trace.
		// @todo mesheditor: Ideally we are not storing an inflated bounds here just for base cage editor interaction
		if( EditableMesh->IsPreviewingSubdivisions() )
		{
			BoundingBoxAndSphere = EditableMesh->ComputeBoundingBoxAndSphere();
		}
		else
		{
			FBox BoundingBox;
			BoundingBox.Init();

			const FMeshDescription* MeshDescription = EditableMesh->GetMeshDescription();

			TVertexAttributesConstRef<FVector> VertexPositions = EditableMesh->GetMeshDescription()->VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );

			for( const FVertexID VertexID : MeshDescription->Vertices().GetElementIDs() )
			{
				if( !EditableMesh->IsOrphanedVertex( VertexID ) )
				{
					BoundingBox += VertexPositions[ VertexID ];
				}
			}

			BoundingBox.GetCenterAndExtents( /* Out */ BoundingBoxAndSphere.Origin, /* Out */ BoundingBoxAndSphere.BoxExtent );

			// Calculate the bounding sphere, using the center of the bounding box as the origin.
			BoundingBoxAndSphere.SphereRadius = 0.0f;

			for( const FVertexID VertexID : MeshDescription->Vertices().GetElementIDs() )
			{
				if( !EditableMesh->IsOrphanedVertex( VertexID ) )
				{
					BoundingBoxAndSphere.SphereRadius = FMath::Max( ( VertexPositions[ VertexID ] - BoundingBoxAndSphere.Origin ).Size(), BoundingBoxAndSphere.SphereRadius );
				}
			}
		}

		this->CachedBoundingBoxAndSphere = BoundingBoxAndSphere;
	}
	
}


void UEditableGeometryCollectionAdapter::UpdateCollision()
{
	// #todo: implement this
}


void UEditableGeometryCollectionAdapter::OnSetVertexAttribute(const UEditableMesh* EditableMesh, const FVertexID VertexID, const FMeshElementAttributeData& Attribute)
{
	if (GeometryCollection)
	{
		TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollection->GetGeometryCollection();
		if (FGeometryCollection* Collection = GeometryCollectionPtr.Get())
		{

			TSharedRef<TManagedArray<FVector> > GCVerticesArray = Collection->GetAttribute<FVector>("Vertex", FGeometryCollection::VerticesGroup);
			TManagedArray<FVector>& GCVertices = *GCVerticesArray;

			const FMeshDescription* MeshDescription = EditableMesh->GetMeshDescription();

			if (Attribute.AttributeName == MeshAttribute::Vertex::Position)
			{
				const FVector NewVertexPosition = Attribute.AttributeValue.GetValue<FVector>();

				// @todo mesheditor: eventually break out subdivided mesh into a different adapter which handles things differently?
				// (may also want different component eventually)
				if (!EditableMesh->IsPreviewingSubdivisions())
				{
					const FVertexInstanceArray& VertexInstances = EditableMesh->GetMeshDescription()->VertexInstances();

					// Set the vertex buffer position of all of the vertex instances for this editable vertex
					for (const FVertexInstanceID VertexInstanceID : MeshDescription->GetVertexVertexInstances(VertexID))
					{
						check(MeshDescription->IsVertexInstanceValid(VertexInstanceID));
						auto& OldPosition = GCVertices[VertexInstanceID.GetValue()]; // temp for debug
						GCVertices[VertexInstanceID.GetValue()] = NewVertexPosition;
					}
				}

				// Update cached bounds.  This is not a "perfect" bounding sphere and centered box.  Instead, we take our current bounds
				// and inflate it to include the updated vertex position, translating the bounds proportionally to reduce how much it
				// needs to be expanded.  The "perfect" bounds will be computed in UpdateBounds() when an interaction is finalized.
				{
					const FVector OffsetFromCenter = NewVertexPosition - CachedBoundingBoxAndSphere.Origin;
					const float SquaredDistanceToCenter = OffsetFromCenter.SizeSquared();
					const float SquaredSphereRadius = CachedBoundingBoxAndSphere.SphereRadius * CachedBoundingBoxAndSphere.SphereRadius;
					if (SquaredDistanceToCenter > SquaredSphereRadius)
					{
						const float DistanceToCenter = FMath::Sqrt(SquaredDistanceToCenter);
						const float RadiusDelta = (DistanceToCenter - CachedBoundingBoxAndSphere.SphereRadius) * 0.5f;
						CachedBoundingBoxAndSphere.SphereRadius += RadiusDelta;
						CachedBoundingBoxAndSphere.Origin += OffsetFromCenter * (RadiusDelta / DistanceToCenter);
					}

					// Update extent
					CachedBoundingBoxAndSphere.BoxExtent.X = FMath::Max(CachedBoundingBoxAndSphere.BoxExtent.X, FMath::Abs(NewVertexPosition.X - CachedBoundingBoxAndSphere.Origin.X));
					CachedBoundingBoxAndSphere.BoxExtent.Y = FMath::Max(CachedBoundingBoxAndSphere.BoxExtent.Y, FMath::Abs(NewVertexPosition.Y - CachedBoundingBoxAndSphere.Origin.Y));
					CachedBoundingBoxAndSphere.BoxExtent.Z = FMath::Max(CachedBoundingBoxAndSphere.BoxExtent.Z, FMath::Abs(NewVertexPosition.X - CachedBoundingBoxAndSphere.Origin.Z));
				}
			}
		}
	}
}


void UEditableGeometryCollectionAdapter::OnSetEdgeAttribute( const UEditableMesh* EditableMesh, const FEdgeID EdgeID, const FMeshElementAttributeData& Attribute )
{
	// Nothing to do here
}


void UEditableGeometryCollectionAdapter::OnSetVertexInstanceAttribute( const UEditableMesh* EditableMesh, const FVertexInstanceID VertexInstanceID, const FMeshElementAttributeData& Attribute )
{
	TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollection->GetGeometryCollection();
	FGeometryCollection* Collection = GeometryCollectionPtr.Get();

	TSharedRef<TManagedArray<FVector> > GCNormalsArray = Collection->GetAttribute<FVector>("Normal", FGeometryCollection::VerticesGroup);
	TSharedRef<TManagedArray<FVector> > GCTangentsArray = Collection->GetAttribute<FVector>("TangentU", FGeometryCollection::VerticesGroup);
	TSharedRef<TManagedArray<FVector2D> > GCUVsArray = Collection->GetAttribute<FVector2D>("UV", FGeometryCollection::VerticesGroup);
	TSharedRef<TManagedArray<FLinearColor> > GCColorsArray = Collection->GetAttribute<FLinearColor>("Color", FGeometryCollection::VerticesGroup);
	TManagedArray<FVector>& GCNormals = *GCNormalsArray;
	TManagedArray<FVector>& GCTangents = *GCTangentsArray;
	TManagedArray<FVector2D>& GCUVs = *GCUVsArray;
	TManagedArray<FLinearColor>& GCColors = *GCColorsArray;

	const TAttributesSet<FVertexInstanceID>& VertexInstanceAttributes = EditableMesh->GetMeshDescription()->VertexInstanceAttributes();

	if( Attribute.AttributeName == MeshAttribute::VertexInstance::Normal ||
		Attribute.AttributeName == MeshAttribute::VertexInstance::Tangent ||
		Attribute.AttributeName == MeshAttribute::VertexInstance::BinormalSign )
	{
		if( !EditableMesh->IsPreviewingSubdivisions() )
		{
			const FVector Normal = VertexInstanceAttributes.GetAttribute<FVector>( VertexInstanceID, MeshAttribute::VertexInstance::Normal );
			const FVector Tangent = VertexInstanceAttributes.GetAttribute<FVector>( VertexInstanceID, MeshAttribute::VertexInstance::Tangent );
			const float BinormalSign = VertexInstanceAttributes.GetAttribute<float>( VertexInstanceID, MeshAttribute::VertexInstance::BinormalSign );

			// @todo mesheditor perf: SetVertexTangents() and VertexTangentX/Y() functions actually does a bit of work to compute the basis every time. 
			// Ideally we can get/set this stuff directly to improve performance.  This became slower after high precision basis values were added.
			// @todo mesheditor perf: this is even more pertinent now we already have the binormal sign!
			/*StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents(
				VertexInstanceID.GetValue(),
				Tangent,
				FVector::CrossProduct( Normal, Tangent ).GetSafeNormal() * BinormalSign,
				Normal );*/

				//#todo: other stuff here?
				GCNormals[VertexInstanceID.GetValue()] = Normal;
				GCTangents[VertexInstanceID.GetValue()] = Tangent;
		}
	}
	else if( Attribute.AttributeName == MeshAttribute::VertexInstance::TextureCoordinate )
	{
		if( !EditableMesh->IsPreviewingSubdivisions() )
		{
			check( Attribute.AttributeIndex < EditableMesh->GetTextureCoordinateCount() );
			GCUVs[VertexInstanceID.GetValue()] = Attribute.AttributeValue.GetValue<FVector2D>();
		}
	}
	else if( Attribute.AttributeName == MeshAttribute::VertexInstance::Color )
	{
		if( !EditableMesh->IsPreviewingSubdivisions() )
		{
			const FVector4 Value = Attribute.AttributeValue.GetValue<FVector4>();
			const FLinearColor LinearColor( Value.X, Value.Y, Value.Z, Value.W );
			const FColor NewColor = LinearColor.ToFColor( true );

			//if( StaticMeshLOD.VertexBuffers.ColorVertexBuffer.GetNumVertices() != EditableMesh->GetMeshDescription()->VertexInstances().GetArraySize() )
			//{
			//	if( LinearColor != FLinearColor::White )
			//	{
			//		// Until now, we haven't needed a vertex color buffer.
			//		// Force one to be generated now that we have a non-white vertex in the mesh.
			//		OnRebuildRenderMesh( EditableMesh );
			//	}
			//}
			//else
			{
				GCColors[VertexInstanceID.GetValue()] = NewColor;
			}
		}
	}
}


void UEditableGeometryCollectionAdapter::OnCreateEmptyVertexRange( const UEditableMesh* EditableMesh, const TArray<FVertexID>& VertexIDs )
{
}


void UEditableGeometryCollectionAdapter::OnCreateVertices( const UEditableMesh* EditableMesh, const TArray<FVertexID>& VertexIDs )
{
}


void UEditableGeometryCollectionAdapter::OnCreateVertexInstances( const UEditableMesh* EditableMesh, const TArray<FVertexInstanceID>& VertexInstanceIDs )
{
	// #todo: implement this
	check(false);
}


void UEditableGeometryCollectionAdapter::OnCreateEdges( const UEditableMesh* EditableMesh, const TArray<FEdgeID>& EdgeIDs )
{
	// Nothing to do here for now
}


void UEditableGeometryCollectionAdapter::OnCreatePolygons( const UEditableMesh* EditableMesh, const TArray<FPolygonID>& PolygonIDs )
{
	// Add mirror polygons for static mesh adapter
	for( const FPolygonID PolygonID : PolygonIDs )
	{
		RenderingPolygons.Insert( PolygonID );
		RenderingPolygons[ PolygonID ].PolygonGroupID = EditableMesh->GetGroupForPolygon( PolygonID );
	}
}


void UEditableGeometryCollectionAdapter::OnChangePolygonVertexInstances( const UEditableMesh* EditableMesh, const TArray<FPolygonID>& PolygonIDs )
{
	OnRetriangulatePolygons( EditableMesh, PolygonIDs );
}


void UEditableGeometryCollectionAdapter::OnRetriangulatePolygons( const UEditableMesh* EditableMesh, const TArray<FPolygonID>& PolygonIDs )
{
	// #todo: implement this
}


void UEditableGeometryCollectionAdapter::OnDeleteVertexInstances( const UEditableMesh* EditableMesh, const TArray<FVertexInstanceID>& VertexInstanceIDs )
{
	// Nothing to do here
}


void UEditableGeometryCollectionAdapter::OnDeleteOrphanVertices( const UEditableMesh* EditableMesh, const TArray<FVertexID>& VertexIDs )
{
	// Nothing to do here
}


void UEditableGeometryCollectionAdapter::OnDeleteEdges( const UEditableMesh* EditableMesh, const TArray<FEdgeID>& EdgeIDs )
{
	// Nothing to do here
}


void UEditableGeometryCollectionAdapter::OnSetEdgesVertices( const UEditableMesh* EditableMesh, const TArray<FEdgeID>& EdgeIDs )
{
	// Nothing to do here
}


void UEditableGeometryCollectionAdapter::OnDeletePolygons( const UEditableMesh* EditableMesh, const TArray<FPolygonID>& PolygonIDs )
{
	for( const FPolygonID PolygonID : PolygonIDs )
	{
		// Removes all of a polygon's triangles (including rendering triangles from the index buffer.)
		DeletePolygonTriangles( EditableMesh, PolygonID );

		// Delete the polygon from the static mesh adapter mirror
		RenderingPolygons.Remove( PolygonID );
	}
}


void UEditableGeometryCollectionAdapter::DeletePolygonTriangles( const UEditableMesh* EditableMesh, const FPolygonID PolygonID )
{
	FAdaptorPolygon& Polygon = RenderingPolygons[ PolygonID ];
	const FPolygonGroupID PolygonGroupID = Polygon.PolygonGroupID;

	FAdaptorPolygon2Group& RenderingPolygonGroup = RenderingPolygonGroups[ PolygonGroupID ];

	const int32 NumTrianglesToRemove = Polygon.TriangulatedPolygonTriangleIndices.Num();
	if( NumTrianglesToRemove > 0 )
	{
		if (GeometryCollection)
		{
			TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollection->GetGeometryCollection();
			if (FGeometryCollection* Collection = GeometryCollectionPtr.Get())
			{

				TSharedRef<TManagedArray<FIntVector> > GCIndicesArray = Collection->GetAttribute<FIntVector>("Indices", FGeometryCollection::FacesGroup);
				TManagedArray<FIntVector>& GCIndices = *GCIndicesArray;

				// Remove all of the polygon's triangles from our editable mesh's triangle list.  While doing this, we'll keep
				// track of all of the rendering mesh triangles that we'll need to remove later on.  We'll also figure out which
				// vertex instances will need to be removed from their corresponding vertex
				for (const FAdaptorTriangleID TriangleIndexToRemove : Polygon.TriangulatedPolygonTriangleIndices)
				{
					const FMeshTriangle& TriToRemove = RenderingPolygonGroup.Triangles[TriangleIndexToRemove];

					// sanity check
					FIntVector TriIndices = GCIndices[TriangleIndexToRemove.GetValue()];
					check(TriToRemove.VertexInstanceID0.GetValue() == TriIndices[0]);
					check(TriToRemove.VertexInstanceID1.GetValue() == TriIndices[1]);
					check(TriToRemove.VertexInstanceID2.GetValue() == TriIndices[2]);

					UE_LOG(LogGeometryCollectionAdapter, Log, TEXT("Deleting Tri_ID %d, Indices %d %d %d "), TriangleIndexToRemove.GetValue(), TriIndices[0], TriIndices[1], TriIndices[2]);

					// Remove this triangle from our editable mesh
					RenderingPolygonGroup.Triangles.Remove(TriangleIndexToRemove);
					GCIndices[TriangleIndexToRemove.GetValue()] = FIntVector(-1, -1, -1);
					//RenderingPolygonGroup.Triangles[TriangleIndexToRemove] = RenderingPolygonGroup.Triangles[FTriangleID2(GCIndices.Num() - 1)];
					//RenderingPolygonGroup.Triangles.Remove(FTriangleID2(GCIndices.Num() - 1));

					//// swap indices then resize group - could resize in batch once all swaps are done
					//GCIndices[TriangleIndexToRemove.GetValue()] = GCIndices[TriIndices.Num() - 1];
					//GeometryCollection->Resize(GCIndices.Num()-1, FGeometryCollection::FacesGroup);

				}

				Polygon.TriangulatedPolygonTriangleIndices.Reset();
			}
		}
	}
}

void UEditableGeometryCollectionAdapter::OnCreatePolygonGroups( const UEditableMesh* EditableMesh, const TArray<FPolygonGroupID>& PolygonGroupIDs)
{
	// #todo: implement this
	check(false);
}


void UEditableGeometryCollectionAdapter::OnSetPolygonGroupAttribute( const UEditableMesh* EditableMesh, const FPolygonGroupID PolygonGroupID, const FMeshElementAttributeData& Attribute )
{
	// #todo: implement this
	check(false);
}


void UEditableGeometryCollectionAdapter::OnDeletePolygonGroups( const UEditableMesh* EditableMesh, const TArray<FPolygonGroupID>& PolygonGroupIDs )
{
	// #todo: implement this
	check(false);
}


void UEditableGeometryCollectionAdapter::OnAssignPolygonsToPolygonGroups( const UEditableMesh* EditableMesh, const TArray<FPolygonGroupForPolygon>& PolygonGroupForPolygons )
{
	// #todo: implement this
	check(false);
}


FPolygonGroupID UEditableGeometryCollectionAdapter::GetSectionForRenderingSectionIndex( const int32 RenderingSectionIndex ) const
{
	for( const FPolygonGroupID PolygonGroupID : RenderingPolygonGroups.GetElementIDs() )
	{
		const FAdaptorPolygon2Group& RenderingPolygonGroup = RenderingPolygonGroups[ PolygonGroupID ];
		if( RenderingPolygonGroup.RenderingSectionIndex == RenderingSectionIndex )
		{
			return PolygonGroupID;
		}
	}

	return FPolygonGroupID::Invalid;
}


void UEditableGeometryCollectionAdapter::LogGeometryCollectionStats(const FString& SourceString)
{
	if (GeometryCollection != nullptr)
	{
		TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollection->GetGeometryCollection();
		if (FGeometryCollection* Collection = GeometryCollectionPtr.Get())
		{
			int32 NumVertices = Collection->NumElements(FGeometryCollection::VerticesGroup);
			int32 NumIndices = Collection->NumElements(FGeometryCollection::FacesGroup);
			int32 NumTransforms = Collection->NumElements(FGeometryCollection::TransformGroup);

			UE_LOG(LogGeometryCollectionAdapter, Log, TEXT("Geometry Collection...%s"), SourceString.GetCharArray().GetData());
			UE_LOG(LogGeometryCollectionAdapter, Log, TEXT("  Num Vertices = %d"), NumVertices);
			UE_LOG(LogGeometryCollectionAdapter, Log, TEXT("  Num Indices = %d"), NumIndices);
			UE_LOG(LogGeometryCollectionAdapter, Log, TEXT("  Num Transforms = %d"), NumTransforms);
		}
	}
}

#if WITH_EDITOR
void UEditableGeometryCollectionAdapter::GeometryHitTest(const FHitParamsIn& InParams, FHitParamsOut& OutParams)
{
	TArray<FTransform> Transforms;
	if (GeometryCollection)
	{
		TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollection->GetGeometryCollection();
		if (FGeometryCollection* Collection = GeometryCollectionPtr.Get())
		{

			GeometryCollectionAlgo::GlobalMatrices(Collection, Transforms);
			checkSlow(Collection->Transform->Num() == Transforms.Num());

			for (int PolyGroupID = 0; PolyGroupID < Transforms.Num(); PolyGroupID++)
			{

				// Shapes are in world space, but we need it in the local space of our component
				FVector ComponentSpaceLaserStart = InParams.ComponentToWorldMatrix.InverseTransformPosition(InParams.MeshEditorInteractorData.LaserStart);
				FVector ComponentSpaceLaserEnd = InParams.ComponentToWorldMatrix.InverseTransformPosition(InParams.MeshEditorInteractorData.LaserEnd);

				ComponentSpaceLaserStart = Transforms[PolyGroupID].InverseTransformPosition(ComponentSpaceLaserStart);
				ComponentSpaceLaserEnd = Transforms[PolyGroupID].InverseTransformPosition(ComponentSpaceLaserEnd);

				FVector GrabCenter = InParams.MeshEditorInteractorData.GrabberSphere.Center;
				FVector GrabW = FVector(InParams.MeshEditorInteractorData.GrabberSphere.W);
				GrabCenter = InParams.ComponentToWorldMatrix.InverseTransformPosition(GrabCenter);
				GrabW = InParams.ComponentToWorldMatrix.InverseTransformVector(GrabW);
				GrabCenter = Transforms[PolyGroupID].InverseTransformPosition(GrabCenter);
				GrabW = Transforms[PolyGroupID].InverseTransformVector(GrabW);
				FSphere ComponentSpaceGrabberSphere(GrabCenter, GrabW.X);

				FVector ComponentSpaceCameraLocation = InParams.ComponentToWorldMatrix.InverseTransformPosition(InParams.CameraToWorld.GetLocation());
				ComponentSpaceCameraLocation = Transforms[PolyGroupID].InverseTransformPosition(ComponentSpaceCameraLocation);

				EInteractorShape HitInteractorShape = EInteractorShape::Invalid;
				FVector ComponentSpaceHitLocation = FVector::ZeroVector;
				FEditableMeshElementAddress MeshElementAddress = FGeometryTests::QueryElement(
					*InParams.EditableMesh,
					InParams.InteractorShape,
					ComponentSpaceGrabberSphere,
					InParams.ComponentSpaceGrabberSphereFuzzyDistance,
					ComponentSpaceLaserStart,
					ComponentSpaceLaserEnd,
					InParams.ComponentSpaceRayFuzzyDistance,
					InParams.OnlyElementType,
					ComponentSpaceCameraLocation,
					InParams.bIsPerspectiveView,
					InParams.ComponentSpaceFuzzyDistanceScaleFactor,
					/* Out */ HitInteractorShape,
					/* Out */ ComponentSpaceHitLocation,
					PolyGroupID);

				if (MeshElementAddress.ElementType != EEditableMeshElementType::Invalid)
				{
					FVector WorldSpaceHitLocation = Transforms[PolyGroupID].TransformPosition(ComponentSpaceHitLocation);
					WorldSpaceHitLocation = InParams.ComponentToWorldMatrix.TransformPosition(WorldSpaceHitLocation);

					const float ClosestDistanceToGrabberSphere = (InParams.MeshEditorInteractorData.GrabberSphere.Center - OutParams.ClosestHoverLocation).Size();
					const float DistanceToGrabberSphere = (InParams.MeshEditorInteractorData.GrabberSphere.Center - WorldSpaceHitLocation).Size();

					const float ClosestDistanceOnRay = (InParams.MeshEditorInteractorData.LaserStart - OutParams.ClosestHoverLocation).Size();
					const float DistanceOnRay = (InParams.MeshEditorInteractorData.LaserStart - WorldSpaceHitLocation).Size();

					//DrawDebugSphere(GetWorld(), WorldSpaceHitLocation, 10.0f, 32, FColor::Red);

					// NOTE: We're preferring any grabber sphere hit over laser hits
					if (OutParams.ClosestComponent == nullptr ||
						(HitInteractorShape == EInteractorShape::GrabberSphere && DistanceToGrabberSphere < ClosestDistanceToGrabberSphere) ||
						(HitInteractorShape == EInteractorShape::Laser && DistanceOnRay < ClosestDistanceOnRay))
					{
						OutParams.ClosestComponent = InParams.HitComponent;
						OutParams.ClosestElementAddress = MeshElementAddress;
						OutParams.ClosestInteractorShape = HitInteractorShape;
						OutParams.ClosestHoverLocation = WorldSpaceHitLocation;

						// #todo: temp stop verts/edges/polys being selected by mistake
						MeshElementAddress.ElementType = EEditableMeshElementType::Fracture;
					}
				}
			}
		}
	}
}
#endif // WITH_EDITOR

