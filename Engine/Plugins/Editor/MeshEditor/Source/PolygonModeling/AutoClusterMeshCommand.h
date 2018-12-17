// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "MeshEditorCommands.h"
#include "EditableMesh.h"
#include "GeometryCollectionCommandCommon.h"
#include "AutoClusterMeshCommand.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAutoClusterCommand, Log, All);

/** Performs clustering of the currently selected geometry collection bones */
UCLASS()
class UAutoClusterMeshCommand : public UMeshEditorInstantCommand, public FGeometryCollectionCommandCommon
{
public:
	GENERATED_BODY()

protected:

	// Overrides
	virtual EEditableMeshElementType GetElementType() const override
	{
		return EEditableMeshElementType::Fracture;
	}
	virtual FUIAction MakeUIAction(class IMeshEditorModeUIContract& MeshEditorMode) override;
	virtual void RegisterUICommand(class FBindingContext* BindingContext) override;
	virtual void Execute(class IMeshEditorModeEditingContract& MeshEditorMode) override;

private:
	void ClusterChildBonesOfASingleMesh(IMeshEditorModeEditingContract& MeshEditorMode, TArray<UEditableMesh*>& SelectedMeshes);
	void ClusterSelectedBones(int FractureLevel, int NumClusters, UEditableMesh* EditableMesh, UGeometryCollectionComponent* GeometryCollectionComponent);
	void GenerateClusterSites(int NumSitesToGenerate, int FractureLevel, TMap<int, FVector>& Locations, TArray<FVector>& Sites);
	void ClusterToNearestSite(int FractureLevel, UGeometryCollectionComponent* GeometryCollectionComponent, TMap<int, FVector>& Locations, TArray<FVector>& Sites);
	int FindNearestSitetoBone(const FVector& Location, const TArray<FVector>& Sites);
};
