// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditableMeshTypes.h"
#include "IMeshEditorModeEditingContract.h"

class FUICommandInfo;
struct FUIAction;

class IMeshEditorModeUIContract : public IMeshEditorModeEditingContract
{

public:

	/** Returns the current selection mode we're in */
	virtual EEditableMeshElementType GetMeshElementSelectionMode() const = 0;
	
	/** Sets the mesh element selection mode to use */
	virtual void SetMeshElementSelectionMode( EEditableMeshElementType ElementType ) = 0;

	/** Returns the type of elements that are selected right now, or Invalid if nothing is selected */
	virtual EEditableMeshElementType GetSelectedMeshElementType() const = 0;

	/** Returns whether the specified element type is selected */
	virtual bool IsMeshElementTypeSelected( EEditableMeshElementType ElementType ) const = 0;

	/** Returns whether either the specified element type is selected, or we're in the selection mode for that element type */
	virtual bool IsMeshElementTypeSelectedOrIsActiveSelectionMode( EEditableMeshElementType ElementType ) const = 0;

	virtual const TArray<TTuple<TSharedPtr<FUICommandInfo>, FUIAction>>& GetCommonActions() const = 0;
	virtual const TArray<TTuple<TSharedPtr<FUICommandInfo>, FUIAction>>& GetVertexActions() const = 0;
	virtual const TArray<TTuple<TSharedPtr<FUICommandInfo>, FUIAction>>& GetEdgeActions() const = 0;
	virtual const TArray<TTuple<TSharedPtr<FUICommandInfo>, FUIAction>>& GetPolygonActions() const = 0;
	virtual const TArray<TTuple<TSharedPtr<FUICommandInfo>, FUIAction>>& GetFractureActions() const = 0;

	virtual const TArray<TTuple<TSharedPtr<FUICommandInfo>, FUIAction>>& GetVertexSelectionModifiers() const = 0;
	virtual const TArray<TTuple<TSharedPtr<FUICommandInfo>, FUIAction>>& GetEdgeSelectionModifiers() const = 0;
	virtual const TArray<TTuple<TSharedPtr<FUICommandInfo>, FUIAction>>& GetPolygonSelectionModifiers() const = 0;
	virtual const TArray<TTuple<TSharedPtr<FUICommandInfo>, FUIAction>>& GetFractureSelectionModifiers() const = 0;

	virtual bool IsEditingPerInstance() const = 0;
	virtual void SetEditingPerInstance( bool bPerInstance ) = 0;

	/** Propagates instance changes to the static mesh asset */
	virtual void PropagateInstanceChanges() = 0;

	/** Whether there are instance changes which can be propagated */
	virtual bool CanPropagateInstanceChanges() const = 0;

	/** Returns the current action to use when interacting the next time with the specified type of mesh element selection mode */
	virtual FName GetEquippedAction( const EEditableMeshElementType ForElementType ) const = 0;

	/** Sets the current action to use when interacting the next time with the specified type of mesh element selection mode */
	virtual void SetEquippedAction( const EEditableMeshElementType ForElementType, const FName ActionToEquip ) = 0;

};
