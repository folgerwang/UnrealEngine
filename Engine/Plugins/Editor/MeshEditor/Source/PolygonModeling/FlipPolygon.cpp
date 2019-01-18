// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "FlipPolygon.h"

#include "EditableMesh.h"
#include "Framework/Commands/UICommandInfo.h"
#include "IMeshEditorModeEditingContract.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "MeshEditorMode"

void UFlipPolygonCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "FlipPolygon", "Flip", "Flip the currently selected polygons.", EUserInterfaceActionType::Button, FInputChord( EKeys::F, EModifierKey::Shift ) );
}

void UFlipPolygonCommand::Execute( IMeshEditorModeEditingContract& MeshEditorMode )
{
	if ( MeshEditorMode.GetActiveAction() != NAME_None )
	{
		return;
	}

	static TMap< UEditableMesh*, TArray<FMeshElement> > MeshesAndPolygons;
	MeshEditorMode.GetSelectedMeshesAndPolygons( MeshesAndPolygons );

	if ( MeshesAndPolygons.Num() == 0 )
	{
		return;
	}

	const FScopedTransaction Transaction( LOCTEXT( "UndoFlipPolygon", "Flip Polygon" ) );

	MeshEditorMode.CommitSelectedMeshes();

	// Refresh selection (committing may have created a new mesh instance)
	MeshEditorMode.GetSelectedMeshesAndPolygons( MeshesAndPolygons );

	// Deselect the elements because the selection visual needs to be updated
	MeshEditorMode.DeselectMeshElements( MeshesAndPolygons );

	// Flip selected polygons
	for( const auto& MeshAndPolygons : MeshesAndPolygons )
	{
		UEditableMesh* EditableMesh = MeshAndPolygons.Key;

		EditableMesh->StartModification( EMeshModificationType::Final, EMeshTopologyChange::TopologyChange );

		static TArray<FPolygonID> PolygonsToFlip;
		PolygonsToFlip.Reset();

		for( const FMeshElement& PolygonElement : MeshAndPolygons.Value )
		{
			const FPolygonID PolygonID( PolygonElement.ElementAddress.ElementID );
			PolygonsToFlip.Add( PolygonID );
		}

		EditableMesh->FlipPolygons( PolygonsToFlip );

		EditableMesh->EndModification();

		MeshEditorMode.TrackUndo( EditableMesh, EditableMesh->MakeUndo() );
	}

	// Re-select the elements to update the selection visual
	for ( const auto& MeshAndPolygons : MeshesAndPolygons )
	{
		MeshEditorMode.SelectMeshElements( MeshAndPolygons.Value );
	}
}

#undef LOCTEXT_NAMESPACE
