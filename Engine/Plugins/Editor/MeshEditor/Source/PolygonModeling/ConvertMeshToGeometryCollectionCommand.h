// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "MeshEditorCommands.h"
#include "ConvertMeshToGeometryCollectionCommand.generated.h"

/** Performs merging of the currently selected meshes */
UCLASS()
class UConvertMeshToGeometryCollectionCommand : public UMeshEditorInstantCommand
{
public:
	GENERATED_BODY()

protected:

	// Overrides
	virtual EEditableMeshElementType GetElementType() const override
	{
		return EEditableMeshElementType::Fracture;
	}
	virtual void RegisterUICommand(class FBindingContext* BindingContext) override;
	virtual void Execute(class IMeshEditorModeEditingContract& MeshEditorMode) override;
};
