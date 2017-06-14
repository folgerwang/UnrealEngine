// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MeshElementViewportTransformable.h"
#include "MeshEditorMode.h"
#include "EditableMesh.h"


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
			if( FMeshEditorMode::IsElementIDValid( MeshElement, EditableMesh ) )
			{
				BoundingBox.Init();

				FTransform ElementTransform = FTransform::Identity;
				switch( MeshElement.ElementAddress.ElementType )
				{
					case EEditableMeshElementType::Vertex:
						BoundingBox += ComponentToBoundingBox.TransformPosition( EditableMesh->GetVertexAttribute( FVertexID( MeshElement.ElementAddress.ElementID ), UEditableMeshAttribute::VertexPosition(), 0 ) );
						break;

					case EEditableMeshElementType::Edge:
					{
						FVertexID EdgeVertexID0, EdgeVertexID1;
						EditableMesh->GetEdgeVertices( FEdgeID( MeshElement.ElementAddress.ElementID ), /* Out */ EdgeVertexID0, /* Out */ EdgeVertexID1 );

						BoundingBox += ComponentToBoundingBox.TransformPosition( EditableMesh->GetVertexAttribute( EdgeVertexID0, UEditableMeshAttribute::VertexPosition(), 0 ) );
						BoundingBox += ComponentToBoundingBox.TransformPosition( EditableMesh->GetVertexAttribute( EdgeVertexID1, UEditableMeshAttribute::VertexPosition(), 0 ) );
					}
					break;

					case EEditableMeshElementType::Polygon:
					{
						const FPolygonID PolygonID( MeshElement.ElementAddress.ElementID );

						static TArray<FVertexID> PerimeterVertexIDs;
						EditableMesh->GetPolygonPerimeterVertices( PolygonID, /* Out */ PerimeterVertexIDs );

						for( const FVertexID VertexID : PerimeterVertexIDs )
						{
							BoundingBox += ComponentToBoundingBox.TransformPosition( EditableMesh->GetVertexAttribute( VertexID, UEditableMeshAttribute::VertexPosition(), 0 ) );
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