// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RemoveEdge.h"
#include "IMeshEditorModeEditingContract.h"
#include "IMeshEditorModeUIContract.h"
#include "Framework/Commands/UICommandInfo.h"
#include "EditableMesh.h"
#include "MeshElement.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "MeshEditorMode"


void URemoveEdgeCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "RemoveEdge", "Remove", "Attempts to remove the selected edge and merge adjacent polygons.", EUserInterfaceActionType::Button, FInputChord( EKeys::BackSpace ) );
}


void URemoveEdgeCommand::Execute( IMeshEditorModeEditingContract& MeshEditorMode )
{
	if( MeshEditorMode.GetActiveAction() != NAME_None )
	{
		return;
	}

	static TMap< UEditableMesh*, TArray< FMeshElement > > MeshesWithEdgesToRemove;
	MeshEditorMode.GetSelectedMeshesAndEdges( MeshesWithEdgesToRemove );

	// @todo mesheditor: Only if one edge is selected, for now.  It gets a bit confusing when performing this operation
	// with more than one edge selected.  However, it can be very useful when collapsing away edges that don't share any
	// common polygons, so we should try to support it.
	if( MeshesWithEdgesToRemove.Num() != 1 )
	{
		const auto& FirstMeshWithEdges = MeshesWithEdgesToRemove.CreateConstIterator();
		if( FirstMeshWithEdges->Get<1>().Num() != 1 )
		{
			return;
		}
	}

	FScopedTransaction Transaction( LOCTEXT( "UndoRemoveEdge", "Remove Edge" ) );

	MeshEditorMode.CommitSelectedMeshes();

	// Refresh selection (committing may have created a new mesh instance)
	MeshEditorMode.GetSelectedMeshesAndEdges( MeshesWithEdgesToRemove );

	MeshEditorMode.DeselectMeshElements( MeshesWithEdgesToRemove );

	TArray<FMeshElement> MeshElementsToSelect;
	for( const auto& MeshAndElements : MeshesWithEdgesToRemove )
	{
		UEditableMesh* EditableMesh = MeshAndElements.Key;

		EditableMesh->StartModification( EMeshModificationType::Final, EMeshTopologyChange::TopologyChange );

		const TArray<FMeshElement>& EdgeElementsToRemove = MeshAndElements.Value;

		for( const FMeshElement& EdgeElementToRemove : EdgeElementsToRemove )
		{
			const FEdgeID EdgeID( EdgeElementToRemove.ElementAddress.ElementID );
			{
				bool bWasEdgeRemoved = false;
				FPolygonID NewPolygonID;

				EditableMesh->TryToRemovePolygonEdge( EdgeID, /* Out */ bWasEdgeRemoved, /* Out */ NewPolygonID );

				if( bWasEdgeRemoved )
				{
					// Select the new polygon
					FMeshElement NewPolygonMeshElement;
					{
						NewPolygonMeshElement.Component = EdgeElementToRemove.Component;
						NewPolygonMeshElement.ElementAddress = EdgeElementToRemove.ElementAddress;
						NewPolygonMeshElement.ElementAddress.ElementType = EEditableMeshElementType::Polygon;
						NewPolygonMeshElement.ElementAddress.ElementID = NewPolygonID;
					}

					MeshElementsToSelect.Add( NewPolygonMeshElement );
				}
				else
				{
					// Couldn't remove the edge
					// @todo mesheditor: Needs good user feedback when this happens
				}
			}
		}

		EditableMesh->EndModification();

		MeshEditorMode.TrackUndo( EditableMesh, EditableMesh->MakeUndo() );
	}

	// Select the polygon leftover after removing the edge
	MeshEditorMode.SelectMeshElements( MeshElementsToSelect );
}


void URemoveEdgeCommand::AddToVRRadialMenuActionsMenu( IMeshEditorModeUIContract& MeshEditorMode, FMenuBuilder& MenuBuilder, TSharedPtr<FUICommandList> CommandList, const FName TEMPHACK_StyleSetName, class UVREditorMode* VRMode )
{
	if( MeshEditorMode.GetMeshElementSelectionMode() == EEditableMeshElementType::Edge )
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT( "VRRemoveEdge", "Remove" ),
			FText(),
			FSlateIcon( TEMPHACK_StyleSetName, "MeshEditorMode.EdgeRemove" ),
			MakeUIAction( MeshEditorMode ),
			NAME_None,
			EUserInterfaceActionType::CollapsedButton
		);
	}
}


#undef LOCTEXT_NAMESPACE

