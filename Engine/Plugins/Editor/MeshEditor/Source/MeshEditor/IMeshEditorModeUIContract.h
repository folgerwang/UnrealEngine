// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditableMeshTypes.h"


class IMeshEditorModeUIContract
{

public:

	/** Returns the current selection mode we're in */
	virtual EEditableMeshElementType GetMeshElementSelectionMode() const = 0;
	
	/** Sets the mesh element selection mode to use */
	virtual void SetMeshElementSelectionMode( EEditableMeshElementType ElementType ) = 0;

	/** Returns the type of elements that are selected right now, or Invalid if nothing is selected */
	virtual EEditableMeshElementType GetSelectedMeshElementType() const = 0;

	virtual const TArray<TTuple<TSharedPtr<FUICommandInfo>, FUIAction>>& GetVertexActions() const = 0;
	virtual const TArray<TTuple<TSharedPtr<FUICommandInfo>, FUIAction>>& GetEdgeActions() const = 0;
	virtual const TArray<TTuple<TSharedPtr<FUICommandInfo>, FUIAction>>& GetPolygonActions() const = 0;

	virtual bool IsEditingPerInstance() const = 0;
	virtual void SetEditingPerInstance( bool bPerInstance ) = 0;

	/** Propagates instance changes to the static mesh asset */
	virtual void PropagateInstanceChanges() = 0;

	/** Whether there are instance changes which can be propagated */
	virtual bool CanPropagateInstanceChanges() const = 0;

};
