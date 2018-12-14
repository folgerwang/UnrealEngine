// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionBoneTreeItem.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "SceneOutlinerPublicTypes.h"
#include "SceneOutlinerDragDrop.h"
#include "SceneOutlinerStandaloneTypes.h"
#include "FractureToolDelegates.h"
#include "GeometryCollection/GeometryCollectionTreeItem.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "Logging/MessageLog.h"

#define LOCTEXT_NAMESPACE "SceneOutliner_SubComponentTreeItem"


namespace SceneOutliner
{

FDragValidationInfo FGeometryCollectionBoneDropTarget::ValidateDrop(FDragDropPayload& DraggedObjects, UWorld& World) const
{
	if (DraggedObjects.Folders)
	{
		return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_IncompatibleGeneric, LOCTEXT("FoldersOnActorError", "Cannot attach folders to components"));
	}

	const UActorComponent* DropTarget = DestinationItem->ParentComponent.Get();

	if (!DropTarget || !DraggedObjects.SubComponents)
	{
		return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_IncompatibleGeneric, FText());
	}

	UGeometryCollectionComponent* GeometryCollectionComponent = Cast<UGeometryCollectionComponent>(DestinationItem->ParentComponent);
	if (!GeometryCollectionComponent)
	{
		return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_IncompatibleGeneric, FText());
	}

	FText AttachErrorMsg;
	bool bCanAttach = true;
	bool bDraggedOntoAttachmentParent = true;

	const auto& DragItems = DraggedObjects.SubComponents.GetValue();

	for (const auto& DragBonePtr : DragItems)
	{
		if (DragBonePtr.IsValid())
		{
			auto DragItem = DragBonePtr.Pin();

			if (DestinationItem->ParentComponent != DragItem->ParentComponent)
			{
				bCanAttach = false;
				break;
			}

			const FGeometryCollectionBoneTreeItem* GeometryCollectionBoneTreeItem = static_cast<const FGeometryCollectionBoneTreeItem*>(DragItem.Get());
			int32 DragBone = GeometryCollectionBoneTreeItem->BoneIndex;
			if (FGeometryCollectionClusteringUtility::NodeExistsOnThisBranch(GeometryCollectionComponent->GetRestCollection()->GetGeometryCollection().Get(), DestinationItem->BoneIndex, DragBone))
			{
				bCanAttach = false;
				break;
			}

		}
	}

	if (bCanAttach)
	{
		const FText Label = FText::FromString(DropTarget->GetName());
		return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_CompatibleAttach, Label);
	}
	else
	{
		const FText Label = FText::FromString(DropTarget->GetName());
		return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_IncompatibleGeneric, Label);
	}
}

void FGeometryCollectionBoneDropTarget::OnDrop(FDragDropPayload& DraggedObjects, UWorld& World, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget)
{
	UActorComponent* DropComponent = DestinationItem->ParentComponent.Get();
	if (!DropComponent)
	{
		return;
	}

	UGeometryCollectionComponent* GeometryCollectionComponent = Cast<UGeometryCollectionComponent>(DropComponent);
	if (!GeometryCollectionComponent || !GeometryCollectionComponent->GetRestCollection())
	{
		return;
	}

	FMessageLog EditorErrors("EditorErrors");
	EditorErrors.NewPage(LOCTEXT("GeomertyCollectionAttachmentsPageLabel", "Geometry Collection attachment"));

	TArray<int32> SelectedBones;
	FSubComponentItemArray DraggedComponents = DraggedObjects.SubComponents.GetValue();

	for (const auto& WeakDropItem : DraggedComponents)
	{
		if (auto DropItem = WeakDropItem.Pin())
		{
			if (DropItem.IsValid())
			{
				check(DestinationItem->ParentComponent == DropItem->ParentComponent);
				const FGeometryCollectionBoneTreeItem* BoneItem = static_cast<const FGeometryCollectionBoneTreeItem*>(DropItem.Get()); // ARG
				SelectedBones.Push(BoneItem->GetBoneIndex());
			}
		}
	}

	{
		// modify parent and child
		const FScopedTransaction Transaction(LOCTEXT("UndoAction_GeometryCollectionHierarchy", "Geometry Collection Attach"));

		// Scoped edit of Geometry Collection
		FGeometryCollectionEdit GeometryCollectionEdit = GeometryCollectionComponent->EditRestCollection();
		UGeometryCollection* GeometryCollection = GeometryCollectionEdit.GetRestCollection();
		GeometryCollection->Modify();

		FGeometryCollectionClusteringUtility::ClusterBonesByContext(GeometryCollection->GetGeometryCollection().Get(), DestinationItem->GetBoneIndex(), SelectedBones);
	}

	// Report errors
	EditorErrors.Notify(NSLOCTEXT("ActorAttachmentError", "AttachmentsFailed", "Attachments Failed!"));

	FFractureToolDelegates::Get().OnComponentsUpdated.Broadcast();
}

FGeometryCollectionBoneTreeItem::FGeometryCollectionBoneTreeItem(UActorComponent* InComponent, FGeometryCollectionTreeItem* InParentTreeItem, uint32 BoneIndex)
	: FSubComponentTreeItem(InComponent), ParentTreeItem(InParentTreeItem), BoneIndex(BoneIndex)
{
	// get a unique index from our ID generator
	UniqueID = SceneOutliner::FTreeItemUniqueIDGenerator::Get().GetNextID();
}

FGeometryCollectionBoneTreeItem::~FGeometryCollectionBoneTreeItem()
{
	// get release index from our ID generator
	FTreeItemUniqueIDGenerator::Get().ReleaseID(UniqueID);
}

/** Get the ID that represents this tree item. Used to reference this item in a map */
FTreeItemID FGeometryCollectionBoneTreeItem::GetID() const
{
	return FTreeItemID(FTreeItemID::EType::GCBone, UniqueID);
}

FTreeItemPtr FGeometryCollectionBoneTreeItem::FindParent(const FTreeItemMap& ExistingItems) const
{
	UActorComponent* ComponentPtr = ParentComponent.Get();
	if (!ComponentPtr)
	{
		return nullptr;
	}

	if (UGeometryCollectionComponent* GCComponent = Cast<UGeometryCollectionComponent>(ComponentPtr))
	{
		if (const UGeometryCollection* GeometryCollectionObject = GCComponent->GetRestCollection())
		{
			TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
			if (FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get())
			{
				const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollection->BoneHierarchy;

				if (Hierarchy[BoneIndex].Parent == FGeometryCollectionBoneNode::InvalidBone)
				{
					return ExistingItems.FindRef(ComponentPtr);
				}
				else
				{
					const TArray<FTreeItemRef>& ParentSubComponentItems = ParentTreeItem->GetSubComponentItems();
					check(Hierarchy[BoneIndex].Parent < ParentSubComponentItems.Num());
					return ParentTreeItem->GetSubComponentItems()[Hierarchy[BoneIndex].Parent];
				}
			}
		}
	}

	return nullptr;
}

void FGeometryCollectionBoneTreeItem::Visit(const ITreeItemVisitor& Visitor) const
{
	Visitor.Visit(*this);
}

void FGeometryCollectionBoneTreeItem::Visit(const IMutableTreeItemVisitor& Visitor)
{
	Visitor.Visit(*this);
}

FString FGeometryCollectionBoneTreeItem::GetDisplayString() const
{
	const UActorComponent* ComponentPtr = ParentComponent.Get();
	if (ComponentPtr)
	{
		const UGeometryCollectionComponent* GCComponent = Cast<UGeometryCollectionComponent>(ComponentPtr);
		if (GCComponent)
		{
			if (const UGeometryCollection* GeometryCollectionObject = GCComponent->GetRestCollection())
			{
				TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
				if (FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get())
				{
					const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollection->BoneHierarchy;
					const TManagedArray<FString>& BoneName = *GeometryCollection->BoneName;
					return (BoneIndex < BoneName.Num()) ? BoneName[BoneIndex] : FString();
				}
			}
		}
	}
	return LOCTEXT("ComponentLabelForMissingComponent", "(Deleted Component)").ToString();
}


UClass* FGeometryCollectionBoneTreeItem::GetIconClass() const
{
	return UGeometryCollection::StaticClass();
}

FDragValidationInfo FGeometryCollectionBoneTreeItem::ValidateDrop(FDragDropPayload& DraggedObjects, UWorld& World) const
{
	FGeometryCollectionBoneDropTarget Target(this);
	return Target.ValidateDrop(DraggedObjects, World);
}

void FGeometryCollectionBoneTreeItem::OnDrop(FDragDropPayload& DraggedObjects, UWorld& World, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget)
{
	FGeometryCollectionBoneDropTarget Target(this);
	return Target.OnDrop(DraggedObjects, World, ValidationInfo, DroppedOnWidget);
}

void FGeometryCollectionBoneTreeItem::OnDoubleClick() const
{
	// nop
}

bool FGeometryCollectionBoneTreeItem::ValidateSubComponentName(const FText& InName, FText& OutErrorMessage)
{
	FText TrimmedLabel = FText::TrimPrecedingAndTrailing(InName);

	if (TrimmedLabel.IsEmpty())
	{
		OutErrorMessage = LOCTEXT("RenameFailed_LeftBlank", "Names cannot be left blank");
		return false;
	}

	if (TrimmedLabel.ToString().Len() >= NAME_SIZE)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("CharCount"), NAME_SIZE);
		OutErrorMessage = FText::Format(LOCTEXT("RenameFailed_TooLong", "Names must be less than {CharCount} characters long."), Arguments);
		return false;
	}

	if (FName(*TrimmedLabel.ToString()) == NAME_None)
	{
		OutErrorMessage = LOCTEXT("RenameFailed_ReservedNameNone", "\"None\" is a reserved term and cannot be used for actor names");
		return false;
	}

	return true;
}

void FGeometryCollectionBoneTreeItem::RenameSubComponent(const FText& InName)
{
	UActorComponent* ComponentPtr = ParentComponent.Get();
	if (!ComponentPtr)
	{
		return;
	}
	UGeometryCollectionComponent* GCComponent = Cast<UGeometryCollectionComponent>(ComponentPtr);
	FGeometryCollectionEdit ScopedEdit = GCComponent->EditRestCollection(false);
	if (UGeometryCollection* GeometryCollectionObject = ScopedEdit.GetRestCollection())
	{
		TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
		if (FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get())
		{
			if (GeometryCollection && BoneIndex < GeometryCollection->BoneName->Num())
			{
				TManagedArray<FString>& BoneNames = *GeometryCollection->BoneName;
				if (BoneNames[BoneIndex] != InName.ToString())
				{
					const FScopedTransaction Transaction(LOCTEXT("SceneOutlinerRenameSubComponentTransaction", "Rename Sub-component"));
					GeometryCollectionObject->Modify();
					FGeometryCollectionClusteringUtility::RenameBone(GeometryCollection, BoneIndex, InName.ToString());
				}
			}
		}
	}
}

FString FGeometryCollectionBoneTreeItem::GetTypeName() const
{
	UActorComponent * ComponentPtr = ParentComponent.Get();
	if (ComponentPtr)
	{
		UGeometryCollectionComponent* GCComponent = Cast<UGeometryCollectionComponent>(ComponentPtr);
		if (GCComponent && GCComponent->GetRestCollection() && GCComponent->GetRestCollection()->GetGeometryCollection() )
		{
			const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GCComponent->GetRestCollection()->GetGeometryCollection()->BoneHierarchy;
			if (BoneIndex >= Hierarchy.Num())
			{
				return LOCTEXT("DeletedBoneTypeName", "Deleted Bone").ToString();
			}
			if (Hierarchy[BoneIndex].Parent == FGeometryCollectionBoneNode::InvalidBone)
			{
				return LOCTEXT("RootBoneTypeName", "Root Bone").ToString();
			}
			if (Hierarchy[BoneIndex].IsGeometry())
			{
				return LOCTEXT("GeometryBoneTypeName", "Geometry Bone").ToString();
			}
			else
			{
				return LOCTEXT("TransformBoneTypeName", "Transform Bone").ToString();
			}
		}
	}

	return LOCTEXT("BoneTypeName", "Bone").ToString();
}

}	// namespace SceneOutliner

#undef LOCTEXT_NAMESPACE
