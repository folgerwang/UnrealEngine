// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "UObject/ObjectKey.h"
#include "UObject/ObjectMacros.h"
#include "Components/SceneComponent.h"
#include "SubComponentTreeItem.h"
#include "SceneOutlinerStandaloneTypes.h"

namespace SceneOutliner
{
	struct FGeometryCollectionTreeItem;
	struct FGeometryCollectionBoneTreeItem;

	/** Helper class to manage moving arbitrary data onto an Component if any */
	struct FGeometryCollectionBoneDropTarget : IDropTarget
	{
		/** The Component this tree item is associated with. */
		const FGeometryCollectionBoneTreeItem* DestinationItem;

		/** Construct this object out of an Component */
		FGeometryCollectionBoneDropTarget(const FGeometryCollectionBoneTreeItem* InItem) : DestinationItem(InItem) {}

	public:

		/** Called to test whether the specified payload can be dropped onto this tree item */
		virtual FDragValidationInfo ValidateDrop(FDragDropPayload& DraggedObjects, UWorld& World) const override;

		/** Called to drop the specified objects on this item. Only called if ValidateDrop() allows. */
		virtual void OnDrop(FDragDropPayload& DraggedObjects, UWorld& World, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget) override;
	};

	/** A tree item that represents an Component in the world */
	struct FGeometryCollectionBoneTreeItem : public FSubComponentTreeItem
	{
	public:
		/** The Component this tree item is associated with. */
		FGeometryCollectionTreeItem* ParentTreeItem;

		/** The Bone/Chunk index */
		int32 BoneIndex;

		/** unique index for use with FTreeItemID */
		FTreeItemUniqueID UniqueID;

		/** Construct this item from an Component */
		FGeometryCollectionBoneTreeItem(UActorComponent* InComponent, FGeometryCollectionTreeItem* InParentTreeItem, uint32 BoneIndex);
		~FGeometryCollectionBoneTreeItem();

		/** Get the ID that represents this tree item. Used to reference this item in a map */
		virtual FTreeItemID GetID() const override;

		/** Get this item's parent item. It is valid to return nullptr if this item has no parent */
		virtual FTreeItemPtr FindParent(const FTreeItemMap& ExistingItems) const override;

 		/** Visit this tree item */
 		virtual void Visit(const ITreeItemVisitor& Visitor) const override;
 		virtual void Visit(const IMutableTreeItemVisitor& Visitor) override;

		/** Get the raw string to display for this tree item - used for sorting */
		virtual FString GetDisplayString() const override;

		/** Get the class type that we will use for this tree item's icon */
		virtual UClass* GetIconClass() const;

		/** Get the Geometry Collection Bone Index associated with this tree item */
		uint32 GetBoneIndex() const { return BoneIndex; }

		/** Called to test whether the specified payload can be dropped onto this tree item */
		virtual FDragValidationInfo ValidateDrop(FDragDropPayload& DraggedObjects, UWorld& World) const override;

		/** Called to drop the specified objects on this item. Only called if ValidateDrop() allows. */
		virtual void OnDrop(FDragDropPayload& DraggedObjects, UWorld& World, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget) override;

		/** Called when tree item is double clicked - only works in certain modes */
		virtual void OnDoubleClick() const override;

		/** Called to validate a new sub-component name before it is set on a bone */
		virtual bool ValidateSubComponentName(const FText& InName, FText& OutErrorMessage) override;

		/** Called when a sub-component is being renamed, in our case renaming on one of the bones */
		virtual void RenameSubComponent(const FText& InName) override;

		/** Get the string that appears in the Type column of the world outliner */
		virtual FString GetTypeName() const override;
	};

} // namespace SceneOutliner
