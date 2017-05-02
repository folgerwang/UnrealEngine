// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshEditorCommands.h"
#include "QuadrangulateMesh.generated.h"


/** Quadrangulates the currently selected mesh */
UCLASS()
class UQuadrangulateMeshCommand : public UMeshEditorCommonCommand
{
	GENERATED_BODY()

protected:

	// Overrides
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;
	virtual void Execute( class IMeshEditorModeEditingContract& MeshEditorMode ) override;

};
