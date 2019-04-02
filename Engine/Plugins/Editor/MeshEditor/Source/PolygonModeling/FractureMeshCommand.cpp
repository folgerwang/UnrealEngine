// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "FractureMeshCommand.h"
#include "IMeshEditorModeEditingContract.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "MeshAttributes.h"
#include "MeshDescription.h"
#include "PackageTools.h"
#include "FractureMesh.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionActor.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "EditorSupportDelegates.h"
#include "FractureToolDelegates.h"

#define LOCTEXT_NAMESPACE "FractureMeshCommand"

DEFINE_LOG_CATEGORY(LogFractureCommand);

void UFractureMeshCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "FractureMesh", " Fracture Mesh", "Performs fracture on selected mesh.", EUserInterfaceActionType::Button, FInputChord() );
}

void UFractureMeshCommand::Execute(IMeshEditorModeEditingContract& MeshEditorMode)
{
	if (MeshEditorMode.GetActiveAction() != NAME_None)
	{
		return;
	}

	if (MeshEditorMode.GetSelectedEditableMeshes().Num() == 0)
	{
		return;
	}

	const UMeshFractureSettings* FractureSettings = MeshEditorMode.GetFractureSettings();

	FScopedTransaction Transaction(LOCTEXT("FractureMesh", "Fracture Mesh"));

	MeshEditorMode.CommitSelectedMeshes();

	TArray<UEditableMesh*> SelectedMeshes = MeshEditorMode.GetSelectedEditableMeshes();

	TArray<AActor*> PlaneActors;
	TArray<UPlaneCut> PlaneCuts;
	if (FractureSettings->CommonSettings->FractureMode == EMeshFractureMode::PlaneCut)
	{
		ExtractPlaneCutsFromPlaneActors(SelectedMeshes, PlaneCuts, PlaneActors);
	}

	for (UEditableMesh* EditableMesh : SelectedMeshes)
	{
		AActor* SelectedActor = GetEditableMeshActor(EditableMesh);
		check(SelectedActor);

		if (FractureSettings->CommonSettings->FractureMode == EMeshFractureMode::PlaneCut)
		{
			if (IsPlaneActor(SelectedActor, PlaneActors))
				continue;

			FractureSettings->PlaneCutSettings->PlaneCuts.Empty();
			for (UPlaneCut Cut : PlaneCuts)
			{
				UPlaneCut LocalCut;
				// values need to be relative to the cut actor's transform
				LocalCut.Position = Cut.Position - SelectedActor->GetTransform().GetTranslation();
				LocalCut.Normal = Cut.Normal;
				FractureSettings->PlaneCutSettings->PlaneCuts.Push(LocalCut);
			}
		}

		EditableMesh->StartModification(EMeshModificationType::Final, EMeshTopologyChange::TopologyChange);
		{
			FractureMesh(SelectedActor, MeshEditorMode, EditableMesh, *FractureSettings);

			UGeometryCollectionComponent* Component = Cast<UGeometryCollectionComponent>(SelectedActor->GetComponentByClass(UGeometryCollectionComponent::StaticClass()));
			if (Component)
			{
				Component->MarkRenderDynamicDataDirty();
				Component->MarkRenderStateDirty();
			}
		}
		EditableMesh->EndModification();

		MeshEditorMode.TrackUndo(EditableMesh, EditableMesh->MakeUndo());
	}

	UpdateExplodedView(MeshEditorMode, EViewResetType::RESET_ALL);

}

void UFractureMeshCommand::ExtractPlaneCutsFromPlaneActors(TArray<UEditableMesh*>& SelectedMeshes, TArray<UPlaneCut>& PlaneCuts, TArray<AActor*>& PlaneActors)
{
	const TArray<AActor*> SelectedActors = GetSelectedActors();

	for (AActor* Actor : SelectedActors)
	{
		if (Actor->GetName().StartsWith("Plane", ESearchCase::IgnoreCase))
		{
			UEditableMesh* CuttingMesh = GetEditableMeshForActor(Actor, SelectedMeshes);

			if (CuttingMesh)
			{
				FTransform PlaneTransform = Actor->GetTransform();
				UPlaneCut PlaneCutSettings;

				for (const auto PolygonID : CuttingMesh->GetMeshDescription()->Polygons().GetElementIDs())
				{
					PlaneCutSettings.Position = PlaneTransform.TransformPosition(CuttingMesh->ComputePolygonCenter(PolygonID));
					PlaneCutSettings.Normal = PlaneTransform.TransformVector(CuttingMesh->ComputePolygonNormal(PolygonID));
					PlaneCuts.Push(PlaneCutSettings);
					break;
				}
				PlaneActors.Push(Actor);
			}
		}
	}
}

bool UFractureMeshCommand::IsPlaneActor(const AActor* SelectedActor, TArray<AActor *>& PlaneActors)
{
	for (const AActor* Actor : PlaneActors)
	{
		if (SelectedActor == Actor)
			return true;
	}

	return false;
}

void UFractureMeshCommand::FractureMesh(AActor* OriginalActor, IMeshEditorModeEditingContract& MeshEditorMode, UEditableMesh* SourceMesh, const UMeshFractureSettings& FractureSettings)
{
	UFractureMesh* FractureIF = NewObject<UFractureMesh>(GetTransientPackage());

	const FTransform& Transform = OriginalActor->GetTransform();
	const FString& Name = OriginalActor->GetActorLabel();

	// Try Get the GeometryCollectionComponent from the editable mesh
	UGeometryCollectionComponent* GeometryCollectionComponent = GetGeometryCollectionComponent(SourceMesh);
	TArray<FGeneratedFracturedChunk> GeneratedChunks;
	TArray<int32> DeletedChunks;

	// if no GeometryCollectionComponent exists then create a Geometry Collection Actor
	if (GeometryCollectionComponent == nullptr)
	{
		// create new GeometryCollectionActor
		AGeometryCollectionActor* NewActor = CreateNewGeometryActor(Name, Transform, SourceMesh);

		FGeometryCollectionEdit GeometryCollectionEdit = NewActor->GetGeometryCollectionComponent()->EditRestCollection();
		UGeometryCollection* GeometryCollectionObject = GeometryCollectionEdit.GetRestCollection();
		check(GeometryCollectionObject);

		// add fracture chunks to this geometry collection
		int RandomSeed = FractureSettings.CommonSettings->RandomSeed;
		FractureIF->FractureMesh(SourceMesh, Name, FractureSettings, -1, Transform, RandomSeed, GeometryCollectionObject, GeneratedChunks, DeletedChunks);

		check(DeletedChunks.Num() == 0);

		for (FGeneratedFracturedChunk& GeneratedChunk : GeneratedChunks)
		{
			GeometryCollectionObject->AppendGeometry(*GeneratedChunk.GeometryCollectionObject);
			FractureIF->FixupHierarchy(0, GeometryCollectionObject, GeneratedChunk, Name);
		}

		ensure(GeometryCollectionObject->GetGeometryCollection()->HasContiguousFaces());
		ensure(GeometryCollectionObject->GetGeometryCollection()->HasContiguousVertices());

		// select the new actor in the editor
		GEditor->SelectActor(OriginalActor, false, true);
		GEditor->SelectActor(NewActor, true, true);

		if (FractureSettings.CommonSettings->DeleteSourceMesh)
		{
			RemoveActor(OriginalActor);
		}
	}
	else
	{
		// scoped edit of collection
		FGeometryCollectionEdit GeometryCollectionEdit = GeometryCollectionComponent->EditRestCollection();
		UGeometryCollection* GeometryCollectionObject = GeometryCollectionEdit.GetRestCollection();
		TSharedPtr<FGeometryCollection> GeometryCollection = GeometryCollectionObject->GetGeometryCollection();

		AddAdditionalAttributesIfRequired(GeometryCollectionObject);
		AddSingleRootNodeIfRequired(GeometryCollectionObject);

		int RandomSeed = FractureSettings.CommonSettings->RandomSeed;

		for (int32 FracturedChunkIndex : GeometryCollectionComponent->GetSelectedBones())
		{
			TArray<int32> LeafBones;
			FGeometryCollectionClusteringUtility::GetLeafBones(GeometryCollection.Get(), FracturedChunkIndex, LeafBones);
			for (int32 LeafBone : LeafBones)
			{
				FractureIF->FractureMesh(SourceMesh, Name, FractureSettings, LeafBone, Transform, RandomSeed++, GeometryCollectionObject, GeneratedChunks, DeletedChunks);
			}
		}

		// delete the parent chunk that has just been fractured into component chunks
		GeometryCollection.Get()->RemoveGeometryElements(DeletedChunks);

		// add the new fracture chunks to the existing geometry collection
		for (FGeneratedFracturedChunk& GeneratedChunk : GeneratedChunks)
		{
			GeometryCollectionObject->AppendGeometry(*GeneratedChunk.GeometryCollectionObject);
			FractureIF->FixupHierarchy(GeneratedChunk.FracturedChunkIndex, GeometryCollectionObject, GeneratedChunk, Name);
		}

		ensure(GeometryCollectionObject->GetGeometryCollection()->HasContiguousFaces());
		ensure(GeometryCollectionObject->GetGeometryCollection()->HasContiguousVertices());

	}

	delete FractureIF;

}

#undef LOCTEXT_NAMESPACE
