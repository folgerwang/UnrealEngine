// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SRigHierarchy.h"
#include "Widgets/Input/SComboButton.h"
#include "EditorStyleSet.h"
#include "Widgets/Input/SSearchBox.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "ControlRigEditor.h"
#include "BlueprintActionDatabase.h"
#include "BlueprintVariableNodeSpawner.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "K2Node_VariableGet.h"
#include "ControlRigBlueprintUtils.h"
#include "NodeSpawners/ControlRigPropertyNodeSpawner.h"
#include "ControlRigHierarchyCommands.h"
#include "ControlRigGraph.h"
#include "ControlRigBlueprint.h"
#include "ControlRigGraphNode.h"
#include "ControlRigGraphSchema.h"
#include "GraphEditorModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "AnimationRuntime.h"
#include "PropertyCustomizationHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor/EditorEngine.h"
#include "HelperUtil.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "ControlRig.h"

#define LOCTEXT_NAMESPACE "SRigHierarchy"

//////////////////////////////////////////////////////////////
/// FRigTreeJoint
///////////////////////////////////////////////////////////
FRigTreeJoint::FRigTreeJoint(const FName& InJoint, TWeakPtr<SRigHierarchy> InHierarchyHandler)
{
	CachedJoint = InJoint;
}

TSharedRef<ITableRow> FRigTreeJoint::MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, TSharedRef<FRigTreeJoint> InRigTreeJoint, TSharedRef<FUICommandList> InCommandList, TSharedPtr<SRigHierarchy> InHierarchy)
{
	return SNew(SRigHierarchyItem, InOwnerTable, InRigTreeJoint, InCommandList)
		.OnRenameJoint(InHierarchy.Get(), &SRigHierarchy::RenameJoint)
		.OnVerifyJointNameChanged(InHierarchy.Get(), &SRigHierarchy::OnVerifyNameChanged);
}

void FRigTreeJoint::RequestRename()
{
	OnRenameRequested.ExecuteIfBound();
}
//////////////////////////////////////////////////////////////
/// SRigHierarchyItem
///////////////////////////////////////////////////////////
void SRigHierarchyItem::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedRef<FRigTreeJoint> InRigTreeJoint, TSharedRef<FUICommandList> InCommandList)
{
	WeakRigTreeJoint = InRigTreeJoint;
	WeakCommandList = InCommandList;

	OnVerifyJointNameChanged = InArgs._OnVerifyJointNameChanged;
	OnRenameJoint = InArgs._OnRenameJoint;

	TSharedPtr< SInlineEditableTextBlock > InlineWidget;

	STableRow<TSharedPtr<FRigTreeJoint>>::Construct(
		STableRow<TSharedPtr<FRigTreeJoint>>::FArguments()
		.Content()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SAssignNew(InlineWidget, SInlineEditableTextBlock)
				.Text(this, &SRigHierarchyItem::GetName)
				//.HighlightText(FilterText)
				.OnVerifyTextChanged(this, &SRigHierarchyItem::OnVerifyNameChanged)
				.OnTextCommitted(this, &SRigHierarchyItem::OnNameCommitted)
				.MultiLine(false)
			]
		], OwnerTable);

	InRigTreeJoint->OnRenameRequested.BindSP(InlineWidget.Get(), &SInlineEditableTextBlock::EnterEditingMode);
}

FText SRigHierarchyItem::GetName() const
{
	return (FText::FromName(WeakRigTreeJoint.Pin()->CachedJoint));
}

bool SRigHierarchyItem::OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage)
{
	const FName NewName = FName(*InText.ToString());
	if (OnVerifyJointNameChanged.IsBound())
	{
		return OnVerifyJointNameChanged.Execute(WeakRigTreeJoint.Pin()->CachedJoint, NewName, OutErrorMessage);
	}

	// if not bound, just allow
	return true;
}

void SRigHierarchyItem::OnNameCommitted(const FText& InText, ETextCommit::Type InCommitType) const
{
	// for now only allow enter
	// because it is important to keep the unique names per pose
	if (InCommitType == ETextCommit::OnEnter)
	{
		FName NewName = FName(*InText.ToString());
		FName OldName = WeakRigTreeJoint.Pin()->CachedJoint;

		if (!OnRenameJoint.IsBound() || OnRenameJoint.Execute(OldName, NewName))
		{
			if (WeakRigTreeJoint.IsValid())
			{
				WeakRigTreeJoint.Pin()->CachedJoint = NewName;
			}
		}
	}
}

///////////////////////////////////////////////////////////

SRigHierarchy::~SRigHierarchy()
{
}

void SRigHierarchy::Construct(const FArguments& InArgs, TSharedRef<FControlRigEditor> InControlRigEditor)
{
	ControlRigEditor = InControlRigEditor;

	ControlRigBlueprint = ControlRigEditor.Pin()->GetControlRigBlueprint();
	// @todo: find a better place to do it
	ControlRigBlueprint->Hierarchy.Initialize();
	// for deleting, renaming, dragging
	CommandList = MakeShared<FUICommandList>();

	InControlRigEditor->OnGraphNodeSelectionChanged().AddSP(this, &SRigHierarchy::HandleGraphSelectionChanged);

	UEditorEngine* Editor = Cast<UEditorEngine>(GEngine);
	if (Editor != nullptr)
	{
		Editor->RegisterForUndo(this);
	}

	BindCommands();

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Top)
		.Padding(0.0f)
		[
			SNew(SBorder)
			.Padding(0.0f)
			.BorderImage(FEditorStyle::GetBrush("DetailsView.CategoryTop"))
			.BorderBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.VAlign(VAlign_Top)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.Padding(3.0f, 1.0f)
					[
						SAssignNew(FilterBox, SSearchBox)
			//			.OnTextChanged(this, &SRigHierarchy::OnFilterTextChanged)
					]
				]
			]
		]
		+SVerticalBox::Slot()
		.Padding(0.0f, 0.0f)
		[
			SNew(SBorder)
			.Padding(2.0f)
			.BorderImage(FEditorStyle::GetBrush("SCSEditor.TreePanel"))
			[
				SAssignNew(TreeView, STreeView<TSharedPtr<FRigTreeJoint>>)
				.TreeItemsSource(&RootJoints)
				.SelectionMode(ESelectionMode::Multi)
				.OnGenerateRow(this, &SRigHierarchy::MakeTableRowWidget)
				.OnGetChildren(this, &SRigHierarchy::HandleGetChildrenForTree)
				.OnSelectionChanged(this, &SRigHierarchy::OnSelectionChanged)
				.OnContextMenuOpening(this, &SRigHierarchy::CreateContextMenu)
				.ItemHeight(24)
			]
		]
	];

	RefreshTreeView();
}

void SRigHierarchy::BindCommands()
{
	// create new command
	const FControlRigHierarchyCommands& Commands = FControlRigHierarchyCommands::Get();
	CommandList->MapAction(Commands.AddItem,
		FExecuteAction::CreateSP(this, &SRigHierarchy::HandleNewItem));

	CommandList->MapAction(Commands.DuplicateItem,
		FExecuteAction::CreateSP(this, &SRigHierarchy::HandleDuplicateItem),
		FCanExecuteAction::CreateSP(this, &SRigHierarchy::CanDuplicateItem));

	CommandList->MapAction(Commands.DeleteItem,
		FExecuteAction::CreateSP(this, &SRigHierarchy::HandleDeleteItem),
		FCanExecuteAction::CreateSP(this, &SRigHierarchy::CanDeleteItem));

	CommandList->MapAction(Commands.RenameItem,
		FExecuteAction::CreateSP(this, &SRigHierarchy::HandleRenameItem),
		FCanExecuteAction::CreateSP(this, &SRigHierarchy::CanRenameItem));
}

void SRigHierarchy::RefreshTreeView()
{
	RootJoints.Reset();
	FilteredRootJoints.Reset();

	if (ControlRigBlueprint.IsValid())
	{
		FRigHierarchy& Hierarchy = ControlRigBlueprint->Hierarchy;

		TMap<FName, TSharedPtr<FRigTreeJoint>> SearchTable;

		for (int32 JointIndex = 0; JointIndex < Hierarchy.Joints.Num(); ++JointIndex)
		{
			FRigJoint& Joint = Hierarchy.Joints[JointIndex];

			// create new item
			TSharedPtr<FRigTreeJoint> NewItem = MakeShared<FRigTreeJoint>(Hierarchy.Joints[JointIndex].Name, SharedThis(this));
			SearchTable.Add(Joint.Name, NewItem);

			if (Joint.ParentName == NAME_None)
			{
				RootJoints.Add(NewItem);
			}
			else
			{
				// you have to find one
				TSharedPtr<FRigTreeJoint>* FoundItem = SearchTable.Find(Joint.ParentName);
				check(FoundItem);
				// add to children list
				FoundItem->Get()->Children.Add(NewItem);
			}
		}

		for (int32 RootIndex = 0; RootIndex < RootJoints.Num(); ++RootIndex)
		{
			SetExpansionRecursive(RootJoints[RootIndex]);
		}
	}

	TreeView->RequestTreeRefresh();
}

void SRigHierarchy::SetExpansionRecursive(TSharedPtr<FRigTreeJoint> InJoint)
{
	TreeView->SetItemExpansion(InJoint, true);

	for (int32 ChildIndex = 0; ChildIndex < InJoint->Children.Num(); ++ChildIndex)
	{
		SetExpansionRecursive(InJoint->Children[ChildIndex]);
	}
}
TSharedRef<ITableRow> SRigHierarchy::MakeTableRowWidget(TSharedPtr<FRigTreeJoint> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return InItem->MakeTreeRowWidget(OwnerTable, InItem.ToSharedRef(), CommandList.ToSharedRef(), SharedThis(this));
}

void SRigHierarchy::HandleGetChildrenForTree(TSharedPtr<FRigTreeJoint> InItem, TArray<TSharedPtr<FRigTreeJoint>>& OutChildren)
{
	OutChildren = InItem.Get()->Children;
}

void SRigHierarchy::HandleGraphSelectionChanged(const TSet<UObject*>& SelectedJoints)
{

}

void SRigHierarchy::OnSelectionChanged(TSharedPtr<FRigTreeJoint> Selection, ESelectInfo::Type SelectInfo)
{
	// need dummy object
	if (Selection.IsValid())
	{
		FRigHierarchy* RigHierarchy = GetInstanceHierarchy();

		if (RigHierarchy)
		{
			const int32 JointIndex = RigHierarchy->GetIndex(Selection->CachedJoint);
			if (JointIndex != INDEX_NONE)
			{
				ControlRigEditor.Pin()->SetDetailStruct(MakeShareable(new FStructOnScope(FRigJoint::StaticStruct(), (uint8*)&RigHierarchy->Joints[JointIndex])));
				ControlRigEditor.Pin()->SelectJoint(Selection->CachedJoint);
				return;
			}
		}

		// if failed, try BP hierarhcy? Todo:
	}
}

TSharedPtr<FRigTreeJoint> FindJoint(const FName& InJointName, TSharedPtr<FRigTreeJoint> CurrentItem)
{
	if (CurrentItem->CachedJoint == InJointName)
	{
		return CurrentItem;
	}

	for (int32 ChildIndex = 0; ChildIndex < CurrentItem->Children.Num(); ++ChildIndex)
	{
		TSharedPtr<FRigTreeJoint> Found = FindJoint(InJointName, CurrentItem->Children[ChildIndex]);
		if (Found.IsValid())
		{
			return Found;
		}
	}

	return TSharedPtr<FRigTreeJoint>();
}

void SRigHierarchy::SelectJoint(const FName& JointName) const
{
	for (int32 RootIndex = 0; RootIndex < RootJoints.Num(); ++RootIndex)
	{
		TSharedPtr<FRigTreeJoint> Found = FindJoint(JointName, RootJoints[RootIndex]);
		if (Found.IsValid())
		{
			TreeView->SetSelection(Found);
		}
	}
}

void SRigHierarchy::ClearDetailPanel() const
{
	ControlRigEditor.Pin()->ClearDetailObject();
}

TSharedPtr< SWidget > SRigHierarchy::CreateContextMenu()
{
	const FControlRigHierarchyCommands& Actions = FControlRigHierarchyCommands::Get();

	const bool CloseAfterSelection = true;
	FMenuBuilder MenuBuilder(CloseAfterSelection, CommandList);
	{
		MenuBuilder.BeginSection("HierarchyEditAction", LOCTEXT("EditAction", "Edit"));
		MenuBuilder.AddMenuEntry(Actions.AddItem);
		MenuBuilder.AddMenuEntry(Actions.DeleteItem);
		MenuBuilder.AddMenuEntry(Actions.DuplicateItem);
		MenuBuilder.AddMenuEntry(Actions.RenameItem);

		MenuBuilder.AddMenuSeparator();
		MenuBuilder.AddSubMenu(
			LOCTEXT("ImportSubMenu", "Import"),
			LOCTEXT("ImportSubMenu_ToolTip", "Insert current pose to selected PoseAsset"),
			FNewMenuDelegate::CreateSP(this, &SRigHierarchy::CreateImportMenu)
		);
	
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

void SRigHierarchy::CreateImportMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddWidget(
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(3)
		[
			SNew(STextBlock)
			.Font(FEditorStyle::GetFontStyle("ControlRig.Hierarchy.Menu"))
			.Text(LOCTEXT("ImportMesh_Title", "Select Mesh"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(3)
		[
			SNew(SObjectPropertyEntryBox)
			.AllowedClass(USkeletalMesh::StaticClass())
			.OnObjectChanged(this, &SRigHierarchy::ImportHierarchy)
		]
		,
		FText()
	);
}

void SRigHierarchy::ImportHierarchy(const FAssetData& InAssetData)
{
	FRigHierarchy* Hier = GetHierarchy();
	USkeletalMesh* Mesh = Cast<USkeletalMesh> (InAssetData.GetAsset());
	if (Mesh && Hier)
	{
		const FReferenceSkeleton& RefSkeleton = Mesh->RefSkeleton;
		const TArray<FMeshBoneInfo>& BoneInfos = RefSkeleton.GetRawRefBoneInfo();
		const TArray<FTransform>& BonePoses = RefSkeleton.GetRawRefBonePose();

		for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
		{
			// only add if you don't have it. This may change in the future
			if (Hier->GetIndex(BoneInfos[BoneIndex].Name) == INDEX_NONE)
			{
				// @todo: add optimized version without sorting, but if no sort, we should make sure not to use find index function
				FName ParentName = (BoneInfos[BoneIndex].ParentIndex != INDEX_NONE) ? BoneInfos[BoneInfos[BoneIndex].ParentIndex].Name : NAME_None;
				Hier->AddJoint(BoneInfos[BoneIndex].Name, ParentName, FAnimationRuntime::GetComponentSpaceTransform(RefSkeleton, BonePoses, BoneIndex));
			}
		}

		RefreshTreeView();
		FSlateApplication::Get().DismissAllMenus();
	}
}

bool SRigHierarchy::IsMultiSelected() const
{
	return TreeView->GetNumItemsSelected() > 0;
}

bool SRigHierarchy::IsSingleSelected() const
{
	return TreeView->GetNumItemsSelected() == 1;
}

void SRigHierarchy::HandleDeleteItem()
{
 	FRigHierarchy* Hierarchy = GetHierarchy();
 	if (Hierarchy)
 	{
		ClearDetailPanel();
		FScopedTransaction Transaction(LOCTEXT("HierarchyTreeDeleteSelected", "Delete selected items from hierarchy"));
		ControlRigBlueprint->Modify();

		// clear detail view display
		ControlRigEditor.Pin()->ClearDetailObject();

		TArray<TSharedPtr<FRigTreeJoint>> SelectedItems = TreeView->GetSelectedItems();

		for (int32 ItemIndex = 0; ItemIndex < SelectedItems.Num(); ++ItemIndex)
		{
			// when you select whole joints, you might not have them anymore
			if (Hierarchy->GetIndex(SelectedItems[ItemIndex]->CachedJoint) != INDEX_NONE)
			{
				Hierarchy->DeleteJoint(SelectedItems[ItemIndex]->CachedJoint, true);
			}
		}

		RefreshTreeView();
 	}
}

bool SRigHierarchy::CanDeleteItem() const
{
	return IsMultiSelected();
}

/** Delete Item */
void SRigHierarchy::HandleNewItem()
{
	FRigHierarchy* Hierarchy = GetHierarchy();
	if (Hierarchy)
	{
		// unselect current selected item
		ClearDetailPanel();

		FScopedTransaction Transaction(LOCTEXT("HierarchyTreeAdded", "Add new item to hierarchy"));
		ControlRigBlueprint->Modify();

		FName ParentName = NAME_None;
		FTransform ParentTransform = FTransform::Identity;

		TArray<TSharedPtr<FRigTreeJoint>> SelectedItems = TreeView->GetSelectedItems();
		if (SelectedItems.Num() > 0)
		{
			ParentName = SelectedItems[0]->CachedJoint;
			ParentTransform = Hierarchy->GetGlobalTransform(ParentName);
		}

		const FName NewJointName = CreateUniqueName(TEXT("NewJoint"));
		Hierarchy->AddJoint(NewJointName, ParentName, ParentTransform);

		RefreshTreeView();

		// reselect current selected item
		SelectJoint(NewJointName);
	}
}

/** Check whether we can deleting the selected item(s) */
bool SRigHierarchy::CanDuplicateItem() const
{
	return IsMultiSelected();
}

/** Duplicate Item */
void SRigHierarchy::HandleDuplicateItem()
{
	FRigHierarchy* Hierarchy = GetHierarchy();
	if (Hierarchy)
	{
		ClearDetailPanel();

		FScopedTransaction Transaction(LOCTEXT("HierarchyTreeDuplicateSelected", "Duplicate selected items from hierarchy"));
		ControlRigBlueprint->Modify();

		TArray<TSharedPtr<FRigTreeJoint>> SelectedItems = TreeView->GetSelectedItems();
		TArray<FName> NewNames;
		for (int32 Index = 0; Index < SelectedItems.Num(); ++Index)
		{
			FName Name = SelectedItems[Index]->CachedJoint;
			FTransform Transform = Hierarchy->GetGlobalTransform(Name);

			FName ParentName = Hierarchy->GetParentName(Name);

			const FName NewName = CreateUniqueName(Name);
			Hierarchy->AddJoint(NewName, ParentName, Transform);
			NewNames.Add(NewName);
		}

		RefreshTreeView();
		
		for (int32 Index = 0; Index < NewNames.Num(); ++Index)
		{
			SelectJoint(NewNames[Index]);
		}
	}
}

/** Check whether we can deleting the selected item(s) */
bool SRigHierarchy::CanRenameItem() const
{
	return IsSingleSelected();
}

/** Delete Item */
void SRigHierarchy::HandleRenameItem()
{
	FRigHierarchy* Hierarchy = GetHierarchy();
	if (Hierarchy)
	{
		ClearDetailPanel();

		FScopedTransaction Transaction(LOCTEXT("HierarchyTreeRenameSelected", "Rename selected item from hierarchy"));
		ControlRigBlueprint->Modify();

		TArray<TSharedPtr<FRigTreeJoint>> SelectedItems = TreeView->GetSelectedItems();
		if (SelectedItems.Num() > 0)
		{
			SelectedItems[0]->RequestRename();
		}
	}
}

FRigHierarchy* SRigHierarchy::GetHierarchy() const
{
	if (ControlRigBlueprint.IsValid())
	{
		return &ControlRigBlueprint->Hierarchy;
	}

	return nullptr;
}

FRigHierarchy* SRigHierarchy::GetInstanceHierarchy() const
{
	if (ControlRigEditor.IsValid())
	{
		UControlRig* ControlRig = ControlRigEditor.Pin()->GetInstanceRig();
		if (ControlRig)
		{
			return &ControlRig->Hierarchy.BaseHierarchy;
		}
	}

	return nullptr;
}
FName SRigHierarchy::CreateUniqueName(const FName& InBaseName) const
{
	return UtilityHelpers::CreateUniqueName(InBaseName, [this](const FName& CurName) { return GetHierarchy()->GetIndex(CurName) == INDEX_NONE; });
}

void SRigHierarchy::PostRedo(bool bSuccess) 
{
	if (bSuccess)
	{
		RefreshTreeView();
	}
}

void SRigHierarchy::PostUndo(bool bSuccess) 
{
	if (bSuccess)
	{
		RefreshTreeView();
	}
}

bool SRigHierarchy::RenameJoint(const FName& OldName, const FName& NewName)
{
	ClearDetailPanel();

	if (OldName == NewName)
	{
		return true;
	}

	// make sure there is no duplicate
	FRigHierarchy* Hierarchy = GetHierarchy();
	if (Hierarchy)
	{
		Hierarchy->Rename(OldName, NewName);
		SelectJoint(NewName);

		return true;
	}

	return false;
}

bool SRigHierarchy::OnVerifyNameChanged(const FName& OldName, const FName& NewName, FText& OutErrorMessage)
{
	if (OldName == NewName)
	{
		return true;
	}

	// make sure there is no duplicate
	FRigHierarchy* Hierarchy = GetHierarchy();
	if (Hierarchy)
	{
		const int32 Found = Hierarchy->GetIndex(OldName);
		if (Found != INDEX_NONE)
		{
			const int32 Duplicate = Hierarchy->GetIndex(NewName);
			if (Duplicate != INDEX_NONE)
			{
				OutErrorMessage = FText::FromString(TEXT("Duplicate name exists"));

				return false;
			}
		}
	}

	return true;
}
#undef LOCTEXT_NAMESPACE