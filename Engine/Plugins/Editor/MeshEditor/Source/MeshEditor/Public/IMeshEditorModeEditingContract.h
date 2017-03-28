// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditableMeshTypes.h"
#include "MeshElement.h"


class IMeshEditorModeEditingContract
{

public:

	enum class EMeshEditAction
	{
		/** Nothing going on right now */
		None,

		/** Selecting mesh elements by 'painting' over multiple elements */
		SelectByPainting,

		/** Moving elements using a transform gizmo */
		MoveUsingGizmo,

		/** Moving selected mesh elements (vertices, edges or polygons) */
		Move,

		/** Split an edge by inserting a vertex.  You can drag to preview where the vertex will be inserted. */
		SplitEdge,

		/** Splits an edge by inserting a new vertex, then immediately starts dragging that vertex */
		SplitEdgeAndDragVertex,

		/** Insert an edge loop */
		InsertEdgeLoop,

		/** Extrude polygon by making a copy of it and allowing you to shift it along the polygon normal axis */
		ExtrudePolygon,

		/** Extrude polygon by making a copy of it and allowing you to move it around freely */
		FreelyExtrudePolygon,

		/** Inset polygon by replacing it with a new polygon that is bordered by polygons of a specific relative size */
		InsetPolygon,

		/** Bevel polygons by adding angled bordering polygons of a specific relative size */
		BevelPolygon,

		/** Extend an edge by making a copy of it and allowing you to move it around */
		ExtendEdge,

		/** Extend a vertex by making a copy of it, creating new polygons to join the geometry together */
		ExtendVertex,

		/** For subdivision meshes, edits how sharp a vertex corner is by dragging in space */
		EditVertexCornerSharpness,

		/** For subdivision meshes, edits how sharp an edge crease is by dragging in space */
		EditEdgeCreaseSharpness,

		/** Freehand vertex drawing */
		DrawVertices,
	};


	/** Gets the interactive action currently being performed (and previewed).  These usually happen over multiple frames, and
	    result in a 'final' application of the change that performs a more exhaustive (and more expensive) update. */
	virtual EMeshEditAction GetActiveAction() const = 0;

	virtual void GetSelectedMeshesAndVertices( TMap<class UEditableMesh*, TArray<FMeshElement>>& OutMeshesAndVertices ) = 0;
	virtual void GetSelectedMeshesAndEdges( TMap<class UEditableMesh*, TArray<FMeshElement>>& OutMeshesAndEdges ) = 0;
	virtual void GetSelectedMeshesAndPolygons( TMap<class UEditableMesh*, TArray<FMeshElement>>& OutMeshesAndPolygons ) = 0;

	/** Selects the specified mesh elements */
	virtual void SelectMeshElements( const TArray<FMeshElement>& MeshElementsToSelect ) = 0;

	/** Deselects the specified mesh elements */
	virtual void DeselectMeshElements( const TArray<FMeshElement>& MeshElementsToDeselect ) = 0;
	virtual void DeselectMeshElements( const TMap<UEditableMesh*, TArray<FMeshElement>>& MeshElementsToDeselect ) = 0;

	/** Commits all selected meshes */
	virtual void CommitSelectedMeshes() = 0;

	/** Clears hover and selection on mesh elements that may no longer be valid.  You'll want to call this if you change the mesh topology */
	virtual void ClearInvalidSelectedElements() = 0;

};
