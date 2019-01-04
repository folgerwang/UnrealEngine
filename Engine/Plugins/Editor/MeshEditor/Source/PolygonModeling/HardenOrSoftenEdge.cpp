// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "HardenOrSoftenEdge.h"
#include "IMeshEditorModeEditingContract.h"
#include "Framework/Commands/UICommandInfo.h"
#include "EditableMesh.h"
#include "MeshElement.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "MeshEditorMode"


namespace HardenOrSoftenEdgeHelpers
{
	void MakeSelectedEdgesHardOrSoft( IMeshEditorModeEditingContract& MeshEditorMode, const bool bMakeEdgesHard )
	{
		if( MeshEditorMode.GetActiveAction() != NAME_None )
		{
			return;
		}

		// Make edges hard or soft
		static TMap< UEditableMesh*, TArray<FMeshElement> > MeshesAndEdges;
		MeshEditorMode.GetSelectedMeshesAndEdges( /* Out */ MeshesAndEdges );

		if( MeshesAndEdges.Num() == 0 )
		{
			return;
		}

		const FScopedTransaction Transaction( bMakeEdgesHard ? LOCTEXT( "UndoHardenEdge", "Harden Edge" ) : LOCTEXT( "UndoSoftenEdge", "Soften Edge" ) );

		MeshEditorMode.CommitSelectedMeshes();

		// Refresh selection (committing may have created a new mesh instance)
		MeshEditorMode.GetSelectedMeshesAndEdges( /* Out */ MeshesAndEdges );

		for( auto& MeshAndEdges : MeshesAndEdges )
		{
			static TArray<FEdgeID> EdgeIDs;
			EdgeIDs.Reset();

			static TArray<bool> EdgesNewIsHard;
			EdgesNewIsHard.Reset();

			for( FMeshElement& EdgeElement : MeshAndEdges.Value )
			{
				const FEdgeID EdgeID( EdgeElement.ElementAddress.ElementID );

				EdgeIDs.Add( EdgeID );
				EdgesNewIsHard.Add( bMakeEdgesHard );
			}


			UEditableMesh* EditableMesh = MeshAndEdges.Key;
			verify( !EditableMesh->AnyChangesToUndo() );

			EditableMesh->StartModification( EMeshModificationType::Final, EMeshTopologyChange::TopologyChange );

			EditableMesh->SetEdgesHardness( EdgeIDs, EdgesNewIsHard );

			EditableMesh->EndModification();

			MeshEditorMode.TrackUndo( EditableMesh, EditableMesh->MakeUndo() );
		}
	}
}


void UHardenEdgeCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "HardenEdge", "Harden", "Sets the edge to be hard.", EUserInterfaceActionType::Button, FInputChord( EKeys::H ) );
}


void UHardenEdgeCommand::Execute( IMeshEditorModeEditingContract& MeshEditorMode )
{
	const bool bShouldHarden = true;
	HardenOrSoftenEdgeHelpers::MakeSelectedEdgesHardOrSoft( MeshEditorMode, bShouldHarden );
}


void USoftenEdgeCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "SoftenEdge", "Soften", "Sets the edge to be soft.", EUserInterfaceActionType::Button, FInputChord( EKeys::H, EModifierKey::Shift ) );
}


void USoftenEdgeCommand::Execute( IMeshEditorModeEditingContract& MeshEditorMode )
{
	const bool bShouldHarden = false;
	HardenOrSoftenEdgeHelpers::MakeSelectedEdgesHardOrSoft( MeshEditorMode, bShouldHarden );
}


#undef LOCTEXT_NAMESPACE

