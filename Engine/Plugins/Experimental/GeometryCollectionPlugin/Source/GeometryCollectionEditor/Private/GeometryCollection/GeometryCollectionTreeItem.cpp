// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionTreeItem.h"
#include "GeometryCollection/GeometryCollectionBoneTreeItem.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "Engine/Selection.h"
#include "Editor.h"
#include "SceneOutlinerDelegates.h"
#include "EditorSupportDelegates.h"
#include "FractureToolDelegates.h"
#include "SOutlinerTreeView.h"

#define LOCTEXT_NAMESPACE "SceneOutliner_GeometryCollectionTreeItem"

namespace SceneOutliner
{

FGeometryCollectionTreeItem::FGeometryCollectionTreeItem(UGeometryCollectionComponent* InGeometryCollection)
	: FComponentTreeItem(InGeometryCollection), GeometryCollectionComponent(InGeometryCollection)
{
	FGeometryCollectionSelection::InitSingleton();

	const UGeometryCollection* GeometryCollectionObject = GeometryCollectionComponent->GetRestCollection();
	if (GeometryCollectionObject && GeometryCollectionObject->GetGeometryCollection())
	{
		const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollectionObject->GetGeometryCollection()->BoneHierarchy;

		// Add a sub item to the outliner tree for each of the bones/chunks in this GeometryCollection
		for (int Element = 0; Element < Hierarchy.Num(); Element++)
		{
			const FGeometryCollectionBoneNode& Node = Hierarchy[Element];
			FTreeItemRef NewItem = MakeShareable(new FGeometryCollectionBoneTreeItem(InGeometryCollection, this, Element));
			SubComponentItems.Add(NewItem);
		}
	}
}

void FGeometryCollectionTreeItem::SynchronizeSubItemSelection(TSharedPtr<SOutlinerTreeView> OutlinerTreeView)
{
	const TArray<int32>& SelectedBones = GeometryCollectionComponent->GetSelectedBones();

	// first clear selections in outliner tree
	for (auto& Item : SubComponentItems)
	{
		OutlinerTreeView->SetItemSelection(Item, false);
	}

	// then mark the new 'selected' set as selected in tree
	for (int32 Element : SelectedBones)
	{
		OutlinerTreeView->SetItemExpansion(SubComponentItems[Element], true);
		OutlinerTreeView->SetItemSelection(SubComponentItems[Element], true);
	}
}

void FGeometryCollectionTreeItem::OnDoubleClick() const
{
	// nop
}

FGeometryCollectionSelection::FGeometryCollectionSelection()
{
	// actor selection change
	USelection::SelectionChangedEvent.AddRaw(this, &FGeometryCollectionSelection::OnActorSelectionChanged);

	//  Capture selection changes of sub-component tree items
	FSceneOutlinerDelegates::Get().OnSubComponentSelectionChanged.AddRaw(this, &FGeometryCollectionSelection::OnSubComponentSelectionChanged);
}

FGeometryCollectionSelection::~FGeometryCollectionSelection()
{
	USelection::SelectionChangedEvent.RemoveAll(this);
	FSceneOutlinerDelegates::Get().OnSubComponentSelectionChanged.RemoveAll(this);

}

void FGeometryCollectionSelection::OnActorSelectionChanged(UObject* Object)
{
	USelection* Selection = Cast<USelection>(Object);

	// process Actors that have been deselected
	for (TWeakObjectPtr<AActor> Actor : SelectedActors)
	{
		if (Actor.IsValid() && !Actor->IsSelected())
		{
			const TArray<UActorComponent*>& ActorComponents = Actor->GetComponentsByClass(UGeometryCollectionComponent::StaticClass());
			for (UActorComponent* ActorComponent : ActorComponents)
			{
				UGeometryCollectionComponent* GeometryCollectionComponent = Cast<UGeometryCollectionComponent>(ActorComponent);
				if (GeometryCollectionComponent)
				{
					FScopedColorEdit ScopedSelection = GeometryCollectionComponent->EditBoneSelection();
					ScopedSelection.SetShowSelectedBones(false);
				}
			}
		}
	}

	SelectedActors.Empty();

	// process Actors that have been selected
	if (Selection == GEditor->GetSelectedActors())
	{
		for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			AActor* Actor = CastChecked<AActor>(*It);
			const TArray<UActorComponent*>& ActorComponents = Actor->GetComponentsByClass(UGeometryCollectionComponent::StaticClass());
			for (UActorComponent* ActorComponent : ActorComponents)
			{
				UGeometryCollectionComponent* GeometryCollectionComponent = Cast<UGeometryCollectionComponent>(ActorComponent);
				if (GeometryCollectionComponent)
				{
					if (!SelectedActors.Contains(Actor))
					{
						// newly selected
						FScopedColorEdit ScopedSelection = GeometryCollectionComponent->EditBoneSelection();
						ScopedSelection.SetShowSelectedBones(true);
						SelectedActors.Push(Actor);
					}
				}
			}
		}
	}
}

void FGeometryCollectionSelection::OnSubComponentSelectionChanged(TArray<FSubComponentTreeItem*>& SubComponentItemSelection)
{
	bool Dirty = false;
	UGeometryCollectionComponent* LastGeometryCollectionComponent = nullptr;
	for (FSubComponentTreeItem* Item : SubComponentItemSelection)
	{
		UGeometryCollectionComponent* GeometryCollectionComponent = Cast<UGeometryCollectionComponent>(Item->ParentComponent);
		if (GeometryCollectionComponent)
		{
			FScopedColorEdit ScopedSelection = GeometryCollectionComponent->EditBoneSelection();

			if (GeometryCollectionComponent != LastGeometryCollectionComponent)
			{
				ScopedSelection.ResetBoneSelection();
			}
			LastGeometryCollectionComponent = GeometryCollectionComponent;

			if (const UGeometryCollection* MeshGeometryCollection = GeometryCollectionComponent->GetRestCollection())
			{
				TSharedPtr<FGeometryCollection> GeometryCollectionPtr = MeshGeometryCollection->GetGeometryCollection();
				if (FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get())
				{
					FGeometryCollectionBoneTreeItem* BoneItem = static_cast<FGeometryCollectionBoneTreeItem*>(Item);
					ScopedSelection.AddSelectedBone(BoneItem->BoneIndex);

					const TArray<int32>& Selected = ScopedSelection.GetSelectedBones();
					TArray<int32> RevisedSelected;
					TArray<int32> Highlighted;
					FGeometryCollectionClusteringUtility::ContextBasedClusterSelection(GeometryCollection, ScopedSelection.GetViewLevel(), Selected, RevisedSelected, Highlighted);
					ScopedSelection.SetSelectedBones(RevisedSelected);
					ScopedSelection.SetHighlightedBones(Highlighted);

					Dirty = true;
				}
			}
		}
	}

	if (Dirty)
	{
		FEditorSupportDelegates::RedrawAllViewports.Broadcast();
	}

}


}	// namespace SceneOutliner

#undef LOCTEXT_NAMESPACE
