// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MeshEditorCommands.h"
#include "MeshEditorStyle.h"
#include "IMeshEditorModeUIContract.h"
#include "Framework/Commands/UIAction.h"
#include "UObject/UObjectIterator.h"


#define LOCTEXT_NAMESPACE "MeshEditorCommands"


namespace MeshEditorCommands
{
	const TArray<UMeshEditorCommand*>& Get()
	{
		static UMeshEditorCommandList* MeshEditorCommandList = nullptr;
		if( MeshEditorCommandList == nullptr )
		{
			MeshEditorCommandList = NewObject<UMeshEditorCommandList>();
			MeshEditorCommandList->AddToRoot();

			MeshEditorCommandList->HarvestMeshEditorCommands();
		}

		return MeshEditorCommandList->MeshEditorCommands;
	}
}


FUIAction UMeshEditorInstantCommand::MakeUIAction( IMeshEditorModeUIContract& MeshEditorMode )
{
	const EEditableMeshElementType ElementType = GetElementType();

	FUIAction UIAction;
	{
		FExecuteAction ExecuteAction( FExecuteAction::CreateLambda( [&MeshEditorMode, this]
		{
			this->Execute( MeshEditorMode );
		} ) );

		if( ElementType == EEditableMeshElementType::Invalid )
		{
			// Common command
			UIAction = FUIAction(
				ExecuteAction,
				FCanExecuteAction::CreateLambda( [&MeshEditorMode] { return MeshEditorMode.GetSelectedEditableMeshes().Num() > 0; } )
			);
		}
		else if( ElementType == EEditableMeshElementType::Any )
		{
			UIAction = FUIAction(
				ExecuteAction,
				FCanExecuteAction::CreateLambda( [&MeshEditorMode] { return MeshEditorMode.GetSelectedMeshElementType() != EEditableMeshElementType::Invalid; } )
			);
		}
		else if (ElementType == EEditableMeshElementType::Fracture)
		{
			UIAction = FUIAction(
				ExecuteAction,
				FCanExecuteAction::CreateLambda([&MeshEditorMode] { return MeshEditorMode.GetSelectedEditableMeshes().Num() > 0; })
			);
		}
		else
		{
			UIAction = FUIAction(
				ExecuteAction,
				FCanExecuteAction::CreateLambda( [&MeshEditorMode, ElementType] { return MeshEditorMode.IsMeshElementTypeSelected( ElementType ); } )
			);
		}
	}

	return UIAction;
}


FUIAction UMeshEditorEditCommand::MakeUIAction( IMeshEditorModeUIContract& MeshEditorMode )
{
	const EEditableMeshElementType ElementType = GetElementType();

	FUIAction UIAction;
	{
		const FName CommandName = GetCommandName();

		UIAction = FUIAction(
			FExecuteAction::CreateLambda( [&MeshEditorMode, ElementType, CommandName] { MeshEditorMode.SetEquippedAction( ElementType, CommandName ); } ),
			FCanExecuteAction::CreateLambda( [&MeshEditorMode, ElementType] { return MeshEditorMode.IsMeshElementTypeSelectedOrIsActiveSelectionMode( ElementType ); } ),
			FIsActionChecked::CreateLambda( [&MeshEditorMode, ElementType, CommandName] { return ( MeshEditorMode.GetEquippedAction( ElementType ) == CommandName ); } )
		);
	}

	return UIAction;
}


FMeshEditorCommonCommands::FMeshEditorCommonCommands() 
	: TCommands<FMeshEditorCommonCommands>(
		"MeshEditorCommon",
		LOCTEXT( "MeshEditorCommon", "Mesh Editor Common" ),
		"MainFrame",
		FMeshEditorStyle::GetStyleSetName()	)
{
}


void FMeshEditorCommonCommands::RegisterCommands()
{
	UI_COMMAND(AddSubdivisionLevel, "Add Subdivision Level", "Increases the number of subdivision levels for the selected mesh.", EUserInterfaceActionType::Button, FInputChord(EKeys::Equals));
	UI_COMMAND(RemoveSubdivisionLevel, "Remove Subdivision Level", "Decreases the number of subdivision levels for the selected mesh.", EUserInterfaceActionType::Button, FInputChord(EKeys::Hyphen));
	UI_COMMAND(ShowVertexNormals, "Show Vertex Normals", "Toggles debug rendering of vertex normals.", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::V));
	UI_COMMAND(MarqueeSelectVertices, "Marquee Select Vertices", "Selects vertices inside the selection box.", EUserInterfaceActionType::None, FInputChord());
	UI_COMMAND(MarqueeSelectEdges, "Marquee Select Edges", "Selects edges inside the selection box.", EUserInterfaceActionType::None, FInputChord());
	UI_COMMAND(MarqueeSelectPolygons, "Marquee Select Polygons", "Selects polygons inside the selection box.", EUserInterfaceActionType::None, FInputChord());
	UI_COMMAND(DrawVertices, "Draw Vertices", "Allows vertices to be freely drawn to create a new polygon.", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(FrameSelectedElements, "Frame Selected Elements", "Moves the viewport camera to frame the currently selected elements.", EUserInterfaceActionType::None, FInputChord(EKeys::F));
	UI_COMMAND(SetVertexSelectionMode, "Set Vertex Selection Mode", "Sets the selection mode so that only vertices will be selected.", EUserInterfaceActionType::None, FInputChord(EKeys::One));
	UI_COMMAND(SetEdgeSelectionMode, "Set Edge Selection Mode", "Sets the selection mode so that only edges will be selected.", EUserInterfaceActionType::None, FInputChord(EKeys::Two));
	UI_COMMAND(SetPolygonSelectionMode, "Set Polygon Selection Mode", "Sets the selection mode so that only polygons will be selected.", EUserInterfaceActionType::None, FInputChord(EKeys::Three));
	UI_COMMAND(SetAnySelectionMode, "Set Any Selection Mode", "Sets the selection mode so that any element type may be selected.", EUserInterfaceActionType::None, FInputChord(EKeys::Four));
	UI_COMMAND(SetFractureSelectionMode, "Set Fracture Selection Mode", "Sets the selection mode for mesh fracturing.", EUserInterfaceActionType::None, FInputChord(EKeys::Five));

	for( UMeshEditorCommand* Command : MeshEditorCommands::Get() )
	{
		if( Command->GetElementType() == EEditableMeshElementType::Invalid )
		{
			Command->RegisterUICommand( this );
		}
	}
}

FMeshEditorAnyElementCommands::FMeshEditorAnyElementCommands() 
	: TCommands<FMeshEditorAnyElementCommands>(
		"MeshEditorAnyElement",
		LOCTEXT( "MeshEditorGeneral", "Mesh Editor Any Element Type" ),
		"MainFrame",
		FMeshEditorStyle::GetStyleSetName()	)
{
}


void FMeshEditorAnyElementCommands::RegisterCommands()
{
	for( UMeshEditorCommand* Command : MeshEditorCommands::Get() )
	{
		if( Command->GetElementType() == EEditableMeshElementType::Any )
		{
			Command->RegisterUICommand( this );
		}
	}
}

FMeshEditorVertexCommands::FMeshEditorVertexCommands() 
	: TCommands<FMeshEditorVertexCommands>(
		"MeshEditorVertex",
		LOCTEXT("MeshEditorVertex", "Mesh Editor Vertex"),
		"MeshEditorCommon",
		FMeshEditorStyle::GetStyleSetName())
{
}


void FMeshEditorVertexCommands::RegisterCommands()
{
	UI_COMMAND(MoveVertex, "Move", "Move selected vertices using a transform gizmo, or click and drag to move vertices directly.", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND(WeldVertices, "Weld", "Weld the selected vertices, keeping the first selected vertex.", EUserInterfaceActionType::Button, FInputChord());

	for( UMeshEditorCommand* Command : MeshEditorCommands::Get() )
	{
		if( Command->GetElementType() == EEditableMeshElementType::Vertex )
		{
			Command->RegisterUICommand( this );
		}
	}
}

FMeshEditorEdgeCommands::FMeshEditorEdgeCommands()
	: TCommands<FMeshEditorEdgeCommands>(
		"MeshEditorEdge",
		LOCTEXT("MeshEditorEdge", "Mesh Editor Edge"),
		"MeshEditorCommon",
		FMeshEditorStyle::GetStyleSetName() )
{
}


void FMeshEditorEdgeCommands::RegisterCommands()
{
	UI_COMMAND(MoveEdge, "Move", "Move selected edges using a transform gizmo, or click and drag to move edges directly.", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND(SelectEdgeLoop, "Select Edge Loop", "Select the edge loops which contain the selected edges.", EUserInterfaceActionType::Button, FInputChord(EKeys::Two, EModifierKey::Shift));

	for( UMeshEditorCommand* Command : MeshEditorCommands::Get() )
	{
		if( Command->GetElementType() == EEditableMeshElementType::Edge )
		{
			Command->RegisterUICommand( this );
		}
	}
}

FMeshEditorPolygonCommands::FMeshEditorPolygonCommands() 
	: TCommands<FMeshEditorPolygonCommands>(
		"MeshEditorPolygon",
		LOCTEXT("MeshEditorPolygon", "Mesh Editor Polygon"),
		"MeshEditorCommon",
		FMeshEditorStyle::GetStyleSetName() )
{
}


void FMeshEditorPolygonCommands::RegisterCommands()
{
	UI_COMMAND(MovePolygon, "Move", "Move selected polygons using a transform gizmo, or click and drag to move polygons directly.", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(TriangulatePolygon, "Triangulate", "Triangulate the currently selected polygons.", EUserInterfaceActionType::Button, FInputChord(EKeys::T));

	for( UMeshEditorCommand* Command : MeshEditorCommands::Get() )
	{
		if( Command->GetElementType() == EEditableMeshElementType::Polygon )
		{
			Command->RegisterUICommand( this );
		}
	}
}


FMeshEditorFractureCommands::FMeshEditorFractureCommands()
	: TCommands<FMeshEditorFractureCommands>(
		"MeshEditorFracture",
		LOCTEXT("MeshEditorFracture", "Mesh Editor Fracture"),
		"MeshEditorCommon",
		FMeshEditorStyle::GetStyleSetName())
{
}


void FMeshEditorFractureCommands::RegisterCommands()
{
	for (UMeshEditorCommand* Command : MeshEditorCommands::Get())
	{
		if (Command->GetElementType() == EEditableMeshElementType::Fracture)
		{
			Command->RegisterUICommand(this);
		}
	}
}


void UMeshEditorCommandList::HarvestMeshEditorCommands()
{
	MeshEditorCommands.Reset();
	for( TObjectIterator<UMeshEditorCommand> CommandCDOIter( RF_NoFlags ); CommandCDOIter; ++CommandCDOIter )
	{
		UMeshEditorCommand* CommandCDO = *CommandCDOIter;
		if( !( CommandCDO->GetClass()->GetClassFlags() & CLASS_Abstract ) )
		{
			MeshEditorCommands.Add( NewObject<UMeshEditorCommand>( this, CommandCDO->GetClass() ) );
		}
	}
}


#undef LOCTEXT_NAMESPACE