// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Stack/SNiagaraStackRendererItem.h"
#include "NiagaraEditorWidgetsStyle.h"
#include "NiagaraEditorStyle.h"
#include "EditorStyleSet.h"
#include "NiagaraRendererProperties.h"
#include "ViewModels/Stack/NiagaraStackRendererItem.h"
#include "ViewModels/Stack/NiagaraStackViewModel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "SNiagaraStackErrorButton.h"

#define LOCTEXT_NAMESPACE "NiagaraStackRendererItem"

void SNiagaraStackRendererItem::Construct(const FArguments& InArgs, UNiagaraStackRendererItem& InRendererItem, UNiagaraStackViewModel* InStackViewModel)
{
	RendererItem = &InRendererItem;
	StackEntryItem = RendererItem;
	StackViewModel = InStackViewModel;

	ChildSlot
	[
		// Renderer icon
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(5, 0, 0, 0)
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image(FSlateIconFinder::FindIconBrushForClass(RendererItem->GetRendererProperties()->GetClass()))
		]
		// Display name
		+ SHorizontalBox::Slot()
		.Padding(5, 0, 0, 0)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.TextStyle(FNiagaraEditorWidgetsStyle::Get(), "NiagaraEditor.Stack.ItemText")
			.ToolTipText_UObject(RendererItem, &UNiagaraStackEntry::GetTooltipText)
			.Text_UObject(RendererItem, &UNiagaraStackEntry::GetDisplayName)
			.HighlightText_UObject(InStackViewModel, &UNiagaraStackViewModel::GetCurrentSearchText)
			.ColorAndOpacity(this, &SNiagaraStackRendererItem::GetTextColorForSearch)
		]
		// Stack issues icon
		+ SHorizontalBox::Slot()
		.Padding(2, 0, 0, 0)
		.AutoWidth()
		[
			SNew(SNiagaraStackErrorButton)
			.IssueSeverity_UObject(RendererItem, &UNiagaraStackRendererItem::GetHighestStackIssueSeverity)
			.ErrorTooltip(this, &SNiagaraStackRendererItem::GetErrorButtonTooltipText)
			.Visibility(this, &SNiagaraStackRendererItem::GetStackIssuesWarningVisibility)
			.OnButtonClicked(this, &SNiagaraStackRendererItem::ExpandEntry)
		]
		// Delete button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
			.IsFocusable(false)
			.ForegroundColor(FNiagaraEditorWidgetsStyle::Get().GetColor("NiagaraEditor.Stack.ForegroundColor"))
			.ToolTipText(LOCTEXT("DeleteRendererToolTip", "Delete this Renderer"))
			.Visibility(this, &SNiagaraStackRendererItem::GetDeleteButtonVisibility)
			.OnClicked(this, &SNiagaraStackRendererItem::DeleteClicked)
			.Content()
			[
				SNew(STextBlock)
				.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.10"))
				.Text(FText::FromString(FString(TEXT("\xf1f8"))))
			]
		]
		// Reset to base Button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(3, 0, 0, 0)
		[
			SNew(SButton)
			.IsFocusable(false)
			.ToolTipText(LOCTEXT("ResetRendererToBaseToolTip", "Reset this renderer to the state defined by the parent emitter"))
			.ButtonStyle(FEditorStyle::Get(), "NoBorder")
			.ContentPadding(0)
			.Visibility(this, &SNiagaraStackRendererItem::GetResetToBaseButtonVisibility)
			.OnClicked(this, &SNiagaraStackRendererItem::ResetToBaseButtonClicked)
			.Content()
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
				.ColorAndOpacity(FSlateColor(FLinearColor::Green))
			]
		]
		// Enabled checkbox
		+ SHorizontalBox::Slot()
		.Padding(2, 0, 0, 0)
		.AutoWidth()
		[
			SNew(SCheckBox)
			.IsChecked(this, &SNiagaraStackRendererItem::CheckEnabledStatus)
			.OnCheckStateChanged(this, &SNiagaraStackRendererItem::OnCheckStateChanged)
		]
	];
}

EVisibility SNiagaraStackRendererItem::GetDeleteButtonVisibility() const
{
	return RendererItem->CanDelete() ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SNiagaraStackRendererItem::DeleteClicked()
{
	RendererItem->Delete();
	return FReply::Handled();
}

EVisibility SNiagaraStackRendererItem::GetResetToBaseButtonVisibility() const
{
	if (RendererItem->CanHaveBase())
	{
		return RendererItem->CanResetToBase() ? EVisibility::Visible : EVisibility::Hidden;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

FReply SNiagaraStackRendererItem::ResetToBaseButtonClicked()
{
	RendererItem->ResetToBase();
	return FReply::Handled();
}

void SNiagaraStackRendererItem::OnCheckStateChanged(ECheckBoxState InCheckState)
{
	RendererItem->SetIsEnabled(InCheckState == ECheckBoxState::Checked);
}

ECheckBoxState SNiagaraStackRendererItem::CheckEnabledStatus() const
{
	return RendererItem->GetIsEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

EVisibility SNiagaraStackRendererItem::GetStackIssuesWarningVisibility() const
{
	return  RendererItem->GetRecursiveStackIssuesCount() > 0? EVisibility::Visible : EVisibility::Collapsed;
}

FText SNiagaraStackRendererItem::GetErrorButtonTooltipText() const
{
	return FText::Format(LOCTEXT("ModuleIssuesTooltip", "This renderer has {0} issues, click to expand."), RendererItem->GetRecursiveStackIssuesCount());
}

#undef LOCTEXT_NAMESPACE