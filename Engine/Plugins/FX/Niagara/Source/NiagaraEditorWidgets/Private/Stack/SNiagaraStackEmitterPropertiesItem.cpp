// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Stack/SNiagaraStackEmitterPropertiesItem.h"
#include "NiagaraEditorWidgetsStyle.h"
#include "NiagaraEditorStyle.h"
#include "EditorStyleSet.h"
#include "ViewModels/Stack/NiagaraStackEmitterSpawnScriptItemGroup.h"
#include "ViewModels/Stack/NiagaraStackViewModel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "NiagaraStackEmitterPropertiesItem"

void SNiagaraStackEmitterPropertiesItem::Construct(const FArguments& InArgs, UNiagaraStackEmitterPropertiesItem& InEmitterPropertiesItem, UNiagaraStackViewModel* InStackViewModel)
{
	EmitterPropertiesItem = &InEmitterPropertiesItem;
	StackEntryItem = EmitterPropertiesItem;
	StackViewModel = InStackViewModel;

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0)
		[
			SNew(STextBlock)
			.TextStyle(FNiagaraEditorWidgetsStyle::Get(), "NiagaraEditor.Stack.ItemText")
			.ToolTipText_UObject(EmitterPropertiesItem, &UNiagaraStackEntry::GetTooltipText)
			.Text_UObject(EmitterPropertiesItem, &UNiagaraStackEntry::GetDisplayName)
			.HighlightText_UObject(InStackViewModel, &UNiagaraStackViewModel::GetCurrentSearchText)
			.ColorAndOpacity(this, &SNiagaraStackEmitterPropertiesItem::GetTextColorForSearch)
		]
		// Reset to base Button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(3, 0, 0, 0)
		[
			SNew(SButton)
			.IsFocusable(false)
			.ToolTipText(LOCTEXT("ResetEmitterPropertiesToBaseToolTip", "Reset the emitter properties to the state defined by the parent emitter"))
			.ButtonStyle(FEditorStyle::Get(), "NoBorder")
			.ContentPadding(0)
			.Visibility(this, &SNiagaraStackEmitterPropertiesItem::GetResetToBaseButtonVisibility)
			.OnClicked(this, &SNiagaraStackEmitterPropertiesItem::ResetToBaseButtonClicked)
			.Content()
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
				.ColorAndOpacity(FSlateColor(FLinearColor::Green))
			]
		]
	];
}

EVisibility SNiagaraStackEmitterPropertiesItem::GetResetToBaseButtonVisibility() const
{
	return EmitterPropertiesItem->CanResetToBase() ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SNiagaraStackEmitterPropertiesItem::ResetToBaseButtonClicked()
{
	EmitterPropertiesItem->ResetToBase();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE