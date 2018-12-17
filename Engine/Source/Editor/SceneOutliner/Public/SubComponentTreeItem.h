// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "ITreeItem.h"
#include "UObject/ObjectKey.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Components/SceneComponent.h"


namespace SceneOutliner
{
	struct SCENEOUTLINER_API FSubComponentTreeItem : public ITreeItem
	{
	public:
		/** The Component this tree item is associated with. */
		mutable TWeakObjectPtr<UActorComponent> ParentComponent;

		/** Construct this item from an Component */
		FSubComponentTreeItem(UActorComponent* InComponent);

		/** Get this item's parent item. It is valid to return nullptr if this item has no parent */
		virtual FTreeItemPtr FindParent(const FTreeItemMap& ExistingItems) const override;

		/** Create this item's parent. It is valid to return nullptr if this item has no parent */
		virtual FTreeItemPtr CreateParent() const override;

		/** Get the class type that we will use for this tree item's icon */
		virtual UClass* GetIconClass() const { return UActorComponent::StaticClass(); }


		/** Called when tree item double clicked - only works in certain modes */
		virtual void OnDoubleClick() const {}

	public:

		/** Get the sort priority given to this item's type */
		virtual int32 GetTypeSortPriority() const override;

		/** Check whether it should be possible to interact with this tree item */
		virtual bool CanInteract() const override;

		/** Process drag and drop */
		virtual void PopulateDragDropPayload(FDragDropPayload& Payload) const override;

		/** Determines whether the entered name is valid for this sub-component */
		virtual bool ValidateSubComponentName(const FText& InName, FText& OutErrorMessage) = 0;

		/** Rename a sub-component of a UActorComponent */
		virtual void RenameSubComponent(const FText& InName) = 0;

		/** Get the string that appears in the Type column of the world outliner */
		virtual FString GetTypeName() const = 0;

	public:

		/** true if this item exists in both the current world and PIE. */
		bool bExistsInCurrentWorldAndPIE;

	};
}

// namespace SceneOutliner
