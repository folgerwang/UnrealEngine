// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshEditorCommands.h"
#include "SplitMesh.generated.h"


/** Attempts to split the mesh into two from a selected plane */
UCLASS()
class USplitMeshCommand : public UMeshEditorInstantCommand
{
	GENERATED_BODY()

protected:

	// Overrides
    virtual EEditableMeshElementType GetElementType() const { return EEditableMeshElementType::Invalid; }
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;
	virtual void Execute( class IMeshEditorModeEditingContract& MeshEditorMode ) override;

};
