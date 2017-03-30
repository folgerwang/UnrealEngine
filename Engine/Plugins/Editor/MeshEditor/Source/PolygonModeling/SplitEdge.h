// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshEditorCommands.h"
#include "SplitEdge.generated.h"


/** Inserts a vertex along an edge and allows the user to move it around */
UCLASS()
class USplitEdgeCommand : public UMeshEditorEdgeCommand
{
	GENERATED_BODY()

protected:

	USplitEdgeCommand()
	  : SplitEdgeMeshesAndEdgesToSplit(),
	    SplitEdgeSplitList()
	{
		bIsMode = true;
		UndoText = NSLOCTEXT( "MeshEditor", "UndoSplitEdge", "Split Edge" );
		bNeedsHoverLocation = false;
		bNeedsDraggingInitiated = true;
	}

	// Overrides
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;
	virtual bool TryStartingToDrag( IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor ) override;
	virtual void ApplyDuringDrag( IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor ) override;

protected:

	/** When splitting an edge and dragging a vertex, this is the list of edges that will be split */
	TMap< class UEditableMesh*, TArray< FMeshElement > > SplitEdgeMeshesAndEdgesToSplit;

	/** When splitting an edge and dragging a vertex, this is the list of split positions along those edges */
	TArray<float> SplitEdgeSplitList;

};
