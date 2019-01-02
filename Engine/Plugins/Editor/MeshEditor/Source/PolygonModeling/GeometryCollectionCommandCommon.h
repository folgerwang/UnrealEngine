// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "MeshEditorCommands.h"
#include "EditableMesh.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollectionBoneNode.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "MeshFractureSettings.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGeometryCommandCommon, Log, All);


/** Common functionality between Fracture & Cluster commands */
class FGeometryCollectionCommandCommon
{
protected:
	/** Try Get the Geometry Collection Component from the editable mesh submesh address, can return nullptr if the editable mesh is not from a Geometry Collection */
	UGeometryCollectionComponent* GetGeometryCollectionComponent(UEditableMesh* SourceMesh);

	/** GetStatic mesh from editable mesh */
	UStaticMesh* GetStaticMesh(UEditableMesh* SourceMesh);

	/** Get the actor associated with this editable mesh */
	AActor* GetEditableMeshActor(UEditableMesh* EditableMesh);
	
	/** Find the editable mesh associated with this actor */
	UEditableMesh* GetEditableMeshForActor(AActor* Actor, TArray<UEditableMesh *>& SelectedMeshes);

	/** Create a Geometry Actor */
	class AGeometryCollectionActor* CreateNewGeometryActor(const FString& Name, const FTransform& Transform, UEditableMesh* SourceMesh);

	/** Create a geomerty Collection asset */
	class UPackage* CreateGeometryCollectionPackage(UGeometryCollection*& GeometryCollection);

	/** Where fracturing is concerned we expect a single parent root node */
	void AddSingleRootNodeIfRequired(UGeometryCollection* GeometryCollection);

	/** Add fracturing attributes to geometry collection */
	void AddAdditionalAttributesIfRequired(UGeometryCollection* OutGeometryCollection);

	/** Remove original actor from world is an option when fracturing meshes since a new Geometry Collection actor is created at this time */
	void RemoveActor(AActor* Actor);

	/** Debug logging of geometry collection details */
	void LogHierarchy(const UGeometryCollection* GeometryCollection);

	/** Update Exploded view from UI slider position */
	void UpdateExplodedView(class IMeshEditorModeEditingContract &MeshEditorMode, EViewResetType ResetType);

	/** The fracture system expects there to be only one root */
	int GetRootBone(const UGeometryCollection* GeometryCollection);

	/** Appen all the selected meshes to a geometry collection */
	void AppendMeshesToGeometryCollection(TArray<UEditableMesh*>& SelectedMeshes, UEditableMesh* SourceMesh, FTransform &SourceActorTransform, UGeometryCollection* GeometryCollection, bool DeleteSourceMesh, TArray<int32>& OutNewNodeElements);

	/** Merge two lists of selections into one */
	void MergeSelections(const UGeometryCollectionComponent* SourceComponent, const TArray<int32>& SelectionB, TArray<int32>& MergedSelectionOut);

	/** Determine center of bone by combining centres of child geometry nodes */
	void GetCenterOfBone(UGeometryCollection* GeometryCollection, int Element, FVector& CentreOut);

	/** recursively find centres of child geometry nodes */
	void CombineCenterOfGeometryRecursive(TArray<FTransform>& Transforms, const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy, int Element, FVector& SumCOMOut, int& CountOut);

	/** Get array of selected actors */
	TArray<AActor*> GetSelectedActors();
};