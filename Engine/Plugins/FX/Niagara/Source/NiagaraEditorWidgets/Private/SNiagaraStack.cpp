// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SNiagaraStack.h"

#include "NiagaraEditorModule.h"
#include "NiagaraEditorCommands.h"
#include "EditorStyleSet.h"
#include "NiagaraEditorWidgetsStyle.h"
#include "NiagaraEditorStyle.h"
#include "NiagaraEmitterHandle.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/Stack/NiagaraStackViewModel.h"
#include "ViewModels/Stack/NiagaraStackSpacer.h"
#include "ViewModels/Stack/NiagaraStackModuleItemOutputCollection.h"
#include "ViewModels/Stack/NiagaraStackModuleItemOutput.h"
#include "ViewModels/Stack/NiagaraStackErrorItem.h"
#include "ViewModels/Stack/NiagaraStackInputCategory.h" 
#include "ViewModels/Stack/NiagaraStackPropertyRow.h"
#include "ViewModels/Stack/NiagaraStackAdvancedExpander.h"
#include "ViewModels/Stack/NiagaraStackFunctionInputCollection.h"
#include "ViewModels/Stack/NiagaraStackModuleItem.h"
#include "ViewModels/Stack/NiagaraStackRendererItem.h"
#include "ViewModels/Stack/NiagaraStackParameterStoreEntry.h"
#include "ViewModels/Stack/NiagaraStackEmitterSpawnScriptItemGroup.h"
#include "ViewModels/Stack/NiagaraStackEventScriptItemGroup.h"
#include "Framework/Application/SlateApplication.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "NiagaraEmitter.h"
#include "IDetailTreeNode.h"
#include "Stack/SNiagaraStackFunctionInputName.h"
#include "Stack/SNiagaraStackFunctionInputValue.h"
#include "Stack/SNiagaraStackEmitterPropertiesItem.h"
#include "Stack/SNiagaraStackEventHandlerPropertiesItem.h"
#include "Stack/SNiagaraStackItemExpander.h"
#include "Stack/SNiagaraStackItemGroup.h"
#include "Stack/SNiagaraStackModuleItem.h"
#include "Stack/SNiagaraStackParameterStoreEntryName.h"
#include "Stack/SNiagaraStackParameterStoreEntryValue.h"
#include "Stack/SNiagaraStackRendererItem.h"
#include "Stack/SNiagaraStackTableRow.h"
#include "NiagaraEditorWidgetsUtilities.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Stack/SNiagaraStackErrorItem.h"
#include "NiagaraStackEditorData.h"
#include "ScopedTransaction.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Stack/SNiagaraStackSpacer.h"

/** Contains data for a socket drag and drop operation in the StackEntry node. */
class FNiagaraStackEntryDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FNiagaraStackEntryDragDropOp, FDecoratedDragDropOp)

public:
	FNiagaraStackEntryDragDropOp(TArray<UNiagaraStackEntry*> InDraggedEntries)
	{
		DraggedEntries = InDraggedEntries;
	}

	const TArray<UNiagaraStackEntry*> GetDraggedEntries() const
	{
		return DraggedEntries;
	}

private:
	TArray<UNiagaraStackEntry*> DraggedEntries;
};

#define LOCTEXT_NAMESPACE "NiagaraStack"

const float SpacerHeight = 6;

const FText SNiagaraStack::OccurencesFormat = NSLOCTEXT("NiagaraStack", "OccurencesFound", "{0} / {1}");

void SNiagaraStack::Construct(const FArguments& InArgs, UNiagaraStackViewModel* InStackViewModel)
{
	StackViewModel = InStackViewModel;
	StackViewModel->OnStructureChanged().AddSP(this, &SNiagaraStack::StackStructureChanged);
	StackViewModel->OnSearchCompleted().AddSP(this, &SNiagaraStack::OnStackSearchComplete); 
	NameColumnWidth = .3f;
	ContentColumnWidth = .7f;
	PinIsPinnedColor = FNiagaraEditorWidgetsStyle::Get().GetColor("NiagaraEditor.Stack.ForegroundColor");
	PinIsUnpinnedColor = PinIsPinnedColor.Desaturate(.4f);
	CurrentPinColor = StackViewModel->GetSystemViewModel()->GetIsEmitterPinned(StackViewModel->GetEmitterHandleViewModel()->AsShared()) ? PinIsPinnedColor: PinIsUnpinnedColor;
	bNeedsJumpToNextOccurence = false;

	ConstructHeaderWidget();
	TSharedPtr<SWidget> HeaderBox;
	ChildSlot
	[
		SAssignNew(HeaderBox, SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 3)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(3)
			[
				HeaderWidget.ToSharedRef()
			]
		]
		+ SVerticalBox::Slot()
		.Padding(0)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(5)
			[
				SAssignNew(StackTree, STreeView<UNiagaraStackEntry*>)
				.OnGenerateRow(this, &SNiagaraStack::OnGenerateRowForStackItem)
				.OnGetChildren(this, &SNiagaraStack::OnGetChildren)
				.TreeItemsSource(&StackViewModel->GetRootEntries())
				.OnTreeViewScrolled(this, &SNiagaraStack::StackTreeScrolled)
			]
		]
	];

	StackTree->SetScrollOffset(StackViewModel->GetLastScrollPosition());

	auto OnHeaderMouseButtonUp = [this](const FGeometry&, const FPointerEvent& MouseEvent) -> FReply
	{
		if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
		{
			FMenuBuilder MenuBuilder(true, nullptr);
			MenuBuilder.BeginSection("EmitterInlineMenuActions", LOCTEXT("EmitterActions", "Emitter Actions"));
			{
				if (!GetEmitterNameIsReadOnly()) // Only allow renaming local copies of Emitters in Systems
				{
					MenuBuilder.AddMenuEntry(
						LOCTEXT("RenameEmitter", "Rename Emitter"),
						LOCTEXT("RenameEmitterToolTip", "Rename this local emitter copy"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateSP(InlineEditableTextBlock.Get(), &SInlineEditableTextBlock::EnterEditingMode)));
				}

				MenuBuilder.AddMenuEntry(
					LOCTEXT("ShowEmitterInContentBrowser", "Show in Content Browser"),
					LOCTEXT("ShowEmitterInContentBrowserToolTip", "Show the emitter in this stack in the Content Browser"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateSP(this, &SNiagaraStack::ShowEmitterInContentBrowser))); 
				
				FUIAction Action(FExecuteAction::CreateSP(this, &SNiagaraStack::SetEmitterEnabled, !StackViewModel->GetEmitterHandleViewModel()->GetIsEnabled()),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(this, &SNiagaraStack::CheckEmitterEnabledStatus, true));
				MenuBuilder.AddMenuEntry(
					LOCTEXT("IsEnabled", "Is Enabled"),
					LOCTEXT("ToggleEmitterEnabledToolTip", "Toggle emitter enabled/disabled state"),
					FSlateIcon(),
					Action,
					NAME_None, 
					EUserInterfaceActionType::Check);

				MenuBuilder.AddMenuEntry(
					LOCTEXT("CollapseStack", "Collapse All"),
					LOCTEXT("CollapseStackToolTip", "Collapses every row in the stack."),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateSP(this, &SNiagaraStack::CollapseAll)));
				
				TSharedPtr<FUICommandInfo> ExpandStackGroups = FNiagaraEditorModule::Get().Commands().CollapseStackToHeaders;
				MenuBuilder.AddMenuEntry(
					ExpandStackGroups->GetLabel(),
					ExpandStackGroups->GetDescription(),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateUObject(StackViewModel, &UNiagaraStackViewModel::CollapseToHeaders)));
			}
			MenuBuilder.EndSection();
			MenuBuilder.BeginSection("EmitterNavigateTo", 
				FText::Format(LOCTEXT("EmitterNavigateTo", "Navigate to {0} Section:"), StackViewModel->GetEmitterHandleViewModel()->GetNameText()));
			{
				// Parse all children of root entries and if they are UNiagaraStackItemGroup then add a navigate menu entry
				TArray<UNiagaraStackEntry*> EntriesToProcess(StackViewModel->GetRootEntries());
				TArray<UNiagaraStackEntry*> RootChildren;
				while (EntriesToProcess.Num() > 0)
				{
					UNiagaraStackEntry* EntryToProcess = EntriesToProcess[0];
					EntriesToProcess.RemoveAtSwap(0);
					EntryToProcess->GetUnfilteredChildren(RootChildren);
				}
				for (auto RootChild: RootChildren)
				{
					UNiagaraStackItemGroup* GroupChild = Cast<UNiagaraStackItemGroup>(RootChild);
					if (GroupChild != nullptr)
					{
						MenuBuilder.AddMenuEntry(
							RootChild->GetDisplayName(),
							FText::Format(LOCTEXT("EmitterTooltip", "Navigate to {0}"), RootChild->GetDisplayName()),
							FSlateIcon(),
							FUIAction(FExecuteAction::CreateSP(this, &SNiagaraStack::NavigateTo, RootChild)));
					}
				}
			}
			MenuBuilder.EndSection();
			
			MenuBuilder.BeginSection("StackActions", LOCTEXT("StackActions", "Stack Actions"));
			if (StackViewModel->HasDismissedStackIssues())
			{
				MenuBuilder.AddMenuEntry(
					LOCTEXT("UndismissIssues", "Undismiss All Stack Issues"),
					LOCTEXT("ShowAssetInContentBrowserToolTip", "Undismiss all issues that were previously dismissed for this stack, if any"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateUObject(StackViewModel, &UNiagaraStackViewModel::UndismissAllIssues)));
			}
			MenuBuilder.EndSection();

			FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
			FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, MenuBuilder.MakeWidget(), MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
			return FReply::Handled();
		}
		return FReply::Unhandled();
	};
	HeaderBox->SetOnMouseButtonUp(FPointerEventHandler::CreateLambda(OnHeaderMouseButtonUp));
	
	PrimeTreeExpansion();
}

void SNiagaraStack::PrimeTreeExpansion()
{
	TArray<UNiagaraStackEntry*> EntriesToProcess(StackViewModel->GetRootEntries());
	while (EntriesToProcess.Num() > 0)
	{
		UNiagaraStackEntry* EntryToProcess = EntriesToProcess[0];
		EntriesToProcess.RemoveAtSwap(0);

		if (EntryToProcess->GetIsExpanded())
		{
			StackTree->SetItemExpansion(EntryToProcess, true);
			EntryToProcess->GetFilteredChildren(EntriesToProcess);
		}
		else
		{
			StackTree->SetItemExpansion(EntryToProcess, false);
		}
	}
}

void SNiagaraStack::ConstructHeaderWidget()
{
	if (!StackViewModel->GetEmitterHandleViewModel().IsValid() || !StackViewModel->GetSystemViewModel().IsValid())
	{
		HeaderWidget = SNullWidget::NullWidget;
	}
	else
	{
		SAssignNew(HeaderWidget, SVerticalBox)
			//~ Enable check box, pin text box, view source emitter button, and external header controls.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				//~ Enabled
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2)
				[
					SNew(SCheckBox)
					.ToolTipText(LOCTEXT("EnabledToolTip", "Toggles whether this emitter is enabled. Disabled emitters don't simulate or render."))
					.IsChecked(StackViewModel->GetEmitterHandleViewModel()->AsShared(), &FNiagaraEmitterHandleViewModel::GetIsEnabledCheckState)
					.OnCheckStateChanged(StackViewModel->GetEmitterHandleViewModel()->AsShared(), &FNiagaraEmitterHandleViewModel::OnIsEnabledCheckStateChanged)
					.Visibility(this, &SNiagaraStack::GetEnableCheckboxVisibility) 
				]
				// Pin
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SButton)
					.IsFocusable(false)
					.ToolTipText(LOCTEXT("PinToolTip", "Pin this emitter"))
					.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
					.ForegroundColor(this, &SNiagaraStack::GetPinColor)
					.ContentPadding(2)
					.OnClicked(this, &SNiagaraStack::PinButtonPressed)
					.Visibility(this, &SNiagaraStack::GetPinEmitterVisibility)
					.Content()
					[
						SNew(STextBlock)
						.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.9"))
						.Text(FText::FromString(FString(TEXT("\xf08d"))))
						.RenderTransformPivot(FVector2D(.5f, .5f))
					]
				]
				// Name and Source Emitter Name
				+ SHorizontalBox::Slot()
				.Padding(2)
				[
					SNew(SWrapBox)
					.Clipping(EWidgetClipping::ClipToBoundsAlways) 
					.UseAllottedWidth(true)
					+ SWrapBox::Slot()
					.Padding(3, 0)
					[						
						SAssignNew(InlineEditableTextBlock, SInlineEditableTextBlock)
						.ToolTipText(this, &SNiagaraStack::GetEmitterNameToolTip)
						.Style(FNiagaraEditorStyle::Get(), "NiagaraEditor.HeadingInlineEditableText") 
						.Clipping(EWidgetClipping::ClipToBoundsAlways)
						.Text(StackViewModel->GetEmitterHandleViewModel()->AsShared(), &FNiagaraEmitterHandleViewModel::GetNameText)
						.OnTextCommitted(this, &SNiagaraStack::OnStackViewNameTextCommitted)
						.OnVerifyTextChanged(StackViewModel->GetEmitterHandleViewModel()->AsShared(), &FNiagaraEmitterHandleViewModel::VerifyNameTextChanged)
						.IsReadOnly(this, &SNiagaraStack::GetEmitterNameIsReadOnly)
					]
					+ SWrapBox::Slot()
					.Padding(3, 0)
					[
						SNew(STextBlock)
						.ToolTipText(this, &SNiagaraStack::GetEmitterNameToolTip)
						.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.SubduedHeadingTextBox")
						.Clipping(EWidgetClipping::ClipToBoundsAlways)
						.Text(this, &SNiagaraStack::GetSourceEmitterNameText)
						.Visibility(this, &SNiagaraStack::GetSourceEmitterNameVisibility) 
					]
 				]
				// Open and Focus Source Emitter
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(2)
				[
					SNew(SButton)
					.IsFocusable(false)
					.ToolTipText(LOCTEXT("OpenAndFocusSourceEmitterToolTip", "Open and Focus Source Emitter"))
					.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
					.ForegroundColor(this, &SNiagaraStack::GetPinColor)
					.ContentPadding(2)
					.OnClicked(this, &SNiagaraStack::OpenSourceEmitter)
					.Visibility(this, &SNiagaraStack::GetOpenSourceEmitterVisibility)
					// GoToSource icon is 30x30px so we scale it down to stay in line with other 12x12px UI
					.DesiredSizeScale(FVector2D(0.55f, 0.55f))
					.Content()
					[
						SNew(SImage)
						.Image(FNiagaraEditorWidgetsStyle::Get().GetBrush("NiagaraEditor.Stack.GoToSourceIcon"))
						.ColorAndOpacity(FNiagaraEditorWidgetsStyle::Get().GetColor("NiagaraEditor.Stack.FlatButtonColor"))
					]
				]
			]

			//~ Stats
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.Padding(2)
			[
				SNew(STextBlock)
				.Text(StackViewModel->GetEmitterHandleViewModel()->GetEmitterViewModel()->AsShared(), &FNiagaraEmitterViewModel::GetStatsText)
			]
			
			//~ Search, view options
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.Padding(2, 4, 2, 4)
			[
				// Search box
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SAssignNew(SearchBox, SSearchBox)
					.HintText(LOCTEXT("StackSearchBoxHint", "Search the stack"))
					.SearchResultData(this, &SNiagaraStack::GetSearchResultData)
					.IsSearching(this, &SNiagaraStack::GetIsSearching)
					.OnTextChanged(this, &SNiagaraStack::OnSearchTextChanged)
					.DelayChangeNotificationsWhileTyping(true)
					.OnTextCommitted(this, &SNiagaraStack::OnSearchBoxTextCommitted)
					.OnSearch(this, &SNiagaraStack::OnSearchBoxSearch)
				]
				// View options
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4, 0, 0, 0)
				[
					SNew(SComboButton)
					.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
					.ForegroundColor(FSlateColor::UseForeground())
					.ToolTipText(LOCTEXT("ViewOptionsToolTip", "View Options"))
					.OnGetMenuContent(this, &SNiagaraStack::GetViewOptionsMenu)
					.ContentPadding(0)
					.MenuPlacement(MenuPlacement_BelowRightAnchor)
					.ButtonContent()
					[
						SNew(SBox)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush("GenericViewButton"))
							.ColorAndOpacity(FNiagaraEditorWidgetsStyle::Get().GetColor("NiagaraEditor.Stack.FlatButtonColor"))
						]
					]
				]
			];
	}
}

FReply SNiagaraStack::PinButtonPressed()
{
	bool bNewPinnedState = !StackViewModel->GetSystemViewModel()->GetIsEmitterPinned(StackViewModel->GetEmitterHandleViewModel()->AsShared());
	StackViewModel->GetSystemViewModel()->SetEmitterPinnedState(StackViewModel->GetEmitterHandleViewModel()->AsShared(), bNewPinnedState);
	CurrentPinColor = bNewPinnedState ? PinIsPinnedColor : PinIsUnpinnedColor;
	return FReply::Handled();
}

void SNiagaraStack::OnSearchTextChanged(const FText& SearchText)
{
	if (StackViewModel->GetCurrentSearchText().CompareTo(SearchText) != 0)
	{
		if (SearchExpandTimer.IsValid())
		{
			UnRegisterActiveTimer(SearchExpandTimer.ToSharedRef());
		}
		// restore expansion state of previous search
		for (auto SearchResult : StackViewModel->GetCurrentSearchResults())
		{
			for (UNiagaraStackEntry* ParentalUnit : SearchResult.EntryPath)
			{
				StackTree->SetItemExpansion(ParentalUnit, ParentalUnit->GetIsExpanded());
			}
		}
		bNeedsJumpToNextOccurence = true;
		StackViewModel->OnSearchTextChanged(SearchText);
	}
}

FReply SNiagaraStack::ScrollToNextMatch()
{
	AddSearchScrollOffset(1);
	return FReply::Handled();
}

FReply SNiagaraStack::ScrollToPreviousMatch()
{
	// move current match to the previous one in the StackTree, wrap around
	AddSearchScrollOffset(-1);
	return FReply::Handled();
}

void SNiagaraStack::AddSearchScrollOffset(int NumberOfSteps)
{
	if (StackViewModel->IsSearching() || StackViewModel->GetCurrentSearchResults().Num() == 0 || NumberOfSteps == 0)
	{
		return;
	}

	StackViewModel->AddSearchScrollOffset(NumberOfSteps);

	StackTree->RequestScrollIntoView(StackViewModel->GetCurrentFocusedEntry());
}

TOptional<SSearchBox::FSearchResultData> SNiagaraStack::GetSearchResultData() const
{
	if (StackViewModel->GetCurrentSearchText().IsEmpty())
	{
		return TOptional<SSearchBox::FSearchResultData>();
	}
	return TOptional<SSearchBox::FSearchResultData>({ StackViewModel->GetCurrentSearchResults().Num(), StackViewModel->GetCurrentFocusedMatchIndex() + 1 });
}

bool SNiagaraStack::GetIsSearching() const
{
	return StackViewModel->IsSearching();
}

bool SNiagaraStack::IsEntryFocusedInSearch(UNiagaraStackEntry* Entry) const
{
	if (StackViewModel && Entry && StackViewModel->GetCurrentFocusedEntry() == Entry)
	{
		return true;
	}
	return false;
}

FReply SNiagaraStack::OpenSourceEmitter() 
{
	if (StackViewModel && StackViewModel->GetEmitterHandleViewModel().IsValid() && StackViewModel->GetEmitterHandleViewModel()->GetEmitterHandle())
	{
		UNiagaraEmitter* Emitter = const_cast<UNiagaraEmitter*>(StackViewModel->GetEmitterHandleViewModel()->GetEmitterHandle()->GetSource());
		if (Emitter != nullptr)
		{
			FAssetEditorManager::Get().OpenEditorForAsset(Emitter);
		}
	}
	return FReply::Handled();
}

EVisibility SNiagaraStack::GetEnableCheckboxVisibility() const
{
	return StackViewModel->GetSystemViewModel()->GetEditMode() == ENiagaraSystemViewModelEditMode::SystemAsset ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SNiagaraStack::GetPinEmitterVisibility() const
{
	return StackViewModel->GetSystemViewModel()->GetEditMode() == ENiagaraSystemViewModelEditMode::SystemAsset ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SNiagaraStack::GetOpenSourceEmitterVisibility() const
{
	return CanOpenSourceEmitter() ? EVisibility::Visible : EVisibility::Collapsed;
}

bool SNiagaraStack::GetEmitterNameIsReadOnly() const
{
	if (StackViewModel && StackViewModel->GetSystemViewModel().IsValid())
	{
		return StackViewModel->GetSystemViewModel()->GetEditMode() == ENiagaraSystemViewModelEditMode::EmitterAsset;
	}
	return true;
}

bool SNiagaraStack::CanOpenSourceEmitter() const
{
	if (StackViewModel && StackViewModel->GetEmitterHandleViewModel().IsValid() && StackViewModel->GetEmitterHandleViewModel()->GetEmitterHandle()
		&& StackViewModel->GetEmitterHandleViewModel()->GetEmitterHandle()->GetSource())
	{
		if (StackViewModel->GetSystemViewModel()->GetEditMode() == ENiagaraSystemViewModelEditMode::SystemAsset)
		{
			return true;
		}
	}

	return false;
}

void SNiagaraStack::SetEmitterEnabled(bool bIsEnabled)
{
	StackViewModel->GetEmitterHandleViewModel()->SetIsEnabled(bIsEnabled);
}

bool SNiagaraStack::CheckEmitterEnabledStatus(bool bIsEnabled)
{
	return StackViewModel->GetEmitterHandleViewModel()->GetIsEnabled() == bIsEnabled;
}

void SNiagaraStack::ShowEmitterInContentBrowser()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	TArray<FAssetData> Assets;
	Assets.Add(FAssetData(StackViewModel->GetEmitterHandleViewModel()->GetEmitterHandle()->GetSource()));
	ContentBrowserModule.Get().SyncBrowserToAssets(Assets);
}

void SNiagaraStack::NavigateTo(UNiagaraStackEntry* Item)
{
	StackTree->RequestScrollIntoView(Item);
}

void CollapseEntriesRecursive(TArray<UNiagaraStackEntry*> Entries)
{
	for (UNiagaraStackEntry* Entry : Entries)
	{
		if (Entry->GetCanExpand())
		{
			Entry->SetIsExpanded(false);
		}
		
		TArray<UNiagaraStackEntry*> Children;
		Entry->GetUnfilteredChildren(Children);
		CollapseEntriesRecursive(Children);
	}
}

void SNiagaraStack::CollapseAll()
{
	CollapseEntriesRecursive(StackViewModel->GetRootEntries());
	StackViewModel->NotifyStructureChanged();
}

TSharedRef<SWidget> SNiagaraStack::GetViewOptionsMenu() const
{
	FMenuBuilder MenuBuilder(false, nullptr);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ShowAllAdvancedLabel", "Show All Advanced"),
		LOCTEXT("ShowAllAdvancedToolTip", "Forces all advanced items to be showing in the stack."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=]() { StackViewModel->SetShowAllAdvanced(!StackViewModel->GetShowAllAdvanced()); }),
			FCanExecuteAction(),
			FGetActionCheckState::CreateLambda([=]() { return StackViewModel->GetShowAllAdvanced() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })),
		NAME_None, EUserInterfaceActionType::Check);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ShowOutputsLabel", "Show Outputs"),
		LOCTEXT("ShowOutputsToolTip", "Whether or now to show module outputs in the stack."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=]() { StackViewModel->SetShowOutputs(!StackViewModel->GetShowOutputs()); }),
			FCanExecuteAction(),
			FGetActionCheckState::CreateLambda([=]() { return StackViewModel->GetShowOutputs() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })),
		NAME_None, EUserInterfaceActionType::Check);

	return MenuBuilder.MakeWidget();
}

FReply SNiagaraStack::OnRowDragDetected(const FGeometry& InGeometry, const FPointerEvent& InPointerEvent, UNiagaraStackEntry* InStackEntry)
{
	if (InStackEntry->CanDrag())
	{
		TArray<UNiagaraStackEntry*> DraggedEntries;
		DraggedEntries.Add(InStackEntry);
		TSharedRef<FNiagaraStackEntryDragDropOp> DragDropOp = MakeShared<FNiagaraStackEntryDragDropOp>(DraggedEntries);
		DragDropOp->CurrentHoverText = InStackEntry->GetDisplayName();
		DragDropOp->CurrentIconBrush = FNiagaraEditorWidgetsStyle::Get().GetBrush(
			FNiagaraStackEditorWidgetsUtilities::GetIconNameForExecutionSubcategory(InStackEntry->GetExecutionSubcategoryName(), true));
		DragDropOp->CurrentIconColorAndOpacity = FNiagaraEditorWidgetsStyle::Get().GetColor(
			FNiagaraStackEditorWidgetsUtilities::GetIconColorNameForExecutionCategory(InStackEntry->GetExecutionCategoryName()));
		DragDropOp->SetupDefaults();
		DragDropOp->Construct();
		return FReply::Handled().BeginDragDrop(DragDropOp);
	}
	return FReply::Unhandled();
}

TOptional<EItemDropZone> SNiagaraStack::OnRowCanAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, UNiagaraStackEntry* InTargetEntry)
{
	TOptional<EItemDropZone> DropZone;
	TSharedPtr<FNiagaraStackEntryDragDropOp> DragDropOp = InDragDropEvent.GetOperationAs<FNiagaraStackEntryDragDropOp>();
	if (DragDropOp.IsValid())
	{
		DragDropOp->ResetToDefaultToolTip();
		TOptional<UNiagaraStackEntry::FDropResult> Result = InTargetEntry->CanDrop(DragDropOp->GetDraggedEntries());
		if (Result.IsSet())
		{
			if (Result.GetValue().DropMessage.IsEmptyOrWhitespace() == false)
			{
				DragDropOp->CurrentHoverText = FText::Format(LOCTEXT("DropFormat", "{0} - {1}"), DragDropOp->GetDefaultHoverText(), Result.GetValue().DropMessage);
			}

			if (Result.GetValue().bCanDrop)
			{
				DropZone = EItemDropZone::OntoItem;
			}
			else 
			{
				DragDropOp->CurrentIconBrush = FEditorStyle::GetBrush("Icons.Error");
				DragDropOp->CurrentIconColorAndOpacity = FLinearColor::White;
			}
		}
	}
	return DropZone;
}

FReply SNiagaraStack::OnRowAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, UNiagaraStackEntry* InTargetEntry)
{
	TSharedPtr<FNiagaraStackEntryDragDropOp> DragDropOp = InDragDropEvent.GetOperationAs<FNiagaraStackEntryDragDropOp>();
	if (DragDropOp.IsValid())
	{
		if (ensureMsgf(InTargetEntry->Drop(DragDropOp->GetDraggedEntries()).IsSet(), TEXT("Failed to drop stack entry when can drop returned true")))
		{
			return FReply::Handled();
		}
	}
	return FReply::Unhandled();
}

void SNiagaraStack::OnStackSearchComplete()
{
	// fire up timer to expand all parentchains!!
	SearchExpandTimer = RegisterActiveTimer(0.7f, FWidgetActiveTimerDelegate::CreateSP(this, &SNiagaraStack::TriggerExpandSearchResults));
}

EActiveTimerReturnType SNiagaraStack::TriggerExpandSearchResults(double InCurrentTime, float InDeltaTime)
{
	ExpandSearchResults();
	if (bNeedsJumpToNextOccurence)
	{
		ScrollToNextMatch();
		bNeedsJumpToNextOccurence = false;
	}
	return EActiveTimerReturnType::Stop;
}

void SNiagaraStack::ExpandSearchResults()
{
	for (auto SearchResult : StackViewModel->GetCurrentSearchResults())
	{
		for (UNiagaraStackEntry* ParentalUnit : SearchResult.EntryPath)
		{
			StackTree->SetItemExpansion(ParentalUnit, true);
		}
	}
}

void SNiagaraStack::OnSearchBoxTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (StackViewModel->GetCurrentSearchText().CompareTo(NewText) != 0)
	{
		if (SearchExpandTimer.IsValid())
		{
			UnRegisterActiveTimer(SearchExpandTimer.ToSharedRef());
			ExpandSearchResults();
			SearchExpandTimer.Reset();
		}
	}
	if (bNeedsJumpToNextOccurence || CommitInfo == ETextCommit::OnEnter) // hasn't been autojumped yet or we hit enter
	{
		AddSearchScrollOffset(+1);
		bNeedsJumpToNextOccurence = false;
	}
}

void SNiagaraStack::OnSearchBoxSearch(SSearchBox::SearchDirection Direction)
{
	if (Direction == SSearchBox::Next)
	{
		ScrollToNextMatch();
	}
	else if (Direction == SSearchBox::Previous)
	{
		ScrollToPreviousMatch();
	}
}

FSlateColor SNiagaraStack::GetTextColorForItem(UNiagaraStackEntry* Item) const
{
	if (IsEntryFocusedInSearch(Item))
	{
		return FSlateColor(FLinearColor(FColor::Orange));
	}
	return FSlateColor::UseForeground();
}

FSlateColor SNiagaraStack::GetPinColor() const
{
	return CurrentPinColor;
}

TSharedRef<ITableRow> SNiagaraStack::OnGenerateRowForStackItem(UNiagaraStackEntry* Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<SNiagaraStackTableRow> Container = ConstructContainerForItem(Item);
	FRowWidgets RowWidgets = ConstructNameAndValueWidgetsForItem(Item, Container);
	Container->SetNameAndValueContent(RowWidgets.NameWidget, RowWidgets.ValueWidget);
	return Container;
}

TSharedRef<SNiagaraStackTableRow> SNiagaraStack::ConstructContainerForItem(UNiagaraStackEntry* Item)
{
	float LeftContentPadding = 4;
	float RightContentPadding = 6;
	FMargin ContentPadding(LeftContentPadding, 0, RightContentPadding, 0);
	FLinearColor ItemBackgroundColor;
	FLinearColor ItemForegroundColor = FNiagaraEditorWidgetsStyle::Get().GetColor("NiagaraEditor.Stack.ForegroundColor");
	bool bIsCategoryIconHighlighted;
	bool bShowExecutionCategoryIcon;
	switch (Item->GetStackRowStyle())
	{
	case UNiagaraStackEntry::EStackRowStyle::None:
		ItemBackgroundColor = FLinearColor::Transparent;
		bIsCategoryIconHighlighted = false;
		bShowExecutionCategoryIcon = false;
		break;
	case UNiagaraStackEntry::EStackRowStyle::GroupHeader:
		ContentPadding = FMargin(LeftContentPadding, 4, 0, 0);
		ItemBackgroundColor = FLinearColor::Transparent;
		ItemForegroundColor = FNiagaraEditorWidgetsStyle::Get().GetColor("NiagaraEditor.Stack.GroupForegroundColor");
		bIsCategoryIconHighlighted = true;
		bShowExecutionCategoryIcon = true;
		break;
	case UNiagaraStackEntry::EStackRowStyle::ItemHeader:
		ContentPadding = FMargin(LeftContentPadding, 2, 2, 2);
		ItemBackgroundColor = FNiagaraEditorWidgetsStyle::Get().GetColor("NiagaraEditor.Stack.Item.HeaderBackgroundColor");
		bIsCategoryIconHighlighted = false;
		bShowExecutionCategoryIcon = true;
		break;
	case UNiagaraStackEntry::EStackRowStyle::ItemContent:
		ContentPadding = FMargin(LeftContentPadding, 3, RightContentPadding, 3);
		ItemBackgroundColor = FNiagaraEditorWidgetsStyle::Get().GetColor("NiagaraEditor.Stack.Item.ContentBackgroundColor");
		bIsCategoryIconHighlighted = false;
		bShowExecutionCategoryIcon = false;
		break;
	case UNiagaraStackEntry::EStackRowStyle::ItemContentAdvanced:
		ContentPadding = FMargin(LeftContentPadding, 3, RightContentPadding, 3);
		ItemBackgroundColor = FNiagaraEditorWidgetsStyle::Get().GetColor("NiagaraEditor.Stack.Item.ContentAdvancedBackgroundColor");
		bIsCategoryIconHighlighted = false;
		bShowExecutionCategoryIcon = false;
		break;
	case UNiagaraStackEntry::EStackRowStyle::ItemFooter:
		ItemBackgroundColor = FNiagaraEditorWidgetsStyle::Get().GetColor("NiagaraEditor.Stack.Item.FooterBackgroundColor");
		bIsCategoryIconHighlighted = false;
		bShowExecutionCategoryIcon = false;
		break;
	case UNiagaraStackEntry::EStackRowStyle::ItemCategory:
		ContentPadding = FMargin(LeftContentPadding, 3, RightContentPadding, 3);
		ItemBackgroundColor = FNiagaraEditorWidgetsStyle::Get().GetColor("NiagaraEditor.Stack.Item.ContentBackgroundColor");
		bIsCategoryIconHighlighted = false;
		bShowExecutionCategoryIcon = false;
		break;
	case UNiagaraStackEntry::EStackRowStyle::StackIssue:
		ContentPadding = FMargin(LeftContentPadding, 3, RightContentPadding, 3);
		ItemBackgroundColor = FNiagaraEditorWidgetsStyle::Get().GetColor("NiagaraEditor.Stack.Item.IssueBackgroundColor");
		bIsCategoryIconHighlighted = false;
		bShowExecutionCategoryIcon = false;
		break;
	default:
		ItemBackgroundColor = FNiagaraEditorWidgetsStyle::Get().GetColor("NiagaraEditor.Stack.UnknownColor");
		bIsCategoryIconHighlighted = false;
		bShowExecutionCategoryIcon = false;
		break;
	}

	return SNew(SNiagaraStackTableRow, StackViewModel, Item, StackTree.ToSharedRef())
		.ContentPadding(ContentPadding)
		.ItemBackgroundColor(ItemBackgroundColor)
		.ItemForegroundColor(ItemForegroundColor)
		.IsCategoryIconHighlighted(bIsCategoryIconHighlighted)
		.ShowExecutionCategoryIcon(bShowExecutionCategoryIcon)
		.NameColumnWidth(this, &SNiagaraStack::GetNameColumnWidth)
		.OnNameColumnWidthChanged(this, &SNiagaraStack::OnNameColumnWidthChanged)
		.ValueColumnWidth(this, &SNiagaraStack::GetContentColumnWidth)
		.OnValueColumnWidthChanged(this, &SNiagaraStack::OnContentColumnWidthChanged)
		.OnDragDetected(this, &SNiagaraStack::OnRowDragDetected, Item)
		.OnCanAcceptDrop(this, &SNiagaraStack::OnRowCanAcceptDrop)
		.OnAcceptDrop(this, &SNiagaraStack::OnRowAcceptDrop);
}

void SNiagaraStack::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (StackViewModel)
	{
		StackViewModel->Tick();
	}
}


SNiagaraStack::FRowWidgets SNiagaraStack::ConstructNameAndValueWidgetsForItem(UNiagaraStackEntry* Item, TSharedRef<SNiagaraStackTableRow> Container)
{
	if (Item->IsA<UNiagaraStackSpacer>())
	{
		FMargin ContentPadding = Container->GetContentPadding();
		Container->SetContentPadding(FMargin(ContentPadding.Left, 0, ContentPadding.Right, 0));
		UNiagaraStackSpacer* SpacerItem = CastChecked<UNiagaraStackSpacer>(Item); 
		return FRowWidgets(SNew(SNiagaraStackSpacer, *SpacerItem)
			.HeightOverride(SpacerHeight * SpacerItem->GetSpacerScale()));
	}
	else if (Item->IsA<UNiagaraStackItemGroup>())
	{
		return FRowWidgets(SNew(SNiagaraStackItemGroup, *CastChecked<UNiagaraStackItemGroup>(Item), StackViewModel));
	}
	else if (Item->IsA<UNiagaraStackModuleItem>())
	{
		TSharedRef<SNiagaraStackModuleItem> ModuleItemWidget = SNew(SNiagaraStackModuleItem, *CastChecked<UNiagaraStackModuleItem>(Item), StackViewModel);
		Container->AddFillRowContextMenuHandler(SNiagaraStackTableRow::FOnFillRowContextMenu::CreateSP(ModuleItemWidget, &SNiagaraStackModuleItem::FillRowContextMenu));
		return FRowWidgets(ModuleItemWidget);
	}
	else if (Item->IsA<UNiagaraStackRendererItem>())
	{
		return FRowWidgets(SNew(SNiagaraStackRendererItem, *CastChecked<UNiagaraStackRendererItem>(Item), StackViewModel));
	}
	else if (Item->IsA<UNiagaraStackFunctionInput>())
	{
		UNiagaraStackFunctionInput* FunctionInput = CastChecked<UNiagaraStackFunctionInput>(Item);
		return FRowWidgets(
			SNew(SNiagaraStackFunctionInputName, FunctionInput, StackViewModel),
			SNew(SNiagaraStackFunctionInputValue, FunctionInput));
	}
	else if (Item->IsA<UNiagaraStackErrorItem>())
	{
		return FRowWidgets(SNew(SNiagaraStackErrorItem, CastChecked<UNiagaraStackErrorItem>(Item), StackViewModel));
	}
	else if (Item->IsA<UNiagaraStackErrorItemLongDescription>())
	{
		Container->SetOverrideNameAlignment(EHorizontalAlignment::HAlign_Fill, EVerticalAlignment::VAlign_Center);
		return FRowWidgets(SNew(STextBlock)
			.TextStyle(FNiagaraEditorWidgetsStyle::Get(), "NiagaraEditor.Stack.ParameterText")
			.ToolTipText_UObject(Item, &UNiagaraStackEntry::GetTooltipText)
			.Text_UObject(Item, &UNiagaraStackEntry::GetDisplayName)
			.ColorAndOpacity(this, &SNiagaraStack::GetTextColorForItem, Item)
			.HighlightText_UObject(StackViewModel, &UNiagaraStackViewModel::GetCurrentSearchText)
			.AutoWrapText(true));
	}
	else if (Item->IsA<UNiagaraStackErrorItemFix>())
	{
		return FRowWidgets(SNew(SNiagaraStackErrorItemFix, CastChecked<UNiagaraStackErrorItemFix>(Item), StackViewModel));
	}
	else if (Item->IsA<UNiagaraStackAdvancedExpander>())
	{
		UNiagaraStackAdvancedExpander* ItemExpander = CastChecked<UNiagaraStackAdvancedExpander>(Item);
		return FRowWidgets(SNew(SNiagaraStackItemExpander, *ItemExpander));
	}
	else if (Item->IsA<UNiagaraStackEmitterPropertiesItem>())
	{
		UNiagaraStackEmitterPropertiesItem* PropertiesItem = CastChecked<UNiagaraStackEmitterPropertiesItem>(Item);
		return FRowWidgets(SNew(SNiagaraStackEmitterPropertiesItem, *PropertiesItem, StackViewModel));
	}
	else if (Item->IsA<UNiagaraStackEventHandlerPropertiesItem>())
	{
		UNiagaraStackEventHandlerPropertiesItem* PropertiesItem = CastChecked<UNiagaraStackEventHandlerPropertiesItem>(Item);
		return FRowWidgets(SNew(SNiagaraStackEventHandlerPropertiesItem, *PropertiesItem, StackViewModel));
	}
	else if (Item->IsA<UNiagaraStackParameterStoreEntry>())
	{
		UNiagaraStackParameterStoreEntry* StackEntry = CastChecked<UNiagaraStackParameterStoreEntry>(Item);
		return FRowWidgets(
			SNew(SNiagaraStackParameterStoreEntryName, StackEntry, StackViewModel),
			SNew(SNiagaraStackParameterStoreEntryValue, StackEntry));
	}
	else if (Item->IsA<UNiagaraStackInputCategory>())
	{
		return FRowWidgets(SNew(STextBlock)
			.TextStyle(FNiagaraEditorWidgetsStyle::Get(), "NiagaraEditor.Stack.CategoryText")
			.ToolTipText_UObject(Item, &UNiagaraStackEntry::GetTooltipText)
			.Text_UObject(Item, &UNiagaraStackEntry::GetDisplayName)
			.ColorAndOpacity(this, &SNiagaraStack::GetTextColorForItem, Item)
			.HighlightText_UObject(StackViewModel, &UNiagaraStackViewModel::GetCurrentSearchText),
			SNullWidget::NullWidget);
	}
	else if (Item->IsA<UNiagaraStackModuleItemOutput>())
	{
		UNiagaraStackModuleItemOutput* ModuleItemOutput = CastChecked<UNiagaraStackModuleItemOutput>(Item);
		return FRowWidgets(
			SNew(STextBlock)
			.TextStyle(FNiagaraEditorWidgetsStyle::Get(), "NiagaraEditor.Stack.DefaultText")
			.ToolTipText_UObject(Item, &UNiagaraStackEntry::GetTooltipText)
			.Text_UObject(Item, &UNiagaraStackEntry::GetDisplayName)
			.ColorAndOpacity(this, &SNiagaraStack::GetTextColorForItem, Item)
			.HighlightText_UObject(StackViewModel, &UNiagaraStackViewModel::GetCurrentSearchText),
			SNew(STextBlock)
			.TextStyle(FNiagaraEditorWidgetsStyle::Get(), "NiagaraEditor.Stack.ParameterText")
			.Text_UObject(ModuleItemOutput, &UNiagaraStackModuleItemOutput::GetOutputParameterHandleText)
			.ColorAndOpacity(this, &SNiagaraStack::GetTextColorForItem, Item)
			.HighlightText_UObject(StackViewModel, &UNiagaraStackViewModel::GetCurrentSearchText));
	}
	else if (Item->IsA<UNiagaraStackFunctionInputCollection>() ||
		Item->IsA<UNiagaraStackModuleItemOutputCollection>())
	{
		return FRowWidgets(
			SNew(STextBlock)
			.TextStyle(FNiagaraEditorWidgetsStyle::Get(), "NiagaraEditor.Stack.DefaultText")
			.ToolTipText_UObject(Item, &UNiagaraStackEntry::GetTooltipText)
			.Text_UObject(Item, &UNiagaraStackEntry::GetDisplayName)
			.ColorAndOpacity(this, &SNiagaraStack::GetTextColorForItem, Item)
			.HighlightText_UObject(StackViewModel, &UNiagaraStackViewModel::GetCurrentSearchText),
			SNullWidget::NullWidget);
	}
	else if (Item->IsA<UNiagaraStackPropertyRow>())
	{
		UNiagaraStackPropertyRow* PropertyRow = CastChecked<UNiagaraStackPropertyRow>(Item);
		FNodeWidgets PropertyRowWidgets = PropertyRow->GetDetailTreeNode()->CreateNodeWidgets();
		if (PropertyRowWidgets.WholeRowWidget.IsValid())
		{
			Container->SetOverrideNameWidth(PropertyRowWidgets.WholeRowWidgetLayoutData.MinWidth, PropertyRowWidgets.WholeRowWidgetLayoutData.MaxWidth);
			Container->SetOverrideNameAlignment(PropertyRowWidgets.WholeRowWidgetLayoutData.HorizontalAlignment, PropertyRowWidgets.WholeRowWidgetLayoutData.VerticalAlignment);
			return FRowWidgets(PropertyRowWidgets.WholeRowWidget.ToSharedRef());
		}
		else
		{
			Container->SetOverrideNameWidth(PropertyRowWidgets.NameWidgetLayoutData.MinWidth, PropertyRowWidgets.NameWidgetLayoutData.MaxWidth);
			Container->SetOverrideNameAlignment(PropertyRowWidgets.NameWidgetLayoutData.HorizontalAlignment, PropertyRowWidgets.NameWidgetLayoutData.VerticalAlignment);
			Container->SetOverrideValueWidth(PropertyRowWidgets.ValueWidgetLayoutData.MinWidth, PropertyRowWidgets.ValueWidgetLayoutData.MaxWidth);
			Container->SetOverrideValueAlignment(PropertyRowWidgets.ValueWidgetLayoutData.HorizontalAlignment, PropertyRowWidgets.ValueWidgetLayoutData.VerticalAlignment);
			return FRowWidgets(PropertyRowWidgets.NameWidget.ToSharedRef(), PropertyRowWidgets.ValueWidget.ToSharedRef());
		}
	}
	else if (Item->IsA<UNiagaraStackItem>())
	{
		return FRowWidgets(SNew(STextBlock)
			.TextStyle(FNiagaraEditorWidgetsStyle::Get(), "NiagaraEditor.Stack.ItemText")
			.ToolTipText_UObject(Item, &UNiagaraStackEntry::GetTooltipText)
			.Text_UObject(Item, &UNiagaraStackEntry::GetDisplayName)
			.ColorAndOpacity(this, &SNiagaraStack::GetTextColorForItem, Item)
			.HighlightText_UObject(StackViewModel, &UNiagaraStackViewModel::GetCurrentSearchText));
	}
	else
	{
		return FRowWidgets(SNew(STextBlock)
			.TextStyle(FNiagaraEditorWidgetsStyle::Get(), "NiagaraEditor.Stack.DefaultText")
			.ToolTipText_UObject(Item, &UNiagaraStackEntry::GetTooltipText)
			.Text_UObject(Item, &UNiagaraStackEntry::GetDisplayName)
			.ColorAndOpacity(this, &SNiagaraStack::GetTextColorForItem, Item)
			.HighlightText_UObject(StackViewModel, &UNiagaraStackViewModel::GetCurrentSearchText));
	}
}

void SNiagaraStack::OnGetChildren(UNiagaraStackEntry* Item, TArray<UNiagaraStackEntry*>& Children)
{
	Item->GetFilteredChildren(Children);
}

void SNiagaraStack::StackTreeScrolled(double ScrollValue)
{
	StackViewModel->SetLastScrollPosition(ScrollValue);
}

float SNiagaraStack::GetNameColumnWidth() const
{
	return NameColumnWidth;
}

float SNiagaraStack::GetContentColumnWidth() const
{
	return ContentColumnWidth;
}

void SNiagaraStack::OnNameColumnWidthChanged(float Width)
{
	NameColumnWidth = Width;
}

void SNiagaraStack::OnContentColumnWidthChanged(float Width)
{
	ContentColumnWidth = Width;
}

void SNiagaraStack::StackStructureChanged()
{
	PrimeTreeExpansion();
	StackTree->RequestTreeRefresh();
}

FText SNiagaraStack::GetSourceEmitterNameText() const 
{
	return StackViewModel->GetEmitterHandleViewModel()->GetSourceNameText();
}

FText SNiagaraStack::GetEmitterNameToolTip() const
{
	if (CanOpenSourceEmitter())
	{
		// We are looking at this Emitter in a System Asset and it has a valid parent Emitter
		TSharedPtr<FNiagaraEmitterHandleViewModel> ThisViewModel = StackViewModel->GetEmitterHandleViewModel();
		return FText::Format(LOCTEXT("EmitterNameAndPath", "{0}\nParent: {1}"), ThisViewModel->GetNameText(), ThisViewModel->GetSourcePathNameText());
	}
	else
	{
		// We are looking at this Emitter in an Emitter Asset or we are looking at this Emitter in a System Asset and it does not have a valid parent Emitter
		return StackViewModel->GetEmitterHandleViewModel()->GetNameText();
	}
}

void SNiagaraStack::OnStackViewNameTextCommitted(const FText& InText, ETextCommit::Type CommitInfo) const
{
	StackViewModel->GetEmitterHandleViewModel()->OnNameTextComitted(InText, CommitInfo);
}

EVisibility SNiagaraStack::GetSourceEmitterNameVisibility() const
{
	return CanOpenSourceEmitter() && GetIsEmitterRenamed() ? EVisibility::Visible : EVisibility::Collapsed;
}

bool SNiagaraStack::GetIsEmitterRenamed() const
{
	const FText CurrentNameText = StackViewModel->GetEmitterHandleViewModel()->GetNameText();
	const FText SourceNameText = StackViewModel->GetEmitterHandleViewModel()->GetSourceNameText();
	return !CurrentNameText.EqualTo(SourceNameText);
}

#undef LOCTEXT_NAMESPACE
