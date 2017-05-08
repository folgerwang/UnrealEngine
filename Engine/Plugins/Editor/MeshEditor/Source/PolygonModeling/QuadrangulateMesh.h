// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshEditorCommands.h"
#include "QuadrangulateMesh.generated.h"


/** Quadrangulates the currently selected mesh */
UCLASS()
class UQuadrangulateMeshCommand : public UMeshEditorInstantCommand
{
	GENERATED_BODY()

protected:

	// Overrides
	virtual EEditableMeshElementType GetElementType() const override
	{
		return EEditableMeshElementType::Invalid;
	}
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;
	virtual void Execute( class IMeshEditorModeEditingContract& MeshEditorMode ) override;

};
