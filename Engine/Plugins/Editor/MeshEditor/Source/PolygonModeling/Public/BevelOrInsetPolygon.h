// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshEditorCommands.h"
#include "BevelOrInsetPolygon.generated.h"


/** Adds a beveled edge to an existing polygon */
UCLASS()
class POLYGONMODELING_API UBevelPolygonCommand : public UMeshEditorEditCommand
{
	GENERATED_BODY()

protected:

	UBevelPolygonCommand()
	{
		UndoText = NSLOCTEXT( "MeshEditor", "UndoBevelPolygon", "Bevel Polygon" );
		bNeedsHoverLocation = true;
		bNeedsDraggingInitiated = false;
	}

	// Overrides
	virtual EEditableMeshElementType GetElementType() const override
	{
		return EEditableMeshElementType::Polygon;
	}
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;
	virtual bool TryStartingToDrag( IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor ) override;
	virtual void ApplyDuringDrag( IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor ) override;
	virtual void AddToVRRadialMenuActionsMenu( class IMeshEditorModeUIContract& MeshEditorMode, class FMenuBuilder& MenuBuilder, TSharedPtr<FUICommandList> CommandList, const FName TEMPHACK_StyleSetName, class UVREditorMode* VRMode ) override;

};


/** Adds a new polygon on the inside of an existing polygon, allowing the user to drag to set exactly where it should be placed. */
UCLASS()
class UInsetPolygonCommand : public UMeshEditorEditCommand
{
	GENERATED_BODY()

protected:

	UInsetPolygonCommand()
	{
		UndoText = NSLOCTEXT( "MeshEditor", "UndoInsetPolygon", "Inset Polygon" );
		bNeedsHoverLocation = true;
		bNeedsDraggingInitiated = false;
	}

	// Overrides
	virtual EEditableMeshElementType GetElementType() const override
	{
		return EEditableMeshElementType::Polygon;
	}
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;
	virtual bool TryStartingToDrag( class IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor ) override;
	virtual void ApplyDuringDrag( IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor ) override;
	virtual void AddToVRRadialMenuActionsMenu( class IMeshEditorModeUIContract& MeshEditorMode, class FMenuBuilder& MenuBuilder, TSharedPtr<FUICommandList> CommandList, const FName TEMPHACK_StyleSetName, class UVREditorMode* VRMode ) override;

};
