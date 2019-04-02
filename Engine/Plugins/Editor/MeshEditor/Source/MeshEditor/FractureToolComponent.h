// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/MeshComponent.h"
#include "GameFramework/Actor.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "MeshFractureSettings.h"

#include "FractureToolComponent.generated.h"

struct FBoneHierarchyConstantData;
struct FBoneHierarchyDynamicData;
class UEditableMesh;

/**
*	UFractureToolComponent
*/
UCLASS()
class UFractureToolComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UFractureToolComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void OnRegister()  override;

	/** Called at end of expansion slider movement, or after a fracture command */
	void OnFractureExpansionEnd();
	/** Called during expansion slider movement */
	void OnFractureExpansionUpdate();
	/** View setting changed */
	void OnVisualisationSettingsChanged(bool ShowBoneColors);
	/** Fracture level view changed */
	void OnFractureLevelChanged(uint8 ViewLevel);

	void UpdateBoneState(UPrimitiveComponent* Component);
	void SetSelectedBones(UEditableMesh* EditableMesh, int32 BoneSelected, bool Multiselection, bool ShowBoneColorsIn);

	void OnSelected(UPrimitiveComponent* SelectedComponent);
	void OnDeselected(UPrimitiveComponent* DeselectedComponent);

	void OnEnterFractureMode();
	void OnExitFractureMode();

	void OnUpdateFractureLevelView(uint8 FractureLevel);
	void OnUpdateExplodedView(uint8 ResetType, uint8 FractureLevel) const;

private:
	TArray<AActor*> GetSelectedActors() const;
	AActor* GetEditableMeshActor(UEditableMesh* EditableMesh);
	AActor* GetEditableMeshActor();
	UGeometryCollection* GetGeometryCollection(const UEditableMesh* SourceMesh);
	UGeometryCollectionComponent* GetGeometryCollectionComponent(UEditableMesh* SourceMesh);
	UGeometryCollectionComponent* GetGeometryCollectionComponent();

	void ExplodeInLevels(class AGeometryCollectionActor* GeometryActor) const;
	void ExplodeLinearly(class AGeometryCollectionActor* GeometryActor, EMeshFractureLevel FractureLevel) const;
	float CalculateComponentScaling(class UGeometryCollectionComponent* GeometryCollectionComponent) const;
	void ShowGeometry(class UGeometryCollection* GeometryCollection, int Index, bool GeometryVisible, bool IncludeChildren = true);
	bool HasExplodedAttributes(AGeometryCollectionActor* GeometryActor) const;

	bool ShowBoneColors;

};
