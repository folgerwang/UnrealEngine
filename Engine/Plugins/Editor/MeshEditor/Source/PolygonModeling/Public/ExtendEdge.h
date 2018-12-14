// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshEditorCommands.h"
#include "ExtendEdge.generated.h"


/** Extends an edge by making a copy of it and allowing you to move it around */
UCLASS()
class POLYGONMODELING_API UExtendEdgeCommand : public UMeshEditorEditCommand
{
	GENERATED_BODY()

protected:

	UExtendEdgeCommand()
	{
		UndoText = NSLOCTEXT( "MeshEditor", "UndoExtendEdge", "Extend Edge" );
		bNeedsHoverLocation = false;
		bNeedsDraggingInitiated = true;
	}

	// Overrides
	virtual EEditableMeshElementType GetElementType() const override
	{
		return EEditableMeshElementType::Edge;
	}
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;
	virtual void ApplyDuringDrag( IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor ) override;
	virtual void AddToVRRadialMenuActionsMenu( class IMeshEditorModeUIContract& MeshEditorMode, class FMenuBuilder& MenuBuilder, TSharedPtr<FUICommandList> CommandList, const FName TEMPHACK_StyleSetName, class UVREditorMode* VRMode ) override;

protected:

};
