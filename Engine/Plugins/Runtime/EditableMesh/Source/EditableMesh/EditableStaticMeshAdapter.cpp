// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "EditableStaticMeshAdapter.h"
#include "EditableMesh.h"
#include "EditableMeshChanges.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "StaticMeshResources.h"
#include "PhysicsEngine/BodySetup.h"	// For collision generation
#include "ProfilingDebugging/ScopedTimers.h"	// For FAutoScopedDurationTimer
#include "EditableMeshFactory.h"
#include "MeshAttributes.h"


const FTriangleID FTriangleID::Invalid( TNumericLimits<uint32>::Max() );


UEditableStaticMeshAdapter::UEditableStaticMeshAdapter()
	: StaticMesh( nullptr ),
	  StaticMeshLODIndex( 0 ),
	  RecreateRenderStateContext(),
	  CachedBoundingBoxAndSphere( FVector::ZeroVector, FVector::ZeroVector, 0.0f )
{
}


inline void UEditableStaticMeshAdapter::EnsureIndexBufferIs32Bit()
{
	FStaticMeshLODResources& StaticMeshLOD = GetStaticMeshLOD();
	if( !StaticMeshLOD.IndexBuffer.Is32Bit() )
	{
		// Need a 32-bit index buffer
		static TArray<uint32> AllIndices;
		StaticMeshLOD.IndexBuffer.GetCopy( /* Out */ AllIndices );
		StaticMeshLOD.IndexBuffer.SetIndices( AllIndices, EIndexBufferStride::Force32Bit );
	}
}


inline void UEditableStaticMeshAdapter::UpdateIndexBufferFormatIfNeeded( const TArray<FMeshTriangle>& Triangles )
{
	const FStaticMeshLODResources& StaticMeshLOD = GetStaticMeshLOD();
	if( !StaticMeshLOD.IndexBuffer.Is32Bit() )
	{
		for( const FMeshTriangle& Triangle : Triangles )
		{
			for( int32 TriangleVertexNumber = 0; TriangleVertexNumber < 3; ++TriangleVertexNumber )
			{
				const FVertexInstanceID VertexInstanceID = Triangle.GetVertexInstanceID( TriangleVertexNumber );
				if( VertexInstanceID.GetValue() > TNumericLimits<uint16>::Max() )
				{
					EnsureIndexBufferIs32Bit();
					return;
				}
			}
		}
	}
}


void UEditableStaticMeshAdapter::InitEditableStaticMesh( UEditableMesh* EditableMesh, class UPrimitiveComponent& Component, const FEditableMeshSubMeshAddress& InitSubMeshAddress )
{
	EditableMesh->SetSubMeshAddress( InitSubMeshAddress );
	StaticMeshLODIndex = InitSubMeshAddress.LODIndex;

	// We're partial to static mesh components, here
	UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>( &Component );
	if( StaticMeshComponent != nullptr )
	{
		UStaticMesh* ComponentStaticMesh = StaticMeshComponent->GetStaticMesh();
		if( ComponentStaticMesh != nullptr && ComponentStaticMesh->HasValidRenderData() )
		{
			this->StaticMesh = ComponentStaticMesh;
			this->OriginalStaticMesh = ComponentStaticMesh;

			const FStaticMeshRenderData& StaticMeshRenderData = *StaticMesh->RenderData;
			if( StaticMeshLODIndex >= 0 && StaticMeshLODIndex < StaticMeshRenderData.LODResources.Num() )
			{
				{
					// @todo mesheditor urgent: Currently, we're disabling many of the optimized index buffers that were precomputed
					// for static meshes when they become editable.  This is just so that we don't have to keep this data up to
					// date as we perform live edits to the geometry.  Later, we should probably get this updated as we go, or 
					// lazily update the buffers when committing a final change or saving.  Without clearing these values, some
					// graphical artifacts will be visible while editing the mesh (flickering shadows, for example.)
					FStaticMeshLODResources& StaticMeshLOD = StaticMesh->RenderData->LODResources[ StaticMeshLODIndex ];
					StaticMeshLOD.bHasAdjacencyInfo = false;
					StaticMeshLOD.bHasDepthOnlyIndices = false;
					StaticMeshLOD.bHasReversedIndices = false;
					StaticMeshLOD.bHasReversedDepthOnlyIndices = false;
					StaticMeshLOD.DepthOnlyNumTriangles = 0;
				}

				const FStaticMeshLODResources& StaticMeshLOD = StaticMeshRenderData.LODResources[ StaticMeshLODIndex ];

				FMeshDescription* MeshDescription = EditableMesh->GetMeshDescription();

				TVertexAttributesRef<FVector> VertexPositions = MeshDescription->VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );

				TVertexInstanceAttributesRef<FVector> VertexInstanceNormals = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector>( MeshAttribute::VertexInstance::Normal );
				TVertexInstanceAttributesRef<FVector> VertexInstanceTangents = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector>( MeshAttribute::VertexInstance::Tangent );
				TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = MeshDescription->VertexInstanceAttributes().GetAttributesRef<float>( MeshAttribute::VertexInstance::BinormalSign );
				TVertexInstanceAttributesRef<FVector4> VertexInstanceColors = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector4>( MeshAttribute::VertexInstance::Color );
				TVertexInstanceAttributesRef<FVector2D> VertexInstanceUVs = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector2D>( MeshAttribute::VertexInstance::TextureCoordinate );

				TEdgeAttributesRef<bool> EdgeHardnesses = MeshDescription->EdgeAttributes().GetAttributesRef<bool>( MeshAttribute::Edge::IsHard );

				TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames = MeshDescription->PolygonGroupAttributes().GetAttributesRef<FName>( MeshAttribute::PolygonGroup::ImportedMaterialSlotName );
				TPolygonGroupAttributesRef<FName> PolygonGroupMaterialAssetNames = MeshDescription->PolygonGroupAttributes().GetAttributesRef<FName>( MeshAttribute::PolygonGroup::MaterialAssetName );
				TPolygonGroupAttributesRef<bool> PolygonGroupCollision = MeshDescription->PolygonGroupAttributes().GetAttributesRef<bool>( MeshAttribute::PolygonGroup::EnableCollision );
				TPolygonGroupAttributesRef<bool> PolygonGroupCastShadow = MeshDescription->PolygonGroupAttributes().GetAttributesRef<bool>( MeshAttribute::PolygonGroup::CastShadow );

				// Store off the number of texture coordinates in this mesh
				const int32 NumUVs = StaticMeshLOD.GetNumTexCoords();
				VertexInstanceUVs.SetNumIndices(NumUVs);
				EditableMesh->TextureCoordinateCount = NumUVs;

				// Vertices
				const int32 NumRenderingVertices = StaticMeshLOD.VertexBuffers.PositionVertexBuffer.GetNumVertices();
				MeshDescription->ReserveNewVertices( NumRenderingVertices );	// possibly more than necessary, but doesn't matter
				MeshDescription->ReserveNewVertexInstances( NumRenderingVertices );

				const bool bHasColor = StaticMeshLOD.VertexBuffers.ColorVertexBuffer.GetNumVertices() > 0;
				check( !bHasColor || StaticMeshLOD.VertexBuffers.ColorVertexBuffer.GetNumVertices() == StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices() );

				// @todo mesheditor cleanup: This code is very similar to the static mesh build code; try to share helper structs
				static TMultiMap<int32, int32> OverlappingRenderingVertexIndices;
				{
					OverlappingRenderingVertexIndices.Reset();

					/** Helper struct for building acceleration structures. */
					struct FIndexAndZ
					{
						float Z;
						int32 Index;

						FIndexAndZ() 
						{
						}

						/** Initialization constructor. */
						FIndexAndZ(int32 InIndex, FVector V)
						{
							Z = 0.30f * V.X + 0.33f * V.Y + 0.37f * V.Z;
							Index = InIndex;
						}
					};

					// Build a temporary array of vertex instance indices, sorted by their Z value.  This will accelerate
					// searching through to find duplicates
					static TArray<FIndexAndZ> RenderingVertexIndicesSortedByZ;
					{
						RenderingVertexIndicesSortedByZ.SetNumUninitialized( NumRenderingVertices, false );
						for( int32 RenderingVertexIndex = 0; RenderingVertexIndex < NumRenderingVertices; ++RenderingVertexIndex )
						{
							const FVector VertexPosition = StaticMeshLOD.VertexBuffers.PositionVertexBuffer.VertexPosition( RenderingVertexIndex );
							RenderingVertexIndicesSortedByZ[ RenderingVertexIndex ] = FIndexAndZ( RenderingVertexIndex, VertexPosition );
						}

						// Sort the vertices by z value
						struct FCompareIndexAndZ
						{
							FORCEINLINE bool operator()( FIndexAndZ const& A, FIndexAndZ const& B ) const { return A.Z < B.Z; }
						};
						RenderingVertexIndicesSortedByZ.Sort( FCompareIndexAndZ() );
					}

					// Search for duplicates, quickly!
					const float ComparisonThreshold = KINDA_SMALL_NUMBER;	// @todo mesheditor: Tweak "weld" threshold
					for( int32 RenderingVertexIterA = 0; RenderingVertexIterA < NumRenderingVertices; ++RenderingVertexIterA )
					{
						// only need to search forward, since we add pairs both ways
						for( int32 RenderingVertexIterB = RenderingVertexIterA + 1; RenderingVertexIterB < NumRenderingVertices; ++RenderingVertexIterB )
						{
							if( FMath::Abs( RenderingVertexIndicesSortedByZ[ RenderingVertexIterB ].Z - RenderingVertexIndicesSortedByZ[ RenderingVertexIterA ].Z ) > ComparisonThreshold )
							{
								break; // can't be any more dups
							}

							const int32 RenderingVertexIndexA = RenderingVertexIndicesSortedByZ[ RenderingVertexIterA ].Index;
							const int32 RenderingVertexIndexB = RenderingVertexIndicesSortedByZ[ RenderingVertexIterB ].Index;

							const FVector VertexPositionA = StaticMeshLOD.VertexBuffers.PositionVertexBuffer.VertexPosition( RenderingVertexIndexA );
							const FVector VertexPositionB = StaticMeshLOD.VertexBuffers.PositionVertexBuffer.VertexPosition( RenderingVertexIndexB );

							if( VertexPositionA.Equals( VertexPositionB, ComparisonThreshold ) )
							{
								OverlappingRenderingVertexIndices.Add( RenderingVertexIndexA, RenderingVertexIndexB );
								OverlappingRenderingVertexIndices.Add( RenderingVertexIndexB, RenderingVertexIndexA );
							}
						}
					}
				}

				// We'll now make sure we have an editable mesh vertex created for every uniquely-positioned vertex instance.
				// Note that it's important that we process all vertices, not only the vertices that are referenced by triangles
				// in the index buffer, because we properly support meshes with vertices that are not yet connected to any
				// polygons.  These vertices will simply not have editable mesh polygons or edges connected to them, but will
				// still be interactable in the editor.
				for( int32 RenderingVertexIndex = 0; RenderingVertexIndex < NumRenderingVertices; ++RenderingVertexIndex )
				{
					const FVector VertexPosition = StaticMeshLOD.VertexBuffers.PositionVertexBuffer.VertexPosition( RenderingVertexIndex );
					const FVertexInstanceID VertexInstanceID( RenderingVertexIndex );

					// Check to see if we already have this vertex
					bool bAlreadyHaveVertexForPosition = false;
					{
						static TArray<int32> ThisRenderingVertexOverlaps;
						ThisRenderingVertexOverlaps.Reset();
						OverlappingRenderingVertexIndices.MultiFind( RenderingVertexIndex, /* Out */ ThisRenderingVertexOverlaps );

						for( int32 OverlappingRenderingVertexIter = 0; OverlappingRenderingVertexIter < ThisRenderingVertexOverlaps.Num(); ++OverlappingRenderingVertexIter )
						{
							const int32 OverlappingRenderingVertexIndex = ThisRenderingVertexOverlaps[ OverlappingRenderingVertexIter ];
							const FVertexInstanceID OverlappingVertexInstanceID( OverlappingRenderingVertexIndex );

							// If the overlapping vertex instance index is smaller than our current index, we can safely assume that
							// we've already processed this vertex position and created an editable mesh vertex for it.
							if( OverlappingRenderingVertexIndex < RenderingVertexIndex )
							{
								check( MeshDescription->IsVertexInstanceValid( OverlappingVertexInstanceID ) );
								const FVertexID ExistingVertexID = MeshDescription->GetVertexInstanceVertex( OverlappingVertexInstanceID );

								// We already have a unique editable vertex for this vertex instance position, so link them!
								MeshDescription->CreateVertexInstanceWithID( VertexInstanceID, ExistingVertexID );

								bAlreadyHaveVertexForPosition = true;

								break;
							}
						}
					}

					if( !bAlreadyHaveVertexForPosition )
					{
						const FVertexID NewVertexID = MeshDescription->CreateVertex();
						VertexPositions[ NewVertexID ] = VertexPosition;

						MeshDescription->CreateVertexInstanceWithID( VertexInstanceID, NewVertexID );
					}

					// Populate the vertex instance attributes
					{
						const FVector Normal = StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ( RenderingVertexIndex );
						const FVector Tangent = StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentX( RenderingVertexIndex );
						const FVector Binormal = StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentY( RenderingVertexIndex );
						const float BinormalSign = GetBasisDeterminantSign(Tangent, Binormal, Normal);
						const FLinearColor Color = bHasColor ? FLinearColor( StaticMeshLOD.VertexBuffers.ColorVertexBuffer.VertexColor( RenderingVertexIndex ) ) : FLinearColor::White;

						VertexInstanceNormals[ VertexInstanceID ] = Normal;
						VertexInstanceTangents[ VertexInstanceID ] = Tangent;
						VertexInstanceBinormalSigns[ VertexInstanceID ] = BinormalSign;
						VertexInstanceColors[ VertexInstanceID ] = Color;
						for( int32 UVIndex = 0; UVIndex < NumUVs; ++UVIndex )
						{
							VertexInstanceUVs.Set( VertexInstanceID, UVIndex, StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV( RenderingVertexIndex, UVIndex ) );
						}
					}
				}


				const FIndexArrayView RenderingIndices = StaticMeshLOD.IndexBuffer.GetArrayView();

				const uint32 NumSections = StaticMeshLOD.Sections.Num();
				MeshDescription->ReserveNewPolygonGroups( NumSections );

				// Add all polygon groups from the static mesh sections
				for( uint32 RenderingSectionIndex = 0; RenderingSectionIndex < NumSections; ++RenderingSectionIndex )
				{
					const FStaticMeshSection& RenderingSection = StaticMeshLOD.Sections[ RenderingSectionIndex ];
					const FStaticMaterial& StaticMaterial = StaticMesh->StaticMaterials[ RenderingSection.MaterialIndex ];
					UMaterialInterface* MaterialInterface = StaticMaterial.MaterialInterface;

					// Create a new polygon group
					const FPolygonGroupID NewPolygonGroupID = MeshDescription->CreatePolygonGroup();
					PolygonGroupImportedMaterialSlotNames[ NewPolygonGroupID ] = StaticMaterial.ImportedMaterialSlotName;
					PolygonGroupMaterialAssetNames[ NewPolygonGroupID ] = FName( *MaterialInterface->GetPathName() );
					PolygonGroupCollision[ NewPolygonGroupID ] = RenderingSection.bEnableCollision;
					PolygonGroupCastShadow[ NewPolygonGroupID ] = RenderingSection.bCastShadow;

					// Create a rendering polygon group for holding the triangulated data and references to the static mesh rendering section.
					// This is indexed by the same FPolygonGroupID as the PolygonGroups.
					RenderingPolygonGroups.Insert( NewPolygonGroupID );
					FRenderingPolygonGroup& NewRenderingPolygonGroup = RenderingPolygonGroups[ NewPolygonGroupID ];

					const uint32 NumSectionTriangles = RenderingSection.NumTriangles;
					NewRenderingPolygonGroup.Triangles.Reserve( NumSectionTriangles );
					NewRenderingPolygonGroup.MaxTriangles = NumSectionTriangles;
					NewRenderingPolygonGroup.RenderingSectionIndex = RenderingSectionIndex;
					NewRenderingPolygonGroup.MaterialIndex = RenderingSection.MaterialIndex;

					MeshDescription->ReserveNewPolygons( NumSectionTriangles );
					MeshDescription->ReserveNewEdges( NumSectionTriangles * 3 );	// more than required, but not a problem

					for( uint32 SectionTriangleIndex = 0; SectionTriangleIndex < NumSectionTriangles; ++SectionTriangleIndex )
					{
						const uint32 RenderingTriangleFirstVertexIndex = FRenderingPolygonGroup::TriangleIndexToRenderingTriangleFirstIndex( RenderingSection, FTriangleID( SectionTriangleIndex ) );

						FVertexInstanceID TriangleVertexInstanceIDs[ 3 ];
						FVertexID TriangleVertexIDs[ 3 ];
						for( uint32 TriangleVertexIndex = 0; TriangleVertexIndex < 3; ++TriangleVertexIndex )
						{
							TriangleVertexInstanceIDs[ TriangleVertexIndex ] = FVertexInstanceID( RenderingIndices[ RenderingTriangleFirstVertexIndex + TriangleVertexIndex ] );
							TriangleVertexIDs[ TriangleVertexIndex ] = MeshDescription->GetVertexInstanceVertex( TriangleVertexInstanceIDs[ TriangleVertexIndex ] );
						}

						// Make sure we have a valid triangle.  The triangle can be invalid because at least two if it's vertex indices 
						// point to the exact same vertex.  The triangle is degenerate.  This can happen due to us welding the overlapping 
						// vertices because they were either extremely close to each other (or exactly overlapping.)  We'll ignore this triangle.
						const bool bIsValidTriangle =
							TriangleVertexIDs[ 0 ] != TriangleVertexIDs[ 1 ] &&
							TriangleVertexIDs[ 1 ] != TriangleVertexIDs[ 2 ] &&
							TriangleVertexIDs[ 2 ] != TriangleVertexIDs[ 0 ];

						if( bIsValidTriangle )
						{
							FEdgeID NewEdgeIDs[ 3 ];

							// Connect edges
							{
								// Add the edges of this triangle
								for( uint32 TriangleEdgeNumber = 0; TriangleEdgeNumber < 3; ++TriangleEdgeNumber )
								{
									const FVertexID VertexID0 = TriangleVertexIDs[ TriangleEdgeNumber ];
									const FVertexID VertexID1 = TriangleVertexIDs[ ( TriangleEdgeNumber + 1 ) % 3 ];
									
									FEdgeID NewEdgeID = MeshDescription->GetVertexPairEdge( VertexID0, VertexID1 );

									if( NewEdgeID == FEdgeID::Invalid )
									{
										NewEdgeID = MeshDescription->CreateEdge( VertexID0, VertexID1 );
									}

									NewEdgeIDs[ TriangleEdgeNumber ] = NewEdgeID;
								}
							}

							// Static meshes only support triangles, so there's no need to triangulate anything yet.  We'll make both
							// a triangle and a polygon here.
							const FTriangleID NewTriangleID = FTriangleID( SectionTriangleIndex );

							NewRenderingPolygonGroup.Triangles.Insert( NewTriangleID );
							FMeshTriangle& NewTriangle = NewRenderingPolygonGroup.Triangles[ NewTriangleID ];

							static TArray<FMeshDescription::FContourPoint> Perimeter;
							Perimeter.Reset( 3 );
							Perimeter.AddUninitialized( 3 );
							for( uint32 TriangleVertexIndex = 0; TriangleVertexIndex < 3; ++TriangleVertexIndex )
							{
								Perimeter[ TriangleVertexIndex ].VertexInstanceID = TriangleVertexInstanceIDs[ TriangleVertexIndex ];
								Perimeter[ TriangleVertexIndex ].EdgeID = NewEdgeIDs[ TriangleVertexIndex ];

								NewTriangle.SetVertexInstanceID( TriangleVertexIndex, TriangleVertexInstanceIDs[ TriangleVertexIndex ] );
							}

							// Insert a polygon into the mesh
							const FPolygonID NewPolygonID = MeshDescription->CreatePolygon( NewPolygonGroupID, Perimeter );

							// Create a rendering polygon mirror, indexed by the same ID
							RenderingPolygons.Insert( NewPolygonID );
							FRenderingPolygon& NewRenderingPolygon = RenderingPolygons[ NewPolygonID ];
							NewRenderingPolygon.PolygonGroupID = NewPolygonGroupID;
							NewRenderingPolygon.TriangulatedPolygonTriangleIndices.Add( NewTriangleID );

							// Add triangle to polygon triangulation array
							MeshDescription->GetPolygonTriangles( NewPolygonID ).Add( NewTriangle );
						}
						else
						{
							// Triangle was not valid.  This will result in an empty entry in our Triangles sparse array.  Luckily,
							// the triangle is already degenerate so we don't need to change anything.  This triangle index will be
							// re-used if a new triangle needs to be created during editing
							// @todo mesheditor: This can cause vertex instances to be orphaned.  Should we delete them?
						}
					}
				}

				// Determine edge hardnesses
				MeshDescription->DetermineEdgeHardnessesFromVertexInstanceNormals();

				// Determine UV seams
				if( NumUVs > 0 )
				{
					MeshDescription->DetermineUVSeamsFromUVs( 0 );
				}

				// Cache polygon tangent bases
				static TArray<FPolygonID> PolygonIDs;
				PolygonIDs.Reset();
				for( const FPolygonID PolygonID : EditableMesh->GetMeshDescription()->Polygons().GetElementIDs() )
				{
					PolygonIDs.Add( PolygonID );
				}

				EditableMesh->GeneratePolygonTangentsAndNormals( PolygonIDs );

#if 0
				// Test tangent generation
				static TArray<FPolygonID> AllPolygons;
				AllPolygons.Reset();
				for( int32 PolygonIndex = 0; PolygonIndex < EditableMesh->Polygons.GetMaxIndex(); ++PolygonIndex )
				{
					if( EditableMesh->Polygons.IsAllocated( PolygonIndex ) )
					{
						EditableMesh->PolygonsPendingNewTangentBasis.Add( FPolygonID( PolygonIndex ) );
					}
				}

				EditableMesh->GenerateTangentsAndNormals();
#endif
			}
		}
	}

	this->CachedBoundingBoxAndSphere = StaticMesh->GetBounds();

#if EDITABLE_MESH_USE_OPENSUBDIV
	EditableMesh->RefreshOpenSubdiv();
#endif
	EditableMesh->RebuildOctree();
}


void UEditableStaticMeshAdapter::InitFromBlankStaticMesh( UEditableMesh* EditableMesh, UStaticMesh& InStaticMesh )
{
	StaticMesh = &InStaticMesh;
}


void UEditableStaticMeshAdapter::InitializeFromEditableMesh( const UEditableMesh* EditableMesh )
{
	// Get the static mesh from the editable mesh submesh address
	const FEditableMeshSubMeshAddress& SubMeshAddress = EditableMesh->GetSubMeshAddress();
	StaticMesh = static_cast<UStaticMesh*>( SubMeshAddress.MeshObjectPtr );

	// @todo mesheditor instancing: sort this out
	OriginalStaticMesh = nullptr;

	// Always targets LOD0 at the moment
	StaticMeshLODIndex = 0;

	RenderingPolygons.Reset();
	RenderingPolygonGroups.Reset();

	const FMeshDescription* MeshDescription = EditableMesh->GetMeshDescription();

	// Create all the required rendering polygon groups (initialized to 'empty', each with a unique rendering section index)
	int32 RenderingSectionIndex = 0;
	for( const FPolygonGroupID PolygonGroupID : MeshDescription->PolygonGroups().GetElementIDs() )
	{
		RenderingPolygonGroups.Insert( PolygonGroupID );
		FRenderingPolygonGroup& RenderingPolygonGroup = RenderingPolygonGroups[ PolygonGroupID ];
		RenderingPolygonGroup.RenderingSectionIndex = RenderingSectionIndex;

		const FName SlotName = MeshDescription->PolygonGroupAttributes().GetAttribute<FName>( PolygonGroupID, MeshAttribute::PolygonGroup::ImportedMaterialSlotName );
		RenderingPolygonGroup.MaterialIndex = StaticMesh->StaticMaterials.IndexOfByPredicate(
			[ &SlotName ]( const FStaticMaterial& StaticMaterial )
			{
				return StaticMaterial.ImportedMaterialSlotName == SlotName;
			}
		);
		RenderingPolygonGroup.MaxTriangles = 0;

		RenderingSectionIndex++;
	}

	// Go through all the polygons, adding their triangles to the rendering polygon group
	for( const FPolygonID PolygonID : MeshDescription->Polygons().GetElementIDs() )
	{
		const FPolygonGroupID PolygonGroupID = MeshDescription->GetPolygonPolygonGroup( PolygonID );
		FRenderingPolygonGroup& RenderingPolygonGroup = RenderingPolygonGroups[ PolygonGroupID ];

		RenderingPolygons.Insert( PolygonID );
		FRenderingPolygon& RenderingPolygon = RenderingPolygons[ PolygonID ];
		RenderingPolygon.PolygonGroupID = PolygonGroupID;

		const TArray<FMeshTriangle>& Triangles = MeshDescription->GetPolygonTriangles( PolygonID );
		for( const FMeshTriangle& Triangle : Triangles )
		{
			const FTriangleID TriangleID = RenderingPolygonGroup.Triangles.Add( Triangle );
			RenderingPolygon.TriangulatedPolygonTriangleIndices.Add( TriangleID );
		}

		RenderingPolygonGroup.MaxTriangles += Triangles.Num();
	}

}


void UEditableStaticMeshAdapter::OnRebuildRenderMesh( const UEditableMesh* EditableMesh )
{
	// @todo mesheditor urgent subdiv: Saw some editable mesh corruption artifacts when testing subDs in VR

	check( RecreateRenderStateContext.IsValid() );

	FStaticMeshLODResources& StaticMeshLOD = GetStaticMeshLOD();

	// Build new vertex buffers
	static TArray< FStaticMeshBuildVertex > StaticMeshBuildVertices;
	StaticMeshBuildVertices.Reset();

	static TArray< uint32 > IndexBuffer;
	IndexBuffer.Reset();

	bool bHasColor = false;

	const FMeshDescription* MeshDescription = EditableMesh->GetMeshDescription();
	check( MeshDescription );

	TPolygonGroupAttributesConstRef<FName> PolygonGroupImportedMaterialSlotNames = MeshDescription->PolygonGroupAttributes().GetAttributesRef<FName>( MeshAttribute::PolygonGroup::ImportedMaterialSlotName );
	TPolygonGroupAttributesConstRef<bool> PolygonGroupCollision = MeshDescription->PolygonGroupAttributes().GetAttributesRef<bool>( MeshAttribute::PolygonGroup::EnableCollision );
	TPolygonGroupAttributesConstRef<bool> PolygonGroupCastShadow = MeshDescription->PolygonGroupAttributes().GetAttributesRef<bool>( MeshAttribute::PolygonGroup::CastShadow );

	const int32 NumPolygonGroups = MeshDescription->PolygonGroups().Num();
	StaticMeshLOD.Sections.Empty( NumPolygonGroups );

	if( EditableMesh->IsPreviewingSubdivisions() )
	{
		check( EditableMesh->GetSubdivisionCount() > 0 );

		// @todo mesheditor subdiv: Only 2 UVs supported for now, just to avoid having to use a dynamic array per vertex; needs a new data layout, probably (SoA)
		const int32 SubdivMeshTextureCoordinateCount = FMath::Min( EditableMesh->GetTextureCoordinateCount(), 2 );

		// The Sections sparse array mirrors the SubdivisionLimitData sections array;
		// iterate through it in parallel in order to get the material index and other section properties
		check( NumPolygonGroups == EditableMesh->SubdivisionLimitData.Sections.Num() );

		int32 SectionNumber = 0;
		for( const FPolygonGroupID PolygonGroupID : MeshDescription->PolygonGroups().GetElementIDs() )
		{
			const FSubdivisionLimitSection& SubdivisionSection = EditableMesh->SubdivisionLimitData.Sections[ SectionNumber ];

			const int32 SectionTriangleCount = SubdivisionSection.SubdividedQuads.Num() * 2;

			// @todo mesheditor subdiv perf: Ideally, if no topology changed we can just fill vertex data and not touch index buffers
			const int32 FirstSectionVertexIndex = StaticMeshBuildVertices.Num();
			StaticMeshBuildVertices.AddUninitialized( SectionTriangleCount * 3 );

			const int32 FirstIndexInSection = IndexBuffer.Num();
			IndexBuffer.Reserve( IndexBuffer.Num() + SectionTriangleCount * 3 );

			// Create new rendering section
			StaticMeshLOD.Sections.Add( FStaticMeshSection() );
			FStaticMeshSection& StaticMeshSection = StaticMeshLOD.Sections.Last();

			StaticMeshSection.FirstIndex = FirstIndexInSection;
			StaticMeshSection.NumTriangles = SectionTriangleCount;
			StaticMeshSection.MinVertexIndex = FirstSectionVertexIndex;
			StaticMeshSection.MaxVertexIndex = FirstSectionVertexIndex + SectionTriangleCount * 3;

			const int32 MaterialIndex = StaticMesh->GetMaterialIndexFromImportedMaterialSlotName( PolygonGroupImportedMaterialSlotNames[ PolygonGroupID ] );
			check( MaterialIndex != INDEX_NONE );

			StaticMeshSection.MaterialIndex = MaterialIndex;
			StaticMeshSection.bEnableCollision = PolygonGroupCollision[ PolygonGroupID ];
			StaticMeshSection.bCastShadow = PolygonGroupCastShadow[ PolygonGroupID ];

			// Fill vertices
			int32 NextVertexIndex = FirstSectionVertexIndex;
			for( int32 QuadNumber = 0; QuadNumber < SubdivisionSection.SubdividedQuads.Num(); ++QuadNumber )
			{
				const FSubdividedQuad& SubdividedQuad = SubdivisionSection.SubdividedQuads[ QuadNumber ];

				// @todo mesheditor subdiv debug
// 				GWarn->Logf( TEXT( "Q%i V%i: U:%0.2f, V:%0.2f" ), QuadNumber, 0, SubdividedQuad.QuadVertex0.TextureCoordinate0.X, SubdividedQuad.QuadVertex0.TextureCoordinate0.Y );
// 				GWarn->Logf( TEXT( "Q%i V%i: U:%0.2f, V:%0.2f" ), QuadNumber, 1, SubdividedQuad.QuadVertex1.TextureCoordinate0.X, SubdividedQuad.QuadVertex1.TextureCoordinate0.Y );
// 				GWarn->Logf( TEXT( "Q%i V%i: U:%0.2f, V:%0.2f" ), QuadNumber, 2, SubdividedQuad.QuadVertex2.TextureCoordinate0.X, SubdividedQuad.QuadVertex2.TextureCoordinate0.Y );
// 				GWarn->Logf( TEXT( "Q%i V%i: U:%0.2f, V:%0.2f" ), QuadNumber, 3, SubdividedQuad.QuadVertex3.TextureCoordinate0.X, SubdividedQuad.QuadVertex3.TextureCoordinate0.Y );

				for( int32 TriangleNumber = 0; TriangleNumber < 2; ++TriangleNumber )
				{
					for( int32 TriangleVertexNumber = 0; TriangleVertexNumber < 3; ++TriangleVertexNumber )
					{
						int32 QuadVertexNumber;
						if( TriangleNumber == 0 )
						{
							QuadVertexNumber = ( TriangleVertexNumber == 0 ) ? 0 : ( TriangleVertexNumber == 1 ? 2 : 1 );
						}
						else
						{
							QuadVertexNumber = ( TriangleVertexNumber == 0 ) ? 0 : ( TriangleVertexNumber == 1 ? 3 : 2 );
						}

						const FSubdividedQuadVertex& QuadVertex = SubdividedQuad.GetQuadVertex( QuadVertexNumber );

						const FVector VertexPosition = EditableMesh->SubdivisionLimitData.VertexPositions[ QuadVertex.VertexPositionIndex ];

						FStaticMeshBuildVertex& StaticMeshVertex = StaticMeshBuildVertices[ NextVertexIndex ];
						StaticMeshVertex.Position = VertexPosition;
						StaticMeshVertex.TangentX = QuadVertex.VertexTangent;
						StaticMeshVertex.TangentY = QuadVertex.VertexBinormalSign * FVector::CrossProduct( QuadVertex.VertexNormal, QuadVertex.VertexTangent );
						StaticMeshVertex.TangentZ = QuadVertex.VertexNormal;

						for( int32 UVIndex = 0; UVIndex < SubdivMeshTextureCoordinateCount; ++UVIndex )
						{
							StaticMeshVertex.UVs[ UVIndex ] = *( &QuadVertex.TextureCoordinate0 + UVIndex );
						}

						StaticMeshVertex.Color = QuadVertex.VertexColor;
						if( StaticMeshVertex.Color != FColor::White )
						{
							bHasColor = true;
						}

						IndexBuffer.Add( NextVertexIndex++ );
					}
				}
			}

			++SectionNumber;
		}
	}
	else
	{
		// set up vertex buffer elements
		StaticMeshBuildVertices.SetNum( MeshDescription->VertexInstances().GetArraySize() );

		TVertexAttributesConstRef<FVector> VertexPositions = MeshDescription->VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );
		TVertexInstanceAttributesConstRef<FVector> VertexInstanceNormals = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector>( MeshAttribute::VertexInstance::Normal );
		TVertexInstanceAttributesConstRef<FVector> VertexInstanceTangents = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector>( MeshAttribute::VertexInstance::Tangent );
		TVertexInstanceAttributesConstRef<float> VertexInstanceBinormalSigns = MeshDescription->VertexInstanceAttributes().GetAttributesRef<float>( MeshAttribute::VertexInstance::BinormalSign );
		TVertexInstanceAttributesConstRef<FVector4> VertexInstanceColors = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector4>( MeshAttribute::VertexInstance::Color );
		TVertexInstanceAttributesConstRef<FVector2D> VertexInstanceUVs = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector2D>( MeshAttribute::VertexInstance::TextureCoordinate );

		for( const FVertexInstanceID VertexInstanceID : MeshDescription->VertexInstances().GetElementIDs() )
		{
			const FLinearColor VertexInstanceColor( VertexInstanceColors[ VertexInstanceID ] );
			if( VertexInstanceColor != FLinearColor::White )
			{
				bHasColor = true;
			}

			// @todo mesheditor: This can be much better optimized now; some of these attribute buffers can be copied in bulk directly to the render buffers.
			FStaticMeshBuildVertex& StaticMeshVertex = StaticMeshBuildVertices[ VertexInstanceID.GetValue() ];

			StaticMeshVertex.Position = VertexPositions[ MeshDescription->GetVertexInstanceVertex( VertexInstanceID ) ];
			StaticMeshVertex.TangentX = VertexInstanceTangents[ VertexInstanceID ];
			StaticMeshVertex.TangentY = FVector::CrossProduct( VertexInstanceNormals[ VertexInstanceID ], VertexInstanceTangents[ VertexInstanceID ] ).GetSafeNormal() * VertexInstanceBinormalSigns[ VertexInstanceID ];
			StaticMeshVertex.TangentZ = VertexInstanceNormals[ VertexInstanceID ];
			StaticMeshVertex.Color = VertexInstanceColor.ToFColor( true );
			for( int32 UVIndex = 0; UVIndex < VertexInstanceUVs.GetNumIndices(); ++UVIndex )
			{
				StaticMeshVertex.UVs[ UVIndex ] = VertexInstanceUVs.Get( VertexInstanceID, UVIndex );
			}
		}

		// Set up index buffer

		for( const FPolygonGroupID PolygonGroupID : MeshDescription->PolygonGroups().GetElementIDs() )
		{
			// Preserve the Sections order by finding the RenderingPolygonGroup with the RenderingSectionIndex that matches the PolygonGroupID
			FPolygonGroupID RenderingGroupID = PolygonGroupID;
			for ( const FPolygonGroupID RenderingPolygonGroupID : RenderingPolygonGroups.GetElementIDs() )
			{
				if ( RenderingPolygonGroups[ RenderingPolygonGroupID ].RenderingSectionIndex == PolygonGroupID.GetValue() )
				{
					RenderingGroupID = RenderingPolygonGroupID;
					break;
				}
			}

			FRenderingPolygonGroup& RenderingPolygonGroup = RenderingPolygonGroups[ RenderingGroupID ];

			// Create new rendering section
			StaticMeshLOD.Sections.Add( FStaticMeshSection() );
			FStaticMeshSection& StaticMeshSection = StaticMeshLOD.Sections.Last();

			StaticMeshSection.FirstIndex = IndexBuffer.Num();
			StaticMeshSection.NumTriangles = RenderingPolygonGroup.Triangles.GetArraySize();
			check( RenderingPolygonGroup.Triangles.GetArraySize() <= RenderingPolygonGroup.MaxTriangles );

			const int32 MaterialIndex = StaticMesh->GetMaterialIndexFromImportedMaterialSlotName( PolygonGroupImportedMaterialSlotNames[ PolygonGroupID ] );
			check( MaterialIndex != INDEX_NONE );
			StaticMeshSection.MaterialIndex = MaterialIndex;
			StaticMeshSection.bEnableCollision = PolygonGroupCollision[ PolygonGroupID ];
			StaticMeshSection.bCastShadow = PolygonGroupCastShadow[ PolygonGroupID ];


			if( RenderingPolygonGroup.Triangles.Num() > 0 )
			{
				IndexBuffer.Reserve( IndexBuffer.Num() + RenderingPolygonGroup.Triangles.GetArraySize() * 3 );
				uint32 MinIndex = TNumericLimits< uint32 >::Max();
				uint32 MaxIndex = TNumericLimits< uint32 >::Min();

				// Find the first valid vertex instance index, so that we have a value we can use for our degenerates
				check( RenderingPolygonGroup.Triangles.Num() > 0 );
				const FVertexInstanceID FirstValidRenderingID = RenderingPolygonGroup.Triangles[ RenderingPolygonGroup.Triangles.GetFirstValidID() ].GetVertexInstanceID( 0 );

				for( int32 TriangleIndex = 0; TriangleIndex < RenderingPolygonGroup.Triangles.GetArraySize(); ++TriangleIndex )
				{
					const FTriangleID TriangleID( TriangleIndex );
					if( RenderingPolygonGroup.Triangles.IsValid( TriangleID ) )
					{
						const FMeshTriangle& Triangle = RenderingPolygonGroup.Triangles[ TriangleID ];
						for( int32 TriVert = 0; TriVert < 3; ++TriVert )
						{
							const uint32 RenderingVertexIndex = Triangle.GetVertexInstanceID( TriVert ).GetValue();
							IndexBuffer.Add( RenderingVertexIndex );
							MinIndex = FMath::Min( MinIndex, RenderingVertexIndex );
							MaxIndex = FMath::Max( MaxIndex, RenderingVertexIndex );
						}
					}
					else
					{
						IndexBuffer.Add( FirstValidRenderingID.GetValue() );
						IndexBuffer.Add( FirstValidRenderingID.GetValue() );
						IndexBuffer.Add( FirstValidRenderingID.GetValue() );
					}
				}

				StaticMeshSection.MinVertexIndex = MinIndex;
				StaticMeshSection.MaxVertexIndex = MaxIndex;

				// Add any index buffer padding.
				// This can be necessary if we have just loaded an editable mesh which had a MaxTriangles count in the editable mesh section
				// greater than the sparse array max size (i.e. an extra gap had been reserved for tris).
				const int32 IndexBufferPadding = RenderingPolygonGroup.MaxTriangles - RenderingPolygonGroup.Triangles.GetArraySize();
				if( IndexBufferPadding > 0 )
				{
					IndexBuffer.AddZeroed( IndexBufferPadding * 3 );
				}
			}
			else
			{
				// No triangles in this section
				StaticMeshSection.MinVertexIndex = 0;
				StaticMeshSection.MaxVertexIndex = 0;
			}
		}
	}

	// Figure out which index buffer stride we need
	bool bNeeds32BitIndices = false;
	for( const FStaticMeshSection& StaticMeshSection : StaticMeshLOD.Sections )
	{
		if( StaticMeshSection.MaxVertexIndex > TNumericLimits<uint16>::Max() )
		{
			bNeeds32BitIndices = true;
		}
	}
	const EIndexBufferStride::Type IndexBufferStride = bNeeds32BitIndices ? EIndexBufferStride::Force32Bit : EIndexBufferStride::Force16Bit;

	StaticMeshLOD.VertexBuffers.PositionVertexBuffer.Init( StaticMeshBuildVertices );
	StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.Init( StaticMeshBuildVertices, EditableMesh->GetTextureCoordinateCount() );

	if( bHasColor )
	{
		StaticMeshLOD.VertexBuffers.ColorVertexBuffer.Init( StaticMeshBuildVertices );
	}
	else
	{
		StaticMeshLOD.VertexBuffers.ColorVertexBuffer.InitFromSingleColor( FColor::White, StaticMeshBuildVertices.Num() );
	}

	StaticMeshLOD.IndexBuffer.SetIndices( IndexBuffer, IndexBufferStride );

	// @todo mesheditor: support the other index buffer types
	StaticMeshLOD.ReversedIndexBuffer.SetIndices( TArray< uint32 >(), IndexBufferStride );
	StaticMeshLOD.DepthOnlyIndexBuffer.SetIndices( TArray< uint32 >(), IndexBufferStride );
	StaticMeshLOD.ReversedDepthOnlyIndexBuffer.SetIndices( TArray< uint32 >(), IndexBufferStride );
	StaticMeshLOD.WireframeIndexBuffer.SetIndices( TArray< uint32 >(), IndexBufferStride );
	StaticMeshLOD.AdjacencyIndexBuffer.SetIndices( TArray< uint32 >(), IndexBufferStride );

	StaticMeshLOD.bHasAdjacencyInfo = false;
	StaticMeshLOD.bHasDepthOnlyIndices = false;
	StaticMeshLOD.bHasReversedIndices = false;
	StaticMeshLOD.bHasReversedDepthOnlyIndices = false;
	StaticMeshLOD.DepthOnlyNumTriangles = 0;
}



void UEditableStaticMeshAdapter::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	Ar.UsingCustomVersion( FEditableMeshCustomVersion::GUID );

	Ar << RenderingPolygons;
	Ar << RenderingPolygonGroups;
}


void UEditableStaticMeshAdapter::OnStartModification( const UEditableMesh* EditableMesh, const EMeshModificationType MeshModificationType, const EMeshTopologyChange MeshTopologyChange )
{
	// @todo mesheditor undo: We're not using traditional transactions to undo mesh changes yet, but we still want to dirty the mesh package
	// Also, should we even need the Initializing type? Should we not wait for the first modification before dirtying the package?

	StaticMesh->MarkPackageDirty();
}


void UEditableStaticMeshAdapter::OnRebuildRenderMeshStart( const UEditableMesh* EditableMesh, const bool bInvalidateLighting )
{
	// We may already have a lock on the rendering resources, if it wasn't released the last time we called EndModification()
	// on the mesh.  This is only the case when rolling back preview changes for a mesh, because we're guaranteed to apply another
	// modification to the same mesh in the very same frame.  So we can avoid having to update the GPU resources twice in one frame.
	if( !RecreateRenderStateContext.IsValid() )
	{
		// We're changing the mesh itself, so ALL static mesh components in the scene will need
		// to be unregistered for this (and reregistered afterwards.)
		const bool bRefreshBounds = true;
		RecreateRenderStateContext = MakeShareable( new FStaticMeshComponentRecreateRenderStateContext( StaticMesh, bInvalidateLighting, bRefreshBounds ) );

		// Release the static mesh's resources.
		StaticMesh->ReleaseResources();

		// Flush the resource release commands to the rendering thread to ensure that the build doesn't occur while a resource is still
		// allocated, and potentially accessing the UStaticMesh.
		StaticMesh->ReleaseResourcesFence.Wait();
	}
}


void UEditableStaticMeshAdapter::OnEndModification( const UEditableMesh* EditableMesh )
{
	// nothing to do here
}


void UEditableStaticMeshAdapter::OnRebuildRenderMeshFinish( const UEditableMesh* EditableMesh, const bool bRebuildBoundsAndCollision, const bool bIsPreviewRollback )
{
	if (!bIsPreviewRollback)
	{
		StaticMesh->InitResources();
	}

	UpdateBounds( EditableMesh, bRebuildBoundsAndCollision );
	
	if( bRebuildBoundsAndCollision )
	{
		UpdateCollision();
	}

	// When rolling back preview changes, we'll skip updating GPU resources.  This is because we're guaranteed to get either 'interim' or
	// 'final' changes to the same mesh later this frame, and we want to avoid updating the GPU resources twice in one frame.
	if( !bIsPreviewRollback )
	{
		// NOTE: This can call InvalidateLightingCache() on all components using this mesh, causing Modify() to be 
		// called on those components!  Just something to be aware of when EndModification() is called within
		// an undo transaction.
		RecreateRenderStateContext.Reset();
	}
}


void UEditableStaticMeshAdapter::OnReindexElements( const UEditableMesh* EditableMesh, const FElementIDRemappings& Remappings )
{
	RenderingPolygons.Remap( Remappings.NewPolygonIndexLookup );
	RenderingPolygonGroups.Remap( Remappings.NewPolygonGroupIndexLookup );

	// Always compact the rendering triangles
	for( const FPolygonGroupID PolygonGroupID : RenderingPolygonGroups.GetElementIDs() )
	{
		FRenderingPolygonGroup& RenderingPolygonGroup = RenderingPolygonGroups[ PolygonGroupID ];
//		const FMeshPolygonGroup& PolygonGroup = EditableMesh->GetMeshDescription()->GetPolygonGroup( PolygonGroupID );

		TSparseArray<int32> TriangleRemappings;
		RenderingPolygonGroup.Triangles.Compact( TriangleRemappings );

		for( const FTriangleID TriangleID : RenderingPolygonGroup.Triangles.GetElementIDs() )
		{
			FMeshTriangle& Triangle = RenderingPolygonGroup.Triangles[ TriangleID ];
			for( int32 TriangleVertexNumber = 0; TriangleVertexNumber < 3; ++TriangleVertexNumber )
			{
				const FVertexInstanceID OriginalVertexInstanceID = Triangle.GetVertexInstanceID( TriangleVertexNumber );
				const FVertexInstanceID NewVertexInstanceID( Remappings.NewVertexInstanceIndexLookup[ OriginalVertexInstanceID.GetValue() ] );
				Triangle.SetVertexInstanceID( TriangleVertexNumber, NewVertexInstanceID );
			}
		}

		RenderingPolygonGroup.MaxTriangles = RenderingPolygonGroup.Triangles.GetArraySize();

		// Fix up references in referencing polygons
		for( const FPolygonID PolygonID : EditableMesh->GetMeshDescription()->GetPolygonGroupPolygons( PolygonGroupID ) )
		{
			FRenderingPolygon& RenderingPolygon = RenderingPolygons[ PolygonID ];
			RenderingPolygon.PolygonGroupID = PolygonGroupID;

			for( FTriangleID& TriangleID : RenderingPolygon.TriangulatedPolygonTriangleIndices )
			{
				TriangleID = FTriangleID( TriangleRemappings[ TriangleID.GetValue() ] );
			}
		}
	}
}


bool UEditableStaticMeshAdapter::IsCommitted( const UEditableMesh* EditableMesh ) const
{
	return StaticMesh->EditableMesh == EditableMesh;
}


bool UEditableStaticMeshAdapter::IsCommittedAsInstance( const UEditableMesh* EditableMesh ) const
{
	return StaticMesh != OriginalStaticMesh;
}


void UEditableStaticMeshAdapter::OnCommit( UEditableMesh* EditableMesh )
{
	if( !IsCommitted( EditableMesh ) )
	{
		// Move the editable mesh to an inner of the static mesh, and set the static mesh's EditableMesh property.
		EditableMesh->Rename( nullptr, StaticMesh, REN_DontCreateRedirectors );
		StaticMesh->EditableMesh = EditableMesh;
	}
}


UEditableMesh* UEditableStaticMeshAdapter::OnCommitInstance( UEditableMesh* EditableMesh, UPrimitiveComponent* ComponentToInstanceTo )
{
	UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>( ComponentToInstanceTo );

	if( StaticMeshComponent )
	{
		// Duplicate the static mesh, putting it as an *inner* of the static mesh component.
		// This is no longer a persistent asset, so clear the appropriate flags.
		UStaticMesh* NewStaticMesh = DuplicateObject( OriginalStaticMesh, StaticMeshComponent );
		NewStaticMesh->ClearFlags( RF_Public | RF_Standalone );

		// Point the static mesh component to the new static mesh instance we just made for it
		StaticMeshComponent->SetStaticMesh( NewStaticMesh );

		// Duplicate this editable mesh to a new instance inside the new static mesh instance, and set the static mesh's EditableMesh property.
		UEditableMesh* NewEditableMesh = DuplicateObject( EditableMesh, NewStaticMesh );

		// Look for the corresponding adapter instance in the duplicated mesh.
		const int32 AdapterIndex = EditableMesh->Adapters.Find( this );
		check( AdapterIndex != INDEX_NONE );
		UEditableStaticMeshAdapter* NewAdapter = Cast<UEditableStaticMeshAdapter>( NewEditableMesh->Adapters[ AdapterIndex ] );

		NewStaticMesh->EditableMesh = NewEditableMesh;
		NewAdapter->StaticMesh = NewStaticMesh;

		// Update the submesh address which will have changed now it's been instanced
		NewEditableMesh->SetSubMeshAddress( UEditableMeshFactory::MakeSubmeshAddress( StaticMeshComponent, EditableMesh->SubMeshAddress.LODIndex ) );
		NewEditableMesh->RebuildRenderMesh();

		return NewEditableMesh;
	}

	return nullptr;
}


void UEditableStaticMeshAdapter::OnRevert( UEditableMesh* EditableMesh )
{
	// @todo
}


UEditableMesh* UEditableStaticMeshAdapter::OnRevertInstance( UEditableMesh* EditableMesh )
{
	// @todo
	return nullptr;
}


void UEditableStaticMeshAdapter::OnPropagateInstanceChanges( UEditableMesh* EditableMesh )
{
	check( false );		// @todo mesheditor: fix me
	//if( IsCommittedAsInstance() )
	//{
	//	// @todo mesheditor: we can only generate submesh addresses from a component. Since we don't have a component, we create a dummy one.
	//	// Not really fond of this.
	//	// Explore other possibilities, e.g. constructing a submesh address by hand (although the contents of MeshObjectPtr are supposed to be opaque)
	//	UStaticMeshComponent* DummyComponent = NewObject<UStaticMeshComponent>();
	//	DummyComponent->SetStaticMesh( OriginalStaticMesh );

	//	UEditableStaticMeshAdapter* NewEditableMesh = DuplicateObject( this, OriginalStaticMesh );
	//	OriginalStaticMesh->EditableMesh = NewEditableMesh;
	//	NewEditableMesh->StaticMesh = OriginalStaticMesh;
	//	NewEditableMesh->SetSubMeshAddress( UEditableMeshFactory::MakeSubmeshAddress( DummyComponent, SubMeshAddress.LODIndex ) );
	//	NewEditableMesh->RebuildRenderMesh();
	//}
}


void UEditableStaticMeshAdapter::UpdateBounds( const UEditableMesh* EditableMesh, const bool bShouldRecompute )
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
	
	FStaticMeshRenderData& StaticMeshRenderData = *StaticMesh->RenderData;
	StaticMeshRenderData.Bounds = CachedBoundingBoxAndSphere;
	StaticMesh->CalculateExtendedBounds();
}


void UEditableStaticMeshAdapter::UpdateCollision()
{
	// @todo mesheditor collision: We're wiping the existing simplified collision and generating a simple bounding
	// box collision, since that's the best we can do without impacting performance.  We always using visibility (complex)
	// collision for traces while mesh editing (for hover/selection), so simplified collision isn't really important.
	const bool bRecreateSimplifiedCollision = true;

	if( StaticMesh->BodySetup == nullptr )
	{
		StaticMesh->CreateBodySetup();
	}

	UBodySetup* BodySetup = StaticMesh->BodySetup;

	// Return of body setup creation failed
	if( !BodySetup )
	{
		return;
	}

	// NOTE: We don't bother calling Modify() on the BodySetup as EndModification() will rebuild this guy after every undo
	// BodySetup->Modify();

	if( bRecreateSimplifiedCollision )
	{
		if( BodySetup->AggGeom.GetElementCount() > 0 )
		{
			BodySetup->RemoveSimpleCollision();
		}
	}

	BodySetup->InvalidatePhysicsData();

	if( bRecreateSimplifiedCollision )
	{
		const FBoxSphereBounds Bounds = StaticMesh->GetBounds();

		FKBoxElem BoxElem;
		BoxElem.Center = Bounds.Origin;
		BoxElem.X = Bounds.BoxExtent.X * 2.0f;
		BoxElem.Y = Bounds.BoxExtent.Y * 2.0f;
		BoxElem.Z = Bounds.BoxExtent.Z * 2.0f;
		BodySetup->AggGeom.BoxElems.Add( BoxElem );
	}

	// Update all static mesh components that are using this mesh
	// @todo mesheditor perf: This is a pretty heavy operation, and overlaps with what we're already doing in RecreateRenderStateContext
	// a little bit.  Ideally we do everything in a single pass.  Furthermore, if this could be updated lazily it would be faster.
	{
		for( FObjectIterator Iter( UStaticMeshComponent::StaticClass() ); Iter; ++Iter )
		{
			UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>( *Iter );
			if( StaticMeshComponent->GetStaticMesh() == StaticMesh )
			{
				// it needs to recreate IF it already has been created
				if( StaticMeshComponent->IsPhysicsStateCreated() )
				{
					StaticMeshComponent->RecreatePhysicsState();
				}
			}
		}
	}
}


void UEditableStaticMeshAdapter::OnSetVertexAttribute( const UEditableMesh* EditableMesh, const FVertexID VertexID, const FMeshElementAttributeData& Attribute )
{
	FStaticMeshLODResources& StaticMeshLOD = GetStaticMeshLOD();

	const FMeshDescription* MeshDescription = EditableMesh->GetMeshDescription();

	if( Attribute.AttributeName == MeshAttribute::Vertex::Position )
	{
		const FVector NewVertexPosition = Attribute.AttributeValue.GetValue<FVector>();

		// @todo mesheditor: eventually break out subdivided mesh into a different adapter which handles things differently?
		// (may also want different component eventually)
		if( !EditableMesh->IsPreviewingSubdivisions() )
		{
			const FVertexInstanceArray& VertexInstances = EditableMesh->GetMeshDescription()->VertexInstances();

			// Set the vertex buffer position of all of the vertex instances for this editable vertex
			for( const FVertexInstanceID VertexInstanceID : MeshDescription->GetVertexVertexInstances( VertexID ) )
			{
				check( MeshDescription->IsVertexInstanceValid( VertexInstanceID ) );
				StaticMeshLOD.VertexBuffers.PositionVertexBuffer.VertexPosition( VertexInstanceID.GetValue() ) = NewVertexPosition;
			}
		}

		// Update cached bounds.  This is not a "perfect" bounding sphere and centered box.  Instead, we take our current bounds
		// and inflate it to include the updated vertex position, translating the bounds proportionally to reduce how much it
		// needs to be expanded.  The "perfect" bounds will be computed in UpdateBounds() when an interaction is finalized.
		{
			const FVector OffsetFromCenter = NewVertexPosition - CachedBoundingBoxAndSphere.Origin;
			const float SquaredDistanceToCenter = OffsetFromCenter.SizeSquared();
			const float SquaredSphereRadius = CachedBoundingBoxAndSphere.SphereRadius * CachedBoundingBoxAndSphere.SphereRadius;
			if( SquaredDistanceToCenter > SquaredSphereRadius )
			{
				const float DistanceToCenter = FMath::Sqrt( SquaredDistanceToCenter );
				const float RadiusDelta = ( DistanceToCenter - CachedBoundingBoxAndSphere.SphereRadius ) * 0.5f;
				CachedBoundingBoxAndSphere.SphereRadius += RadiusDelta;
				CachedBoundingBoxAndSphere.Origin += OffsetFromCenter * ( RadiusDelta / DistanceToCenter );
			}

			// Update extent
			CachedBoundingBoxAndSphere.BoxExtent.X = FMath::Max( CachedBoundingBoxAndSphere.BoxExtent.X, FMath::Abs( NewVertexPosition.X - CachedBoundingBoxAndSphere.Origin.X ) );
			CachedBoundingBoxAndSphere.BoxExtent.Y = FMath::Max( CachedBoundingBoxAndSphere.BoxExtent.Y, FMath::Abs( NewVertexPosition.Y - CachedBoundingBoxAndSphere.Origin.Y ) );
			CachedBoundingBoxAndSphere.BoxExtent.Z = FMath::Max( CachedBoundingBoxAndSphere.BoxExtent.Z, FMath::Abs( NewVertexPosition.X - CachedBoundingBoxAndSphere.Origin.Z ) );
		}
	}
}


void UEditableStaticMeshAdapter::OnSetEdgeAttribute( const UEditableMesh* EditableMesh, const FEdgeID EdgeID, const FMeshElementAttributeData& Attribute )
{
	// Nothing to do here
}


void UEditableStaticMeshAdapter::OnSetVertexInstanceAttribute( const UEditableMesh* EditableMesh, const FVertexInstanceID VertexInstanceID, const FMeshElementAttributeData& Attribute )
{
	const TAttributesSet<FVertexInstanceID>& VertexInstanceAttributes = EditableMesh->GetMeshDescription()->VertexInstanceAttributes();
	FStaticMeshLODResources& StaticMeshLOD = GetStaticMeshLOD();

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
			StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents(
				VertexInstanceID.GetValue(),
				Tangent,
				FVector::CrossProduct( Normal, Tangent ).GetSafeNormal() * BinormalSign,
				Normal );
		}
	}
	else if( Attribute.AttributeName == MeshAttribute::VertexInstance::TextureCoordinate )
	{
		if( !EditableMesh->IsPreviewingSubdivisions() )
		{
			check( Attribute.AttributeIndex < EditableMesh->GetTextureCoordinateCount() );
			StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.SetVertexUV( VertexInstanceID.GetValue(), Attribute.AttributeIndex, Attribute.AttributeValue.GetValue<FVector2D>() );
		}
	}
	else if( Attribute.AttributeName == MeshAttribute::VertexInstance::Color )
	{
		if( !EditableMesh->IsPreviewingSubdivisions() )
		{
			const FVector4 Value = Attribute.AttributeValue.GetValue<FVector4>();
			const FLinearColor LinearColor( Value.X, Value.Y, Value.Z, Value.W );
			const FColor NewColor = LinearColor.ToFColor( true );

			if( StaticMeshLOD.VertexBuffers.ColorVertexBuffer.GetNumVertices() != EditableMesh->GetMeshDescription()->VertexInstances().GetArraySize() )
			{
				if( LinearColor != FLinearColor::White )
				{
					// Until now, we haven't needed a vertex color buffer.
					// Force one to be generated now that we have a non-white vertex in the mesh.
					OnRebuildRenderMesh( EditableMesh );
				}
			}
			else
			{
				StaticMeshLOD.VertexBuffers.ColorVertexBuffer.VertexColor( VertexInstanceID.GetValue() ) = NewColor;
			}
		}
	}
}


void UEditableStaticMeshAdapter::OnCreateEmptyVertexRange( const UEditableMesh* EditableMesh, const TArray<FVertexID>& VertexIDs )
{
}


void UEditableStaticMeshAdapter::OnCreateVertices( const UEditableMesh* EditableMesh, const TArray<FVertexID>& VertexIDs )
{
}


void UEditableStaticMeshAdapter::OnCreateVertexInstances( const UEditableMesh* EditableMesh, const TArray<FVertexInstanceID>& VertexInstanceIDs )
{
	if( !EditableMesh->IsPreviewingSubdivisions() )
	{
		const FMeshDescription* MeshDescription = EditableMesh->GetMeshDescription();
		FStaticMeshLODResources& StaticMeshLOD = GetStaticMeshLOD();
		const int32 NumUVs = MeshDescription->VertexInstanceAttributes().GetAttributeIndexCount<FVector2D>( MeshAttribute::VertexInstance::TextureCoordinate );
		TVertexAttributesConstRef<FVector> VertexPositions = MeshDescription->VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );
		const bool bHasColors = StaticMeshLOD.VertexBuffers.ColorVertexBuffer.GetNumVertices() > 0;

		// Determine if we need to grow the render buffers
		const int32 OldVertexBufferRenderingVertexCount = StaticMeshLOD.VertexBuffers.PositionVertexBuffer.GetNumVertices();
		const int32 NumNewVertexBufferRenderingVertices = FMath::Max( 0, MeshDescription->VertexInstances().GetArraySize() - OldVertexBufferRenderingVertexCount );

		static TArray<FStaticMeshBuildVertex> RenderingVerticesToAppend;
		RenderingVerticesToAppend.SetNumUninitialized( NumNewVertexBufferRenderingVertices, false );

		for( const FVertexInstanceID VertexInstanceID : VertexInstanceIDs )
		{
			const FVertexID ReferencedVertexID = MeshDescription->GetVertexInstanceVertex( VertexInstanceID );

			const int32 NewRenderingVertexIndex = VertexInstanceID.GetValue();	// Rendering vertex indices are the same as vertex instance IDs

			if( NewRenderingVertexIndex < OldVertexBufferRenderingVertexCount )
			{
				// Rendering vertex is within the already allocated buffer. Initialize the new vertices to some defaults
				// @todo mesheditor: these defaults should come from the attributes themselves
				StaticMeshLOD.VertexBuffers.PositionVertexBuffer.VertexPosition( NewRenderingVertexIndex ) = VertexPositions[ ReferencedVertexID ];
				StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents(
					NewRenderingVertexIndex,
					FVector::ZeroVector,
					FVector::ZeroVector,
					FVector::ZeroVector );

				for( int32 UVIndex = 0; UVIndex < NumUVs; ++UVIndex )
				{
					StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.SetVertexUV( NewRenderingVertexIndex, UVIndex, FVector2D::ZeroVector );
				}

				if( bHasColors )
				{
					StaticMeshLOD.VertexBuffers.ColorVertexBuffer.VertexColor( NewRenderingVertexIndex ) = FColor::White;
				}
			}
			else
			{
				// Rendering vertex needs to be added in a new block
				const int32 AppendVertexNumber = NewRenderingVertexIndex - OldVertexBufferRenderingVertexCount;
				check( AppendVertexNumber >= 0 && AppendVertexNumber < NumNewVertexBufferRenderingVertices );
				FStaticMeshBuildVertex& RenderingVertexToAppend = RenderingVerticesToAppend[ AppendVertexNumber ];

				// Initialize the new vertices to some defaults
				RenderingVertexToAppend.Position = VertexPositions[ ReferencedVertexID ];
				RenderingVertexToAppend.TangentX = FVector::ZeroVector;
				RenderingVertexToAppend.TangentY = FVector::ZeroVector;
				RenderingVertexToAppend.TangentZ = FVector::ZeroVector;

				for( int32 UVIndex = 0; UVIndex < NumUVs; ++UVIndex )
				{
					RenderingVertexToAppend.UVs[ UVIndex ] = FVector2D::ZeroVector;
				}

				RenderingVertexToAppend.Color = FColor::White;
			}
		}

		if( RenderingVerticesToAppend.Num() > 0 )
		{
			StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.AppendVertices( RenderingVerticesToAppend.GetData(), RenderingVerticesToAppend.Num() );
			StaticMeshLOD.VertexBuffers.PositionVertexBuffer.AppendVertices( RenderingVerticesToAppend.GetData(), RenderingVerticesToAppend.Num() );

			if( bHasColors )
			{
				StaticMeshLOD.VertexBuffers.ColorVertexBuffer.AppendVertices( RenderingVerticesToAppend.GetData(), RenderingVerticesToAppend.Num() );
			}
		}
	}
}


void UEditableStaticMeshAdapter::OnCreateEdges( const UEditableMesh* EditableMesh, const TArray<FEdgeID>& EdgeIDs )
{
	// Nothing to do here for now
}


void UEditableStaticMeshAdapter::OnCreatePolygons( const UEditableMesh* EditableMesh, const TArray<FPolygonID>& PolygonIDs )
{
	// Add mirror polygons for static mesh adapter
	for( const FPolygonID PolygonID : PolygonIDs )
	{
		RenderingPolygons.Insert( PolygonID );
		RenderingPolygons[ PolygonID ].PolygonGroupID = EditableMesh->GetGroupForPolygon( PolygonID );
	}
}


void UEditableStaticMeshAdapter::OnChangePolygonVertexInstances( const UEditableMesh* EditableMesh, const TArray<FPolygonID>& PolygonIDs )
{
	OnRetriangulatePolygons( EditableMesh, PolygonIDs );
}


void UEditableStaticMeshAdapter::OnRetriangulatePolygons( const UEditableMesh* EditableMesh, const TArray<FPolygonID>& PolygonIDs )
{
	const FMeshDescription* MeshDescription = EditableMesh->GetMeshDescription();

	for( const FPolygonID PolygonID : PolygonIDs )
	{
		FRenderingPolygon& RenderingPolygon = RenderingPolygons[ PolygonID ];
		const FPolygonGroupID PolygonGroupID = RenderingPolygon.PolygonGroupID;
		FRenderingPolygonGroup& RenderingPolygonGroup = RenderingPolygonGroups[ PolygonGroupID ];
		const TArray<FMeshTriangle>& Triangles = MeshDescription->GetPolygonTriangles( PolygonID );

		// Check to see whether the index buffer needs to be updated
		bool bNeedsUpdatedTriangles = false;
		{
			if( RenderingPolygon.TriangulatedPolygonTriangleIndices.Num() != Triangles.Num() )
			{
				// Triangle count has changed, so we definitely need new triangles!
				bNeedsUpdatedTriangles = true;
			}
			else
			{
				// See if the triangulation has changed even if the number of triangles is the same
				for( int32 Index = 0; Index < Triangles.Num(); ++Index )
				{
					const FMeshTriangle& OldTriangle = RenderingPolygonGroup.Triangles[ RenderingPolygon.TriangulatedPolygonTriangleIndices[ Index ] ];
					const FMeshTriangle& NewTriangle = Triangles[ Index ];

					if( OldTriangle.VertexInstanceID0 != NewTriangle.VertexInstanceID0 ||
						OldTriangle.VertexInstanceID1 != NewTriangle.VertexInstanceID1 ||
						OldTriangle.VertexInstanceID2 != NewTriangle.VertexInstanceID2 )
					{
						bNeedsUpdatedTriangles = true;
						break;
					}
				}
			}
		}

		// Has anything changed?
		if( bNeedsUpdatedTriangles )
		{
			const uint32 RenderingSectionIndex = RenderingPolygonGroup.RenderingSectionIndex;
			FStaticMeshLODResources& StaticMeshLOD = GetStaticMeshLOD();
			FStaticMeshSection& RenderingSection = StaticMeshLOD.Sections[ RenderingSectionIndex ];

			// Remove the old triangles
			DeletePolygonTriangles( EditableMesh, PolygonID );

			// Add new triangles
			{
				// This is the number of triangles we are about to add
				const int32 NumNewTriangles = Triangles.Num();

				// This is the number of entries currently unused in the Triangles sparse array
				const int32 NumFreeTriangles = RenderingPolygonGroup.Triangles.GetArraySize() - RenderingPolygonGroup.Triangles.Num();

				// This is the number of triangles we need to make extra space for (in both the sparse array and the index buffer)
				const int32 NumTrianglesToReserve = FMath::Max( 0, NumNewTriangles - NumFreeTriangles );

				// This is the number of triangles we will need to have allocated in the index buffer after adding the new triangles
				const int32 NewTotalTriangles = RenderingPolygonGroup.Triangles.GetArraySize() + NumTrianglesToReserve;

				// Reserve extra triangles if necessary.
				if( NumTrianglesToReserve > 0 )
				{
					RenderingPolygonGroup.Triangles.Reserve( NewTotalTriangles );
				}

				// Keep track of new min/max vertex indices
				int32 MinVertexIndex = RenderingSection.MinVertexIndex;
				int32 MaxVertexIndex = RenderingSection.MaxVertexIndex;

				// Create empty triangles for all of the new triangles we need, and keep track of their triangle indices
				static TArray<FTriangleID> NewTriangleIDs;
				{
					NewTriangleIDs.SetNumUninitialized( NumNewTriangles, false );

					for( int32 TriangleToAddNumber = 0; TriangleToAddNumber < NumNewTriangles; ++TriangleToAddNumber )
					{
						const FTriangleID NewTriangleID = RenderingPolygonGroup.Triangles.Add();
						NewTriangleIDs[ TriangleToAddNumber ] = NewTriangleID;

						FMeshTriangle& NewTriangle = RenderingPolygonGroup.Triangles[ NewTriangleID ];
						for( int32 TriangleVertexNumber = 0; TriangleVertexNumber < 3; ++TriangleVertexNumber )
						{
							const FVertexInstanceID VertexInstanceID = Triangles[ TriangleToAddNumber ].GetVertexInstanceID( TriangleVertexNumber );
							NewTriangle.SetVertexInstanceID( TriangleVertexNumber, VertexInstanceID );
							MinVertexIndex = FMath::Min( MinVertexIndex, VertexInstanceID.GetValue() );
							MaxVertexIndex = FMath::Max( MaxVertexIndex, VertexInstanceID.GetValue() );
						}

						RenderingPolygon.TriangulatedPolygonTriangleIndices.Add( NewTriangleID );
					}
				}

				// Update the index buffer format if the index range exceeds 16 bit values.
				if( !EditableMesh->IsPreviewingSubdivisions() )
				{
					UpdateIndexBufferFormatIfNeeded( Triangles );
				}

				// If we need more space in the index buffer for this section, allocate it here
				if( NewTotalTriangles > RenderingPolygonGroup.MaxTriangles )
				{
					const int32 NumExtraTriangles = NewTotalTriangles + IndexBufferInterSectionGap - RenderingPolygonGroup.MaxTriangles;

					// Get current number of triangles allocated for this section
					const int32 MaxTriangles = RenderingPolygonGroup.MaxTriangles;
					RenderingPolygonGroup.MaxTriangles += NumExtraTriangles;

					if( !EditableMesh->IsPreviewingSubdivisions() )
					{
						const uint32 FirstIndex = RenderingSection.FirstIndex;

						// Make room in the index buffer for the extra triangles, and update the mesh section's maximum triangle count
						StaticMeshLOD.IndexBuffer.InsertIndices( FirstIndex + MaxTriangles * 3, nullptr, NumExtraTriangles * 3 );

						// Adjust first index for all subsequent render sections to account for the extra indices just inserted.
						// It is guaranteed that index buffer indices are in the same order as the rendering sections.
						const uint32 NumRenderingSections = StaticMeshLOD.Sections.Num();
						uint32 NextRenderingSectionIndex = RenderingSectionIndex;
						while( ++NextRenderingSectionIndex < NumRenderingSections )
						{
							check( StaticMeshLOD.Sections[ NextRenderingSectionIndex ].FirstIndex >= FirstIndex );
							StaticMeshLOD.Sections[ NextRenderingSectionIndex ].FirstIndex += NumExtraTriangles * 3;
						}
					}
				}

				if( !EditableMesh->IsPreviewingSubdivisions() )
				{
					for( int32 TriangleToAddNumber = 0; TriangleToAddNumber < NumNewTriangles; ++TriangleToAddNumber )
					{
						const FTriangleID NewTriangleID = NewTriangleIDs[ TriangleToAddNumber ];

						for( int32 TriangleVertexNumber = 0; TriangleVertexNumber < 3; ++TriangleVertexNumber )
						{
							StaticMeshLOD.IndexBuffer.SetIndex(
								FRenderingPolygonGroup::TriangleIndexToRenderingTriangleFirstIndex( RenderingSection, NewTriangleID ) + TriangleVertexNumber,
								RenderingPolygonGroup.Triangles[ NewTriangleID ].GetVertexInstanceID( TriangleVertexNumber ).GetValue() );
						}
					}

					if( NumTrianglesToReserve > 0 )
					{
						RenderingSection.NumTriangles += NumTrianglesToReserve;
					}

					RenderingSection.MinVertexIndex = MinVertexIndex;
					RenderingSection.MaxVertexIndex = MaxVertexIndex;
				}
			}
		}
	}
}


void UEditableStaticMeshAdapter::OnDeleteVertexInstances( const UEditableMesh* EditableMesh, const TArray<FVertexInstanceID>& VertexInstanceIDs )
{
	// Nothing to do here
}


void UEditableStaticMeshAdapter::OnDeleteOrphanVertices( const UEditableMesh* EditableMesh, const TArray<FVertexID>& VertexIDs )
{
	// Nothing to do here
}


void UEditableStaticMeshAdapter::OnDeleteEdges( const UEditableMesh* EditableMesh, const TArray<FEdgeID>& EdgeIDs )
{
	// Nothing to do here
}


void UEditableStaticMeshAdapter::OnSetEdgesVertices( const UEditableMesh* EditableMesh, const TArray<FEdgeID>& EdgeIDs )
{
	// Nothing to do here
}


void UEditableStaticMeshAdapter::OnDeletePolygons( const UEditableMesh* EditableMesh, const TArray<FPolygonID>& PolygonIDs )
{
	for( const FPolygonID PolygonID : PolygonIDs )
	{
		// Removes all of a polygon's triangles (including rendering triangles from the index buffer.)
		DeletePolygonTriangles( EditableMesh, PolygonID );

		// Delete the polygon from the static mesh adapter mirror
		RenderingPolygons.Remove( PolygonID );
	}
}


void UEditableStaticMeshAdapter::DeletePolygonTriangles( const UEditableMesh* EditableMesh, const FPolygonID PolygonID )
{
	FRenderingPolygon& Polygon = RenderingPolygons[ PolygonID ];
	const FPolygonGroupID PolygonGroupID = Polygon.PolygonGroupID;

	FRenderingPolygonGroup& RenderingPolygonGroup = RenderingPolygonGroups[ PolygonGroupID ];

	const int32 NumTrianglesToRemove = Polygon.TriangulatedPolygonTriangleIndices.Num();
	if( NumTrianglesToRemove > 0 )
	{
		bool bUpdateMinMax = false;

		// Kill the polygon's rendering triangles in the static mesh

		FStaticMeshLODResources& StaticMeshLOD = GetStaticMeshLOD();
		const uint32 RenderingSectionIndex = RenderingPolygonGroup.RenderingSectionIndex;
		FStaticMeshSection& RenderingSection = StaticMeshLOD.Sections[ RenderingSectionIndex ];

		// Update the index buffer by removing entries, and the rendering sections with new section counts
		if( !EditableMesh->IsPreviewingSubdivisions() )
		{
			// If the min or max vertex index is about to be deleted, refresh the range
			const int32 MinVertexIndex = RenderingSection.MinVertexIndex;
			const int32 MaxVertexIndex = RenderingSection.MaxVertexIndex;

			for( const FTriangleID TriangleIDToRemove : Polygon.TriangulatedPolygonTriangleIndices )
			{
				const FMeshTriangle& Triangle = RenderingPolygonGroup.Triangles[ TriangleIDToRemove ];

				for( int32 TriangleVertexNumber = 0; TriangleVertexNumber < 3; ++TriangleVertexNumber )
				{
					const FVertexInstanceID VertexInstanceID = Triangle.GetVertexInstanceID( TriangleVertexNumber );
					if( VertexInstanceID.GetValue() == MinVertexIndex || VertexInstanceID.GetValue() == MaxVertexIndex )
					{
						bUpdateMinMax = true;
						break;
					}
				}

				if( bUpdateMinMax )
				{
					break;
				}
			}
		}

		// Remove all of the polygon's triangles from our editable mesh's triangle list.  While doing this, we'll keep
		// track of all of the rendering mesh triangles that we'll need to remove later on.  We'll also figure out which
		// vertex instances will need to be removed from their corresponding vertex
		for( const FTriangleID TriangleIndexToRemove : Polygon.TriangulatedPolygonTriangleIndices )
		{
			// Remove this triangle from our editable mesh
			RenderingPolygonGroup.Triangles.Remove( TriangleIndexToRemove );
		}

		if( !EditableMesh->IsPreviewingSubdivisions() )
		{
			if( bUpdateMinMax )
			{
				int32 MinVertexIndex = TNumericLimits<int32>::Max();
				int32 MaxVertexIndex = TNumericLimits<int32>::Min();

				for( const FTriangleID TriangleID : RenderingPolygonGroup.Triangles.GetElementIDs() )
				{
					const FMeshTriangle& Triangle = RenderingPolygonGroup.Triangles[ TriangleID ];

					for( int32 TriangleVertexNumber = 0; TriangleVertexNumber < 3; ++TriangleVertexNumber )
					{
						const FVertexInstanceID VertexInstanceID = Triangle.GetVertexInstanceID( TriangleVertexNumber );
						if( VertexInstanceID.GetValue() < MinVertexIndex )
						{
							MinVertexIndex = VertexInstanceID.GetValue();
						}

						if( VertexInstanceID.GetValue() > MaxVertexIndex )
						{
							MaxVertexIndex = VertexInstanceID.GetValue();
						}
					}
				}

				RenderingSection.MinVertexIndex = MinVertexIndex;
				RenderingSection.MaxVertexIndex = MaxVertexIndex;
			}

			// @todo mesheditor urgent: What about other index buffers in the mesh (DepthOnlyIndexBuffer, Wireframe, etc.)  We need to remove our triangles from those too!

			for( const FTriangleID SectionTriangleIDToRemove : Polygon.TriangulatedPolygonTriangleIndices )
			{
				const uint32 RenderingTriangleFirstVertexIndex = FRenderingPolygonGroup::TriangleIndexToRenderingTriangleFirstIndex( RenderingSection, SectionTriangleIDToRemove );

				// Make the indices degenerate.  We don't want to actually remove the indices from the index buffer, as that's can
				// be a really slow operation.  The mesh can be compacted later on to free up the memory.
				for( int32 TriangleVertexNumber = 0; TriangleVertexNumber < 3; ++TriangleVertexNumber )
				{
					StaticMeshLOD.IndexBuffer.SetIndex( RenderingTriangleFirstVertexIndex + TriangleVertexNumber, RenderingSection.MinVertexIndex );
				}
			}
		}

		Polygon.TriangulatedPolygonTriangleIndices.Reset();
	}
}


inline const FStaticMeshLODResources& UEditableStaticMeshAdapter::GetStaticMeshLOD() const
{
	const FStaticMeshRenderData& StaticMeshRenderData = *StaticMesh->RenderData;
	const FStaticMeshLODResources& StaticMeshLOD = StaticMeshRenderData.LODResources[ StaticMeshLODIndex ];
	return StaticMeshLOD;
}


FStaticMeshLODResources& UEditableStaticMeshAdapter::GetStaticMeshLOD()
{
	FStaticMeshRenderData& StaticMeshRenderData = *StaticMesh->RenderData;
	FStaticMeshLODResources& StaticMeshLOD = StaticMeshRenderData.LODResources[ StaticMeshLODIndex ];
	return StaticMeshLOD;
}


void UEditableStaticMeshAdapter::OnCreatePolygonGroups( const UEditableMesh* EditableMesh, const TArray<FPolygonGroupID>& PolygonGroupIDs)
{
	const FMeshDescription* MeshDescription = EditableMesh->GetMeshDescription();
	check( MeshDescription );

	TPolygonGroupAttributesConstRef<FName> PolygonGroupImportedMaterialSlotNames = MeshDescription->PolygonGroupAttributes().GetAttributesRef<FName>( MeshAttribute::PolygonGroup::ImportedMaterialSlotName );
	TPolygonGroupAttributesConstRef<FName> PolygonGroupMaterialAssetSlotNames = MeshDescription->PolygonGroupAttributes().GetAttributesRef<FName>( MeshAttribute::PolygonGroup::MaterialAssetName );
	TPolygonGroupAttributesConstRef<bool> PolygonGroupCollision = MeshDescription->PolygonGroupAttributes().GetAttributesRef<bool>( MeshAttribute::PolygonGroup::EnableCollision );
	TPolygonGroupAttributesConstRef<bool> PolygonGroupCastShadow = MeshDescription->PolygonGroupAttributes().GetAttributesRef<bool>( MeshAttribute::PolygonGroup::CastShadow );

	for( const FPolygonGroupID PolygonGroupID : PolygonGroupIDs )
	{
		UMaterialInterface* Material = LoadObject<UMaterialInterface>( nullptr, *PolygonGroupMaterialAssetSlotNames[ PolygonGroupID ].ToString() );
		const int32 MaterialIndex = StaticMesh->StaticMaterials.Emplace(
			Material,
			PolygonGroupImportedMaterialSlotNames[ PolygonGroupID ]
#if WITH_EDITORONLY_DATA
			, PolygonGroupImportedMaterialSlotNames[ PolygonGroupID ]
#endif
		);

		uint32 LODSectionIndex = 0;
		if( !EditableMesh->IsPreviewingSubdivisions() )
		{
			// Need to create a new rendering section. This is added to the end of the array.
			FStaticMeshLODResources& StaticMeshLOD = GetStaticMeshLOD();

			LODSectionIndex = StaticMeshLOD.Sections.Emplace();
			FStaticMeshSection& StaticMeshSection = StaticMeshLOD.Sections[ LODSectionIndex ];

			// Initially the section is empty, and it occupies zero elements in the index buffer.
			// It is still placed in the correct location within the index buffer, immediately following the previous section,
			// as it is a requirement that consecutive sections are placed contiguously in the index buffer.
			// Determine the first index based on the index range of the previous rendering section.
			if( LODSectionIndex == 0 )
			{
				StaticMeshSection.FirstIndex = 0;
			}
			else
			{
				FStaticMeshSection& PreviousStaticMeshSection = StaticMeshLOD.Sections[ LODSectionIndex - 1 ];
				const FPolygonGroupID PreviousPolygonGroupID = GetSectionForRenderingSectionIndex( LODSectionIndex - 1 );
				check( PreviousPolygonGroupID != FPolygonGroupID::Invalid );
				StaticMeshSection.FirstIndex = PreviousStaticMeshSection.FirstIndex + RenderingPolygonGroups[ PreviousPolygonGroupID ].MaxTriangles * 3;

				// @todo mesheditor: if this check is valid, we can dispense with the above and just set StaticMeshSection.FirstIndex according to the current length of the idnex buffer.
				check( StaticMeshLOD.IndexBuffer.GetNumIndices() == StaticMeshSection.FirstIndex );
			}

			// Fill in the remaining rendering section properties.
			StaticMeshSection.NumTriangles = 0;
			StaticMeshSection.MinVertexIndex = 0;
			StaticMeshSection.MaxVertexIndex = 0;
			StaticMeshSection.bEnableCollision = PolygonGroupCollision[ PolygonGroupID ];
			StaticMeshSection.bCastShadow = PolygonGroupCastShadow[ PolygonGroupID ];
			StaticMeshSection.MaterialIndex = MaterialIndex;

#if WITH_EDITORONLY_DATA
			// SectionInfoMap must be synced with the info of the new Section
			FMeshSectionInfo Info;
			Info.bEnableCollision = StaticMeshSection.bEnableCollision;
			Info.bCastShadow = StaticMeshSection.bCastShadow;
			Info.MaterialIndex = StaticMeshSection.MaterialIndex;
			StaticMesh->SectionInfoMap.Set( StaticMeshLODIndex, LODSectionIndex, Info );
#endif
		}

		// Insert the rendering polygon group for keeping track of these index buffer properties
		RenderingPolygonGroups.Insert( PolygonGroupID );
		FRenderingPolygonGroup& RenderingPolygonGroup = RenderingPolygonGroups[ PolygonGroupID ];

		RenderingPolygonGroup.RenderingSectionIndex = LODSectionIndex;
		RenderingPolygonGroup.MaterialIndex = MaterialIndex;
		RenderingPolygonGroup.MaxTriangles = 0;
	}
}


void UEditableStaticMeshAdapter::OnSetPolygonGroupAttribute( const UEditableMesh* EditableMesh, const FPolygonGroupID PolygonGroupID, const FMeshElementAttributeData& Attribute )
{
	const FMeshDescription* MeshDescription = EditableMesh->GetMeshDescription();

	FRenderingPolygonGroup& RenderingPolygonGroup = RenderingPolygonGroups[ PolygonGroupID ];
	FStaticMeshLODResources& StaticMeshLOD = GetStaticMeshLOD();
	FStaticMeshSection& StaticMeshSection = StaticMeshLOD.Sections[ RenderingPolygonGroup.RenderingSectionIndex ];

	const FName ImportedMaterialSlotName = MeshDescription->PolygonGroupAttributes().GetAttribute<FName>( PolygonGroupID, MeshAttribute::PolygonGroup::ImportedMaterialSlotName, 0 );
	const FName MaterialAssetName = MeshDescription->PolygonGroupAttributes().GetAttribute<FName>( PolygonGroupID, MeshAttribute::PolygonGroup::MaterialAssetName, 0 );

	const int32 MaterialIndex = StaticMesh->StaticMaterials.IndexOfByPredicate(
		[ &ImportedMaterialSlotName ]( const FStaticMaterial& StaticMaterial ) { return StaticMaterial.ImportedMaterialSlotName == ImportedMaterialSlotName; }
	);
	check( MaterialIndex != INDEX_NONE );

	if ( Attribute.AttributeName == MeshAttribute::PolygonGroup::ImportedMaterialSlotName )
	{
		StaticMesh->StaticMaterials[ RenderingPolygonGroup.MaterialIndex ].ImportedMaterialSlotName = ImportedMaterialSlotName;
	}
	else if( Attribute.AttributeName == MeshAttribute::PolygonGroup::MaterialAssetName )
	{
		UMaterialInterface* Material = LoadObject<UMaterialInterface>( nullptr, *MaterialAssetName.ToString() );
		StaticMesh->StaticMaterials[ RenderingPolygonGroup.MaterialIndex ] = FStaticMaterial(
			Material,
			ImportedMaterialSlotName
#if WITH_EDITORONLY_DATA
			, ImportedMaterialSlotName
#endif
		);
	}
	else if( Attribute.AttributeName == MeshAttribute::PolygonGroup::CastShadow )
	{
		StaticMeshSection.bCastShadow = Attribute.AttributeValue.GetValue<bool>();
	}
	else if( Attribute.AttributeName == MeshAttribute::PolygonGroup::EnableCollision )
	{
		StaticMeshSection.bEnableCollision = Attribute.AttributeValue.GetValue<bool>();
	}
}


void UEditableStaticMeshAdapter::OnDeletePolygonGroups( const UEditableMesh* EditableMesh, const TArray<FPolygonGroupID>& PolygonGroupIDs )
{
	TPolygonGroupAttributesConstRef<FName> PolygonGroupImportedMaterialSlotNames = EditableMesh->GetMeshDescription()->PolygonGroupAttributes().GetAttributesRef<FName>( MeshAttribute::PolygonGroup::ImportedMaterialSlotName );

	for( const FPolygonGroupID PolygonGroupID : PolygonGroupIDs )
	{
		FRenderingPolygonGroup& RenderingPolygonGroup = RenderingPolygonGroups[ PolygonGroupID ];

		// Remove material slot associated with section
		const int32 MaterialIndex = RenderingPolygonGroup.MaterialIndex;
		StaticMesh->StaticMaterials.RemoveAt( MaterialIndex );

		// Adjust rendering indices held by sections: any index above the one we just deleted now needs to be decremented.
		const uint32 RenderingSectionIndex = RenderingPolygonGroup.RenderingSectionIndex;

		for( const FPolygonGroupID PolygonGroupIDToAdjust : RenderingPolygonGroups.GetElementIDs() )
		{
			FRenderingPolygonGroup& PolygonGroupToAdjust = RenderingPolygonGroups[ PolygonGroupIDToAdjust ];

			if( PolygonGroupToAdjust.RenderingSectionIndex > RenderingSectionIndex )
			{
				PolygonGroupToAdjust.RenderingSectionIndex--;
			}

			if( PolygonGroupToAdjust.MaterialIndex > MaterialIndex )
			{
				PolygonGroupToAdjust.MaterialIndex--;
			}
		}

		if( !EditableMesh->IsPreviewingSubdivisions() )
		{
			FStaticMeshLODResources& StaticMeshLOD = GetStaticMeshLOD();
			const uint32 FirstIndex = StaticMeshLOD.Sections[ RenderingSectionIndex ].FirstIndex;

			// Get current number of triangles allocated for this section
			const int32 MaxTriangles = RenderingPolygonGroup.MaxTriangles;

			// Remove indices from this poisition in the index buffer
			StaticMeshLOD.IndexBuffer.RemoveIndicesAt( FirstIndex, MaxTriangles * 3 );

			// Adjust first index for all subsequent render sections to account for the indices just removed.
			// It is guaranteed that index buffer indices are in the same order as the rendering sections.
			const uint32 NumRenderingSections = StaticMeshLOD.Sections.Num();
			for( uint32 Index = RenderingSectionIndex + 1; Index < NumRenderingSections; ++Index )
			{
				check( StaticMeshLOD.Sections[ Index ].FirstIndex >= FirstIndex );
				StaticMeshLOD.Sections[ Index ].FirstIndex -= MaxTriangles * 3;
			}

			// Adjust material indices for any sections to account for the fact that one has been removed
			for( uint32 Index = 0; Index < NumRenderingSections; ++Index )
			{
				FStaticMeshSection& StaticMeshSection = StaticMeshLOD.Sections[ Index ];
				if( StaticMeshSection.MaterialIndex > MaterialIndex )
				{
					StaticMeshSection.MaterialIndex--;

#if WITH_EDITORONLY_DATA
					// SectionInfoMap must be synced with the info of the modified Section
					FMeshSectionInfo SectionInfo = StaticMesh->SectionInfoMap.Get( StaticMeshLODIndex, Index );
					--SectionInfo.MaterialIndex;
					StaticMesh->SectionInfoMap.Set( StaticMeshLODIndex, Index, SectionInfo );
#endif
				}
			}

			StaticMeshLOD.Sections.RemoveAt( RenderingSectionIndex );

#if WITH_EDITORONLY_DATA
			// SectionInfoMap must be re-indexed to account for the removed Section
			uint32 NumSectionInfo = StaticMesh->SectionInfoMap.GetSectionNumber( StaticMeshLODIndex );
			for ( uint32 Index = RenderingSectionIndex + 1; Index < NumSectionInfo; ++Index )
			{
				FMeshSectionInfo SectionInfo = StaticMesh->SectionInfoMap.Get( StaticMeshLODIndex, Index );
				StaticMesh->SectionInfoMap.Set( StaticMeshLODIndex, Index - 1, SectionInfo );
			}
			// And remove the last SectionInfo from the map which is now invalid
			StaticMesh->SectionInfoMap.Remove( StaticMeshLODIndex, NumSectionInfo - 1 );
#endif
		}

		// Remove the rendering polygon group from the sparse array
		RenderingPolygonGroups.Remove( PolygonGroupID );
	}
}


void UEditableStaticMeshAdapter::OnAssignPolygonsToPolygonGroups( const UEditableMesh* EditableMesh, const TArray<FPolygonGroupForPolygon>& PolygonGroupForPolygons )
{
	static TArray<FPolygonID> PolygonIDsToRebuild;
	PolygonIDsToRebuild.Reset( PolygonGroupForPolygons.Num() );

	for( const FPolygonGroupForPolygon& PolygonGroupForPolygon : PolygonGroupForPolygons )
	{
		const FPolygonID PolygonID = PolygonGroupForPolygon.PolygonID;
		const FPolygonGroupID NewPolygonGroupID = PolygonGroupForPolygon.PolygonGroupID;

		DeletePolygonTriangles( EditableMesh, PolygonID );

		FRenderingPolygon& RenderingPolygon = RenderingPolygons[ PolygonID ];
		RenderingPolygon.PolygonGroupID = NewPolygonGroupID;

		PolygonIDsToRebuild.Add( PolygonID );
	}

	OnRetriangulatePolygons( EditableMesh, PolygonIDsToRebuild );
}


FPolygonGroupID UEditableStaticMeshAdapter::GetSectionForRenderingSectionIndex( const int32 RenderingSectionIndex ) const
{
	for( const FPolygonGroupID PolygonGroupID : RenderingPolygonGroups.GetElementIDs() )
	{
		const FRenderingPolygonGroup& RenderingPolygonGroup = RenderingPolygonGroups[ PolygonGroupID ];
		if( RenderingPolygonGroup.RenderingSectionIndex == RenderingSectionIndex )
		{
			return PolygonGroupID;
		}
	}

	return FPolygonGroupID::Invalid;
}
