// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditableMeshTypes.h"
#include "MeshElement.h"


// @todo mesheditor extensibility: This should probably be removed after we've evicted all current mesh editing actions to another module
namespace EMeshEditAction
{
	typedef FName Type;

	/** Nothing going on right now */
	MESHEDITOR_API extern const FName None;

	/** Selecting mesh elements by 'painting' over multiple elements */
	MESHEDITOR_API extern const FName SelectByPainting;

	/** Moving elements using a transform gizmo */
	MESHEDITOR_API extern const FName MoveUsingGizmo;

	/** Moving selected mesh elements (vertices, edges or polygons) */
	MESHEDITOR_API extern const FName Move;

	/** Split an edge by inserting a vertex.  You can drag to preview where the vertex will be inserted. */
	MESHEDITOR_API extern const FName SplitEdge;

	/** Splits an edge by inserting a new vertex, then immediately starts dragging that vertex */
	MESHEDITOR_API extern const FName SplitEdgeAndDragVertex;

	/** Extrude polygon by making a copy of it and allowing you to shift it along the polygon normal axis */
	MESHEDITOR_API extern const FName ExtrudePolygon;

	/** Extrude polygon by making a copy of it and allowing you to move it around freely */
	MESHEDITOR_API extern const FName FreelyExtrudePolygon;

	/** Inset polygon by replacing it with a new polygon that is bordered by polygons of a specific relative size */
	MESHEDITOR_API extern const FName InsetPolygon;

	/** Bevel polygons by adding angled bordering polygons of a specific relative size */
	MESHEDITOR_API extern const FName BevelPolygon;

	/** Extend an edge by making a copy of it and allowing you to move it around */
	MESHEDITOR_API extern const FName ExtendEdge;

	/** Extend a vertex by making a copy of it, creating new polygons to join the geometry together */
	MESHEDITOR_API extern const FName ExtendVertex;

	/** For subdivision meshes, edits how sharp a vertex corner is by dragging in space */
	MESHEDITOR_API extern const FName EditVertexCornerSharpness;

	/** For subdivision meshes, edits how sharp an edge crease is by dragging in space */
	MESHEDITOR_API extern const FName EditEdgeCreaseSharpness;

	/** Freehand vertex drawing */
	MESHEDITOR_API extern const FName DrawVertices;
}



class IMeshEditorModeEditingContract
{

public:


	/** Gets the interactive action currently being performed (and previewed).  These usually happen over multiple frames, and
	    result in a 'final' application of the change that performs a more exhaustive (and more expensive) update. */
	virtual EMeshEditAction::Type GetActiveAction() const = 0;

	/** Stores undo state for the specified object.  This will store the state different depending on whether we're
	    currently in the middle of previewing a temporary change to meshes (bIsCapturingUndoForPreview) */
	virtual void TrackUndo( UObject* Object, TUniquePtr<FChange> RevertChange ) = 0;

	virtual void GetSelectedMeshesAndVertices( TMap<class UEditableMesh*, TArray<FMeshElement>>& OutMeshesAndVertices ) = 0;
	virtual void GetSelectedMeshesAndEdges( TMap<class UEditableMesh*, TArray<FMeshElement>>& OutMeshesAndEdges ) = 0;
	virtual void GetSelectedMeshesAndPolygons( TMap<class UEditableMesh*, TArray<FMeshElement>>& OutMeshesAndPolygons ) = 0;

	/** Selects the specified mesh elements */
	virtual void SelectMeshElements( const TArray<FMeshElement>& MeshElementsToSelect ) = 0;

	/** Deselects all mesh elements */
	virtual void DeselectAllMeshElements() = 0;

	/** Deselects the specified mesh elements */
	virtual void DeselectMeshElements( const TArray<FMeshElement>& MeshElementsToDeselect ) = 0;
	virtual void DeselectMeshElements( const TMap<UEditableMesh*, TArray<FMeshElement>>& MeshElementsToDeselect ) = 0;

	/** Commits all selected meshes */
	virtual void CommitSelectedMeshes() = 0;

	/** Clears hover and selection on mesh elements that may no longer be valid.  You'll want to call this if you change the mesh topology */
	virtual void ClearInvalidSelectedElements() = 0;

	/** Given an interactor and a mesh, finds edges under the interactor along with their exact split position (progress along the edge) */
	virtual void FindEdgeSplitUnderInteractor( class UViewportInteractor* ViewportInteractor, const UEditableMesh* EditableMesh, const TArray<FMeshElement>& EdgeElements, TArray<float>& OutSplits ) = 0;

	/** When performing an interactive action that was initiated using an interactor, this is the interactor that was used. */
	virtual class UViewportInteractor* GetActiveActionInteractor() = 0;

};
