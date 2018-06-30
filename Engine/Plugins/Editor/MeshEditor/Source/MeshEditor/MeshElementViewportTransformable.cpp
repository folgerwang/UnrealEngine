// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MeshElementViewportTransformable.h"
#include "MeshEditorMode.h"
#include "EditableMesh.h"
#include "MeshAttributes.h"


const FTransform FMeshElementViewportTransformable::GetTransform() const
{
	return CurrentTransform;
}


bool FMeshElementViewportTransformable::IsUnorientedPoint() const
{
	// Vertex elements never support rotation or scaling when only one is selected
	return MeshElement.ElementAddress.ElementType == EEditableMeshElementType::Vertex;
}


void FMeshElementViewportTransformable::ApplyTransform( const FTransform& NewTransform, const bool bSweep )
{
	CurrentTransform = NewTransform;
}


FBox FMeshElementViewportTransformable::BuildBoundingBox( const FTransform& BoundingBoxToWorld ) const
{
	FBox BoundingBox = FBox( ForceInit );

	if( MeshElement.IsValidMeshElement() )
	{
		const FTransform WorldToBoundingBox = BoundingBoxToWorld.Inverse();
		const FTransform& ComponentToWorld = MeshElement.Component.Get()->GetComponentToWorld();
		const FTransform ComponentToBoundingBox = ComponentToWorld * WorldToBoundingBox;

		UEditableMesh* EditableMesh = MeshEditorMode.FindOrCreateEditableMesh( *MeshElement.Component, MeshElement.ElementAddress.SubMeshAddress );
		if( EditableMesh != nullptr )
		{
			if( MeshElement.IsElementIDValid( EditableMesh ) )
			{
				const TVertexAttributeArray<FVector>& VertexPositions = EditableMesh->GetMeshDescription()->VertexAttributes().GetAttributes<FVector>( MeshAttribute::Vertex::Position );
				BoundingBox.Init();

				FTransform ElementTransform = FTransform::Identity;
				switch( MeshElement.ElementAddress.ElementType )
				{
					case EEditableMeshElementType::Vertex:
						BoundingBox += ComponentToBoundingBox.TransformPosition( VertexPositions[ FVertexID( MeshElement.ElementAddress.ElementID ) ] );
						break;

					case EEditableMeshElementType::Edge:
					{
						FVertexID EdgeVertexID0, EdgeVertexID1;
						EditableMesh->GetEdgeVertices( FEdgeID( MeshElement.ElementAddress.ElementID ), /* Out */ EdgeVertexID0, /* Out */ EdgeVertexID1 );

						BoundingBox += ComponentToBoundingBox.TransformPosition( VertexPositions[ EdgeVertexID0 ] );
						BoundingBox += ComponentToBoundingBox.TransformPosition( VertexPositions[ EdgeVertexID1 ] );
					}
					break;

					case EEditableMeshElementType::Polygon:
					{
						const FPolygonID PolygonID( MeshElement.ElementAddress.ElementID );

						static TArray<FVertexID> PerimeterVertexIDs;
						EditableMesh->GetPolygonPerimeterVertices( PolygonID, /* Out */ PerimeterVertexIDs );

						for( const FVertexID VertexID : PerimeterVertexIDs )
						{
							BoundingBox += ComponentToBoundingBox.TransformPosition( VertexPositions[ VertexID ] );
						}
					}
					break;

					default:
						check( 0 );
				}
			}
		}
	}

	return BoundingBox;
}