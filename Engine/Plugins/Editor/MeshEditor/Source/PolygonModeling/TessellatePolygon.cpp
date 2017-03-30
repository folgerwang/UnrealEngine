// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "TessellatePolygon.h"
#include "IMeshEditorModeEditingContract.h"
#include "UICommandInfo.h"
#include "EditableMesh.h"
#include "MeshElement.h"


#define LOCTEXT_NAMESPACE "MeshEditorMode"


void UTessellatePolygonCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "TessellatePolygon", "Tessellate Polygon", "Tessellate selected polygons into smaller polygons.", EUserInterfaceActionType::Button, FInputChord() );
}


void UTessellatePolygonCommand::Execute( IMeshEditorModeEditingContract& MeshEditorMode )
{
	if( MeshEditorMode.GetActiveAction() != NAME_None )
	{
		return;
	}

	static TMap<UEditableMesh*, TArray<FMeshElement>> SelectedMeshesAndPolygons;
	MeshEditorMode.GetSelectedMeshesAndPolygons( SelectedMeshesAndPolygons );
	if( SelectedMeshesAndPolygons.Num() == 0 )
	{
		return;
	}

	FScopedTransaction Transaction( LOCTEXT( "UndoTessellatePolygon", "Tessellate Polygon" ) );

	MeshEditorMode.CommitSelectedMeshes();

	// Refresh selection (committing may have created a new mesh instance)
	MeshEditorMode.GetSelectedMeshesAndPolygons( SelectedMeshesAndPolygons );

	// Deselect the mesh elements before we delete them.  This will make sure they become selected again after undo.
	MeshEditorMode.DeselectMeshElements( SelectedMeshesAndPolygons );

	static TArray<FMeshElement> MeshElementsToSelect;
	MeshElementsToSelect.Reset();

	for( const auto& SelectedMeshAndPolygons : SelectedMeshesAndPolygons )
	{
		UEditableMesh* EditableMesh = SelectedMeshAndPolygons.Key;
		const TArray<FMeshElement>& PolygonElements = SelectedMeshAndPolygons.Value;

		static TArray<FPolygonRef> PolygonsToTessellate;
		PolygonsToTessellate.Reset( PolygonElements.Num() );

		for( const auto& PolygonElement : PolygonElements )
		{
			const FPolygonRef PolygonRef( PolygonElement.ElementAddress.SectionID, FPolygonID( PolygonElement.ElementAddress.ElementID ) );
			PolygonsToTessellate.Add( PolygonRef );
		}


		EditableMesh->StartModification( EMeshModificationType::Final, EMeshTopologyChange::TopologyChange );

		// @todo mesheditor: Expose as configurable parameter
		const ETriangleTessellationMode TriangleTessellationMode = ETriangleTessellationMode::FourTriangles;

		static TArray<FPolygonRef> NewPolygonRefs;
		EditableMesh->TessellatePolygons( PolygonsToTessellate, TriangleTessellationMode, /* Out */ NewPolygonRefs );

		// Select the new polygons
		for( const FPolygonRef NewPolygonRef : NewPolygonRefs )
		{
			FMeshElement NewPolygonMeshElement;
			{
				NewPolygonMeshElement.Component = PolygonElements[0].Component;
				NewPolygonMeshElement.ElementAddress = PolygonElements[0].ElementAddress;
				NewPolygonMeshElement.ElementAddress.ElementType = EEditableMeshElementType::Polygon;
				NewPolygonMeshElement.ElementAddress.SectionID = NewPolygonRef.SectionID;
				NewPolygonMeshElement.ElementAddress.ElementID = NewPolygonRef.PolygonID;
			}

			MeshElementsToSelect.Add( NewPolygonMeshElement );
		}

		EditableMesh->EndModification();

		MeshEditorMode.TrackUndo( EditableMesh, EditableMesh->MakeUndo() );
	}

	// Make sure we're not still hovering over a polygon we removed
	MeshEditorMode.ClearInvalidSelectedElements();

	// Select the new smaller polygons
	MeshEditorMode.SelectMeshElements( MeshElementsToSelect );
}


#undef LOCTEXT_NAMESPACE

