// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ListView.h"
#include "Widgets/Views/STreeView.h"
#include "TreeView.generated.h"

DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnGetItemChildrenDynamic, UObject*, Item, TArray<UObject*>&, Children);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemExpansionChangedDynamic, UObject*, Item, bool, bIsExpanded);

/**
 * Similar to ListView, but can display a hierarchical tree of elements.
 * The base items source for the tree identifies the root items, each of which can have n associated child items.
 * There is no hard limit to the nesting - child items can have children and so on
 */
UCLASS()
class UMG_API UTreeView : public UListView
{
	GENERATED_BODY()

public:
	UTreeView(const FObjectInitializer& ObjectInitializer);
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	
	/** Attempts to expand/collapse the given item (only relevant if the item has children) */
	UFUNCTION(BlueprintCallable, Category = TreeView)
	void SetItemExpansion(UObject* Item, bool bExpandItem);

	/** Expands all items with children */
	UFUNCTION(BlueprintCallable, Category = TreeView)
	void ExpandAll();

	/** Collapses all currently expanded items */
	UFUNCTION(BlueprintCallable, Category = TreeView)
	void CollapseAll();

	template <typename ObjectT>
	void SetOnGetItemChildren(ObjectT* InUserObject, typename TSlateDelegates<UObject*>::FOnGetChildren::TUObjectMethodDelegate<ObjectT>::FMethodPtr InMethodPtr)
	{
		static_assert(TIsDerivedFrom<ObjectT, UObject>::IsDerived, "Only UObject ptrs can be passed directly when binding to SetOnGetItemChildren. Pass a shared ref for non-UObject binders.");
		OnGetItemChildren = TSlateDelegates<UObject*>::FOnGetChildren::CreateUObject(InUserObject, InMethodPtr);
	}
	template <typename ObjectT>
	void SetOnGetItemChildren(TSharedRef<ObjectT> InUserObject, typename TSlateDelegates<UObject*>::FOnGetChildren::TUObjectMethodDelegate<ObjectT>::FMethodPtr InMethodPtr)
	{
		OnGetItemChildren = TSlateDelegates<UObject*>::FOnGetChildren::CreateSP(InUserObject, InMethodPtr);
	}

protected:
	virtual TSharedRef<STableViewBase> RebuildListWidget() override;
	virtual void OnItemClickedInternal(UObject* ListItem) override;

	virtual void OnItemExpansionChangedInternal(UObject* Item, bool bIsExpanded) override;
	virtual void OnGetChildrenInternal(UObject* Item, TArray<UObject*>& OutChildren) const override;

	/** STreeView construction helper - useful if using a custom STreeView subclass */
	template <template<typename> class TreeViewT = STreeView>
	TSharedRef<TreeViewT<UObject*>> ConstructTreeView()
	{
		MyListView = MyTreeView = ITypedUMGListView<UObject*>::ConstructTreeView<TreeViewT>(this, ListItems, SelectionMode, bClearSelectionOnClick, ConsumeMouseWheel);
		return StaticCastSharedRef<TreeViewT<UObject*>>(MyTreeView.ToSharedRef());
	}


	TSharedPtr<STreeView<UObject*>> MyTreeView;

private:
	/** Called to get the list of children (if any) that correspond to the given item. Only called if the native C++ version of the event is not bound. */
	UPROPERTY(EditAnywhere, Category = Events, meta = (IsBindableEvent, AllowPrivateAccess = true, DisplayName = "On Get Item Children"))
	FOnGetItemChildrenDynamic BP_OnGetItemChildren;

	UPROPERTY(BlueprintAssignable, Category = Events, meta = (AllowPrivateAccess = true, DisplayName = "On Item Expansion Changed"))
	FOnItemExpansionChangedDynamic BP_OnItemExpansionChanged;

	TSlateDelegates<UObject*>::FOnGetChildren OnGetItemChildren;
	FOnItemExpansionChanged OnItemExpansionChangedEvent;
};
