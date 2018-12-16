// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DeleteMeshElement.h"

#include "ContentBrowserModule.h"
#include "EditableMesh.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IContentBrowserSingleton.h"
#include "IMeshEditorModeEditingContract.h"
#include "IMeshEditorModeUIContract.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "MeshEditorMode"

void UDeleteMeshElementCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "DeleteMeshElement", "Delete", "Delete selected mesh elements, including polygons partly defined by selected elements.", EUserInterfaceActionType::Button, FInputChord( EKeys::Delete ) );
}

void UDeleteMeshElementCommand::Execute( IMeshEditorModeEditingContract& MeshEditorMode )
{
	if( MeshEditorMode.GetActiveAction() != NAME_None )
	{
		return;
	}

	TMap< UEditableMesh*, TArray< FMeshElement > > MeshesWithElementsToDelete;
	MeshEditorMode.GetSelectedMeshesAndElements( EEditableMeshElementType::Any, MeshesWithElementsToDelete );
	if( MeshesWithElementsToDelete.Num() == 0 )
	{
		return;
	}

	FScopedTransaction Transaction( LOCTEXT( "UndoDeleteMeshElement", "Delete" ) );

	MeshEditorMode.CommitSelectedMeshes();

	// Refresh selection (committing may have created a new mesh instance)
	MeshEditorMode.GetSelectedMeshesAndElements( EEditableMeshElementType::Any, MeshesWithElementsToDelete );

	// Deselect the mesh elements before we delete them.  This will make sure they become selected again after undo.
	MeshEditorMode.DeselectMeshElements( MeshesWithElementsToDelete );

	for( const auto& MeshAndElements : MeshesWithElementsToDelete )
	{
		UEditableMesh* EditableMesh = MeshAndElements.Key;

		EditableMesh->StartModification( EMeshModificationType::Final, EMeshTopologyChange::TopologyChange );

		for( const FMeshElement& MeshElementToDelete : MeshAndElements.Value )
		{
			const bool bDeleteOrphanedEdges = true;
			const bool bDeleteOrphanedVertices = true;
			const bool bDeleteOrphanedVertexInstances = true;
			const bool bDeleteEmptySections = true;

			// If we deleted the same polygon on multiple selected instances of the same mesh, the polygon could already have been deleted
			// by the time we get here
			if( MeshElementToDelete.IsElementIDValid( EditableMesh ) )
			{
				if( MeshElementToDelete.ElementAddress.ElementType == EEditableMeshElementType::Vertex )
				{
					EditableMesh->DeleteVertexAndConnectedEdgesAndPolygons(
						FVertexID( MeshElementToDelete.ElementAddress.ElementID ),
						bDeleteOrphanedEdges,
						bDeleteOrphanedVertices,
						bDeleteOrphanedVertexInstances,
						bDeleteEmptySections );

				}
				else if( MeshElementToDelete.ElementAddress.ElementType == EEditableMeshElementType::Edge )
				{
					EditableMesh->DeleteEdgeAndConnectedPolygons(
						FEdgeID( MeshElementToDelete.ElementAddress.ElementID ),
						bDeleteOrphanedEdges,
						bDeleteOrphanedVertices,
						bDeleteOrphanedVertexInstances,
						bDeleteEmptySections );
				}
				else if( MeshElementToDelete.ElementAddress.ElementType == EEditableMeshElementType::Polygon )
				{
					static TArray<FPolygonID> PolygonIDsToDelete;
					PolygonIDsToDelete.Reset();
					PolygonIDsToDelete.Add( FPolygonID( MeshElementToDelete.ElementAddress.ElementID ) );
					EditableMesh->DeletePolygons( 
						PolygonIDsToDelete,
						bDeleteOrphanedEdges,
						bDeleteOrphanedVertices,
						bDeleteOrphanedVertexInstances,
						bDeleteEmptySections );
				}
			}
		}

		EditableMesh->EndModification();

		MeshEditorMode.TrackUndo( EditableMesh, EditableMesh->MakeUndo() );
	}
}

void UDeleteMeshElementCommand::AddToVRRadialMenuActionsMenu( IMeshEditorModeUIContract& MeshEditorMode, FMenuBuilder& MenuBuilder, TSharedPtr<FUICommandList> CommandList, const FName TEMPHACK_StyleSetName, class UVREditorMode* VRMode )
{
	FSlateIcon DeleteIcon;
	switch ( MeshEditorMode.GetMeshElementSelectionMode() )
	{
	case EEditableMeshElementType::Vertex:
		DeleteIcon = FSlateIcon( TEMPHACK_StyleSetName, "MeshEditorMode.VertexDelete" );
		break;
	case EEditableMeshElementType::Edge:
		DeleteIcon = FSlateIcon( TEMPHACK_StyleSetName, "MeshEditorMode.EdgeDelete" );
		break;
	case EEditableMeshElementType::Polygon:
		DeleteIcon = FSlateIcon( TEMPHACK_StyleSetName, "MeshEditorMode.PolyDelete" );
		break;
	}

	if ( MeshEditorMode.GetMeshElementSelectionMode() == EEditableMeshElementType::Vertex ||
		MeshEditorMode.GetMeshElementSelectionMode() == EEditableMeshElementType::Edge ||
		MeshEditorMode.GetMeshElementSelectionMode() == EEditableMeshElementType::Polygon )
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("Delete", "Delete"),
			FText(),
			DeleteIcon,
			MakeUIAction( MeshEditorMode ),
			NAME_None,
			EUserInterfaceActionType::CollapsedButton
		);
	}
}

#undef LOCTEXT_NAMESPACE
