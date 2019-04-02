// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ComponentTreeItem.h"
#include "Templates/SharedPointer.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "SceneOutlinerPublicTypes.h"
#include "DragAndDrop/ActorDragDropGraphEdOp.h"
#include "SceneOutlinerDragDrop.h"
#include "SceneOutlinerStandaloneTypes.h"


#define LOCTEXT_NAMESPACE "SceneOutliner_ComponentTreeItem"

namespace SceneOutliner
{

FDragValidationInfo FComponentDropTarget::ValidateDrop(FDragDropPayload& DraggedObjects, UWorld& World) const
{
	// we don't allow drag and drop for now
	FText AttachErrorMsg;
	bool bCanAttach = false;
	return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_IncompatibleGeneric, AttachErrorMsg);
}

void FComponentDropTarget::OnDrop(FDragDropPayload& DraggedObjects, UWorld& World, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget)
{

}

FComponentTreeItem::FComponentTreeItem(UActorComponent* InComponent)
	: Component(InComponent)
	, ID(InComponent)
{
	AActor* OwningActor = InComponent->GetOwner();
	bExistsInCurrentWorldAndPIE = GEditor->ObjectsThatExistInEditorWorld.Get(OwningActor);
}

FTreeItemPtr FComponentTreeItem::FindParent(const FTreeItemMap& ExistingItems) const
{
	UActorComponent* ComponentPtr = Component.Get();
	if (!ComponentPtr)
	{
		return nullptr;
	}
	
	// Parent actor should have already been added to the tree
	AActor* ParentActor = ComponentPtr->GetOwner();
	if (ParentActor)
	{
		return ExistingItems.FindRef(ParentActor);
	}
	else
	{
		const bool bShouldShowFolders = SharedData->Mode == ESceneOutlinerMode::ActorBrowsing || SharedData->bOnlyShowFolders;

		const FName ComponentFolder = *ComponentPtr->GetDetailedInfo();
		if (bShouldShowFolders && !ComponentFolder.IsNone())
		{
			return ExistingItems.FindRef(ComponentFolder);
		}
	}

	if (UWorld* World = ComponentPtr->GetWorld())
	{
		return ExistingItems.FindRef(World);
	}

	return nullptr;
}

FTreeItemPtr FComponentTreeItem::CreateParent() const
{
	UActorComponent* ComponentPtr = Component.Get();
	if (!ComponentPtr)
	{
		return nullptr;
	}

	AActor* ParentActor = ComponentPtr->GetOwner();
	if (ParentActor)
	{
		return MakeShareable(new FActorTreeItem(ParentActor));
	}
	else if (!ParentActor)
	{
		const bool bShouldShowFolders = SharedData->Mode == ESceneOutlinerMode::ActorBrowsing || SharedData->bOnlyShowFolders;

		const FName ComponentFolder = *ComponentPtr->GetDetailedInfo();
		if (bShouldShowFolders && !ComponentFolder.IsNone())
		{
			return MakeShareable(new FFolderTreeItem(ComponentFolder));
		}

		if (UWorld* World = ComponentPtr->GetWorld())
		{
			return MakeShareable(new FWorldTreeItem(World));
		}
	}

	return nullptr;
}

void FComponentTreeItem::Visit(const ITreeItemVisitor& Visitor) const
{
	Visitor.Visit(*this);
}

void FComponentTreeItem::Visit(const IMutableTreeItemVisitor& Visitor)
{
	Visitor.Visit(*this);
}

FTreeItemID FComponentTreeItem::GetID() const
{
	return ID;
}

FString FComponentTreeItem::GetDisplayString() const
{
	const UActorComponent* ComponentPtr = Component.Get();
	return ComponentPtr ? ComponentPtr->GetClass()->GetFName().ToString() : LOCTEXT("ComponentLabelForMissingComponent", "(Deleted Component)").ToString();
}

int32 FComponentTreeItem::GetTypeSortPriority() const
{
	return ETreeItemSortOrder::Actor;
}

bool FComponentTreeItem::CanInteract() const
{
	UActorComponent* ComponentPtr = Component.Get();
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

void FComponentTreeItem::PopulateDragDropPayload(FDragDropPayload& Payload) const
{
	// no drag and drop
}

FDragValidationInfo FComponentTreeItem::ValidateDrop(FDragDropPayload& DraggedObjects, UWorld& World) const
{
	FComponentDropTarget Target(Component.Get());
	return Target.ValidateDrop(DraggedObjects, World);
}

void FComponentTreeItem::OnDrop(FDragDropPayload& DraggedObjects, UWorld& World, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget)
{
	FComponentDropTarget Target(Component.Get());
	return Target.OnDrop(DraggedObjects, World, ValidationInfo, DroppedOnWidget);
}


} // namespace SceneOutliner

#undef LOCTEXT_NAMESPACE
