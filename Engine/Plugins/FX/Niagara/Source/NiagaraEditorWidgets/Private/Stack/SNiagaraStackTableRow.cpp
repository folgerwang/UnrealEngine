// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Stack/SNiagaraStackTableRow.h"
#include "NiagaraEditorWidgetsStyle.h"
#include "EditorStyleSet.h"
#include "ViewModels/Stack/NiagaraStackViewModel.h"
#include "ViewModels/Stack/NiagaraStackItem.h"
#include "ViewModels/Stack/NiagaraStackItemGroup.h"
#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "NiagaraEditorWidgetsUtilities.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "Toolkits/AssetEditorManager.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "NiagaraStackTableRow"

const float IndentSize = 12;

void SNiagaraStackTableRow::Construct(const FArguments& InArgs, UNiagaraStackViewModel* InStackViewModel, UNiagaraStackEntry* InStackEntry, const TSharedRef<STreeView<UNiagaraStackEntry*>>& InOwnerTree)
{
	ContentPadding = InArgs._ContentPadding;
	bIsCategoryIconHighlighted = InArgs._IsCategoryIconHighlighted;
	bShowExecutionCategoryIcon = InArgs._ShowExecutionCategoryIcon;
	NameColumnWidth = InArgs._NameColumnWidth;
	ValueColumnWidth = InArgs._ValueColumnWidth;
	NameColumnWidthChanged = InArgs._OnNameColumnWidthChanged;
	ValueColumnWidthChanged = InArgs._OnValueColumnWidthChanged;
	StackViewModel = InStackViewModel;
	StackEntry = InStackEntry;
	OwnerTree = InOwnerTree;

	ExpandedImage = FCoreStyle::Get().GetBrush("TreeArrow_Expanded");
	CollapsedImage = FCoreStyle::Get().GetBrush("TreeArrow_Collapsed");

	InactiveItemBackgroundColor = InArgs._ItemBackgroundColor;
	ActiveItemBackgroundColor = InactiveItemBackgroundColor + FLinearColor(.05f, .05f, .05f, 0.0f);
	ForegroundColor = InArgs._ItemForegroundColor;

	ExecutionCategoryToolTipText = InStackEntry->GetExecutionSubcategoryName() != NAME_None
		? FText::Format(LOCTEXT("ExecutionCategoryToolTipFormat", "{0} - {1}"), FText::FromName(InStackEntry->GetExecutionCategoryName()), FText::FromName(InStackEntry->GetExecutionSubcategoryName()))
		: FText::FromName(InStackEntry->GetExecutionCategoryName());

	ConstructInternal(
		STableRow<UNiagaraStackEntry*>::FArguments()
			.OnDragDetected(InArgs._OnDragDetected)
			.OnCanAcceptDrop(InArgs._OnCanAcceptDrop)
			.OnAcceptDrop(InArgs._OnAcceptDrop)
		, OwnerTree.ToSharedRef());
}

void SNiagaraStackTableRow::SetOverrideNameWidth(TOptional<float> InMinWidth, TOptional<float> InMaxWidth)
{
	NameMinWidth = InMinWidth;
	NameMaxWidth = InMaxWidth;
}

void SNiagaraStackTableRow::SetOverrideNameAlignment(EHorizontalAlignment InHAlign, EVerticalAlignment InVAlign)
{
	NameHorizontalAlignment = InHAlign;
	NameVerticalAlignment = InVAlign;
}

void SNiagaraStackTableRow::SetOverrideValueWidth(TOptional<float> InMinWidth, TOptional<float> InMaxWidth)
{
	ValueMinWidth = InMinWidth;
	ValueMaxWidth = InMaxWidth;
}

void SNiagaraStackTableRow::SetOverrideValueAlignment(EHorizontalAlignment InHAlign, EVerticalAlignment InVAlign)
{
	ValueHorizontalAlignment = InHAlign;
	ValueVerticalAlignment = InVAlign;
}

FMargin SNiagaraStackTableRow::GetContentPadding() const
{
	return ContentPadding;
}

void SNiagaraStackTableRow::SetContentPadding(FMargin InContentPadding)
{
	ContentPadding = InContentPadding;
}

void SNiagaraStackTableRow::SetNameAndValueContent(TSharedRef<SWidget> InNameWidget, TSharedPtr<SWidget> InValueWidget)
{
	FSlateColor IconColor = FNiagaraEditorWidgetsStyle::Get().GetColor(FNiagaraStackEditorWidgetsUtilities::GetColorNameForExecutionCategory(StackEntry->GetExecutionCategoryName()));
	if (bIsCategoryIconHighlighted)
	{
		IconColor = FNiagaraEditorWidgetsStyle::Get().GetColor(FNiagaraStackEditorWidgetsUtilities::GetIconColorNameForExecutionCategory(StackEntry->GetExecutionCategoryName()));
	}
	TSharedRef<SHorizontalBox> NameContent = SNew(SHorizontalBox)
	.Clipping(EWidgetClipping::OnDemand)
	// Indent
	+ SHorizontalBox::Slot()
	.AutoWidth()
	[
		SNew(SBox)
		.WidthOverride(this, &SNiagaraStackTableRow::GetIndentSize)
	]
	// Expand button
	+ SHorizontalBox::Slot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	.Padding(0, 0, 1, 0)
	[
		SNew(SBox)
		.WidthOverride(14)
		[
			SNew(SButton)
			.ButtonStyle(FCoreStyle::Get(), "NoBorder")
			.Visibility(this, &SNiagaraStackTableRow::GetExpanderVisibility)
			.OnClicked(this, &SNiagaraStackTableRow::ExpandButtonClicked)
			.ForegroundColor(FSlateColor::UseForeground())
			.ContentPadding(2)
			.HAlign(HAlign_Center)
			[
				SNew(SImage)
				.Image(this, &SNiagaraStackTableRow::GetExpandButtonImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		]
	]
	// Execution sub-category icon
	+ SHorizontalBox::Slot()
	.AutoWidth()
	.Padding(FMargin(1, 1, 4, 1))
	.VAlign(EVerticalAlignment::VAlign_Center)
	.HAlign(EHorizontalAlignment::HAlign_Center)
	[
		SNew(SBox)
		.WidthOverride(FNiagaraEditorWidgetsStyle::Get().GetFloat("NiagaraEditor.Stack.IconHighlightedSize"))
		.HAlign(EHorizontalAlignment::HAlign_Center)
		.VAlign(EVerticalAlignment::VAlign_Center)
		.ToolTipText(ExecutionCategoryToolTipText)
		.Visibility(this, &SNiagaraStackTableRow::GetExecutionCategoryIconVisibility)
		[
			SNew(SImage)
			.Visibility(this, &SNiagaraStackTableRow::GetExecutionCategoryIconVisibility)
			.Image(FNiagaraEditorWidgetsStyle::Get().GetBrush(FNiagaraStackEditorWidgetsUtilities::GetIconNameForExecutionSubcategory(StackEntry->GetExecutionSubcategoryName(), bIsCategoryIconHighlighted)))
			.ColorAndOpacity(IconColor)
		]
	]
	// Name content
	+ SHorizontalBox::Slot()
	[
		InNameWidget
	];

	TSharedPtr<SWidget> ChildContent;

	if (InValueWidget.IsValid())
	{
		ChildContent = SNew(SSplitter)
		.Style(FNiagaraEditorWidgetsStyle::Get(), "NiagaraEditor.Stack.Splitter")
		.PhysicalSplitterHandleSize(1.0f)
		.HitDetectionSplitterHandleSize(5.0f)

		+ SSplitter::Slot()
		.Value(NameColumnWidth)
		.OnSlotResized(SSplitter::FOnSlotResized::CreateSP(this, &SNiagaraStackTableRow::OnNameColumnWidthChanged))
		[
			SNew(SBox)
			.Padding(FMargin(ContentPadding.Left, ContentPadding.Top, 5, ContentPadding.Bottom))
			.HAlign(NameHorizontalAlignment)
			.VAlign(NameVerticalAlignment)
			.MinDesiredWidth(NameMinWidth.IsSet() ? NameMinWidth.GetValue() : FOptionalSize())
			.MaxDesiredWidth(NameMaxWidth.IsSet() ? NameMaxWidth.GetValue() : FOptionalSize())
			[
				NameContent
			]
		]

		// Value
		+ SSplitter::Slot()
		.Value(ValueColumnWidth)
		.OnSlotResized(SSplitter::FOnSlotResized::CreateSP(this, &SNiagaraStackTableRow::OnValueColumnWidthChanged))
		[
			SNew(SBox)
			.Padding(FMargin(4, ContentPadding.Top, ContentPadding.Right, ContentPadding.Bottom))
			.HAlign(ValueHorizontalAlignment)
			.VAlign(ValueVerticalAlignment)
			.MinDesiredWidth(ValueMinWidth.IsSet() ? ValueMinWidth.GetValue() : FOptionalSize())
			.MaxDesiredWidth(ValueMaxWidth.IsSet() ? ValueMaxWidth.GetValue() : FOptionalSize())
			[
				InValueWidget.ToSharedRef()
			]
		];
	}
	else
	{
		ChildContent = SNew(SBox)
		.Padding(ContentPadding)
		.HAlign(NameHorizontalAlignment)
		.VAlign(NameVerticalAlignment)
		.MinDesiredWidth(NameMinWidth.IsSet() ? NameMinWidth.GetValue() : FOptionalSize())
		.MaxDesiredWidth(NameMaxWidth.IsSet() ? NameMaxWidth.GetValue() : FOptionalSize())
		[
			NameContent
		];
	}

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FNiagaraEditorWidgetsStyle::Get().GetColor(FNiagaraStackEditorWidgetsUtilities::GetColorNameForExecutionCategory(StackEntry->GetExecutionCategoryName())))
			.Visibility(this, &SNiagaraStackTableRow::GetRowVisibility)
			.Padding(FMargin(9, 0, 9, 0))
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("WhiteBrush"))
				.BorderBackgroundColor(this, &SNiagaraStackTableRow::GetItemBackgroundColor)
				.ForegroundColor(ForegroundColor)
				.Padding(0)
				[
					ChildContent.ToSharedRef()
				]
			]
		]
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(FNiagaraEditorWidgetsStyle::Get().GetBrush("NiagaraEditor.Stack.SearchResult"))
			.BorderBackgroundColor(FNiagaraEditorWidgetsStyle::Get().GetColor("NiagaraEditor.Stack.SearchHighlightColor"))
			.Visibility(this, &SNiagaraStackTableRow::GetSearchResultBorderVisibility)
			.Padding(FMargin(0))
			[
				SNullWidget::NullWidget
			]
		]
	];
}

bool SNiagaraStackTableRow::GetIsRowActive() const
{
	return IsHovered();
}

void SNiagaraStackTableRow::AddFillRowContextMenuHandler(FOnFillRowContextMenu FillRowContextMenuHandler)
{
	OnFillRowContextMenuHanders.Add(FillRowContextMenuHandler);
}

FReply SNiagaraStackTableRow::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	return FReply::Unhandled();
}

FReply SNiagaraStackTableRow::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		FMenuBuilder MenuBuilder(true, nullptr);
		MenuBuilder.BeginSection("ModuleActions", LOCTEXT("ModuleActions", "Module Actions"));
		for (FOnFillRowContextMenu& OnFillRowContextMenuHandler : OnFillRowContextMenuHanders)
		{
			OnFillRowContextMenuHandler.ExecuteIfBound(MenuBuilder);
		}
		MenuBuilder.EndSection();
		if (StackEntry->GetExternalAsset() != nullptr)
		{
			MenuBuilder.BeginSection("AssetActions", LOCTEXT("AssetActions", "Asset Actions"));
			MenuBuilder.AddMenuEntry(
				LOCTEXT("OpenAndFocusAsset", "Open and focus Asset"),
				FText::Format(LOCTEXT("OpenAndFocusAssetTooltip", "Open {0} in separate editor"), StackEntry->GetDisplayName()),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SNiagaraStackTableRow::OpenSourceAsset)));
			MenuBuilder.AddMenuEntry(
				LOCTEXT("ShowAssetInContentBrowser", "Show in Content Browser"),
				FText::Format(LOCTEXT("ShowAssetInContentBrowserToolTip", "Navigate to {0} in the Content Browser window"), StackEntry->GetDisplayName()),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SNiagaraStackTableRow::ShowAssetInContentBrowser)));
			MenuBuilder.EndSection();
		}
		TArray<UNiagaraStackEntry*> EntriesToProcess;
		TArray<UNiagaraStackEntry*> NavigationEntries;
		StackViewModel->GetPathForEntry(StackEntry, EntriesToProcess);
		for (UNiagaraStackEntry* Parent : EntriesToProcess)
		{
			UNiagaraStackItemGroup* GroupParent = Cast<UNiagaraStackItemGroup>(Parent);
			UNiagaraStackItem* ItemParent = Cast<UNiagaraStackItem>(Parent);
			if (GroupParent != nullptr)
			{
				MenuBuilder.BeginSection("StackRowNavigateTo", LOCTEXT("NavigateToSection", "Navigate to:"));
				MenuBuilder.AddMenuEntry(
					LOCTEXT("TopOfSection", "Top of section"),
					FText::Format(LOCTEXT("NavigateToFormatted", "Navigate to {0}"), Parent->GetDisplayName()),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateSP(this, &SNiagaraStackTableRow::NavigateTo, Parent)));
			}
			if (ItemParent != nullptr)
			{
				MenuBuilder.AddMenuEntry(
					LOCTEXT("TopOfModule", "Top of module"),
					FText::Format(LOCTEXT("NavigateToFormatted", "Navigate to {0}"), Parent->GetDisplayName()),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateSP(this, &SNiagaraStackTableRow::NavigateTo, Parent)));
			}
			if (GroupParent != nullptr)
			{
				MenuBuilder.EndSection();
			}
		}

		MenuBuilder.BeginSection("StackActions", LOCTEXT("StackActions", "Stack Actions"));
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ExpandAllItems", "Expand all"),
			LOCTEXT("ExpandAllItemsToolTip", "Expand all items under this header."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SNiagaraStackTableRow::ExpandChildren)));
		MenuBuilder.AddMenuEntry(
			LOCTEXT("CollapseAllItems", "Collapse all"),
			LOCTEXT("CollapseAllItemsToolTip", "Collapse all items under this header."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SNiagaraStackTableRow::CollapseChildren)));
		MenuBuilder.EndSection();

		FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
		FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, MenuBuilder.MakeWidget(), MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
		return FReply::Handled();
	}
	return STableRow<UNiagaraStackEntry*>::OnMouseButtonUp(MyGeometry, MouseEvent);
}

void SNiagaraStackTableRow::CollapseChildren()
{
	TArray<UNiagaraStackEntry*> Children;
	StackEntry->GetUnfilteredChildren(Children);
	for (UNiagaraStackEntry* Child : Children)
	{
		if (Child->GetCanExpand())
		{
			Child->SetIsExpanded(false);
		}
	}
	// Calling SetIsExpanded doesn't broadcast structure change automatically due to the expense of synchronizing
	// expanded state with the tree to prevent items being expanded on tick, so we call this manually here.
	StackEntry->OnStructureChanged().Broadcast();
}

void SNiagaraStackTableRow::ExpandChildren()
{
	TArray<UNiagaraStackEntry*> Children;
	StackEntry->GetUnfilteredChildren(Children);
	for (UNiagaraStackEntry* Child : Children)
	{
		if (Child->GetCanExpand())
		{
			Child->SetIsExpanded(true);
		}
	}
	// Calling SetIsExpanded doesn't broadcast structure change automatically due to the expense of synchronizing
	// expanded state with the tree to prevent items being expanded on tick, so we call this manually here.
	StackEntry->OnStructureChanged().Broadcast();
}

EVisibility SNiagaraStackTableRow::GetRowVisibility() const
{
	return StackEntry->GetShouldShowInStack()
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

EVisibility SNiagaraStackTableRow::GetExecutionCategoryIconVisibility() const
{
	return bShowExecutionCategoryIcon && StackEntry->GetExecutionSubcategoryName() != NAME_None
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

FOptionalSize SNiagaraStackTableRow::GetIndentSize() const
{
	return StackEntry->GetIndentLevel() * IndentSize;
}

EVisibility SNiagaraStackTableRow::GetExpanderVisibility() const
{
	if (StackEntry->GetCanExpand())
	{
		// TODO Cache this and refresh the cache when the entries structure changes.
		TArray<UNiagaraStackEntry*> Children;
		StackEntry->GetFilteredChildren(Children);
		return Children.Num() > 0
			? EVisibility::Visible
			: EVisibility::Hidden;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

FReply SNiagaraStackTableRow::ExpandButtonClicked()
{
	StackEntry->SetIsExpanded(!StackEntry->GetIsExpanded());
	// Calling SetIsExpanded doesn't broadcast structure change automatically due to the expense of synchronizing
	// expanded state with the tree to prevent items being expanded on tick, so we call this manually here.
	StackEntry->OnStructureChanged().Broadcast();
	return FReply::Handled();
}

const FSlateBrush* SNiagaraStackTableRow::GetExpandButtonImage() const
{
	return StackEntry->GetIsExpanded() ? ExpandedImage : CollapsedImage;
}

void SNiagaraStackTableRow::OnNameColumnWidthChanged(float Width)
{
	NameColumnWidthChanged.ExecuteIfBound(Width);
}

void SNiagaraStackTableRow::OnValueColumnWidthChanged(float Width)
{
	ValueColumnWidthChanged.ExecuteIfBound(Width);
}

FSlateColor SNiagaraStackTableRow::GetItemBackgroundColor() const
{
	return GetIsRowActive() ? ActiveItemBackgroundColor : InactiveItemBackgroundColor;
}

EVisibility SNiagaraStackTableRow::GetSearchResultBorderVisibility() const
{
	return StackViewModel->GetCurrentFocusedEntry() == StackEntry
		? EVisibility::HitTestInvisible
		: EVisibility::Hidden;
}

void SNiagaraStackTableRow::NavigateTo(UNiagaraStackEntry* Item)
{
	OwnerTree->RequestNavigateToItem(Item, 0);
}

void SNiagaraStackTableRow::OpenSourceAsset()
{
	FAssetEditorManager::Get().OpenEditorForAsset(StackEntry->GetExternalAsset());
}

void SNiagaraStackTableRow::ShowAssetInContentBrowser()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	TArray<FAssetData> Assets;
	Assets.Add(FAssetData(StackEntry->GetExternalAsset()));
	ContentBrowserModule.Get().SyncBrowserToAssets(Assets);
}

#undef LOCTEXT_NAMESPACE