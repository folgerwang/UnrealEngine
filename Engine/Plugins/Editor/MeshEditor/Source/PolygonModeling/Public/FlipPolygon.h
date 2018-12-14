// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshEditorCommands.h"

#include "FlipPolygon.generated.h"

UCLASS()
class POLYGONMODELING_API UFlipPolygonCommand : public UMeshEditorInstantCommand
{
	GENERATED_BODY()

public:

	// Overrides
	virtual EEditableMeshElementType GetElementType() const override { return EEditableMeshElementType::Polygon; }

	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;
	virtual void Execute( class IMeshEditorModeEditingContract& MeshEditorMode ) override;
};