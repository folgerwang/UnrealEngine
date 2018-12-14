// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RemoveVertex.h"
#include "IMeshEditorModeEditingContract.h"
#include "IMeshEditorModeUIContract.h"
#include "Framework/Commands/UICommandInfo.h"
#include "EditableMesh.h"
#include "MeshElement.h"
#include "ScopedTransaction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/UICommandList.h"


#define LOCTEXT_NAMESPACE "MeshEditorMode"


void URemoveVertexCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "RemoveVertex", "Remove", "Attempts to remove the selected vertex, keeping the polygon intact.", EUserInterfaceActionType::Button, FInputChord( EKeys::BackSpace ) );
}


void URemoveVertexCommand::Execute( IMeshEditorModeEditingContract& MeshEditorMode )
{
	if( MeshEditorMode.GetActiveAction() != NAME_None )
	{
		return;
	}

	static TMap< UEditableMesh*, TArray<FMeshElement> > MeshesWithVerticesToRemove;
	MeshEditorMode.GetSelectedMeshesAndVertices( MeshesWithVerticesToRemove );

	// @todo mesheditor: Only if one vertex is selected, for now.  It gets a bit confusing when performing this operation
	// with more than one vertex selected.  However, it can be very useful when collapsing away vertices that don't share any
	// common polygons, so we should try to support it.
	if( MeshesWithVerticesToRemove.Num() != 1 )
	{
		const auto& FirstMeshWithVertices = MeshesWithVerticesToRemove.CreateConstIterator();
		if( FirstMeshWithVertices->Get<1>().Num() != 1 )
		{
			return;
		}
	}

	FScopedTransaction Transaction( LOCTEXT( "UndoRemoveVertex", "Remove Vertex" ) );

	MeshEditorMode.CommitSelectedMeshes();

	// Refresh selection (committing may have created a new mesh instance)
	MeshEditorMode.GetSelectedMeshesAndVertices( MeshesWithVerticesToRemove );

	// Deselect the mesh elements before we delete them.  This will make sure they become selected again after undo.
	MeshEditorMode.DeselectMeshElements( MeshesWithVerticesToRemove );

	TArray<FMeshElement> MeshElementsToSelect;
	for( const auto& MeshAndElements : MeshesWithVerticesToRemove )
	{
		UEditableMesh* EditableMesh = MeshAndElements.Key;

		EditableMesh->StartModification( EMeshModificationType::Final, EMeshTopologyChange::TopologyChange );

		const TArray<FMeshElement>& VertexElementsToRemove = MeshAndElements.Value;

		for( const FMeshElement& VertexElementToRemove : VertexElementsToRemove )
		{
			const FVertexID VertexID( VertexElementToRemove.ElementAddress.ElementID );
			{
				bool bWasVertexRemoved = false;
				FEdgeID NewEdgeID = FEdgeID::Invalid;

				EditableMesh->TryToRemoveVertex( VertexID, /* Out */ bWasVertexRemoved, /* Out */ NewEdgeID );

				if( bWasVertexRemoved )
				{
					// Select the new edge
					FMeshElement NewEdgeMeshElement;
					{
						NewEdgeMeshElement.Component = VertexElementToRemove.Component;
						NewEdgeMeshElement.ElementAddress = VertexElementToRemove.ElementAddress;
						NewEdgeMeshElement.ElementAddress.ElementType = EEditableMeshElementType::Edge;
						NewEdgeMeshElement.ElementAddress.ElementID = NewEdgeID;
					}

					MeshElementsToSelect.Add( NewEdgeMeshElement );
				}
				else
				{
					// Couldn't remove the vertex
					// @todo mesheditor: Needs good user feedback when this happens
					// @todo mesheditor: If this fails, it will already potentially have created a new instance. To be 100% correct, it needs to do a prepass
					// to determine whether the operation can complete successfully before actually doing it.
				}
			}
		}

		EditableMesh->EndModification();

		MeshEditorMode.TrackUndo( EditableMesh, EditableMesh->MakeUndo() );
	}

	MeshEditorMode.SelectMeshElements( MeshElementsToSelect );
}


void URemoveVertexCommand::AddToVRRadialMenuActionsMenu( IMeshEditorModeUIContract& MeshEditorMode, FMenuBuilder& MenuBuilder, TSharedPtr<FUICommandList> CommandList, const FName TEMPHACK_StyleSetName, class UVREditorMode* VRMode )
{
	if( MeshEditorMode.GetMeshElementSelectionMode() == EEditableMeshElementType::Vertex )
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT( "VRRemoveVertex", "Remove" ),
			FText(),
			FSlateIcon( TEMPHACK_StyleSetName, "MeshEditorMode.VertexRemove" ),
			MakeUIAction( MeshEditorMode ),
			NAME_None,
			EUserInterfaceActionType::CollapsedButton
		);
	}
}


#undef LOCTEXT_NAMESPACE

