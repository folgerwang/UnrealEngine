// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "EditableMeshChanges.h"
#include "EditableMesh.h"


TUniquePtr<FChange> FDeleteOrphanVerticesChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );

	EditableMesh->DeleteOrphanVertices( Input.VertexIDsToDelete );

	return EditableMesh->MakeUndo();
}


FString FDeleteOrphanVerticesChange::ToString() const
{
	return FString::Printf(
		TEXT( "Delete Orphan Vertices [VertexIDsToDelete:%s]" ),
		*LogHelpers::ArrayToString( Input.VertexIDsToDelete ) );
}


TUniquePtr<FChange> FDeleteVertexInstancesChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );

	EditableMesh->DeleteVertexInstances( Input.VertexInstanceIDsToDelete, Input.bDeleteOrphanedVertices );

	return EditableMesh->MakeUndo();
}


FString FDeleteVertexInstancesChange::ToString() const
{
	return FString::Printf(
		TEXT( "Delete Vertex Instances [VertexInstanceIDsToDelete:%s]" ),
		*LogHelpers::ArrayToString( Input.VertexInstanceIDsToDelete ) );
}


TUniquePtr<FChange> FDeleteEdgesChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );

	EditableMesh->DeleteEdges( Input.EdgeIDsToDelete, Input.bDeleteOrphanedVertices );

	return EditableMesh->MakeUndo();
}


FString FDeleteEdgesChange::ToString() const
{
	return FString::Printf(
		TEXT( "Delete Edges [EdgeIDsToDelete:%s, bDeleteOrphanedVertices:%s]" ),
		*LogHelpers::ArrayToString( Input.EdgeIDsToDelete ),
		*LogHelpers::BoolToString( Input.bDeleteOrphanedVertices ) );
}


TUniquePtr<FChange> FCreateVerticesChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );

	static TArray<FVertexID> UnusedNewVertexIDs;
	EditableMesh->CreateVertices( Input.VerticesToCreate, /* Out */ UnusedNewVertexIDs );

	return EditableMesh->MakeUndo();
}


FString FCreateVerticesChange::ToString() const
{
	return FString::Printf(
		TEXT( "Create Vertices [VerticesToCreate:%s]" ),
		*LogHelpers::ArrayToString( Input.VerticesToCreate ) );
}


TUniquePtr<FChange> FCreateVertexInstancesChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );

	static TArray<FVertexInstanceID> UnusedNewVertexInstanceIDs;
	EditableMesh->CreateVertexInstances( Input.VertexInstancesToCreate, /* Out */ UnusedNewVertexInstanceIDs );

	return EditableMesh->MakeUndo();
}


FString FCreateVertexInstancesChange::ToString() const
{
	return FString::Printf(
		TEXT( "Create Vertex Instances [VertexInstancesToCreate:%s]" ),
		*LogHelpers::ArrayToString( Input.VertexInstancesToCreate ) );
}


TUniquePtr<FChange> FCreateEdgesChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );

	static TArray<FEdgeID> UnusedNewEdgeIDs;
	EditableMesh->CreateEdges( Input.EdgesToCreate, /* Out */ UnusedNewEdgeIDs );

	return EditableMesh->MakeUndo();
}


FString FCreateEdgesChange::ToString() const
{
	return FString::Printf(
		TEXT( "Create Edges [EdgesToCreate:%s]" ),
		*LogHelpers::ArrayToString( Input.EdgesToCreate ) );
}


TUniquePtr<FChange> FCreatePolygonsChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );
	
	static TArray<FPolygonID> UnusedNewPolygonIDs;
	static TArray<FEdgeID> UnusedNewEdgeIDs;
	EditableMesh->CreatePolygons( Input.PolygonsToCreate, /* Out */ UnusedNewPolygonIDs, /* Out */ UnusedNewEdgeIDs );

	return EditableMesh->MakeUndo();
}


FString FCreatePolygonsChange::ToString() const
{
	return FString::Printf(
		TEXT( "Create Polygons [PolygonsToCreate:%s]" ),
		*LogHelpers::ArrayToString( Input.PolygonsToCreate ) );
}


TUniquePtr<FChange> FDeletePolygonsChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );

	EditableMesh->DeletePolygons( Input.PolygonIDsToDelete, Input.bDeleteOrphanedEdges, Input.bDeleteOrphanedVertices, Input.bDeleteOrphanedVertexInstances, Input.bDeleteEmptySections );
	return EditableMesh->MakeUndo();
}


FString FDeletePolygonsChange::ToString() const
{
	return FString::Printf(
		TEXT( "Delete Polygons [PolygonIDsToDelete:%s, %s, %s]" ),
		*LogHelpers::ArrayToString( Input.PolygonIDsToDelete ),
		*LogHelpers::BoolToString( Input.bDeleteOrphanedEdges ),
		*LogHelpers::BoolToString( Input.bDeleteOrphanedVertices ),
		*LogHelpers::BoolToString( Input.bDeleteEmptySections ) );
}

TUniquePtr<FChange> FFlipPolygonsChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );

	EditableMesh->FlipPolygons( Input.PolygonIDsToFlip );
	return EditableMesh->MakeUndo();
}


FString FFlipPolygonsChange::ToString() const
{
	return FString::Printf(
		TEXT( "Flip Polygons [PolygonIDsToFlip:%s]" ),
		*LogHelpers::ArrayToString( Input.PolygonIDsToFlip ) );
}

TUniquePtr<FChange> FSetVerticesAttributesChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );

	EditableMesh->SetVerticesAttributes( Input.AttributesForVertices );
	return EditableMesh->MakeUndo();
}


FString FSetVerticesAttributesChange::ToString() const
{
	return FString::Printf(
		TEXT( "Set Vertices Attributes [AttributesForVertices:%s]" ),
		*LogHelpers::ArrayToString( Input.AttributesForVertices ) );
}


TUniquePtr<FChange> FSetVertexInstancesAttributesChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );

	EditableMesh->SetVertexInstancesAttributes( Input.AttributesForVertexInstances );
	return EditableMesh->MakeUndo();
}


FString FSetVertexInstancesAttributesChange::ToString() const
{
	return FString::Printf(
		TEXT( "Set Vertex Instances Attributes [AttributesForVertexInstances:%s]" ),
		*LogHelpers::ArrayToString( Input.AttributesForVertexInstances ) );
}


TUniquePtr<FChange> FSetEdgesAttributesChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );

	EditableMesh->SetEdgesAttributes( Input.AttributesForEdges );
	return EditableMesh->MakeUndo();
}


FString FSetEdgesAttributesChange::ToString() const
{
	return FString::Printf(
		TEXT( "Set Edges Attributes [AttributesForEdges:%s]" ),
		*LogHelpers::ArrayToString( Input.AttributesForEdges ) );
}


TUniquePtr<FChange> FSetPolygonsVertexAttributesChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );

	EditableMesh->SetPolygonsVertexAttributes( Input.VertexAttributesForPolygons );
	return EditableMesh->MakeUndo();
}


FString FSetPolygonsVertexAttributesChange::ToString() const
{
	return FString::Printf(
		TEXT( "Set Polygons Vertex Attributes [VertexAttributesForPolygons:%s]" ),
		*LogHelpers::ArrayToString( Input.VertexAttributesForPolygons ) );
}


TUniquePtr<FChange> FChangePolygonsVertexInstancesChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );

	EditableMesh->ChangePolygonsVertexInstances( Input.VertexInstancesForPolygons );
	return EditableMesh->MakeUndo();
}


FString FChangePolygonsVertexInstancesChange::ToString() const
{
	return FString::Printf(
		TEXT( "Change Polygons Vertex Instances [VertexInstancesForPolygons:%s]" ),
		*LogHelpers::ArrayToString( Input.VertexInstancesForPolygons ) );
}


TUniquePtr<FChange> FSetEdgesVerticesChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );

	EditableMesh->SetEdgesVertices( Input.VerticesForEdges );
	return EditableMesh->MakeUndo();
}



FString FSetEdgesVerticesChange::ToString() const
{
	return FString::Printf(
		TEXT( "Set Edges Vertices [VerticesForEdges:%s]" ),
		*LogHelpers::ArrayToString( Input.VerticesForEdges ) );
}


TUniquePtr<FChange> FInsertPolygonPerimeterVerticesChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );

	EditableMesh->InsertPolygonPerimeterVertices( Input.PolygonID, Input.InsertBeforeVertexNumber, Input.VerticesToInsert );
	return EditableMesh->MakeUndo();
}


FString FInsertPolygonPerimeterVerticesChange::ToString() const
{
	return FString::Printf(
		TEXT( "Insert Polygon Perimeter Vertices [PolygonID:%s, InsertBeforeVertexNumber:%lu, VerticesToInsert:%s]" ),
		*Input.PolygonID.ToString(),
		Input.InsertBeforeVertexNumber,
		*LogHelpers::ArrayToString( Input.VerticesToInsert ) );
}


TUniquePtr<FChange> FRemovePolygonPerimeterVerticesChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );

	EditableMesh->RemovePolygonPerimeterVertices( Input.PolygonID, Input.FirstVertexNumberToRemove, Input.NumVerticesToRemove, Input.bDeleteOrphanedVertexInstances );
	return EditableMesh->MakeUndo();
}


FString FRemovePolygonPerimeterVerticesChange::ToString() const
{
	return FString::Printf(
		TEXT( "Remove Polygon Perimeter Vertices [PolygonID:%s, FirstVertexNumberToRemove:%lu, NumVerticesToRemove:%lu]" ),
		*Input.PolygonID.ToString(),
		Input.FirstVertexNumberToRemove,
		Input.NumVerticesToRemove );
}


TUniquePtr<FChange> FStartOrEndModificationChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );

	if( Input.bStartModification )
	{
		EditableMesh->StartModification( Input.MeshModificationType, Input.MeshTopologyChange );
	}
	else
	{
		const bool bFromUndo = true;
		EditableMesh->EndModification( bFromUndo );
	}

	return EditableMesh->MakeUndo();
}


FString FStartOrEndModificationChange::ToString() const
{
	return FString::Printf(
		TEXT( "%s Modification (MeshModificationType:%i, MeshTopologyChange:%i)" ),
		Input.bStartModification ? TEXT( "Start" ) : TEXT( "End" ),
		(int32)Input.MeshModificationType,
		(int32)Input.MeshTopologyChange );
}


TUniquePtr<FChange> FSetSubdivisionCountChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );

	EditableMesh->SetSubdivisionCount( Input.NewSubdivisionCount );
	return EditableMesh->MakeUndo();
}


FString FSetSubdivisionCountChange::ToString() const
{
	return FString::Printf(
		TEXT( "Set Subdivision Count [NewSubdivisionCount:%i]" ),
		Input.NewSubdivisionCount );
}


TUniquePtr<FChange> FCreatePolygonGroupsChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );

	static TArray<FPolygonGroupID> UnusedPolygonGroupIDs;
	EditableMesh->CreatePolygonGroups( Input.PolygonGroupsToCreate, UnusedPolygonGroupIDs );
	return EditableMesh->MakeUndo();
}


FString FCreatePolygonGroupsChange::ToString() const
{
	return FString::Printf(
		TEXT( "Create PolygonGroups [PolygonGroupsToCreate:%s]" ),
		*LogHelpers::ArrayToString( Input.PolygonGroupsToCreate ) );
}


TUniquePtr<FChange> FDeletePolygonGroupsChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );

	EditableMesh->DeletePolygonGroups( Input.PolygonGroupIDs );
	return EditableMesh->MakeUndo();
}


FString FDeletePolygonGroupsChange::ToString() const
{
	return FString::Printf(
		TEXT( "Delete PolygonGroups [PolygonGroupIDs:%s]" ),
		*LogHelpers::ArrayToString( Input.PolygonGroupIDs ) );
}


TUniquePtr<FChange> FAssignPolygonsToPolygonGroupChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );

	EditableMesh->AssignPolygonsToPolygonGroups( Input.PolygonGroupForPolygons, Input.bDeleteOrphanedPolygonGroups );
	return EditableMesh->MakeUndo();
}


FString FAssignPolygonsToPolygonGroupChange::ToString() const
{
	return FString::Printf(
		TEXT( "Assign Polygons To PolygonGroups [PolygonGroupForPolygons:%s]" ),
		*LogHelpers::ArrayToString( Input.PolygonGroupForPolygons ) );
}

