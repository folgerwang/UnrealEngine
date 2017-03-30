// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshEditorCommands.h"
#include "BevelOrInsetPolygon.generated.h"


/** Adds a beveled edge to an existing polygon */
UCLASS()
class UBevelPolygonCommand : public UMeshEditorPolygonCommand
{
	GENERATED_BODY()

protected:

	UBevelPolygonCommand()
	{
		bIsMode = true;
		UndoText = NSLOCTEXT( "MeshEditor", "UndoBevelPolygon", "Bevel Polygon" );
		bNeedsHoverLocation = true;
		bNeedsDraggingInitiated = false;
	}

	// Overrides
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;
	virtual bool TryStartingToDrag( IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor ) override;
	virtual void ApplyDuringDrag( IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor, bool& bOutShouldDeselectAllFirst, TArray<FMeshElement>& OutMeshElementsToSelect ) override;
	virtual void AddToVRRadialMenuActionsMenu( class IMeshEditorModeUIContract& MeshEditorMode, class FMenuBuilder& MenuBuilder, TSharedPtr<FUICommandList> CommandList, const FName TEMPHACK_StyleSetName, class UVREditorMode* VRMode ) override;

};


/** Adds a new polygon on the inside of an existing polygon, allowing the user to drag to set exactly where it should be placed. */
UCLASS()
class UInsetPolygonCommand : public UMeshEditorPolygonCommand
{
	GENERATED_BODY()

protected:

	UInsetPolygonCommand()
	{
		bIsMode = true;
		UndoText = NSLOCTEXT( "MeshEditor", "UndoInsetPolygon", "Inset Polygon" );
		bNeedsHoverLocation = true;
		bNeedsDraggingInitiated = false;
	}

	// Overrides
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;
	virtual bool TryStartingToDrag( class IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor ) override;
	virtual void ApplyDuringDrag( IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor, bool& bOutShouldDeselectAllFirst, TArray<FMeshElement>& OutMeshElementsToSelect ) override;
	virtual void AddToVRRadialMenuActionsMenu( class IMeshEditorModeUIContract& MeshEditorMode, class FMenuBuilder& MenuBuilder, TSharedPtr<FUICommandList> CommandList, const FName TEMPHACK_StyleSetName, class UVREditorMode* VRMode ) override;

};
