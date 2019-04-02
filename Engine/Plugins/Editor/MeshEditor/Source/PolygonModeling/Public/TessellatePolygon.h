// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshEditorCommands.h"
#include "TessellatePolygon.generated.h"


/** Tessellates selected polygons into smaller polygons */
UCLASS()
class POLYGONMODELING_API UTessellatePolygonCommand : public UMeshEditorInstantCommand
{
	GENERATED_BODY()

protected:

	// Overrides
	virtual EEditableMeshElementType GetElementType() const override
	{
		return EEditableMeshElementType::Polygon;
	}
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;
	virtual void Execute( class IMeshEditorModeEditingContract& MeshEditorMode ) override;

};
