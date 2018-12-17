// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshEditorCommands.h"
#include "ExtrudePolygon.generated.h"


/** Extrudes the polygon along an axis */
UCLASS()
class POLYGONMODELING_API UExtrudePolygonCommand : public UMeshEditorEditCommand
{
	GENERATED_BODY()

protected:

	UExtrudePolygonCommand()
		: ExtrudePolygonAxisOrigin( FVector::ZeroVector ),
		  ExtrudePolygonAxisDirection( FVector::ZeroVector )
	{
		UndoText = NSLOCTEXT( "MeshEditor", "UndoExtrudePolygon", "Extrude Polygon" );
		bNeedsHoverLocation = false;
		bNeedsDraggingInitiated = false;
	}

	/** Figures out how far to extrude the polygon based on where the interactor is aiming */
	void FindExtrudeDistanceUsingInteractor(
		class IMeshEditorModeEditingContract& MeshEditorMode,
		class UViewportInteractor* ViewportInteractor,
		const class UEditableMesh* EditableMesh,
		const FVector AxisOrigin,
		const FVector AxisDirection,
		const float AxisLength,
		float& OutExtrudeDistance );

	// Overrides
	virtual EEditableMeshElementType GetElementType() const override
	{
		return EEditableMeshElementType::Polygon;
	}
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;
	virtual bool TryStartingToDrag( IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor ) override;
	virtual void ApplyDuringDrag( IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor ) override;
	virtual void AddToVRRadialMenuActionsMenu( class IMeshEditorModeUIContract& MeshEditorMode, class FMenuBuilder& MenuBuilder, TSharedPtr<FUICommandList> CommandList, const FName TEMPHACK_StyleSetName, class UVREditorMode* VRMode ) override;

protected:

	/** When extruding polygons, we need to keep track of the interactor's impact point and normal, because
	    the user is going to be aiming their interactor along that axis to choose an extrusion point */
	FVector ExtrudePolygonAxisOrigin;
	FVector ExtrudePolygonAxisDirection;

};
