// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "MeshEditorCommands.h"
#include "EditableMesh.h"
#include "GeometryCollectionCommandCommon.h"
#include "CollapseSelectedHierarchyCommand.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCollapseSelectedHierarchyCommand, Log, All);

/** Performs merging of the currently selected meshes */
UCLASS()
class UCollapseSelectedHierarchyCommand : public UMeshEditorInstantCommand, public FGeometryCollectionCommandCommon
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

private:
	void CollapseHierarchies(IMeshEditorModeEditingContract& MeshEditorMode, TArray<UEditableMesh*>& SelectedMeshes);
};
