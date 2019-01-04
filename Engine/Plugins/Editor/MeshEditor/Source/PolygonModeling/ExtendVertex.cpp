// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ExtendVertex.h"
#include "IMeshEditorModeEditingContract.h"
#include "IMeshEditorModeUIContract.h"
#include "Framework/Commands/UICommandInfo.h"
#include "EditableMesh.h"
#include "MeshElement.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/UICommandList.h"
#include "ViewportInteractor.h"

#define LOCTEXT_NAMESPACE "MeshEditorMode"


void UExtendVertexCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "ExtendVertex", "Extend", "Creates a new triangle from the selected vertex by clicking and dragging outward from the vertex.  The new triangle will connect to the next closest neighbor vertex.", EUserInterfaceActionType::RadioButton, FInputChord() );
}


void UExtendVertexCommand::ApplyDuringDrag( IMeshEditorModeEditingContract& MeshEditorMode, UViewportInteractor* ViewportInteractor )
{
	static TMap< UEditableMesh*, TArray< FMeshElement > > MeshesWithVerticesToExtend;
	MeshEditorMode.GetSelectedMeshesAndVertices( /* Out */ MeshesWithVerticesToExtend );

	if( MeshesWithVerticesToExtend.Num() > 0 )
	{
		MeshEditorMode.DeselectAllMeshElements();

		static TArray<FMeshElement> MeshElementsToSelect;
		MeshElementsToSelect.Reset();

		for( auto& MeshAndVertices : MeshesWithVerticesToExtend )
		{
			UEditableMesh* EditableMesh = MeshAndVertices.Key;
			const TArray<FMeshElement>& VertexElementsToExtend = MeshAndVertices.Value;

			static TArray<FVertexID> VertexIDsToExtend;
			VertexIDsToExtend.Reset();

			for( const FMeshElement& VertexElementToExtend : VertexElementsToExtend )
			{
				const FVertexID VertexID( VertexElementToExtend.ElementAddress.ElementID );

				VertexIDsToExtend.Add( VertexID );
			}


			verify( !EditableMesh->AnyChangesToUndo() );

			{
				UPrimitiveComponent* ComponentPtr = VertexElementsToExtend[ 0 ].Component.Get();
				check( ComponentPtr != nullptr );
				UPrimitiveComponent& Component = *ComponentPtr;
				const FMatrix ComponentToWorldMatrix = Component.GetRenderMatrix();

				const FVector ComponentSpaceDragToLocation = ComponentToWorldMatrix.InverseTransformPosition( ViewportInteractor->GetInteractorData().LastDragToLocation );

				// Extend the vertex
				const bool bOnlyExtendClosestEdge = true;	// @todo mesheditor urgent extrude: Make optional somehow
				static TArray<FVertexID> NewExtendedVertexIDs;
				NewExtendedVertexIDs.Reset();
				EditableMesh->ExtendVertices( VertexIDsToExtend, bOnlyExtendClosestEdge, ComponentSpaceDragToLocation, /* Out */ NewExtendedVertexIDs );


				// Make sure the new vertices are selected
				for( int32 NewVertexNumber = 0; NewVertexNumber < NewExtendedVertexIDs.Num(); ++NewVertexNumber )
				{
					const FVertexID NewExtendedVertexID = NewExtendedVertexIDs[ NewVertexNumber ];

					const FMeshElement& MeshElement = VertexElementsToExtend[ NewVertexNumber ];

					FMeshElement NewExtendedVertexMeshElement;
					{
						NewExtendedVertexMeshElement.Component = MeshElement.Component;
						NewExtendedVertexMeshElement.ElementAddress = MeshElement.ElementAddress;
						NewExtendedVertexMeshElement.ElementAddress.ElementID = NewExtendedVertexID;
					}

					// Queue selection of this new element.  We don't want it to be part of the current action.
					MeshElementsToSelect.Add( NewExtendedVertexMeshElement );
				}
			}

			MeshEditorMode.TrackUndo( EditableMesh, EditableMesh->MakeUndo() );
		}

		MeshEditorMode.SelectMeshElements( MeshElementsToSelect );
	}
}


void UExtendVertexCommand::AddToVRRadialMenuActionsMenu( IMeshEditorModeUIContract& MeshEditorMode, FMenuBuilder& MenuBuilder, TSharedPtr<FUICommandList> CommandList, const FName TEMPHACK_StyleSetName, class UVREditorMode* VRMode )
{
	if( MeshEditorMode.GetMeshElementSelectionMode() == EEditableMeshElementType::Vertex )
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT( "VRExtendVertex", "Extend" ),
			FText(),
			FSlateIcon( TEMPHACK_StyleSetName, "MeshEditorMode.VertexExtend" ),
			MakeUIAction( MeshEditorMode ),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	}
}



#undef LOCTEXT_NAMESPACE

