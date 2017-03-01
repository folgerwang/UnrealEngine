// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
	
	static TArray<FPolygonRef> UnusedNewPolygonRefs;
	static TArray<FEdgeID> UnusedNewEdgeIDs;
	EditableMesh->CreatePolygons( Input.PolygonsToCreate, /* Out */ UnusedNewPolygonRefs, /* Out */ UnusedNewEdgeIDs );

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

	EditableMesh->DeletePolygons( Input.PolygonRefsToDelete, Input.bDeleteOrphanedEdges, Input.bDeleteOrphanedVertices, Input.bDeleteEmptySections );
	return EditableMesh->MakeUndo();
}


FString FDeletePolygonsChange::ToString() const
{
	return FString::Printf(
		TEXT( "Delete Polygons [PolygonRefsToDelete:%s, %s, %s]" ),
		*LogHelpers::ArrayToString( Input.PolygonRefsToDelete ),
		*LogHelpers::BoolToString( Input.bDeleteOrphanedEdges ),
		*LogHelpers::BoolToString( Input.bDeleteOrphanedVertices ),
		*LogHelpers::BoolToString( Input.bDeleteEmptySections ) );
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

	EditableMesh->InsertPolygonPerimeterVertices( Input.PolygonRef, Input.InsertBeforeVertexNumber, Input.VerticesToInsert );
	return EditableMesh->MakeUndo();
}


FString FInsertPolygonPerimeterVerticesChange::ToString() const
{
	return FString::Printf(
		TEXT( "Insert Polygon Perimeter Vertices [PolygonRef:%s, InsertBeforeVertexNumber:%lu, VerticesToInsert:%s]" ),
		*Input.PolygonRef.ToString(),
		Input.InsertBeforeVertexNumber,
		*LogHelpers::ArrayToString( Input.VerticesToInsert ) );
}


TUniquePtr<FChange> FRemovePolygonPerimeterVerticesChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );

	EditableMesh->RemovePolygonPerimeterVertices( Input.PolygonRef, Input.FirstVertexNumberToRemove, Input.NumVerticesToRemove );
	return EditableMesh->MakeUndo();
}


FString FRemovePolygonPerimeterVerticesChange::ToString() const
{
	return FString::Printf(
		TEXT( "Remove Polygon Perimeter Vertices [PolygonRef:%s, FirstVertexNumberToRemove:%lu, NumVerticesToRemove:%lu]" ),
		*Input.PolygonRef.ToString(),
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


TUniquePtr<FChange> FRetrianglulatePolygonsChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );

	EditableMesh->RetriangulatePolygons( Input.PolygonRefs, Input.bOnlyOnUndo );
	return EditableMesh->MakeUndo();
}


FString FRetrianglulatePolygonsChange::ToString() const
{
	return FString::Printf(
		TEXT( "Retriangulate Polygons [PolygonRefs:%s, bOnlyOnUndo:%s]" ),
		*LogHelpers::ArrayToString( Input.PolygonRefs ),
		*LogHelpers::BoolToString( Input.bOnlyOnUndo ) );
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


TUniquePtr<FChange> FCreateSectionChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );

	EditableMesh->CreateSection( Input.SectionToCreate );
	return EditableMesh->MakeUndo();
}


FString FCreateSectionChange::ToString() const
{
	return FString::Printf(
		TEXT( "Create Section [Material:%s, bEnableCollision:%i, bCastShadow:%i]" ),
		Input.SectionToCreate.Material ? *Input.SectionToCreate.Material->GetName() : TEXT( "<none>" ),
		Input.SectionToCreate.bEnableCollision,
		Input.SectionToCreate.bCastShadow );
}


TUniquePtr<FChange> FDeleteSectionChange::Execute( UObject* Object )
{
	UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
	verify( !EditableMesh->AnyChangesToUndo() );

	EditableMesh->DeleteSection( Input.SectionID );
	return EditableMesh->MakeUndo();
}


FString FDeleteSectionChange::ToString() const
{
	return FString::Printf(
		TEXT( "Delete Section [SectionID:%i]" ),
		Input.SectionID.GetValue() );
}


