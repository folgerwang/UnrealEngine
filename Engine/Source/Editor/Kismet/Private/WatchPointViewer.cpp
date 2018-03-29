// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WatchPointViewer.h"

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "EditorStyleSet.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "K2Node_Event.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "GraphEditorActions.h"
#include "EdGraphSchema_K2.h"

#include "Editor.h"

#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Input/SHyperlink.h"

#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#include "KismetNodes/KismetNodeInfoContext.h"
#include "Stats/Stats.h"

#define LOCTEXT_NAMESPACE "WatchPointViewer"

struct FWatchRow
{
	FWatchRow(
		UBlueprint* InBP,
		const UEdGraphNode* InNode,
		const UEdGraphPin* InPin,
		FText InBlueprintName,
		FText InGraphName,
		FText InNodeName,
		FText InDisplayName,
		FText InValue,
		FText InType
	)
		: BP(InBP)
		, Node(InNode)
		, Pin(InPin)
		, BlueprintName(MoveTemp(InBlueprintName))
		, GraphName(MoveTemp(InGraphName))
		, NodeName(MoveTemp(InNodeName))
		, DisplayName(MoveTemp(InDisplayName))
		, Value(MoveTemp(InValue))
		, Type(MoveTemp(InType))
	{
		ObjectBeingDebugged = BP ? BP->GetObjectBeingDebugged() : nullptr;
		ObjectBeingDebuggedName = ObjectBeingDebugged ? FText::FromString(ObjectBeingDebugged->GetName()) : LOCTEXT("Unknown Object", "Unknown object");
	}

	FWatchRow(
		UBlueprint* InBP,
		const UEdGraphNode* InNode,
		const UEdGraphPin* InPin,
		FText InBlueprintName,
		FText InGraphName,
		FText InNodeName,
		FDebugInfo Info
	)
		: BP(InBP)
		, Node(InNode)
		, Pin(InPin)
		, BlueprintName(MoveTemp(InBlueprintName))
		, GraphName(MoveTemp(InGraphName))
		, NodeName(MoveTemp(InNodeName))
		, DisplayName(MoveTemp(Info.DisplayName))
		, Value(MoveTemp(Info.Value))
		, Type(MoveTemp(Info.Type))
	{
		BlueprintName = MoveTemp(InBlueprintName);
		ObjectBeingDebugged = BP ? BP->GetObjectBeingDebugged() : nullptr;
		ObjectBeingDebuggedName = ObjectBeingDebugged ? FText::FromString(ObjectBeingDebugged->GetName()) : LOCTEXT("Unknown Object", "Unknown object");
		for (const FDebugInfo& ChildInfo : Info.Children)
		{
			Children.Add(MakeShared<FWatchRow>(InBP, InNode, InPin, BlueprintName, GraphName, NodeName, ChildInfo));
		}
	}

	// this can't be const because we store watches in the blueprint
	UBlueprint* BP;
	const UEdGraphNode* Node;
	const UEdGraphPin* Pin;
	// this can't be const because SelectActor takes a non-const actor
	UObject* ObjectBeingDebugged;
	
	FText BlueprintName;
	FText ObjectBeingDebuggedName;
	FText GraphName;
	FText NodeName;
	FText DisplayName;
	FText Value;
	FText Type;

	TArray<TSharedRef<FWatchRow>> Children;

	// used for copying entries in the watch viewer
	FText GetTextForEntry() const
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ObjectName"), FText::FromString(ObjectBeingDebugged ? ObjectBeingDebugged->GetName() : TEXT("")));
		Args.Add(TEXT("BlueprintName"), BlueprintName);
		Args.Add(TEXT("GraphName"), GraphName);
		Args.Add(TEXT("NodeName"), NodeName);
		Args.Add(TEXT("DisplayName"), DisplayName);
		Args.Add(TEXT("Type"), Type);
		Args.Add(TEXT("Value"), Value);
		return FText::Format(LOCTEXT("WatchEntry", "{ObjectName}({BlueprintName}) {GraphName} {NodeName} {DisplayName}({Type}): {Value}"), Args);
	}
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDisplayedWatchWindowChanged, TArray<TSharedRef<FWatchRow>>*);
FOnDisplayedWatchWindowChanged WatchListSubscribers;

/**
* Widget that visualizes the contents of a FWatchRow.
*/
class SWatchTreeWidgetItem : public SMultiColumnTableRow<TSharedRef<FWatchRow>>
{
public:

	SLATE_BEGIN_ARGS(SWatchTreeWidgetItem)
		: _WatchToVisualize()
	{ }

	SLATE_ARGUMENT(TSharedPtr<FWatchRow>, WatchToVisualize)

	SLATE_END_ARGS()

public:

	/**
	* Construct child widgets that comprise this widget.
	*
	* @param InArgs Declaration from which to construct this widget.
	*/
	void Construct(const FArguments& InArgs, class SWatchViewer* InOwner, const TSharedRef<STableViewBase>& InOwnerTableView);

public:

	// SMultiColumnTableRow overrides
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

protected:
	FText GetDebuggedObjectName() const
	{
		return WatchRow->ObjectBeingDebuggedName;
	}

	FText GetBlueprintName() const
	{
		return WatchRow->BlueprintName;
	}

	FText GetGraphName() const
	{
		return WatchRow->GraphName;
	}

	FText GetNodeName() const
	{
		return WatchRow->NodeName;
	}

	FText GetVariableName() const
	{
		return WatchRow->DisplayName;
	}

	FText GetValue() const
	{
		return WatchRow->Value;
	}

	FText GetType() const
	{
		return WatchRow->Type;
	}

	void HandleHyperlinkDebuggedObjectNavigate() const
	{
		if (AActor* Actor = Cast<AActor>(WatchRow->ObjectBeingDebugged))
		{
			GEditor->SelectActor(Actor, true, true, true);
		}
	}

	void HandleHyperlinkNodeNavigate() const
	{
		if (WatchRow.IsValid() && WatchRow->Node)
		{
			FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(WatchRow->Node);
		}
	}

private:

	/** The info about the widget that we are visualizing. */
	TSharedPtr<FWatchRow> WatchRow;

	SWatchViewer* Owner;
};

typedef STreeView<TSharedRef<FWatchRow>> SWatchTree;

class SWatchViewer : public SCompoundWidget
{
	friend class FBlueprintEditor;
public:
	SLATE_BEGIN_ARGS(SWatchViewer){}
	SLATE_END_ARGS()

	SWatchViewer()
	{
		FKismetDebugUtilities::WatchedPinsListChangedEvent.AddRaw(this, &SWatchViewer::HandleWatchedPinsChanged);
	}
	~SWatchViewer()
	{
		FKismetDebugUtilities::WatchedPinsListChangedEvent.RemoveAll(this);
	}

	void Construct(const FArguments& InArgs, TArray<TSharedRef<FWatchRow>>* InWatchSource);
	TSharedRef<ITableRow> HandleGenerateRow(TSharedRef<FWatchRow> InWatchRow, const TSharedRef<STableViewBase>& OwnerTable);
	void HandleGetChildren(TSharedRef<FWatchRow> InWatchRow, TArray<TSharedRef<FWatchRow>>& OutChildren);
	void HandleWatchedPinsChanged(UBlueprint* BlueprintObj);
	void UpdateWatches(TArray<TSharedRef<FWatchRow>>* WatchValues);
	void CopySelectedRows() const;
	void StopWatchingPin() const;

	TSharedPtr<SWatchTree> WatchTreeWidget;
	TArray<TSharedRef<FWatchRow>>* WatchSource;

	TSharedPtr< FUICommandList > CommandList;
};

void SWatchViewer::Construct(const FArguments& InArgs, TArray<TSharedRef<FWatchRow>>* InWatchSource)
{
	CommandList = MakeShareable( new FUICommandList );
	CommandList->MapAction( 
		FGenericCommands::Get().Copy,
		FExecuteAction::CreateSP( this, &SWatchViewer::CopySelectedRows ),
		// we need to override the default 'can execute' because we want to be available during debugging:
		FCanExecuteAction::CreateStatic( [](){ return true; } )
	);

	CommandList->MapAction(
		FGraphEditorCommands::Get().StopWatchingPin,
		FExecuteAction::CreateSP(this, &SWatchViewer::StopWatchingPin),
		FCanExecuteAction::CreateStatic([]() { return true; })
	);

	WatchSource = InWatchSource;

	const auto ContextMenuOpened = [](TWeakPtr<FUICommandList> InCommandList, TWeakPtr<SWatchViewer> ControlOwnerWeak) -> TSharedPtr<SWidget>
	{
		const bool CloseAfterSelection = true;
		FMenuBuilder MenuBuilder( CloseAfterSelection, InCommandList.Pin() );
		MenuBuilder.AddMenuEntry(FGraphEditorCommands::Get().StopWatchingPin);
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Copy);
		return MenuBuilder.MakeWidget();
	};

	const auto EmptyWarningVisibility = [](TWeakPtr<SWatchViewer> ControlOwnerWeak) -> EVisibility
	{
		TSharedPtr<SWatchViewer> ControlOwner = ControlOwnerWeak.Pin();
		if (ControlOwner.IsValid() &&
			ControlOwner->WatchSource &&
			ControlOwner->WatchSource->Num() > 0)
		{
			return EVisibility::Hidden;
		}
		return EVisibility::Visible;
	};

	const auto WatchViewIsEnabled = [](TWeakPtr<SWatchViewer> ControlOwnerWeak) -> bool
	{
		TSharedPtr<SWatchViewer> ControlOwner = ControlOwnerWeak.Pin();
		if (ControlOwner.IsValid() &&
			ControlOwner->WatchSource &&
			ControlOwner->WatchSource->Num() > 0)
		{
			return true;
		}
		return false;
	};

	// Cast due to TSharedFromThis inheritance issues:
	TSharedRef<SWatchViewer> SelfTyped = StaticCastSharedRef<SWatchViewer>(AsShared());
	TWeakPtr<SWatchViewer> SelfWeak = SelfTyped;
	TWeakPtr<FUICommandList> CommandListWeak = CommandList;

	ChildSlot
	[
		SNew(SBorder)
		.Padding(4)
		.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			[
				SAssignNew(WatchTreeWidget, SWatchTree)
				.ItemHeight(25.0f)
				.TreeItemsSource(WatchSource)
				.OnGenerateRow(this, &SWatchViewer::HandleGenerateRow)
				.OnGetChildren(this, &SWatchViewer::HandleGetChildren)
				.OnContextMenuOpening(FOnContextMenuOpening::CreateStatic(ContextMenuOpened, CommandListWeak, SelfWeak))
				.IsEnabled(
					TAttribute<bool>::Create(
						TAttribute<bool>::FGetter::CreateStatic(WatchViewIsEnabled, SelfWeak)
					)
				)
				.HeaderRow
				(
					SNew(SHeaderRow)
					+SHeaderRow::Column(TEXT("ObjectName"))
					.FillWidth(.2f)
					.VAlignHeader(VAlign_Center)
					.HeaderContent()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ObjectName", "Object Name"))
						.ToolTipText(LOCTEXT("ObjectNameTooltip", "Name of the object being debugged"))
					]
					+ SHeaderRow::Column(TEXT("GraphName"))
					.FillWidth(.2f)
					.VAlignHeader(VAlign_Center)
					.HeaderContent()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("GraphName", "Graph Name"))
						.ToolTipText(LOCTEXT("GraphNameTooltip", "Name of the source blueprint graph for this variable"))
					]
					+ SHeaderRow::Column(TEXT("NodeName"))
					.FillWidth(.3f)
					.VAlignHeader(VAlign_Center)
					.HeaderContent()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("NodeName", "Node Name"))
						.ToolTipText(LOCTEXT("NodeNameTooltip", "Name of the source blueprint graph node for this variable"))
					]
					+ SHeaderRow::Column(TEXT("VariableName"))
					.FillWidth(.3f)
					.VAlignHeader(VAlign_Center)
					.HeaderContent()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("VariableName", "Variable Name"))
						.ToolTipText(LOCTEXT("VariabelNameTooltip", "Name of the variable"))
					]
					+ SHeaderRow::Column(TEXT("Value"))
					.FillWidth(.8f)
					.VAlignHeader(VAlign_Center)
					.HeaderContent()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Value", "Value"))
						.ToolTipText(LOCTEXT("ValueTooltip", "Current value of this variable"))
					]
				)
			]
			+SOverlay::Slot()
			.Padding(32.f)
			[
				SNew(STextBlock)
				.Text(
					LOCTEXT("NoWatches", "No watches to display")
				)
				.Justification(ETextJustify::Center)
				.Visibility(
					TAttribute<EVisibility>::Create(
						TAttribute<EVisibility>::FGetter::CreateStatic(EmptyWarningVisibility, SelfWeak)
					)
				)
			]
		]
	];

	WatchListSubscribers.AddSP(StaticCastSharedRef<SWatchViewer>(AsShared()), &SWatchViewer::UpdateWatches);
}

TSharedRef<ITableRow> SWatchViewer::HandleGenerateRow(TSharedRef<FWatchRow> InWatchRow, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SWatchTreeWidgetItem, this, OwnerTable)
		.WatchToVisualize(InWatchRow);
}

void SWatchViewer::HandleGetChildren(TSharedRef<FWatchRow> InWatchRow, TArray<TSharedRef<FWatchRow>>& OutChildren)
{
	OutChildren = InWatchRow->Children;
}

void SWatchViewer::HandleWatchedPinsChanged(UBlueprint* BlueprintObj)
{
	WatchViewer::UpdateDisplayedWatches(BlueprintObj);
}

void SWatchViewer::UpdateWatches(TArray<TSharedRef<FWatchRow>>* Watches)
{
	WatchTreeWidget->RequestTreeRefresh();
}

void SWatchViewer::CopySelectedRows() const
{
	FString StringToCopy;

	// We want to copy in the order displayed, not the order selected, so iterate the list and build up the string:
	if (WatchSource)
	{
		for (const TSharedRef<FWatchRow>& Item : *WatchSource)
		{
			if (WatchTreeWidget->IsItemSelected(Item))
			{
				StringToCopy.Append(Item->GetTextForEntry().ToString());
				StringToCopy.Append(TEXT("\r\n"));
			}
		}
	}

	if (!StringToCopy.IsEmpty())
	{
		FPlatformApplicationMisc::ClipboardCopy(*StringToCopy);
	}
}

void SWatchTreeWidgetItem::Construct(const FArguments& InArgs, SWatchViewer* InOwner, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	this->WatchRow = InArgs._WatchToVisualize;
	Owner = InOwner;

	SMultiColumnTableRow< TSharedRef<FWatchRow> >::Construct(SMultiColumnTableRow< TSharedRef<FWatchRow> >::FArguments().Padding(1), InOwnerTableView);
}

void SWatchViewer::StopWatchingPin() const
{
	TArray<TSharedRef<FWatchRow>> SelectedRows = WatchTreeWidget->GetSelectedItems();
	for (TSharedRef<FWatchRow>& Row : SelectedRows)
	{
		FKismetDebugUtilities::TogglePinWatch(Row->BP, Row->Pin);
	}
}

TSharedRef<SWidget> SWatchTreeWidgetItem::GenerateWidgetForColumn(const FName& ColumnName)
{
	const static FName NAME_ObjectName(TEXT("ObjectName"));
	const static FName NAME_GraphName(TEXT("GraphName"));
	const static FName NAME_NodeName(TEXT("NodeName"));
	const static FName NAME_VariableName(TEXT("VariableName"));
	const static FName NAME_Value(TEXT("Value"));

	if (ColumnName == NAME_ObjectName)
	{
		return SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(FMargin(2.0f, 0.0f))
			[
				SNew(SHyperlink)
				.Text(this, &SWatchTreeWidgetItem::GetDebuggedObjectName)
				.ToolTipText(this, &SWatchTreeWidgetItem::GetBlueprintName)
				.OnNavigate(this, &SWatchTreeWidgetItem::HandleHyperlinkDebuggedObjectNavigate)
			];
	}
	else if (ColumnName == NAME_GraphName)
	{
		return SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(FMargin(2.0f, 0.0f))
			[
				SNew(STextBlock)
				.Text(this, &SWatchTreeWidgetItem::GetGraphName)
			];
	}
	else if (ColumnName == NAME_NodeName)
	{
		FString Comment; 
		if (WatchRow->Node->NodeComment.Len() > 0)
		{
			Comment = TEXT("\n\n");
			Comment.Append(WatchRow->Node->NodeComment);
		}
		FText TooltipText = FText::Format(LOCTEXT("NodeTooltip", "Find the {0} node in the blueprint graph.{1}"), GetNodeName(), FText::FromString(Comment));
		return SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(FMargin(2.0f, 0.0f))
			[
				SNew(SHyperlink)
				.Text(this, &SWatchTreeWidgetItem::GetNodeName)
				.ToolTipText(TooltipText)
				.OnNavigate(this, &SWatchTreeWidgetItem::HandleHyperlinkNodeNavigate)
			];
	}
	else if (ColumnName == NAME_VariableName)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SExpanderArrow, SharedThis(this))
			]
		+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SWatchTreeWidgetItem::GetVariableName)
				.ToolTipText(this, &SWatchTreeWidgetItem::GetType)
			];
	}
	else if (ColumnName == NAME_Value)
	{
		return SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(FMargin(2.0f, 0.0f))
			[
				SNew(STextBlock)
				.Text(this, &SWatchTreeWidgetItem::GetValue)
			];
	}
	else
	{
		return SNullWidget::NullWidget;
	}
}

// Proxy array of the watches. This allows us to manually refresh UI state when changes are made:
TArray<TSharedRef<FWatchRow>> Private_WatchSource;

void WatchViewer::UpdateDisplayedWatches(UBlueprint* BlueprintObj)
{
	if (!ensure(BlueprintObj))
	{
		return;
	}
	Private_WatchSource.Reset();

	FText BlueprintName = FText::FromString(BlueprintObj->GetName());

	for (const FEdGraphPinReference& PinRef : BlueprintObj->WatchedPins)
	{
		UEdGraphPin* Pin = PinRef.Get();

		FText GraphName = FText::FromString(Pin->GetOwningNode()->GetGraph()->GetName());
		FText NodeName = Pin->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView);

		FDebugInfo DebugInfo;
		const FKismetDebugUtilities::EWatchTextResult WatchStatus = FKismetDebugUtilities::GetDebugInfo(DebugInfo, BlueprintObj, BlueprintObj->GetObjectBeingDebugged(), Pin);

		if (WatchStatus != FKismetDebugUtilities::EWTR_Valid)
		{
			const UEdGraphSchema* Schema = Pin->GetOwningNode()->GetSchema();
			DebugInfo.DisplayName = Schema->GetPinDisplayName(Pin);
			DebugInfo.Type = UEdGraphSchema_K2::TypeToText(Pin->PinType);

			switch (WatchStatus)
			{
			case FKismetDebugUtilities::EWTR_NotInScope:
				DebugInfo.Value = LOCTEXT("NotInScope", "(not in scope)");
				break;

			case FKismetDebugUtilities::EWTR_NoProperty:
				DebugInfo.Value = LOCTEXT("NoDebugData", "(no debug data)");
				break;

			case FKismetDebugUtilities::EWTR_NoDebugObject:
				DebugInfo.Value = LOCTEXT("NoDebugObject", "(no debug object)");
				break;

			default:
				// do nothing
				break;
			}
		}

		Private_WatchSource.Add(
			MakeShared<FWatchRow>(
				BlueprintObj,
				Pin->GetOwningNode(),
				Pin,
				BlueprintName,
				GraphName,
				NodeName,
				DebugInfo
				)
		);
	}

	// Notify subscribers:
	WatchListSubscribers.Broadcast(&Private_WatchSource);
}

void WatchViewer::RegisterTabSpawner()
{
	const auto SpawnWatchViewTab = []( const FSpawnTabArgs& Args )
	{
		return SNew(SDockTab)
			.TabRole( ETabRole::NomadTab )
			.Label( LOCTEXT("TabTitle", "Watch Window") )
			[
				SNew(SBorder)
				.BorderImage( FEditorStyle::GetBrush("Docking.Tab.ContentAreaBrush") )
				[
					SNew(SWatchViewer, &Private_WatchSource)
				]
			];
	};
	
	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();

	const FName TabName = TEXT("WatchViewer");
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner( TabName, FOnSpawnTab::CreateStatic(SpawnWatchViewTab) )
		.SetDisplayName( LOCTEXT("TabTitle", "Watch Window") )
		.SetTooltipText( LOCTEXT("TooltipText", "Open the watch window tab.") )
		.SetGroup( MenuStructure.GetDeveloperToolsDebugCategory() );
}

#undef LOCTEXT_NAMESPACE
