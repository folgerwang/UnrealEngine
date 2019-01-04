// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ClusterMeshCommand.h"
#include "IMeshEditorModeEditingContract.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "MeshAttributes.h"
#include "MeshDescription.h"
#include "PackageTools.h"
#include "MeshFractureSettings.h"
#include "EditableMeshFactory.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionActor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "EditorSupportDelegates.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"


#define LOCTEXT_NAMESPACE "ClusterMeshCommand"

DEFINE_LOG_CATEGORY(LogClusterCommand);


void UClusterMeshCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "ClusterMesh", "Cluster Under New Node", "Performs cluster of selected meshes by reparenting to a new node.", EUserInterfaceActionType::Button, FInputChord() );
}

void UClusterMeshCommand::Execute(IMeshEditorModeEditingContract& MeshEditorMode)
{
	if (MeshEditorMode.GetActiveAction() != NAME_None)
	{
		return;
	}

	if (MeshEditorMode.GetSelectedEditableMeshes().Num() == 0)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("ClusterMesh", "Cluster Under New Node"));

	MeshEditorMode.CommitSelectedMeshes();

	TArray<UEditableMesh*> SelectedActors = MeshEditorMode.GetSelectedEditableMeshes();
	ClusterMeshes(MeshEditorMode, SelectedActors);

	UpdateExplodedView(MeshEditorMode, EViewResetType::RESET_TRANSFORMS); 
}

void UClusterMeshCommand::ClusterMeshes(IMeshEditorModeEditingContract& MeshEditorMode, TArray<UEditableMesh*>& SelectedMeshes)
{
	if (SelectedMeshes.Num() == 1 && GetGeometryCollectionComponent(SelectedMeshes[0]))
	{
		// Combining child bones from within a single Editable Mesh that already is a Geometry Collection
		ClusterChildBonesOfASingleMesh(MeshEditorMode, SelectedMeshes);
	}
	else
	{
		// Combining Separate meshes into a single Geometry Collection as leaf nodes
		ClusterMultipleMeshes(MeshEditorMode, SelectedMeshes);
	}
}

void UClusterMeshCommand::ClusterMultipleMeshes(IMeshEditorModeEditingContract& MeshEditorMode, TArray<UEditableMesh*>& SelectedMeshes)
{
	UGeometryCollectionComponent* GeometryCollectionComponent = nullptr;
	UEditableMesh* SourceMesh = nullptr;
	FTransform SourceActorTransform = FTransform::Identity;
	AGeometryCollectionActor* NewActor = nullptr;

	// find first geometry collection component
	for (UEditableMesh* EditableMesh : SelectedMeshes)
	{
		GeometryCollectionComponent = GetGeometryCollectionComponent(EditableMesh);
		if (GeometryCollectionComponent != nullptr)
		{
			SourceMesh = EditableMesh;
			AActor* SelectedActor = GetEditableMeshActor(SourceMesh);
			SourceActorTransform = SelectedActor->GetTransform();

			break;
		}
	}

	TArray<int32> NewNodeElements;
	UGeometryCollectionComponent* SourceGeometryCollectionComponent = nullptr;

	// if no GeometryCollectionComponent exits amongst all selected items then make a fresh Geometry Collection Actor 
	// and use its GeometryCollection as destination for all the selected geometry
	if (GeometryCollectionComponent == nullptr)
	{
		// just get required details from the first selected editable mesh actor
		AActor* SelectedActor = GetEditableMeshActor(SelectedMeshes[0]);
		check(SelectedActor);
		const FString& Name = SelectedActor->GetActorLabel();
		SourceActorTransform = SelectedActor->GetTransform();

		// The scale of each individual mesh will be part of transform passed in to AppendStaticMesh
		SourceActorTransform.SetScale3D(FVector(1.0f, 1.0f, 1.0f));
		NewActor = CreateNewGeometryActor(Name, SourceActorTransform, SelectedMeshes[0]);

		// scoped edit of collection
		SourceGeometryCollectionComponent = NewActor->GetGeometryCollectionComponent();
	}
	else
	{
		SourceGeometryCollectionComponent = GetGeometryCollectionComponent(SourceMesh);
	}

	check(SourceGeometryCollectionComponent);

	// scoped edit of collection
	FGeometryCollectionEdit GeometryCollectionEdit = SourceGeometryCollectionComponent->EditRestCollection();
	if (UGeometryCollection* GeometryCollectionObject = GeometryCollectionEdit.GetRestCollection())
	{
		TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
		if (FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get())
		{

			// add the extraneous meshes to our geometry collection
			bool DeleteSourceMesh = MeshEditorMode.GetFractureSettings()->CommonSettings->DeleteSourceMesh;
			AppendMeshesToGeometryCollection(SelectedMeshes, SourceMesh, SourceActorTransform, GeometryCollectionObject, DeleteSourceMesh, NewNodeElements);

			// merge original selection with newly created bones that were added to our geometry collection 
			TArray<int32> SourceElements;
			MergeSelections(SourceGeometryCollectionComponent, NewNodeElements, SourceElements);

			// cluster Selected Bones under the first selected bone
			int32 InsertAtIndex = SourceElements[0];
			FGeometryCollectionClusteringUtility::ClusterBonesUnderNewNode(GeometryCollection, InsertAtIndex, SourceElements, false);

			if (NewActor)
			{
				// select the new actor in the editor
				GEditor->SelectActor(NewActor, true, true);
			}

			if (GeometryCollectionObject)
			{
				LogHierarchy(GeometryCollectionObject);
			}
		}
	}
}


void UClusterMeshCommand::ClusterChildBonesOfASingleMesh(IMeshEditorModeEditingContract& MeshEditorMode, TArray<UEditableMesh*>& SelectedMeshes)
{
	for (UEditableMesh* EditableMesh : SelectedMeshes)
	{
		AActor* SelectedActor = GetEditableMeshActor(EditableMesh);
		check(SelectedActor);

		EditableMesh->StartModification(EMeshModificationType::Final, EMeshTopologyChange::TopologyChange);
		{
			UGeometryCollectionComponent* Component = Cast<UGeometryCollectionComponent>(SelectedActor->GetComponentByClass(UGeometryCollectionComponent::StaticClass()));

			if (Component)
			{
				ClusterSelectedBones(EditableMesh, Component);
				Component->MarkRenderDynamicDataDirty();
				Component->MarkRenderStateDirty();
			}
		}
		EditableMesh->EndModification();

		MeshEditorMode.TrackUndo(EditableMesh, EditableMesh->MakeUndo());
	}
}


void UClusterMeshCommand::ClusterSelectedBones(UEditableMesh* EditableMesh, UGeometryCollectionComponent* GeometryCollectionComponent)
{
	check(EditableMesh);
	check(GeometryCollectionComponent);
	const TArray<int32> SelectedBones = GeometryCollectionComponent->GetSelectedBones();

	if (SelectedBones.Num() > 1)
	{
		FGeometryCollectionEdit GeometryCollectionEdit = GeometryCollectionComponent->EditRestCollection();
		if (UGeometryCollection* GeometryCollectionObject = GeometryCollectionEdit.GetRestCollection())
		{
			TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
			if (FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get())
			{

				AddAdditionalAttributesIfRequired(GeometryCollectionObject);
				AddSingleRootNodeIfRequired(GeometryCollectionObject);

				//UE_LOG(LogClusterCommand, Log, TEXT("Hierarchy Before Clustering ..."));
				//LogHierarchy(GeometryCollection);

				// cluster Selected Bones under the first selected bone
				int32 InsertAtIndex = SelectedBones[0];
				FGeometryCollectionClusteringUtility::ClusterBonesUnderNewNode(GeometryCollection, InsertAtIndex, SelectedBones, false);

				//UE_LOG(LogClusterCommand, Log, TEXT("Hierarchy After Clustering ..."));
				//LogHierarchy(GeometryCollection);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
