// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "MeshEditorCommands.h"
#include "EditableMesh.h"
#include "MeshFractureSettings.h"
#include "GeneratedFracturedChunk.h"
#include "GeometryCollectionCommandCommon.h"
#include "FractureMeshCommand.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogFractureCommand, Log, All);


/** Performs Voronoi or Slicing fracture of the currently selected mesh */
UCLASS()
class UFractureMeshCommand : public UMeshEditorInstantCommand, public FGeometryCollectionCommandCommon
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
	/** Performs fracturing of an Editable Mesh */
	void FractureMesh(AActor* OriginalActor, IMeshEditorModeEditingContract& MeshEditorMode, UEditableMesh* SourceMesh, const UMeshFractureSettings& FractureSettings);

	/** Extract plane cut settings from plane Actor in same Scene */
	void ExtractPlaneCutsFromPlaneActors(TArray<UEditableMesh*>& SelectedMeshes, TArray<UPlaneCut>& PlaneCuts, TArray<AActor*>& PlaneActors);
	bool IsPlaneActor(const AActor* SelectedActor, TArray<AActor *>& PlaneActors);

};
