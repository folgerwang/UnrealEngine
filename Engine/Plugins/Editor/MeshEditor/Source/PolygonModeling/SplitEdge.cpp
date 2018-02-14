// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SplitEdge.h"
#include "IMeshEditorModeEditingContract.h"
#include "IMeshEditorModeUIContract.h"
#include "Framework/Commands/UICommandInfo.h"
#include "EditableMesh.h"
#include "MeshElement.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/UICommandList.h"
#include "ViewportInteractor.h"

#define LOCTEXT_NAMESPACE "MeshEditorMode"


void USplitEdgeCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "InsertVertex", "Insert Vertex", "Inserts a vertex at a specific position along an edge as you click and drag, splitting the edge into two.", EUserInterfaceActionType::RadioButton, FInputChord() );
}


bool USplitEdgeCommand::TryStartingToDrag( IMeshEditorModeEditingContract& MeshEditorMode, UViewportInteractor* ViewportInteractor )
{
	bool bHaveEdge = false;

	// Figure out what to split
	MeshEditorMode.GetSelectedMeshesAndEdges( /* Out */ SplitEdgeMeshesAndEdgesToSplit );

	SplitEdgeSplitList.Reset();

	if( SplitEdgeMeshesAndEdgesToSplit.Num() > 0 )
	{
		for( auto& MeshAndEdges : SplitEdgeMeshesAndEdgesToSplit )
		{
			UEditableMesh* EditableMesh = MeshAndEdges.Key;
			const TArray<FMeshElement>& EdgeElements = MeshAndEdges.Value;

			// Figure out where to split
			FEdgeID ClosestEdgeID = FEdgeID::Invalid;
			float Split = 0.0f;
			const bool bFoundSplit = MeshEditorMode.FindEdgeSplitUnderInteractor( ViewportInteractor, EditableMesh, EdgeElements, /* Out */ ClosestEdgeID, /* Out */ Split );

			if( bFoundSplit )
			{
				SplitEdgeSplitList.Add( Split );
				bHaveEdge = true;
				break;
			}
		}
	}

	return bHaveEdge;
}


void USplitEdgeCommand::ApplyDuringDrag( IMeshEditorModeEditingContract& MeshEditorMode, UViewportInteractor* ViewportInteractor )
{
	if( SplitEdgeMeshesAndEdgesToSplit.Num() > 0 && SplitEdgeSplitList.Num() > 0 )
	{
		MeshEditorMode.DeselectAllMeshElements();

		static TArray<FMeshElement> MeshElementsToSelect;
		MeshElementsToSelect.Reset();

		for( auto& MeshAndEdges : SplitEdgeMeshesAndEdgesToSplit )
		{
			UEditableMesh* EditableMesh = MeshAndEdges.Key;
			verify( !EditableMesh->AnyChangesToUndo() );

			const TArray<FMeshElement>& EdgeElements = MeshAndEdges.Value;

			for( const FMeshElement& EdgeElement : EdgeElements )
			{
				const FEdgeID EdgeID( EdgeElement.ElementAddress.ElementID );

				static TArray<FVertexID> NewVertexIDs;
				NewVertexIDs.Reset();

				EditableMesh->SplitEdge( EdgeID, SplitEdgeSplitList, /* Out */ NewVertexIDs );

				// Select all of the new vertices that were created by splitting the edge
				{
					for( const FVertexID NewVertexID : NewVertexIDs )
					{
						FMeshElement MeshElementToSelect;
						{
							MeshElementToSelect.Component = EdgeElement.Component;
							MeshElementToSelect.ElementAddress.SubMeshAddress = EdgeElement.ElementAddress.SubMeshAddress;
							MeshElementToSelect.ElementAddress.ElementType = EEditableMeshElementType::Vertex;
							MeshElementToSelect.ElementAddress.ElementID = NewVertexID;
						}

						// Queue selection of this new element.  We don't want it to be part of the current action.
						MeshElementsToSelect.Add( MeshElementToSelect );
					}
				}
			}

			MeshEditorMode.TrackUndo( EditableMesh, EditableMesh->MakeUndo() );
		}

		MeshEditorMode.SelectMeshElements( MeshElementsToSelect );
	}
}



#undef LOCTEXT_NAMESPACE

