// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "QuadrangulateMesh.h"
#include "IMeshEditorModeEditingContract.h"
#include "IMeshEditorModeUIContract.h"
#include "UICommandInfo.h"
#include "EditableMesh.h"
#include "MeshElement.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "MeshEditorMode"


void UQuadrangulateMeshCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "QuadrangulateMesh", "Quadrangulate Mesh", "Quadrangulates the selected mesh.", EUserInterfaceActionType::Button, FInputChord() );
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
		static TArray<FPolygonRef> NewPolygonRefs;
		EditableMesh->StartModification( EMeshModificationType::Final, EMeshTopologyChange::TopologyChange );
		EditableMesh->QuadrangulateMesh( NewPolygonRefs );
		EditableMesh->EndModification();

		MeshEditorMode.TrackUndo( EditableMesh, EditableMesh->MakeUndo() );
	}
}



#undef LOCTEXT_NAMESPACE

