// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Views/STreeView.h"
#include "UObject/Class.h"
#include "BlueprintActionFilter.h"
#include "BlueprintNodeSpawner.h"

class FControlRigEditor;
class SSearchBox;
class FUICommandList;
class FExtender;

/** An item in the tree */
class FControlRigTreeNode : public TSharedFromThis<FControlRigTreeNode>
{
public:
	FControlRigTreeNode(const FBlueprintActionInfo& InBlueprintActionInfo, const FBlueprintActionContext& InBlueprintActionContext);

public:
	/** Action info */
	FBlueprintActionInfo BlueprintActionInfo;

	/** Display info */
	FBlueprintActionUiSpec BlueprintActionUiSpec;
};

/** Widget allowing editing of a control rig's structure */
class SControlRig : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SControlRig) {}
	SLATE_END_ARGS()

	friend class SControlRigItem;

	~SControlRig();

	void Construct(const FArguments& InArgs, TSharedRef<FControlRigEditor> InControlRigEditor);

	// SWidget interface
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	/** Bind commands that this widget handles */
	void BindCommands();

	void HandleAddUnit(UStruct* InUnitStruct);

	/** Rebuild the tree view */
	void RefreshTreeView();

	/** Handle the BP database changing (e.g. adding a new variable) */
	void HandleDatabaseActionsChanged(UObject* ActionsKey);

	/** Make a row widget for the table */
	TSharedRef<ITableRow> MakeTableRowWidget(TSharedPtr<FControlRigTreeNode> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Get children for the tree */
	void HandleGetChildrenForTree(TSharedPtr<FControlRigTreeNode> InItem, TArray<TSharedPtr<FControlRigTreeNode>>& OutChildren);

	/** Handle deleting the selected item(s) */
	void HandleDeleteItem();

	/** Check whether we can deleting the selected item(s) */
	bool CanDeleteItem() const;

	/** Extend the context menu of the graph editor */
	TSharedRef<FExtender> GetGraphEditorMenuExtender(const TSharedRef<class FUICommandList>, const class UEdGraph* Graph, const class UEdGraphNode* Node, const UEdGraphPin* Pin, bool bConst);
	void HandleExtendGraphEditorMenu(FMenuBuilder& InMenuBuilder);

	/** Sync up selection with the graph */
	void HandleGraphSelectionChanged(const TSet<UObject*>& SelectedNodes);

	/** Sync graph selection with us */
	void HandleTreeSelectionChanged(TSharedPtr<FControlRigTreeNode> InItem, ESelectInfo::Type InSelectInfo);

	/** Called by Slate when the filter box changes text. */
	void OnFilterTextChanged(const FText& InFilterText);

private:
	/** Our owning control rig editor */
	TWeakPtr<FControlRigEditor> ControlRigEditor;

	/** Search box widget */
	TSharedPtr<SSearchBox> FilterBox;

	/** Current text typed into NameFilterBox */
	FText FilterText;

	/** Display Only Control Units for manipulation */
	bool bDisplayControlUnitsOnly;

	/** Tree view widget */
	TSharedPtr<STreeView<TSharedPtr<FControlRigTreeNode>>> TreeView;

	/** Backing array for tree view */
	TArray<TSharedPtr<FControlRigTreeNode>> RootNodes;

	/** Backing array for tree view (filtered, displayed) */
	TArray<TSharedPtr<FControlRigTreeNode>> FilteredRootNodes;

	/** Action context for generating menu display info */
	FBlueprintActionContext BlueprintActionContext;
	
	/** Command list we bind to */
	TSharedPtr<FUICommandList> CommandList;

	/** Delegate handle for the hook into the graph editor */
	FDelegateHandle GraphEditorDelegateHandle;

	/** Recursion guard when selecting */
	bool bSelecting;

	void OnDisplayControlUnitsOnlyChanged(ECheckBoxState NewState);
	ECheckBoxState IsDisplayControlUnitsOnlyChecked() const;
};