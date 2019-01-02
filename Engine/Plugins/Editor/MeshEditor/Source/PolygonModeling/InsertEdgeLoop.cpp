// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "InsertEdgeLoop.h"
#include "IMeshEditorModeEditingContract.h"
#include "IMeshEditorModeUIContract.h"
#include "Framework/Commands/UICommandInfo.h"
#include "EditableMesh.h"
#include "MeshElement.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/UICommandList.h"


#define LOCTEXT_NAMESPACE "MeshEditorMode"


void UInsertEdgeLoopCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "InsertEdgeLoop", "Insert Edge Loop", "Inserts a loop of edges at a specific location along the selected edge as you click and drag.  If a valid loop cannot be determined, no edges will be inserted.", EUserInterfaceActionType::RadioButton, FInputChord() );
}


void UInsertEdgeLoopCommand::ApplyDuringDrag( IMeshEditorModeEditingContract& MeshEditorMode, UViewportInteractor* ViewportInteractor )
{
	// Insert edge loop
	static TMap< UEditableMesh*, TArray< FMeshElement > > SelectedMeshesAndEdges;
	MeshEditorMode.GetSelectedMeshesAndEdges( /* Out */ SelectedMeshesAndEdges );

	if( SelectedMeshesAndEdges.Num() > 0 )
	{
		// Deselect the edges first, since they'll be deleted or split up while inserting the edge loop,
		// and we want them to be re-selected after undo
		MeshEditorMode.DeselectAllMeshElements();

		static TArray<FMeshElement> MeshElementsToSelect;
		MeshElementsToSelect.Reset();

		for( auto& MeshAndEdges : SelectedMeshesAndEdges )
		{
			UEditableMesh* EditableMesh = MeshAndEdges.Key;
			verify( !EditableMesh->AnyChangesToUndo() );

			const TArray<FMeshElement>& EdgeElements = MeshAndEdges.Value;

			// Figure out where to add the loop along the edge
			FEdgeID ClosestEdgeID = FEdgeID::Invalid;
			float Split = 0.0f;
			const bool bFoundSplit = MeshEditorMode.FindEdgeSplitUnderInteractor( MeshEditorMode.GetActiveActionInteractor(), EditableMesh, EdgeElements, /* Out */ ClosestEdgeID, /* Out */ Split );

			// Insert the edge loop
			if( bFoundSplit )
			{
				for( const FMeshElement& EdgeMeshElement : EdgeElements )
				{
					const FEdgeID EdgeID( EdgeMeshElement.ElementAddress.ElementID );

					static TArray<FEdgeID> NewEdgeIDs;
					NewEdgeIDs.Reset();

					static TArray<float> Splits;	// @todo mesheditor edgeloop: Add support for inserting multiple splits at once!
					Splits.Reset();
					Splits.Add( Split );
					EditableMesh->InsertEdgeLoop( EdgeID, Splits, /* Out */ NewEdgeIDs );

					// Select all of the new edges that were created by inserting the loop
					if( NewEdgeIDs.Num() > 0 )
					{
						for( const FEdgeID NewEdgeID : NewEdgeIDs )
						{
							FMeshElement MeshElementToSelect;
							{
								MeshElementToSelect.Component = EdgeMeshElement.Component;
								MeshElementToSelect.ElementAddress.SubMeshAddress = EdgeMeshElement.ElementAddress.SubMeshAddress;
								MeshElementToSelect.ElementAddress.ElementType = EEditableMeshElementType::Edge;
								MeshElementToSelect.ElementAddress.ElementID = NewEdgeID;
							}

							// Queue selection of this new element.  We don't want it to be part of the current action.
							MeshElementsToSelect.Add( MeshElementToSelect );
						}
					}
				}

				MeshEditorMode.TrackUndo( EditableMesh, EditableMesh->MakeUndo() );
			}
		}

		MeshEditorMode.SelectMeshElements( MeshElementsToSelect );
	}
}


void UInsertEdgeLoopCommand::AddToVRRadialMenuActionsMenu( IMeshEditorModeUIContract& MeshEditorMode, FMenuBuilder& MenuBuilder, TSharedPtr<FUICommandList> CommandList, const FName TEMPHACK_StyleSetName, class UVREditorMode* VRMode )
{
	if( MeshEditorMode.GetMeshElementSelectionMode() == EEditableMeshElementType::Edge )
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("VRInsertEdgeLoop", "Insert Loop"),
			FText(),
			FSlateIcon(TEMPHACK_StyleSetName, "MeshEditorMode.EdgeInsert"),	// @todo mesheditor extensibility: TEMPHACK for style; Need PolygonModelingStyle, probably.  Or we're just cool with exporting MeshEditorModeStyle, since we're all the same plugin after all.
			MakeUIAction( MeshEditorMode ),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
			);
	}
}


#undef LOCTEXT_NAMESPACE

