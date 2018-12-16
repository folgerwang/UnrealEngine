// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshEditorCommands.h"

#include "AssignMaterial.generated.h"

/** Assigns the highlighted material to the currently selected polygon(s) */
UCLASS()
class POLYGONMODELING_API UAssignMaterialCommand : public UMeshEditorInstantCommand
{
	GENERATED_BODY()

public:

	// Overrides
	virtual EEditableMeshElementType GetElementType() const override { return EEditableMeshElementType::Polygon; }

	// UMeshEditorCommand overrides
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;
	virtual void Execute( class IMeshEditorModeEditingContract& MeshEditorMode ) override;
};