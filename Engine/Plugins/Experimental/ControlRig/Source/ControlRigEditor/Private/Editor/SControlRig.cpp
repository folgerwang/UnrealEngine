// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SControlRig.h"
#include "Widgets/Input/SComboButton.h"
#include "EditorStyleSet.h"
#include "Widgets/Input/SSearchBox.h"
#include "SControlRigUnitCombo.h"
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
#include "SControlRigItem.h"
#include "ControlRigBlueprintCommands.h"
#include "ControlRigGraph.h"
#include "ControlRigBlueprint.h"
#include "ControlRigGraphNode.h"
#include "ControlRigGraphSchema.h"
#include "GraphEditorModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Algo/Transform.h"
#include "Widgets/Input/SCheckBox.h"
#include "Units/RigUnit_Control.h"

#define LOCTEXT_NAMESPACE "SControlRig"

FControlRigTreeNode::FControlRigTreeNode(const FBlueprintActionInfo& InBlueprintActionInfo, const FBlueprintActionContext& InBlueprintActionContext)
	: BlueprintActionInfo(InBlueprintActionInfo)
{
	BlueprintActionUiSpec = BlueprintActionInfo.NodeSpawner->GetUiSpec(InBlueprintActionContext, InBlueprintActionInfo.GetBindings());
}

SControlRig::~SControlRig()
{
	FBlueprintActionDatabase& ActionDatabase = FBlueprintActionDatabase::Get();
	ActionDatabase.OnEntryRemoved().RemoveAll(this);
	ActionDatabase.OnEntryUpdated().RemoveAll(this);

	FGraphEditorModule& GraphEditorModule = FModuleManager::LoadModuleChecked<FGraphEditorModule>(TEXT("GraphEditor"));
	TArray<FGraphEditorModule::FGraphEditorMenuExtender_SelectedNode>& ExtenderDelegates = GraphEditorModule.GetAllGraphEditorContextMenuExtender();
	ExtenderDelegates.RemoveAll([this](const FGraphEditorModule::FGraphEditorMenuExtender_SelectedNode& Delegate) { return Delegate.GetHandle() == GraphEditorDelegateHandle; });
}

void SControlRig::Construct(const FArguments& InArgs, TSharedRef<FControlRigEditor> InControlRigEditor)
{
	ControlRigEditor = InControlRigEditor;
	bSelecting = false;
	bDisplayControlUnitsOnly = false;

	BlueprintActionContext.Blueprints.Add(InControlRigEditor->GetBlueprintObj());

	CommandList = MakeShared<FUICommandList>();

	FGraphEditorModule& GraphEditorModule = FModuleManager::LoadModuleChecked<FGraphEditorModule>(TEXT("GraphEditor"));
	TArray<FGraphEditorModule::FGraphEditorMenuExtender_SelectedNode>& ExtenderDelegates = GraphEditorModule.GetAllGraphEditorContextMenuExtender();

	ExtenderDelegates.Add(FGraphEditorModule::FGraphEditorMenuExtender_SelectedNode::CreateSP(this, &SControlRig::GetGraphEditorMenuExtender));
	GraphEditorDelegateHandle = ExtenderDelegates.Last().GetHandle();

	InControlRigEditor->OnGraphNodeSelectionChanged().AddSP(this, &SControlRig::HandleGraphSelectionChanged);

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
					.Padding(3.0f, 3.0f)
					.AutoWidth()
					.HAlign(HAlign_Left)
					[
						SNew(SControlRigUnitCombo, InControlRigEditor)
						.OnRigUnitSelected(this, &SControlRig::HandleAddUnit)
						.ToolTipText(LOCTEXT("AddUnit_Tooltip", "Adds a new unit to this rig"))
					]
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.Padding(3.0f, 1.0f)
					[
						SAssignNew(FilterBox, SSearchBox)
						.OnTextChanged(this, &SControlRig::OnFilterTextChanged)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SCheckBox)
						.OnCheckStateChanged(this, &SControlRig::OnDisplayControlUnitsOnlyChanged)
						.IsChecked(this, &SControlRig::IsDisplayControlUnitsOnlyChecked)
						.CheckedImage(FControlRigEditorStyle::Get().GetBrush("ControlRig.ControlUnitOn"))
						.CheckedPressedImage(FControlRigEditorStyle::Get().GetBrush("ControlRig.ControlUnitOn"))
						.CheckedHoveredImage(FControlRigEditorStyle::Get().GetBrush("ControlRig.ControlUnitOn"))
						.UncheckedImage(FControlRigEditorStyle::Get().GetBrush("ControlRig.ControlUnitOff"))
						.UncheckedPressedImage(FControlRigEditorStyle::Get().GetBrush("ControlRig.ControlUnitOff"))
						.UncheckedHoveredImage(FControlRigEditorStyle::Get().GetBrush("ControlRig.ControlUnitOff"))
						.ToolTipText(LOCTEXT("DisplayControlUnit_Tooltip", "Display Only Control Units"))
						.ForegroundColor(FEditorStyle::GetSlateColor("DefaultForeground"))
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
				SAssignNew(TreeView, STreeView<TSharedPtr<FControlRigTreeNode>>)
				.TreeItemsSource(&FilteredRootNodes)
				.SelectionMode(ESelectionMode::Multi)
				.OnGenerateRow(this, &SControlRig::MakeTableRowWidget)
				.OnGetChildren(this, &SControlRig::HandleGetChildrenForTree)
//				.OnSetExpansionRecursive(this, &SControlRig::SetItemExpansionRecursive)
				.OnSelectionChanged(this, &SControlRig::HandleTreeSelectionChanged)
//				.OnContextMenuOpening(this, &SControlRig::CreateContextMenu)
//				.OnItemScrolledIntoView(this, &SControlRig::OnItemScrolledIntoView)
//				.OnMouseButtonDoubleClick(this, &SControlRig::HandleItemDoubleClicked)
				.ItemHeight(24)
			]
		]
	];

	FBlueprintActionDatabase& ActionDatabase = FBlueprintActionDatabase::Get();
	ActionDatabase.OnEntryRemoved().AddSP(this, &SControlRig::HandleDatabaseActionsChanged);
	ActionDatabase.OnEntryUpdated().AddSP(this, &SControlRig::HandleDatabaseActionsChanged);

	RefreshTreeView();
}

void SControlRig::OnDisplayControlUnitsOnlyChanged(ECheckBoxState NewState)
{
	bDisplayControlUnitsOnly = (NewState == ECheckBoxState::Checked);

	RefreshTreeView();
}

ECheckBoxState SControlRig::IsDisplayControlUnitsOnlyChecked() const
{
	return bDisplayControlUnitsOnly? ECheckBoxState::Checked: ECheckBoxState::Unchecked;
}
void SControlRig::OnFilterTextChanged(const FText& InFilterText)
{
	FilterText = InFilterText;

	RefreshTreeView();
}

void SControlRig::BindCommands()
{
	const FControlRigBlueprintCommands& Commands = FControlRigBlueprintCommands::Get();

	CommandList->MapAction(Commands.DeleteItem,
		FExecuteAction::CreateSP(this, &SControlRig::HandleDeleteItem),
		FCanExecuteAction::CreateSP(this, &SControlRig::CanDeleteItem));
}

FReply SControlRig::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SControlRig::HandleAddUnit(UStruct* InUnitStruct)
{
	const FScopedTransaction Transaction( LOCTEXT("AddRigUnit", "Add Rig Unit") );

	UBlueprint* Blueprint = ControlRigEditor.Pin()->GetBlueprintObj();
	FControlRigBlueprintUtils::AddUnitMember(Blueprint, InUnitStruct);
}

void SControlRig::HandleDatabaseActionsChanged(UObject* ActionsKey)
{
	if(ActionsKey == ControlRigEditor.Pin()->GetBlueprintObj())
	{
		RefreshTreeView();
	}
}

void SControlRig::RefreshTreeView()
{
	auto ShouldDisplay = [this](const UProperty* InProperty, const FString& InFilteredString)
	{
		if (InProperty)
		{
			if (bDisplayControlUnitsOnly)
			{
				// see if it's struct, and see if its name matches
				if (const UStructProperty* StructProperty = Cast<UStructProperty>(InProperty))
				{
					if (StructProperty->Struct->IsChildOf((FRigUnit_Control::StaticStruct())))
					{
						return true;
					}
				}
			}
			else
			{
				if (InFilteredString.IsEmpty())
				{
					return true;
				}

				if (InProperty->GetName().Contains(InFilteredString))
				{
					return true;
				}

				// see if it's struct, and see if its name matches
				if (const UStructProperty* StructProperty = Cast<UStructProperty>(InProperty))
				{
					if (StructProperty->Struct->GetName().Contains(InFilteredString))
					{
						return true;
					}
				}
			}
		}

		return false;
	};

	RootNodes.Reset();
	FilteredRootNodes.Reset();

	UBlueprint* Blueprint = ControlRigEditor.Pin()->GetBlueprintObj();

	FString FilteredString = FilterText.ToString();

	FBlueprintActionDatabase::FActionRegistry const& ActionDatabase = FBlueprintActionDatabase::Get().GetAllActions();
	if(const FBlueprintActionDatabase::FActionList* ActionList = ActionDatabase.Find(Blueprint))
	{
		for (UBlueprintNodeSpawner const* NodeSpawner : *ActionList)
		{
			// Allow spawning of variables only, as all of our units are member variables, along with conventional properties
			if(UControlRigPropertyNodeSpawner const* ControlRigPropertyNodeSpawner = Cast<UControlRigPropertyNodeSpawner>(NodeSpawner))
			{
				FBlueprintActionInfo ActionInfo(Blueprint, NodeSpawner);
				TSharedPtr<FControlRigTreeNode> NewNode = MakeShared<FControlRigTreeNode>(ActionInfo, BlueprintActionContext);
				RootNodes.Add(NewNode);

				const UProperty* Property = ControlRigPropertyNodeSpawner->GetProperty();
			
				if (ShouldDisplay(Property, FilteredString))
				{
					FilteredRootNodes.Add(NewNode);
				}
			}
		}
	}

	TreeView->RequestTreeRefresh();
}

TSharedRef<ITableRow> SControlRig::MakeTableRowWidget(TSharedPtr<FControlRigTreeNode> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SControlRigItem, OwnerTable, SharedThis(this), InItem.ToSharedRef(), CommandList.ToSharedRef());
}

void SControlRig::HandleGetChildrenForTree(TSharedPtr<FControlRigTreeNode> InItem, TArray<TSharedPtr<FControlRigTreeNode>>& OutChildren)
{
	
}

void SControlRig::HandleDeleteItem()
{
	UControlRigBlueprint* ControlRigBlueprint = Cast<UControlRigBlueprint>(ControlRigEditor.Pin()->GetBlueprintObj());

	TArray<TSharedPtr<FControlRigTreeNode>> SelectedItems;
	TreeView->GetSelectedItems(SelectedItems);
	if(SelectedItems.Num() > 0)
	{
		FScopedTransaction Transaction(LOCTEXT("DeleteRigItem", "Delete rig item"));

		ControlRigBlueprint->Modify();

		for(const TSharedPtr<FControlRigTreeNode>& SelectedItem : SelectedItems)
		{
			UControlRigPropertyNodeSpawner const* ControlRigPropertyNodeSpawner = CastChecked<UControlRigPropertyNodeSpawner>(SelectedItem->BlueprintActionInfo.NodeSpawner);
			const FName PropertyName = ControlRigPropertyNodeSpawner->GetProperty()->GetFName();

			// first remove all nodes
			for(UEdGraph* Graph : ControlRigBlueprint->UbergraphPages)
			{
				// We dont want to concurrently modify Graph->Nodes, so we store nodes to remove here
				TArray<UEdGraphNode*> NodesToRemove;
				for(UEdGraphNode* Node : Graph->Nodes)
				{
					if(UControlRigGraphNode* ControlRigGraphNode = Cast<UControlRigGraphNode>(Node))
					{
						if(ControlRigGraphNode->GetPropertyName() == PropertyName)
						{
							NodesToRemove.Add(Node);
						}
					}
				}

				for(UEdGraphNode* NodeToRemove : NodesToRemove)
				{
					FBlueprintEditorUtils::RemoveNode(ControlRigBlueprint, NodeToRemove, true);
				}
			}

			// Also remove backing variable
			FBlueprintEditorUtils::RemoveMemberVariable(ControlRigBlueprint, PropertyName);
		}
	}
}

bool SControlRig::CanDeleteItem() const
{
	return TreeView->GetNumItemsSelected() > 0;
}

TSharedRef<FExtender> SControlRig::GetGraphEditorMenuExtender(const TSharedRef<class FUICommandList>, const class UEdGraph* Graph, const class UEdGraphNode* Node, const UEdGraphPin* Pin, bool bConst)
{
	TSharedRef<FExtender> Extender(new FExtender());

	if (Graph->IsA<UControlRigGraph>() && Node->IsA<UControlRigGraphNode>())
	{
		Extender->AddMenuExtension(
			"ContextMenu",
			EExtensionHook::After,
			nullptr,
			FMenuExtensionDelegate::CreateRaw(this, &SControlRig::HandleExtendGraphEditorMenu));
	}

	return Extender;
}

void SControlRig::HandleExtendGraphEditorMenu(FMenuBuilder& InMenuBuilder)
{
	const FControlRigBlueprintCommands& Commands = FControlRigBlueprintCommands::Get();

	InMenuBuilder.PushCommandList(CommandList.ToSharedRef());
	InMenuBuilder.BeginSection(TEXT("ControlRigItem"), LOCTEXT("ControlRigItemHeader", "Control Rig Item"));
	{
	}
	InMenuBuilder.EndSection();
	InMenuBuilder.PopCommandList();
}

void SControlRig::HandleGraphSelectionChanged(const TSet<UObject*>& SelectedNodes)
{
	if(!bSelecting)
	{
		TGuardValue<bool> GuardValue(bSelecting, true);

		TreeView->ClearSelection();

		for(const TSharedPtr<FControlRigTreeNode>& Node : FilteredRootNodes)
		{
			UControlRigPropertyNodeSpawner const* ControlRigPropertyNodeSpawner = CastChecked<UControlRigPropertyNodeSpawner>(Node->BlueprintActionInfo.NodeSpawner);

			for(UObject* SelectedNode : SelectedNodes)
			{
				if(UControlRigGraphNode* ControlRigGraphNode = Cast<UControlRigGraphNode>(SelectedNode))
				{
					if(ControlRigGraphNode->GetPropertyName() == ControlRigPropertyNodeSpawner->GetProperty()->GetFName())
					{
						TreeView->SetItemSelection(Node, true);
						break;
					}
				}
			}
		}
	}
}

void SControlRig::HandleTreeSelectionChanged(TSharedPtr<FControlRigTreeNode> InItem, ESelectInfo::Type InSelectInfo)
{
	if(!bSelecting)
	{
		TGuardValue<bool> GuardValue(bSelecting, true);

		TArray<TSharedPtr<FControlRigTreeNode>> SelectedItems;
		TreeView->GetSelectedItems(SelectedItems);

		TArray<FString> SelectedNodePropertyPaths;
		Algo::Transform(SelectedItems, SelectedNodePropertyPaths,
			[](const TSharedPtr<FControlRigTreeNode>& InSelectedTreeItem) -> FString
			{
				UControlRigPropertyNodeSpawner const* ControlRigPropertyNodeSpawner = CastChecked<UControlRigPropertyNodeSpawner>(InSelectedTreeItem->BlueprintActionInfo.NodeSpawner);
				return ControlRigPropertyNodeSpawner->GetProperty()->GetName();
			});
		ControlRigEditor.Pin()->SetSelectedNodes(SelectedNodePropertyPaths);
	}
}

#undef LOCTEXT_NAMESPACE