// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshEditorCommands.h"
#include "EditVertexOrEdgeSharpness.generated.h"


/** For subdivision meshes, edits how sharp a vertex corner is by dragging in space */
UCLASS()
class POLYGONMODELING_API UEditVertexCornerSharpnessCommand : public UMeshEditorEditCommand
{
	GENERATED_BODY()

protected:

	UEditVertexCornerSharpnessCommand()
	{
		UndoText = NSLOCTEXT( "MeshEditor", "UndoEditVertexCornerSharpness", "Edit Vertex Corner Sharpness" );
		bNeedsHoverLocation = false;
		bNeedsDraggingInitiated = false;
	}

	// Overrides
	virtual EEditableMeshElementType GetElementType() const override
	{
		return EEditableMeshElementType::Vertex;
	}
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;
	virtual bool TryStartingToDrag( IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor ) override;
	virtual void ApplyDuringDrag( IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor ) override;
};


/** For subdivision meshes, edits how sharp an edge crease is by dragging in space */
UCLASS()
class UEditEdgeCreaseSharpnessCommand : public UMeshEditorEditCommand
{
	GENERATED_BODY()

protected:

	UEditEdgeCreaseSharpnessCommand()
	{
		UndoText = NSLOCTEXT( "MeshEditor", "UndoEditEdgeCreaseSharpness", "Edit Edge Crease Sharpness" );
		bNeedsHoverLocation = false;
		bNeedsDraggingInitiated = false;
	}

	// Overrides
	virtual EEditableMeshElementType GetElementType() const override
	{
		return EEditableMeshElementType::Edge;
	}
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;
	virtual bool TryStartingToDrag( IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor ) override;
	virtual void ApplyDuringDrag( IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor ) override;
};
