// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshEditorCommands.h"
#include "ExtendVertex.generated.h"


/** Extend a vertex by making a copy of it, creating new polygons to join the geometry together */
UCLASS()
class POLYGONMODELING_API UExtendVertexCommand : public UMeshEditorEditCommand
{
	GENERATED_BODY()

protected:

	UExtendVertexCommand()
	{
		UndoText = NSLOCTEXT( "MeshEditor", "UndoExtendVertex", "Extend Vertex" );
		bNeedsHoverLocation = false;
		bNeedsDraggingInitiated = true;
	}

	// Overrides
	virtual EEditableMeshElementType GetElementType() const override
	{
		return EEditableMeshElementType::Vertex;
	}
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;
	virtual void ApplyDuringDrag( IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor ) override;
	virtual void AddToVRRadialMenuActionsMenu( class IMeshEditorModeUIContract& MeshEditorMode, class FMenuBuilder& MenuBuilder, TSharedPtr<FUICommandList> CommandList, const FName TEMPHACK_StyleSetName, class UVREditorMode* VRMode ) override;

protected:

};
