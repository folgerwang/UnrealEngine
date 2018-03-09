// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshEditorCommands.h"
#include "SplitEdge.generated.h"


/** Inserts a vertex along an edge and allows the user to move it around */
UCLASS()
class USplitEdgeCommand : public UMeshEditorEditCommand
{
	GENERATED_BODY()

protected:

	USplitEdgeCommand()
	{
		UndoText = NSLOCTEXT( "MeshEditor", "UndoSplitEdge", "Split Edge" );
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
};
