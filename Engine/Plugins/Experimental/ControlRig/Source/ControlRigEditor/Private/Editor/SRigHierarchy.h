// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Views/STreeView.h"
#include "Hierarchy.h"
#include "EditorUndoClient.h"

class SRigHierarchy;
class FControlRigEditor;
class SSearchBox;
class FUICommandList;
class UControlRigBlueprint;
struct FAssetData;
class FMenuBuilder;
class SRigHierarchyItem;

DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnRenameJoint, const FName& /*OldName*/, const FName& /*NewName*/);
DECLARE_DELEGATE_RetVal_ThreeParams(bool, FOnVerifyJointNameChanged, const FName& /*OldName*/, const FName& /*NewName*/, FText& /*OutErrorMessage*/);

/** An item in the tree */
class FRigTreeJoint : public TSharedFromThis<FRigTreeJoint>
{
public:
	FRigTreeJoint(const FName& InJoint, TWeakPtr<SRigHierarchy> InHierarchyHandler);
public:
	/** Joint Data to display */
	FName CachedJoint;
	TArray<TSharedPtr<FRigTreeJoint>> Children;

	TSharedRef<ITableRow> MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, TSharedRef<FRigTreeJoint> InRigTreeJoint, TSharedRef<FUICommandList> InCommandList, TSharedPtr<SRigHierarchy> InHierarchy);

	void RequestRename();

	/** Delegate for when the context menu requests a rename */
	DECLARE_DELEGATE(FOnRenameRequested);
	FOnRenameRequested OnRenameRequested;
};


 class SRigHierarchyItem : public STableRow<TSharedPtr<FRigTreeJoint>>
{
	SLATE_BEGIN_ARGS(SRigHierarchyItem) {}
		/** Callback when the text is committed. */
		SLATE_EVENT(FOnRenameJoint, OnRenameJoint)
		/** Called whenever the text is changed interactively by the user */
		SLATE_EVENT(FOnVerifyJointNameChanged, OnVerifyJointNameChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedRef<FRigTreeJoint> InControlRigTreeJoint, TSharedRef<FUICommandList> InCommandList);
	void OnNameCommitted(const FText& InText, ETextCommit::Type InCommitType) const;
	bool OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage);

private:
	TWeakPtr<FRigTreeJoint> WeakRigTreeJoint;
	TWeakPtr<FUICommandList> WeakCommandList;
	
	FOnRenameJoint OnRenameJoint;
	FOnVerifyJointNameChanged OnVerifyJointNameChanged;

	FText GetName() const;
};

/** Widget allowing editing of a control rig's structure */
class SRigHierarchy : public SCompoundWidget, public FEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS(SRigHierarchy) {}
	SLATE_END_ARGS()

	~SRigHierarchy();

	void Construct(const FArguments& InArgs, TSharedRef<FControlRigEditor> InControlRigEditor);

private:
	/** Bind commands that this widget handles */
	void BindCommands();

	/** Rebuild the tree view */
	void RefreshTreeView();

	/** Make a row widget for the table */
	TSharedRef<ITableRow> MakeTableRowWidget(TSharedPtr<FRigTreeJoint> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Get children for the tree */
	void HandleGetChildrenForTree(TSharedPtr<FRigTreeJoint> InItem, TArray<TSharedPtr<FRigTreeJoint>>& OutChildren);

	/** Check whether we can deleting the selected item(s) */
	bool CanDeleteItem() const;

	/** Delete Item */
	void HandleDeleteItem();

	/** Delete Item */
	void HandleNewItem();

	/** Check whether we can deleting the selected item(s) */
	bool CanDuplicateItem() const;

	/** Delete Item */
	void HandleDuplicateItem();

	/** Check whether we can deleting the selected item(s) */
	bool CanRenameItem() const;

	/** Delete Item */
	void HandleRenameItem();

	/** Sync up selection with the graph */
	void HandleGraphSelectionChanged(const TSet<UObject*>& SelectedJoints);

	/** Set Selection Changed */
	void OnSelectionChanged(TSharedPtr<FRigTreeJoint> Selection, ESelectInfo::Type SelectInfo);

	TSharedPtr< SWidget > CreateContextMenu();

	// FEditorUndoClient
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
private:
	/** Our owning control rig editor */
	TWeakPtr<FControlRigEditor> ControlRigEditor;

	/** Search box widget */
	TSharedPtr<SSearchBox> FilterBox;
	FText FilterText;

	void OnFilterTextChanged(const FText& SearchText);

	/** Tree view widget */
	TSharedPtr<STreeView<TSharedPtr<FRigTreeJoint>>> TreeView;

	/** Backing array for tree view */
	TArray<TSharedPtr<FRigTreeJoint>> RootJoints;

	/** Backing array for tree view (filtered, displayed) */
	TArray<TSharedPtr<FRigTreeJoint>> FilteredRootJoints;

	TWeakObjectPtr<UControlRigBlueprint> ControlRigBlueprint;
	
	/** Command list we bind to */
	TSharedPtr<FUICommandList> CommandList;

	bool IsMultiSelected() const;
	bool IsSingleSelected() const;

	FRigHierarchy* GetHierarchy() const;
	FRigHierarchy* GetInstanceHierarchy() const;

	void ImportHierarchy(const FAssetData& InAssetData);
	void CreateImportMenu(FMenuBuilder& MenuBuilder);
	void CreateRefreshMenu(FMenuBuilder& MenuBuilder);
	void RefreshHierarchy(const FAssetData& InAssetData);

	FName CreateUniqueName(const FName& InBaseName) const;

	void SetExpansionRecursive(TSharedPtr<FRigTreeJoint> InJoints);

	void ClearDetailPanel() const;
	void SelectJoint(const FName& JointName) const;

public:
	bool RenameJoint(const FName& OldName, const FName& NewName);
	bool OnVerifyNameChanged(const FName& OldName, const FName& NewName, FText& OutErrorMessage);
};