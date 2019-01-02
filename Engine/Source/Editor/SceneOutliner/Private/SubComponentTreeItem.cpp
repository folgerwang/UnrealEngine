// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SubComponentTreeItem.h"
#include "Editor.h"
#include "SceneOutlinerPublicTypes.h"
#include "DragAndDrop/ActorDragDropGraphEdOp.h"
#include "SceneOutlinerDragDrop.h"
#include "SceneOutlinerStandaloneTypes.h"

#define LOCTEXT_NAMESPACE "SceneOutliner_SubCompoonentTreeItem"

namespace SceneOutliner
{

FSubComponentTreeItem::FSubComponentTreeItem(UActorComponent* InComponent)
	: ParentComponent(InComponent)
{
	AActor* OwningActor = InComponent->GetOwner();
	bExistsInCurrentWorldAndPIE = GEditor->ObjectsThatExistInEditorWorld.Get(OwningActor);
}

FTreeItemPtr FSubComponentTreeItem::FindParent(const FTreeItemMap& ExistingItems) const
{
	UActorComponent* ComponentPtr = ParentComponent.Get();
	if (!ComponentPtr)
	{
		return nullptr;
	}

	return ExistingItems.FindRef(ComponentPtr);
}

FTreeItemPtr FSubComponentTreeItem::CreateParent() const
{
	check(false);
	return nullptr;
}

int32 FSubComponentTreeItem::GetTypeSortPriority() const
{
	return ETreeItemSortOrder::Actor;
}

bool FSubComponentTreeItem::CanInteract() const
{
	UActorComponent* ComponentPtr = ParentComponent.Get();
	if (!ComponentPtr || !Flags.bInteractive)
	{
		return false;
	}

	const bool bInSelected = true;
	const bool bSelectEvenIfHidden = true;		// @todo outliner: Is this actually OK?

	AActor* ParentActor = ComponentPtr->GetOwner(); 
	if (!GEditor->CanSelectActor(ParentActor, bInSelected, bSelectEvenIfHidden)) 
	{
		return false;
	}

	return true;
}


void FSubComponentTreeItem::PopulateDragDropPayload(FDragDropPayload& Payload) const
{
	UActorComponent* ActorComponent = ParentComponent.Get();
	if (ActorComponent)
	{
		if (!Payload.SubComponents)
		{
			Payload.SubComponents = FSubComponentItemArray();
		}

		TWeakPtr<const FSubComponentTreeItem> WeakPtr = SharedThis(this);
		Payload.SubComponents->Add(WeakPtr);
	}
}


} // namespace SceneOutliner

#undef LOCTEXT_NAMESPACE
