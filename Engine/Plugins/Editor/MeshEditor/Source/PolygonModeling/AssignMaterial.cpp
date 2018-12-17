// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AssignMaterial.h"

#include "ContentBrowserModule.h"
#include "EditableMesh.h"
#include "Framework/Commands/UICommandInfo.h"
#include "IContentBrowserSingleton.h"
#include "IMeshEditorModeUIContract.h"
#include "MeshEditorUtilities.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "MeshEditorMode"

void UAssignMaterialCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "AssignMaterial", "Assign Material", "Assigns the highlighted material in the Content Browser to the currently selected polygons.", EUserInterfaceActionType::Button, FInputChord( EKeys::M ) );
}

void UAssignMaterialCommand::Execute( IMeshEditorModeEditingContract& MeshEditorMode )
{
	IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>( "ContentBrowser" ).Get();

	static TArray<FAssetData> SelectedAssets;
	ContentBrowser.GetSelectedAssets( SelectedAssets );

	UMaterialInterface* SelectedMaterial = FAssetData::GetFirstAsset<UMaterialInterface>( SelectedAssets );

	if ( MeshEditorMode.GetActiveAction() != NAME_None )
	{
		return;
	}

	static TMap< UEditableMesh*, TArray<FMeshElement> > MeshesAndPolygons;
	MeshEditorMode.GetSelectedMeshesAndPolygons( /* Out */ MeshesAndPolygons );

	if ( MeshesAndPolygons.Num() == 0 )
	{
		return;
	}

	const FScopedTransaction Transaction( LOCTEXT( "UndoAssignMaterialToPolygon", "Assign Material to Polygon" ) );

	MeshEditorMode.CommitSelectedMeshes();

	// Refresh selection (committing may have created a new mesh instance)
	MeshEditorMode.GetSelectedMeshesAndPolygons( /* Out */ MeshesAndPolygons );
	for( const auto& MeshAndPolygons : MeshesAndPolygons )
	{
		UEditableMesh* EditableMesh = MeshAndPolygons.Key;

		FMeshEditorUtilities::AssignMaterialToPolygons( SelectedMaterial, EditableMesh, MeshAndPolygons.Value );

		MeshEditorMode.TrackUndo( EditableMesh, EditableMesh->MakeUndo() );
	}
}

#undef LOCTEXT_NAMESPACE
