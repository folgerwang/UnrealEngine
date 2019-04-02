// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AutoClusterMeshCommand.h"
#include "IMeshEditorModeEditingContract.h"
#include "IMeshEditorModeUIContract.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Framework/Commands/UIAction.h"
#include "MeshFractureSettings.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "EditorSupportDelegates.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"

#define LOCTEXT_NAMESPACE "ClusterMeshCommand"

DEFINE_LOG_CATEGORY(LogAutoClusterCommand);

FUIAction UAutoClusterMeshCommand::MakeUIAction(class IMeshEditorModeUIContract& MeshEditorMode)
{
	FUIAction UIAction;
	{
		FExecuteAction ExecuteAction(FExecuteAction::CreateLambda([&MeshEditorMode, this]
		{
			this->Execute(MeshEditorMode);
		}));

		// The 'Auto-cluster' button is only available when there is a geometry collection selected and we are viewing Level 1 in the hierarchy
		// button is grayed out at other times
		UIAction = FUIAction(
			ExecuteAction,
			FCanExecuteAction::CreateLambda([&MeshEditorMode] { return (MeshEditorMode.GetSelectedEditableMeshes().Num() > 0)
				&& MeshEditorMode.GetFractureSettings()->CommonSettings->ViewMode == EMeshFractureLevel::Level1; })
		);
	}
	return UIAction;

}

void UAutoClusterMeshCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "AutoClusterMesh", "Auto Cluster", "Performs Voronoi Cluster.", EUserInterfaceActionType::Button, FInputChord() );
}

void UAutoClusterMeshCommand::Execute(IMeshEditorModeEditingContract& MeshEditorMode)
{
	if (MeshEditorMode.GetActiveAction() != NAME_None)
	{
		return;
	}

	if (MeshEditorMode.GetSelectedEditableMeshes().Num() == 0)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("AutoClusterMesh", "Auto Cluster Mersh"));

	MeshEditorMode.CommitSelectedMeshes();

	TArray<UEditableMesh*> SelectedMeshes = MeshEditorMode.GetSelectedEditableMeshes();

	// we only handle clustering of a single geometry collection
	if (SelectedMeshes.Num() == 1 && GetGeometryCollectionComponent(SelectedMeshes[0]))
	{
		// Combining child bones from within a single Editable Mesh that already is a Geometry Collection
		ClusterChildBonesOfASingleMesh(MeshEditorMode, SelectedMeshes);
	}

	UpdateExplodedView(MeshEditorMode, EViewResetType::RESET_TRANSFORMS);
}


void UAutoClusterMeshCommand::ClusterChildBonesOfASingleMesh(IMeshEditorModeEditingContract& MeshEditorMode, TArray<UEditableMesh*>& SelectedMeshes)
{
	const UMeshFractureSettings* FratureSettings = MeshEditorMode.GetFractureSettings();
	int8 FractureLevel = FratureSettings->CommonSettings->GetFractureLevelNumber();
	int NumClusters = FratureSettings->UniformSettings->NumberVoronoiSites;

	for (UEditableMesh* EditableMesh : SelectedMeshes)
	{
		AActor* SelectedActor = GetEditableMeshActor(EditableMesh);
		check(SelectedActor);

		EditableMesh->StartModification(EMeshModificationType::Final, EMeshTopologyChange::TopologyChange);
		{
			UGeometryCollectionComponent* Component = Cast<UGeometryCollectionComponent>(SelectedActor->GetComponentByClass(UGeometryCollectionComponent::StaticClass()));

			if (Component)
			{
				ClusterSelectedBones(FractureLevel, NumClusters, EditableMesh, Component);
			}
		}
		EditableMesh->EndModification();

		MeshEditorMode.TrackUndo(EditableMesh, EditableMesh->MakeUndo());
	}
}


void UAutoClusterMeshCommand::ClusterSelectedBones(int FractureLevel, int NumClusters, UEditableMesh* EditableMesh, UGeometryCollectionComponent* GeometryCollectionComponent)
{
	check(EditableMesh);
	check(GeometryCollectionComponent);

	if (FractureLevel > 0)
	{
		FGeometryCollectionEdit GeometryCollectionEdit = GeometryCollectionComponent->EditRestCollection();
		if (UGeometryCollection* GeometryCollectionObject = GeometryCollectionEdit.GetRestCollection())
		{
			TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
			if (FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get())
			{
				const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollection->BoneHierarchy;

				TMap<int, FVector> Locations;
				for (int Element = 0; Element < Hierarchy.Num(); Element++)
				{
					const FGeometryCollectionBoneNode& Node = Hierarchy[Element];
					if (Node.Level == FractureLevel)
					{
						FVector Centre(0, 0, 0);
						GetCenterOfBone(GeometryCollectionObject, Element, Centre);
						Locations.Add(Element, Centre);
					}
				}

				TArray<FVector> Sites;
				GenerateClusterSites(NumClusters, FractureLevel, Locations, Sites);
				if (Sites.Num() > 0)
				{
					ClusterToNearestSite(FractureLevel, GeometryCollectionComponent, Locations, Sites);
				}

				GeometryCollectionComponent->MarkRenderDynamicDataDirty();
				GeometryCollectionComponent->MarkRenderStateDirty();
				FEditorSupportDelegates::RedrawAllViewports.Broadcast();
			}
		}
	}
	
}

void UAutoClusterMeshCommand::GenerateClusterSites(int NumSitesToGenerate, int FractureLevel, TMap<int, FVector>& Locations, TArray<FVector>& Sites)
{
	TArray<int> Keys;
	for (auto& Location : Locations)
	{
		Keys.Push(Location.Key);
	}

	if (Keys.Num() > 0)
	{
		for (int SiteIndex = 0; SiteIndex < NumSitesToGenerate; SiteIndex++)
		{
			int RandomIndex = FMath::Rand() % Keys.Num();
			Sites.Push(Locations[Keys[RandomIndex]]);
		}
	}
}

void UAutoClusterMeshCommand::ClusterToNearestSite(int FractureLevel, UGeometryCollectionComponent* GeometryCollectionComponent, TMap<int, FVector>& Locations, TArray<FVector>& Sites)
{
	FGeometryCollectionEdit GeometryCollectionEdit = GeometryCollectionComponent->EditRestCollection();
	if (UGeometryCollection* GeometryCollectionObject = GeometryCollectionEdit.GetRestCollection())
	{
		TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
		if (FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get())
		{

			TArray<TArray<int>> SiteToBone;
			SiteToBone.AddDefaulted(Sites.Num());

			for (auto& location : Locations)
			{
				int NearestSite = FindNearestSitetoBone(location.Value, Sites);
				if (NearestSite >= 0)
				{
					SiteToBone[NearestSite].Push(location.Key);
				}
			}

			for (int SiteIndex = 0; SiteIndex < Sites.Num(); SiteIndex++)
			{
				if (SiteToBone[SiteIndex].Num() > 0)
				{
					FGeometryCollectionClusteringUtility::ClusterBonesUnderNewNode(GeometryCollection, SiteToBone[SiteIndex][0], SiteToBone[SiteIndex], false);
				}
			}
		}
	}
}

int UAutoClusterMeshCommand::FindNearestSitetoBone(const FVector& BoneLocation, const TArray<FVector>& Sites)
{
	// brute force search
	int ClosestSite = -1;
	float ClosestDistSqr = FLT_MAX;
	for (int SiteIndex = 0; SiteIndex < Sites.Num(); SiteIndex++)
	{
		const FVector& SiteLocation = Sites[SiteIndex];
		float DistanceSqr = FVector::DistSquared(SiteLocation, BoneLocation);
		if (DistanceSqr < ClosestDistSqr)
		{
			ClosestDistSqr = DistanceSqr;
			ClosestSite = SiteIndex;
		}
	}

	return ClosestSite;
}


#undef LOCTEXT_NAMESPACE
