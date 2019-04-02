// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "ITreeItem.h"
#include "UObject/ObjectKey.h"
#include "ComponentTreeItem.h"

class UGeometryCollectionComponent;

namespace SceneOutliner
{
	class FGeometryCollectionSelection
	{
	public:
		FGeometryCollectionSelection();
		~FGeometryCollectionSelection();

		/** This will trigger from both an actor selection change in the world, or one in scene outliner */
		void OnActorSelectionChanged(UObject* Object);

		/** Capture selection changes of sub-component tree items */
		void OnSubComponentSelectionChanged(TArray<FSubComponentTreeItem*>& SubComponentItemSelection);

		TArray<TWeakObjectPtr<AActor>> SelectedActors;

		GEOMETRYCOLLECTIONEDITOR_API static FGeometryCollectionSelection& InitSingleton()
		{
			static FGeometryCollectionSelection Singleton;
			return Singleton;
		}

	};

	/** A tree item that represents an GeometryCollection in the world */
	struct FGeometryCollectionTreeItem : FComponentTreeItem
	{
		/** The GeometryCollection this tree item is associated with. */
		mutable TWeakObjectPtr<UGeometryCollectionComponent> GeometryCollectionComponent;

		/** Construct this item from an GeometryCollection */
		FGeometryCollectionTreeItem(UGeometryCollectionComponent* InGeometryCollection);

		virtual void SynchronizeSubItemSelection(TSharedPtr<class SOutlinerTreeView> OutlinerTreeView) override;

		/** Called when tree item is double clicked */
		virtual void OnDoubleClick() const override;

	public:

		/** true if this item exists in both the current world and PIE. */
		bool bExistsInCurrentWorldAndPIE;

	};

} // namespace SceneOutliner
