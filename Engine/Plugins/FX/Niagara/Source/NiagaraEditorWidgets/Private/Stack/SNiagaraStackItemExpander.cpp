// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Stack/SNiagaraStackItemExpander.h"
#include "ViewModels/Stack/NiagaraStackAdvancedExpander.h"
#include "EditorStyleSet.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "NiagaraStackItemExpander"

void SNiagaraStackItemExpander::Construct(const FArguments& InArgs, UNiagaraStackAdvancedExpander& InItemExpander)
{
	ShowAdvancedExpander = &InItemExpander;
	ExpandedToolTipText = LOCTEXT("HideAdvancedToolTip", "Hide Advanced");
	CollapsedToolTipText = LOCTEXT("ShowAdvancedToolTip", "Show Advanced");

	ChildSlot
	[
		SNew(SButton)
		.ButtonStyle(FEditorStyle::Get(), "NoBorder")
		.HAlign(HAlign_Center)
		.ContentPadding(2)
		.ToolTipText(this, &SNiagaraStackItemExpander::GetToolTipText)
		.OnClicked(this, &SNiagaraStackItemExpander::ExpandButtonClicked)
		.IsFocusable(false)
		.Content()
		[
			SNew(SImage)
			.Image(this, &SNiagaraStackItemExpander::GetButtonBrush)
		]
	];
}

const FSlateBrush* SNiagaraStackItemExpander::GetButtonBrush() const
{
	if (IsHovered())
	{
		return ShowAdvancedExpander->GetShowAdvanced()
			? FEditorStyle::GetBrush("DetailsView.PulldownArrow.Up.Hovered")
			: FEditorStyle::GetBrush("DetailsView.PulldownArrow.Down.Hovered");
	}
	else
	{
		return ShowAdvancedExpander->GetShowAdvanced()
			? FEditorStyle::GetBrush("DetailsView.PulldownArrow.Up")
			: FEditorStyle::GetBrush("DetailsView.PulldownArrow.Down");
	}
}

FText SNiagaraStackItemExpander::GetToolTipText() const
{
	return ShowAdvancedExpander->GetShowAdvanced() ? ExpandedToolTipText : CollapsedToolTipText;
}

FReply SNiagaraStackItemExpander::ExpandButtonClicked()
{
	ShowAdvancedExpander->ToggleShowAdvanced();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE