// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "QuadrangulateMesh.h"
#include "IMeshEditorModeEditingContract.h"
#include "IMeshEditorModeUIContract.h"
#include "Framework/Commands/UICommandInfo.h"
#include "EditableMesh.h"
#include "MeshElement.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "MeshEditorMode"


void UQuadrangulateMeshCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "QuadrangulateMesh", "Quadrangulate", "Quadrangulates the selected mesh.", EUserInterfaceActionType::Button, FInputChord() );
}


void UQuadrangulateMeshCommand::Execute( IMeshEditorModeEditingContract& MeshEditorMode )
{
	if( MeshEditorMode.GetActiveAction() != NAME_None )
	{
		return;
	}

	if( MeshEditorMode.GetSelectedEditableMeshes().Num() == 0 )
	{
		return;
	}

	FScopedTransaction Transaction( LOCTEXT( "UndoQuadrangulateMesh", "Quadrangulate Mesh" ) );

	MeshEditorMode.CommitSelectedMeshes();

	const TArray<UEditableMesh*>& SelectedMeshes = MeshEditorMode.GetSelectedEditableMeshes();

	MeshEditorMode.DeselectAllMeshElements();

	for( UEditableMesh* EditableMesh : SelectedMeshes )
	{
		EditableMesh->StartModification( EMeshModificationType::Final, EMeshTopologyChange::TopologyChange );
		{
			static TArray<FPolygonID> NewPolygonIDs;
			EditableMesh->QuadrangulateMesh( NewPolygonIDs );
		}
		EditableMesh->EndModification();

		MeshEditorMode.TrackUndo( EditableMesh, EditableMesh->MakeUndo() );
	}
}



#undef LOCTEXT_NAMESPACE

