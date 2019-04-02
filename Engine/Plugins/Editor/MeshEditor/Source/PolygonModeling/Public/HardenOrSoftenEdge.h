// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshEditorCommands.h"
#include "HardenOrSoftenEdge.generated.h"


/** Makes an edge hard */
UCLASS()
class POLYGONMODELING_API UHardenEdgeCommand : public UMeshEditorInstantCommand
{
	GENERATED_BODY()

protected:

	// Overrides
	virtual EEditableMeshElementType GetElementType() const override
	{
		return EEditableMeshElementType::Edge;
	}
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;
	virtual void Execute( class IMeshEditorModeEditingContract& MeshEditorMode ) override;

};


/** Makes an edge soft */
UCLASS()
class USoftenEdgeCommand : public UMeshEditorInstantCommand
{
	GENERATED_BODY()

protected:

	// Overrides
	virtual EEditableMeshElementType GetElementType() const override
	{
		return EEditableMeshElementType::Edge;
	}
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;
	virtual void Execute( class IMeshEditorModeEditingContract& MeshEditorMode ) override;

};
