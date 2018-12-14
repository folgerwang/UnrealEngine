// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ExtendEdge.h"
#include "IMeshEditorModeEditingContract.h"
#include "IMeshEditorModeUIContract.h"
#include "Framework/Commands/UICommandInfo.h"
#include "EditableMesh.h"
#include "MeshElement.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/UICommandList.h"
#include "ViewportInteractor.h"

#define LOCTEXT_NAMESPACE "MeshEditorMode"


void UExtendEdgeCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "ExtendEdge", "Extend", "Creates a new polygon from the selected edge by clicking and dragging outward from an edge.", EUserInterfaceActionType::RadioButton, FInputChord() );
}


void UExtendEdgeCommand::ApplyDuringDrag( IMeshEditorModeEditingContract& MeshEditorMode, UViewportInteractor* ViewportInteractor )
{
	// Extend edge
	static TMap< UEditableMesh*, TArray<FMeshElement> > MeshesWithEdgesToExtend;
	MeshEditorMode.GetSelectedMeshesAndEdges( /* Out */ MeshesWithEdgesToExtend );

	if( MeshesWithEdgesToExtend.Num() > 0 )
	{
		MeshEditorMode.DeselectAllMeshElements();

		static TArray<FMeshElement> MeshElementsToSelect;
		MeshElementsToSelect.Reset();

		for( auto& MeshAndEdges : MeshesWithEdgesToExtend )
		{
			UEditableMesh* EditableMesh = MeshAndEdges.Key;
			const TArray<FMeshElement>& EdgeElementsToExtend = MeshAndEdges.Value;

			static TArray<FEdgeID> EdgeIDsToExtend;
			EdgeIDsToExtend.Reset();

			for( const FMeshElement& EdgeElementToExtend : EdgeElementsToExtend )
			{
				const FEdgeID EdgeID( EdgeElementToExtend.ElementAddress.ElementID );

				EdgeIDsToExtend.Add( EdgeID );
			}


			verify( !EditableMesh->AnyChangesToUndo() );

			{
				// Extend the edge
				const bool bWeldNeighbors = true;	// @todo mesheditor urgent extrude: Make optional somehow
				static TArray<FEdgeID> NewExtendedEdgeIDs;
				NewExtendedEdgeIDs.Reset();
				EditableMesh->ExtendEdges( EdgeIDsToExtend, bWeldNeighbors, /* Out */ NewExtendedEdgeIDs );

				// Make sure the new edges are selected
				for( int32 NewEdgeNumber = 0; NewEdgeNumber < NewExtendedEdgeIDs.Num(); ++NewEdgeNumber )
				{
					const FEdgeID NewExtendedEdgeID = NewExtendedEdgeIDs[ NewEdgeNumber ];

					const FMeshElement& MeshElement = EdgeElementsToExtend[ NewEdgeNumber ];

					FMeshElement NewExtendedEdgeMeshElement;
					{
						NewExtendedEdgeMeshElement.Component = MeshElement.Component;
						NewExtendedEdgeMeshElement.ElementAddress = MeshElement.ElementAddress;
						NewExtendedEdgeMeshElement.ElementAddress.ElementID = NewExtendedEdgeID;
					}

					// Queue selection of this new element.  We don't want it to be part of the current action.
					MeshElementsToSelect.Add( NewExtendedEdgeMeshElement );
				}
			}

			MeshEditorMode.TrackUndo( EditableMesh, EditableMesh->MakeUndo() );
		}

		MeshEditorMode.SelectMeshElements( MeshElementsToSelect );
	}
}


void UExtendEdgeCommand::AddToVRRadialMenuActionsMenu( IMeshEditorModeUIContract& MeshEditorMode, FMenuBuilder& MenuBuilder, TSharedPtr<FUICommandList> CommandList, const FName TEMPHACK_StyleSetName, class UVREditorMode* VRMode )
{
	if( MeshEditorMode.GetMeshElementSelectionMode() == EEditableMeshElementType::Edge )
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT( "VRExtendEdge", "Extend" ),
			FText(),
			FSlateIcon( TEMPHACK_StyleSetName, "MeshEditorMode.EdgeExtend" ),
			MakeUIAction( MeshEditorMode ),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	}
}



#undef LOCTEXT_NAMESPACE

