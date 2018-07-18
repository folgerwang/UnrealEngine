// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Stack/SNiagaraStackEventHandlerPropertiesItem.h"
#include "NiagaraEditorWidgetsStyle.h"
#include "NiagaraEditorStyle.h"
#include "EditorStyleSet.h"
#include "ViewModels/Stack/NiagaraStackEventScriptItemGroup.h"
#include "ViewModels/Stack/NiagaraStackViewModel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "NiagaraStackEventHandlerPropertiesItem"

void SNiagaraStackEventHandlerPropertiesItem::Construct(const FArguments& InArgs, UNiagaraStackEventHandlerPropertiesItem& InEventHandlerPropertiesItem, UNiagaraStackViewModel* InStackViewModel)
{
	EventHandlerPropertiesItem = &InEventHandlerPropertiesItem;
	StackEntryItem = EventHandlerPropertiesItem;
	StackViewModel = InStackViewModel;

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0)
		[
			SNew(STextBlock)
			.TextStyle(FNiagaraEditorWidgetsStyle::Get(), "NiagaraEditor.Stack.ItemText")
			.ToolTipText_UObject(EventHandlerPropertiesItem, &UNiagaraStackEntry::GetTooltipText)
			.Text_UObject(EventHandlerPropertiesItem, &UNiagaraStackEntry::GetDisplayName)
			.HighlightText_UObject(InStackViewModel, &UNiagaraStackViewModel::GetCurrentSearchText)
			.ColorAndOpacity(this, &SNiagaraStackEventHandlerPropertiesItem::GetTextColorForSearch)
		]
		// Reset to base Button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(3, 0, 0, 0)
		[
			SNew(SButton)
			.IsFocusable(false)
			.ToolTipText(LOCTEXT("ResetEventHandlerPropertiesToBaseToolTip", "Reset the event handler properties to the state defined by the parent emitter"))
			.ButtonStyle(FEditorStyle::Get(), "NoBorder")
			.ContentPadding(0)
			.Visibility(this, &SNiagaraStackEventHandlerPropertiesItem::GetResetToBaseButtonVisibility)
			.OnClicked(this, &SNiagaraStackEventHandlerPropertiesItem::ResetToBaseButtonClicked)
			.Content()
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
				.ColorAndOpacity(FSlateColor(FLinearColor::Green))
			]
		]
	];
}

EVisibility SNiagaraStackEventHandlerPropertiesItem::GetResetToBaseButtonVisibility() const
{
	return EventHandlerPropertiesItem->CanResetToBase() ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SNiagaraStackEventHandlerPropertiesItem::ResetToBaseButtonClicked()
{
	EventHandlerPropertiesItem->ResetToBase();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE