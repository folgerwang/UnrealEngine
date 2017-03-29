// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshEditorCommands.h"
#include "InsertEdgeLoop.generated.h"


/** With an edge select, inserts a loop of edge perpendicular to that edge while dragging */
UCLASS()
class UInsertEdgeLoopCommand : public UMeshEditorEdgeCommand
{
	GENERATED_BODY()

protected:

	UInsertEdgeLoopCommand()
	{
		bIsMode = true;
		UndoText = NSLOCTEXT( "MeshEditor", "UndoInsertEdgeLoop", "Insert Edge Loop" );
		bNeedsHoverLocation = true;
		bNeedsDraggingInitiated = false;
	}

	// Overrides
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;
	virtual void ApplyDuringDrag( IMeshEditorModeEditingContract& MeshEditorMode, bool& bOutShouldDeselectAllFirst, TArray<FMeshElement>& OutMeshElementsToSelect ) override;
	virtual void AddToVRRadialMenuActionsMenu( class IMeshEditorModeUIContract& MeshEditorMode, class FMenuBuilder& MenuBuilder, TSharedPtr<FUICommandList> CommandList, const FName TEMPHACK_StyleSetName, class UVREditorMode* VRMode ) override;

};
