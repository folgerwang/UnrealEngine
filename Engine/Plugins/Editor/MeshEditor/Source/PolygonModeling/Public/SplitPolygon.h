// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshEditorCommands.h"
#include "SplitPolygon.generated.h"


/** Base class for polygon splitting */
UCLASS( abstract )
class POLYGONMODELING_API USplitPolygonCommand : public UMeshEditorEditCommand
{
	GENERATED_BODY()

protected:

	USplitPolygonCommand()
	  : Component( nullptr ),
		EditableMesh( nullptr ),
		StartingEdgeID( FEdgeID::Invalid ),
		StartingVertexID( FVertexID::Invalid ),
		EdgeSplit( 0.0f )
	{
		UndoText = NSLOCTEXT( "MeshEditor", "UndoSplitPolygon", "Split Polygon" );
		bNeedsHoverLocation = true;
		bNeedsDraggingInitiated = false;
	}

	// Overrides
	virtual bool TryStartingToDrag( IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor ) override;
	virtual void ApplyDuringDrag( IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor ) override;

protected:

	/** The component we're editing */
	UPROPERTY()
	class UPrimitiveComponent* Component;	// @todo now: Should be weak? 

	/** The mesh we're editing */
	UPROPERTY()
	class UEditableMesh* EditableMesh;		// @todo now: Should be weak?

	/** The edge we started our operation on */
	FEdgeID StartingEdgeID;

	/** The edge or vertex we started our split operation on */
	FVertexID StartingVertexID;

	/** If we're starting on an edge, the progress along that edge to start at */
	float EdgeSplit;

};


/** Splits a polygon into two, starting with a vertex on that polygon */
UCLASS()
class USplitPolygonFromVertexCommand : public USplitPolygonCommand
{
	GENERATED_BODY()

protected:

	// Overrides
	virtual EEditableMeshElementType GetElementType() const override
	{
		return EEditableMeshElementType::Vertex;
	}
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;

};


/** Splits a polygon into two, starting with a point along an edge */
UCLASS()
class USplitPolygonFromEdgeCommand : public USplitPolygonCommand
{
	GENERATED_BODY()

protected:

	// Overrides
	virtual EEditableMeshElementType GetElementType() const override
	{
		return EEditableMeshElementType::Edge;
	}
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;

};


/** Splits a polygon into two, starting with an edge on a polygon */
UCLASS()
class USplitPolygonFromPolygonCommand : public USplitPolygonCommand
{
	GENERATED_BODY()

protected:

	// Overrides
	virtual EEditableMeshElementType GetElementType() const override
	{
		return EEditableMeshElementType::Polygon;
	}
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;

};
