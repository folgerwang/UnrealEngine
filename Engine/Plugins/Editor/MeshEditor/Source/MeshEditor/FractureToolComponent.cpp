// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved. 

#include "FractureToolComponent.h"
#include "Async/ParallelFor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "EditableMesh.h"
#include "MeshFractureSettings.h"

#include "GeometryCollection/GeometryCollectionActor.h"
#include "Engine/Selection.h"
#include "Editor.h"
#include "EditableMeshFactory.h"
#include "Materials/Material.h"
#include "FractureToolDelegates.h"
#include "EditorSupportDelegates.h"


DEFINE_LOG_CATEGORY_STATIC(UFractureToolComponentLogging, NoLogging, All);


UFractureToolComponent::UFractureToolComponent(const FObjectInitializer& ObjectInitializer) : ShowBoneColors(true)
{
}


void UFractureToolComponent::OnRegister()
{
	Super::OnRegister();

	FFractureToolDelegates::Get().OnFractureExpansionEnd.AddUObject(this, &UFractureToolComponent::OnFractureExpansionEnd);
	FFractureToolDelegates::Get().OnFractureExpansionUpdate.AddUObject(this, &UFractureToolComponent::OnFractureExpansionUpdate);
	FFractureToolDelegates::Get().OnVisualizationSettingsChanged.AddUObject(this, &UFractureToolComponent::OnVisualisationSettingsChanged);
	FFractureToolDelegates::Get().OnUpdateExplodedView.AddUObject(this, &UFractureToolComponent::OnUpdateExplodedView);
	FFractureToolDelegates::Get().OnUpdateFractureLevelView.AddUObject(this, &UFractureToolComponent::OnUpdateFractureLevelView);
}

void UFractureToolComponent::OnFractureExpansionEnd()
{
	UGeometryCollectionComponent* GeometryCollectionComponent = GetGeometryCollectionComponent();
	if (GeometryCollectionComponent)
	{
		FScopedColorEdit EditBoneColor = GeometryCollectionComponent->EditBoneSelection();
		EditBoneColor.SetShowBoneColors(ShowBoneColors);
	}
}

void UFractureToolComponent::OnFractureExpansionUpdate()
{
	UGeometryCollectionComponent* GeometryCollectionComponent = GetGeometryCollectionComponent();
	if (GeometryCollectionComponent)
	{
		FScopedColorEdit EditBoneColor = GeometryCollectionComponent->EditBoneSelection();
		EditBoneColor.SetShowBoneColors(ShowBoneColors);
	}
}

void UFractureToolComponent::OnVisualisationSettingsChanged(bool ShowBoneColorsIn)
{
	UGeometryCollectionComponent* GeometryCollectionComponent = GetGeometryCollectionComponent();
	if (GeometryCollectionComponent)
	{
		FScopedColorEdit EditBoneColor = GeometryCollectionComponent->EditBoneSelection();
		EditBoneColor.SetShowBoneColors(ShowBoneColorsIn);
		ShowBoneColors = ShowBoneColorsIn;
	}
}

void UFractureToolComponent::OnFractureLevelChanged(uint8 ViewLevelIn)
{
	UGeometryCollectionComponent* GeometryCollectionComponent = GetGeometryCollectionComponent();
	if (GeometryCollectionComponent)
	{
		FScopedColorEdit EditBoneColor = GeometryCollectionComponent->EditBoneSelection();
		EditBoneColor.SetLevelViewMode(ViewLevelIn - 1);

		const UGeometryCollection* RestCollection = GeometryCollectionComponent->GetRestCollection();
		if (RestCollection)
		{
			// Reset the selected bones as previous selection most likely won't make sense after changing the actively viewed level
			UEditableMesh* EditableMesh = Cast<UEditableMesh>(RestCollection->EditableMesh);
			if (EditableMesh)
			{
				EditBoneColor.ResetBoneSelection();
			}
		}
	}
}

void UFractureToolComponent::UpdateBoneState(UPrimitiveComponent* Component)
{
	UGeometryCollectionComponent* GeometryCollectionComponent = Cast<UGeometryCollectionComponent>(Component);
	if (GeometryCollectionComponent)
	{
		// this scoped method will refresh bone colors
		FScopedColorEdit EditBoneColor = GeometryCollectionComponent->EditBoneSelection();
	}
}

void UFractureToolComponent::SetSelectedBones(UEditableMesh* EditableMesh, int32 BoneSelected, bool Multiselection, bool ShowBoneColorsIn)
{
	UGeometryCollectionComponent* GeometryCollectionComponent = GetGeometryCollectionComponent(EditableMesh);
	if (GeometryCollectionComponent)
	{
		FScopedColorEdit EditBoneColor = GeometryCollectionComponent->EditBoneSelection();

		if (UGeometryCollection* MeshGeometryCollection = GetGeometryCollection(EditableMesh))
		{
			TSharedPtr<FGeometryCollection> GeometryCollectionPtr = MeshGeometryCollection->GetGeometryCollection();
			if (FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get())
			{

				// has the color mode been toggled
				ShowBoneColors = ShowBoneColorsIn;
				if (EditBoneColor.GetShowBoneColors() != ShowBoneColors)
				{
					EditBoneColor.SetShowBoneColors(ShowBoneColors);
				}
				EditBoneColor.SetShowSelectedBones(true);
				bool BoneWasAlreadySelected = EditBoneColor.IsBoneSelected(BoneSelected);

				// if multiselect then append new BoneSelected to what is already selected, otherwise we just clear and replace the old selection with BoneSelected
				if (!Multiselection)
				{
					EditBoneColor.ResetBoneSelection();
				}

				// toggle the bone selection
				if (BoneWasAlreadySelected)
				{
					EditBoneColor.ClearSelectedBone(BoneSelected);
				}
				else
				{
					EditBoneColor.AddSelectedBone(BoneSelected);
				}

				// The actual selection made is based on the hierarchy and the view mode
				if (GeometryCollection)
				{
					const TArray<int32>& Selected = EditBoneColor.GetSelectedBones();
					TArray<int32> RevisedSelected;
					TArray<int32> Highlighted;
					FGeometryCollectionClusteringUtility::ContextBasedClusterSelection(GeometryCollection, EditBoneColor.GetViewLevel(), Selected, RevisedSelected, Highlighted);
					EditBoneColor.SetSelectedBones(RevisedSelected);
					EditBoneColor.SetHighlightedBones(Highlighted);

					FFractureToolDelegates::Get().OnComponentSelectionChanged.Broadcast(GeometryCollectionComponent);
				}
			}

			FEditorSupportDelegates::RedrawAllViewports.Broadcast();

		}
	}
}


void UFractureToolComponent::OnSelected(UPrimitiveComponent* SelectedComponent)
{
	UGeometryCollectionComponent* GeometryCollectionComponent = Cast<UGeometryCollectionComponent>(SelectedComponent);
	if (GeometryCollectionComponent)
	{
		FScopedColorEdit EditBoneColor = GeometryCollectionComponent->EditBoneSelection();

		EditBoneColor.SetShowBoneColors(ShowBoneColors);
		EditBoneColor.SetShowSelectedBones(true);
	}

}

void UFractureToolComponent::OnDeselected(UPrimitiveComponent* DeselectedComponent)
{
	UGeometryCollectionComponent* GeometryCollectionComponent = Cast<UGeometryCollectionComponent>(DeselectedComponent);
	if (GeometryCollectionComponent)
	{
		FScopedColorEdit EditBoneColor = GeometryCollectionComponent->EditBoneSelection();

		EditBoneColor.SetShowBoneColors(false);
		EditBoneColor.SetShowSelectedBones(false);
	}
}

void UFractureToolComponent::OnEnterFractureMode()
{

}

void UFractureToolComponent::OnExitFractureMode()
{
	// find all the selected geometry collections and turn off color rendering mode
	AActor* ReturnActor = nullptr;
	const TArray<AActor*>& SelectedActors = GetSelectedActors();

	for (int i = 0; i < SelectedActors.Num(); i++)
	{
		TArray<UActorComponent*> PrimitiveComponents = SelectedActors[i]->GetComponentsByClass(UPrimitiveComponent::StaticClass());
		for (UActorComponent* PrimitiveActorComponent : PrimitiveComponents)
		{
			UPrimitiveComponent* Component = CastChecked<UPrimitiveComponent>(PrimitiveActorComponent);

			if (Component)
			{
				OnDeselected(Component);
			}
		}
	}
}

TArray<AActor*> UFractureToolComponent::GetSelectedActors() const
{
	USelection* SelectedActors = GEditor->GetSelectedActors();
	TArray<AActor*> Actors;
	for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		AActor* Actor = Cast<AActor>(*Iter);
		if (Actor)
		{
			Actors.Add(Actor);
		}
	}
	return Actors;
}

AActor* UFractureToolComponent::GetEditableMeshActor(UEditableMesh* EditableMesh)
{
	AActor* ReturnActor = nullptr;
	const TArray<AActor*>& SelectedActors = GetSelectedActors();

	for (int i = 0; i < SelectedActors.Num(); i++)
	{
		TArray<UActorComponent*> PrimitiveComponents = SelectedActors[i]->GetComponentsByClass(UPrimitiveComponent::StaticClass());
		for (UActorComponent* PrimitiveActorComponent : PrimitiveComponents)
		{
			UPrimitiveComponent* Component = CastChecked<UPrimitiveComponent>(PrimitiveActorComponent);
			FEditableMeshSubMeshAddress SubMeshAddress = UEditableMeshFactory::MakeSubmeshAddress(Component, 0);

			if (EditableMesh->GetSubMeshAddress() == SubMeshAddress)
			{
				ReturnActor = Component->GetOwner();
				break;
			}
		}
	}

	return ReturnActor;
}

AActor* UFractureToolComponent::GetEditableMeshActor()
{
	AActor* ReturnActor = nullptr;
	const TArray<AActor*>& SelectedActors = GetSelectedActors();

	for (int i = 0; i < SelectedActors.Num(); i++)
	{
		TArray<UActorComponent*> PrimitiveComponents = SelectedActors[i]->GetComponentsByClass(UPrimitiveComponent::StaticClass());
		for (UActorComponent* PrimitiveActorComponent : PrimitiveComponents)
		{
			UPrimitiveComponent* Component = CastChecked<UPrimitiveComponent>(PrimitiveActorComponent);

			if (Component)
			{
				ReturnActor = Component->GetOwner();
				break;
			}
		}
	}

	return ReturnActor;
}

UGeometryCollectionComponent* UFractureToolComponent::GetGeometryCollectionComponent(UEditableMesh* SourceMesh)
{
	check(SourceMesh);
	UGeometryCollectionComponent* GeometryCollectionComponent = nullptr;
	AActor* Actor = GetEditableMeshActor(SourceMesh);
	AGeometryCollectionActor* GeometryCollectionActor = Cast<AGeometryCollectionActor>(Actor);
	if (GeometryCollectionActor)
	{
		GeometryCollectionComponent = GeometryCollectionActor->GetGeometryCollectionComponent();
	}

	return GeometryCollectionComponent;
}

UGeometryCollectionComponent* UFractureToolComponent::GetGeometryCollectionComponent()
{
	UGeometryCollectionComponent* GeometryCollectionComponent = nullptr;
	AActor* Actor = GetEditableMeshActor();
	AGeometryCollectionActor* GeometryCollectionActor = Cast<AGeometryCollectionActor>(Actor);
	if (GeometryCollectionActor)
	{
		GeometryCollectionComponent = GeometryCollectionActor->GetGeometryCollectionComponent();
	}

	return GeometryCollectionComponent;
}

UGeometryCollection* UFractureToolComponent::GetGeometryCollection(const UEditableMesh* SourceMesh)
{
	check(SourceMesh);
	const FEditableMeshSubMeshAddress& SubMeshAddress = SourceMesh->GetSubMeshAddress();
	return Cast<UGeometryCollection>(static_cast<UObject*>(SubMeshAddress.MeshObjectPtr));
}

void UFractureToolComponent::OnUpdateFractureLevelView(uint8 FractureLevel)
{
	TArray<AActor*> ActorList = GetSelectedActors();

	for (int ActorIndex = 0; ActorIndex < ActorList.Num(); ActorIndex++)
	{
		AGeometryCollectionActor* GeometryActor = Cast<AGeometryCollectionActor>(ActorList[ActorIndex]);

		if (GeometryActor)
		{
			check(GeometryActor->GeometryCollectionComponent);
			FGeometryCollectionEdit GeometryCollectionEdit = GeometryActor->GeometryCollectionComponent->EditRestCollection();
			UGeometryCollection* GeometryCollection = GeometryCollectionEdit.GetRestCollection();
			check(GeometryCollection);
			GeometryActor->GeometryCollectionComponent->MarkRenderStateDirty();
		}
	}

	OnUpdateExplodedView(static_cast<uint8>(EViewResetType::RESET_TRANSFORMS), FractureLevel);

	// Visualization parameters have been modified
	OnFractureLevelChanged(FractureLevel);
}

void UFractureToolComponent::OnUpdateExplodedView(uint8 ResetTypeIn, uint8 FractureLevelIn) const
{
	EMeshFractureLevel FractureLevel = static_cast<EMeshFractureLevel>(FractureLevelIn);
	EViewResetType ResetType = static_cast<EViewResetType>(ResetTypeIn);
	const TArray<AActor*> ActorList = GetSelectedActors();

	// when viewing individual fracture levels we use straight forward explosion algorithm
	EExplodedViewMode ViewMode = EExplodedViewMode::Linear;

	// when viewing all pieces, let the expansion happen one level at a time
	if (FractureLevel == EMeshFractureLevel::AllLevels)
		ViewMode = EExplodedViewMode::SplitLevels;

	for (int ActorIndex = 0; ActorIndex < ActorList.Num(); ActorIndex++)
	{
		AGeometryCollectionActor* GeometryActor = Cast<AGeometryCollectionActor>(ActorList[ActorIndex]);
		if (GeometryActor)
		{
			check(GeometryActor->GeometryCollectionComponent);
			if (!HasExplodedAttributes(GeometryActor))
				continue;

			switch (ViewMode)
			{
			case EExplodedViewMode::SplitLevels:
				ExplodeInLevels(GeometryActor);
				break;

			case EExplodedViewMode::Linear:
			default:
				ExplodeLinearly(GeometryActor, FractureLevel);
				break;
			}

			GeometryActor->GeometryCollectionComponent->MarkRenderStateDirty();
		}
	}

	if (ResetType == EViewResetType::RESET_ALL)
	{
		// Force an update using the output GeometryCollection which may not have existed before the fracture
		FFractureToolDelegates::Get().OnFractureExpansionEnd.Broadcast();
	}
	else
	{
		// only the transforms will have updated
		FFractureToolDelegates::Get().OnFractureExpansionUpdate.Broadcast();
	}

	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}

void UFractureToolComponent::ExplodeInLevels(AGeometryCollectionActor* GeometryActor) const
{
	check(GeometryActor->GeometryCollectionComponent);
	FGeometryCollectionEdit GeometryCollectionEdit = GeometryActor->GeometryCollectionComponent->EditRestCollection();
	UGeometryCollection* GeometryCollection = GeometryCollectionEdit.GetRestCollection();

	if (GeometryCollection)
	{
		TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollection->GetGeometryCollection();
		if (FGeometryCollection* Collection = GeometryCollectionPtr.Get())
		{
			float ComponentScaling = CalculateComponentScaling(GeometryActor->GeometryCollectionComponent);

			TSharedRef<TManagedArray<FTransform> > TransformArray = Collection->GetAttribute<FTransform>("Transform", FGeometryCollection::TransformGroup);
			TSharedRef<TManagedArray<FVector> > ExplodedVectorsArray = Collection->GetAttribute<FVector>("ExplodedVector", FGeometryCollection::TransformGroup);
			TSharedRef<TManagedArray<FTransform> > ExplodedTransformsArray = Collection->GetAttribute<FTransform>("ExplodedTransform", FGeometryCollection::TransformGroup);
			TSharedRef<TManagedArray<FGeometryCollectionBoneNode > > HierarchyArray = Collection->GetAttribute<FGeometryCollectionBoneNode>("BoneHierarchy", FGeometryCollection::TransformGroup);

			TManagedArray<FTransform> & Transform = *TransformArray;
			TManagedArray<FVector>& ExplodedVectors = *ExplodedVectorsArray;
			TManagedArray<FTransform>& ExplodedTransforms = *ExplodedTransformsArray;
			TManagedArray<FGeometryCollectionBoneNode >& Hierarchy = *HierarchyArray;


			int32 NumTransforms = Collection->NumElements(FGeometryCollection::TransformGroup);
			int32 MaxFractureLevel = -1;
			for (int i = 0; i < NumTransforms; i++)
			{
				if (Hierarchy[i].Level > MaxFractureLevel)
					MaxFractureLevel = Hierarchy[i].Level;
			}

			for (int Level = 1; Level <= MaxFractureLevel; Level++)
			{
				for (int t = 0; t < NumTransforms; t++)
				{
					if (Hierarchy[t].Level != Level)
						continue;

					int32 FractureLevel = Level - 1;
					if (FractureLevel >= 0)
					{
						if (FractureLevel > 7)
							FractureLevel = 7;

						// smaller chunks appear to explode later than their parents
						float UseVal = FMath::Max(0.0f, UMeshFractureSettings::ExplodedViewExpansion - 0.1f * FractureLevel);

						// due to the fact that the levels break later the overall range gets shorter
						// so compensate for this making the later fragments move farther/faster than the earlier ones
						UseVal *= (0.95f / (1.0f - 0.1f * FractureLevel));

						for (int i = 0; i < FractureLevel; i++)
						{
							UseVal *= UseVal;
						}
						FVector NewPos = ExplodedTransforms[t].GetLocation() + ComponentScaling * ExplodedVectors[t] * UseVal;
						Transform[t].SetLocation(NewPos);
					}
				}
			}
		}
	}
}

void UFractureToolComponent::ExplodeLinearly(AGeometryCollectionActor* GeometryActor, EMeshFractureLevel FractureLevel) const
{
	check(GeometryActor->GeometryCollectionComponent);
	FGeometryCollectionEdit GeometryCollectionEdit = GeometryActor->GeometryCollectionComponent->EditRestCollection();
	UGeometryCollection* GeometryCollection = GeometryCollectionEdit.GetRestCollection();

	if (GeometryCollection)
	{
		TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollection->GetGeometryCollection();
		if (FGeometryCollection* Collection = GeometryCollectionPtr.Get())
		{
			float ComponentScaling = CalculateComponentScaling(GeometryActor->GeometryCollectionComponent);

			const TSharedRef<TManagedArray<FVector> > ExplodedVectorsArray = Collection->GetAttribute<FVector>("ExplodedVector", FGeometryCollection::TransformGroup);
			const TSharedRef<TManagedArray<FTransform> > ExplodedTransformsArray = Collection->GetAttribute<FTransform>("ExplodedTransform", FGeometryCollection::TransformGroup);
			const TManagedArray<FVector>& ExplodedVectors = *ExplodedVectorsArray;
			const TManagedArray<FTransform>& ExplodedTransforms = *ExplodedTransformsArray;

			TManagedArray<FTransform> & Transform = *Collection->Transform;
			const TManagedArray<FGeometryCollectionBoneNode >& Hierarchy = *Collection->BoneHierarchy;


			int32 NumTransforms = Collection->NumElements(FGeometryCollection::TransformGroup);
			int32 FractureLevelNumber = static_cast<int8>(FractureLevel) - static_cast<int8>(EMeshFractureLevel::Level0);
			int32 MaxFractureLevel = FractureLevelNumber;
			for (int i = 0; i < NumTransforms; i++)
			{
				if (Hierarchy[i].Level > MaxFractureLevel)
					MaxFractureLevel = Hierarchy[i].Level;
			}

			for (int Level = 1; Level <= MaxFractureLevel; Level++)
			{
				for (int t = 0; t < NumTransforms; t++)
				{
					if (Hierarchy[t].Level == FractureLevelNumber)
					{
						FVector NewPos = ExplodedTransforms[t].GetLocation() + ComponentScaling * ExplodedVectors[t] * UMeshFractureSettings::ExplodedViewExpansion;
						Transform[t].SetLocation(NewPos);
					}
					else
					{
						FVector NewPos = ExplodedTransforms[t].GetLocation();
						Transform[t].SetLocation(NewPos);
					}
				}
			}
		}
	}
}

float UFractureToolComponent::CalculateComponentScaling(UGeometryCollectionComponent* GeometryCollectionComponent) const
{
	FBoxSphereBounds Bounds;

	check(GeometryCollectionComponent);
	FGeometryCollectionEdit GeometryCollectionEdit = GeometryCollectionComponent->EditRestCollection();
	if (UGeometryCollection* GeometryCollectionObject = GeometryCollectionEdit.GetRestCollection())
	{
		TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
		if (FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get())
		{

			// reset the transforms so the component is no longer exploded, otherwise get bounds of exploded state which is a moving target
			TSharedRef<TManagedArray<FTransform> > TransformsArray = GeometryCollection->GetAttribute<FTransform>("Transform", FGeometryCollection::TransformGroup);
			TManagedArray<FTransform> & Transforms = *TransformsArray;

			for (int i = 0; i < Transforms.Num(); i++)
			{
				Transforms[i].SetLocation(FVector::ZeroVector);
			}
			Bounds = GeometryCollectionComponent->CalcBounds(FTransform::Identity);
		}
	}
	return Bounds.SphereRadius * 0.01f * 0.2f;
}

void UFractureToolComponent::ShowGeometry(class UGeometryCollection* GeometryCollectionObject, int Index, bool GeometryVisible, bool IncludeChildren)
{
	TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
	if (FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get())
	{
		// #todo: the way the visibility is defined in the GeometryCollection makes this operation really slow - best if the visibility was at bone level
		TSharedRef<TManagedArray<int32> > BoneMapArray = GeometryCollection->GetAttribute<int32>("BoneMap", FGeometryCollection::VerticesGroup);
		TSharedRef<TManagedArray<FIntVector> > IndicesArray = GeometryCollection->GetAttribute<FIntVector>("Indices", FGeometryCollection::FacesGroup);
		TSharedRef<TManagedArray<bool> > VisibleArray = GeometryCollection->GetAttribute<bool>("Visible", FGeometryCollection::FacesGroup);

		TManagedArray<int32>& BoneMap = *BoneMapArray;
		TManagedArray<FIntVector>&  Indices = *IndicesArray;
		TManagedArray<bool>&  Visible = *VisibleArray;


		for (int32 i = 0; i < Indices.Num(); i++)
		{

			if (BoneMap[Indices[i][0]] == Index || (IncludeChildren && BoneMap[Indices[i][0]] > Index))
			{
				Visible[i] = GeometryVisible;
			}
		}
	}
}

bool UFractureToolComponent::HasExplodedAttributes(AGeometryCollectionActor* GeometryActor) const
{
	if (GeometryActor &&  GeometryActor->GeometryCollectionComponent && GeometryActor->GeometryCollectionComponent->GetRestCollection() &&
		GeometryActor->GeometryCollectionComponent->GetRestCollection()->GetGeometryCollection())
	{
		return GeometryActor->GeometryCollectionComponent->GetRestCollection()->GetGeometryCollection()->HasAttribute("ExplodedVector", FGeometryCollection::TransformGroup);
	}
	return false;
}
