// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshEditorCommands.h"
#include "InsertEdgeLoop.generated.h"


/** With an edge selected, inserts a loop of edge perpendicular to that edge while dragging */
UCLASS()
class POLYGONMODELING_API UInsertEdgeLoopCommand : public UMeshEditorEditCommand
{
	GENERATED_BODY()

protected:

	UInsertEdgeLoopCommand()
	{
		UndoText = NSLOCTEXT( "MeshEditor", "UndoInsertEdgeLoop", "Insert Edge Loop" );
		bNeedsHoverLocation = true;
		bNeedsDraggingInitiated = false;
	}

	// Overrides
	virtual EEditableMeshElementType GetElementType() const override
	{
		return EEditableMeshElementType::Edge;
	}
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;
	virtual void ApplyDuringDrag( IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor ) override;
	virtual void AddToVRRadialMenuActionsMenu( class IMeshEditorModeUIContract& MeshEditorMode, class FMenuBuilder& MenuBuilder, TSharedPtr<FUICommandList> CommandList, const FName TEMPHACK_StyleSetName, class UVREditorMode* VRMode ) override;

};
