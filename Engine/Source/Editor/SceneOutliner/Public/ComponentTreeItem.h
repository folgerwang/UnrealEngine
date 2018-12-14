// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "ITreeItem.h"
#include "UObject/ObjectKey.h"
#include "Components/SceneComponent.h"


namespace SceneOutliner
{
	/** Helper class to manage moving arbitrary data onto an Component if any */
	struct FComponentDropTarget : IDropTarget
	{
		/** The Component this tree item is associated with. */
		TWeakObjectPtr<UActorComponent> Component;
		
		/** Construct this object out of an Component */
		FComponentDropTarget(UActorComponent* InComponent) : Component(InComponent) {}

	public:

		/** Called to test whether the specified payload can be dropped onto this tree item */
		virtual FDragValidationInfo ValidateDrop(FDragDropPayload& DraggedObjects, UWorld& World) const override;

		/** Called to drop the specified objects on this item. Only called if ValidateDrop() allows. */
		virtual void OnDrop(FDragDropPayload& DraggedObjects, UWorld& World, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget) override;

	protected:

	};

	/** A tree item that represents an Component in the world */
	struct SCENEOUTLINER_API FComponentTreeItem : ITreeItem
	{
		/** The Component this tree item is associated with. */
		mutable TWeakObjectPtr<UActorComponent> Component;

		/** Constant identifier for this tree item */
		const FObjectKey ID;

		/** Construct this item from an Component */
		FComponentTreeItem(UActorComponent* InComponent);

		/** Get this item's parent item. It is valid to return nullptr if this item has no parent */
		virtual FTreeItemPtr FindParent(const FTreeItemMap& ExistingItems) const override;

		/** Create this item's parent. It is valid to return nullptr if this item has no parent */
		virtual FTreeItemPtr CreateParent() const override;

 		/** Visit this tree item */
 		virtual void Visit(const ITreeItemVisitor& Visitor) const override;
 		virtual void Visit(const IMutableTreeItemVisitor& Visitor) override;

		/** Called when tree item double clicked - only works in certain modes */
		virtual void OnDoubleClick() const {}
	public:

		/** Get the ID that represents this tree item. Used to reference this item in a map */
		virtual FTreeItemID GetID() const override;

		/** Get the raw string to display for this tree item - used for sorting */
		virtual FString GetDisplayString() const override;

		/** Get the sort priority given to this item's type */
		virtual int32 GetTypeSortPriority() const override;

		/** Check whether it should be possible to interact with this tree item */
		virtual bool CanInteract() const override;

	public:

		/** Populate the specified drag/drop payload with any relevant information for this type */
		virtual void PopulateDragDropPayload(FDragDropPayload& Payload) const override;

		/** Called to test whether the specified payload can be dropped onto this tree item */
		virtual FDragValidationInfo ValidateDrop(FDragDropPayload& DraggedObjects, UWorld& World) const override;

		/** Called to drop the specified objects on this item. Only called if ValidateDrop() allows. */
		virtual void OnDrop(FDragDropPayload& DraggedObjects, UWorld& World, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget) override;

	public:

		/** true if this item exists in both the current world and PIE. */
		bool bExistsInCurrentWorldAndPIE;

	};

} // namespace SceneOutliner
