// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshEditorCommands.h"
#include "EditVertexOrEdgeSharpness.generated.h"


/** For subdivision meshes, edits how sharp a vertex corner is by dragging in space */
UCLASS()
class UEditVertexCornerSharpnessCommand : public UMeshEditorVertexCommand
{
	GENERATED_BODY()

protected:

	UEditVertexCornerSharpnessCommand()
	{
		bIsMode = true;
		UndoText = NSLOCTEXT( "MeshEditor", "UndoEditVertexCornerSharpness", "Edit Vertex Corner Sharpness" );
		bNeedsHoverLocation = false;
		bNeedsDraggingInitiated = false;
	}

	// Overrides
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;
	virtual bool TryStartingToDrag( IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor ) override;
	virtual void ApplyDuringDrag( IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor ) override;
};


/** For subdivision meshes, edits how sharp an edge crease is by dragging in space */
UCLASS()
class UEditEdgeCreaseSharpnessCommand : public UMeshEditorEdgeCommand
{
	GENERATED_BODY()

protected:

	UEditEdgeCreaseSharpnessCommand()
	{
		bIsMode = true;
		UndoText = NSLOCTEXT( "MeshEditor", "UndoEditEdgeCreaseSharpness", "Edit Edge Crease Sharpness" );
		bNeedsHoverLocation = false;
		bNeedsDraggingInitiated = false;
	}

	// Overrides
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;
	virtual bool TryStartingToDrag( IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor ) override;
	virtual void ApplyDuringDrag( IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor ) override;
};
