// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Stack/SNiagaraStackItemGroup.h"
#include "NiagaraEditorWidgetsStyle.h"
#include "NiagaraEditorStyle.h"
#include "EditorStyleSet.h"
#include "Stack/SNiagaraStackItemGroupAddMenu.h"
#include "ViewModels/Stack/NiagaraStackItemGroup.h"
#include "ViewModels/Stack/NiagaraStackViewModel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "SNiagaraStackErrorButton.h"

#define LOCTEXT_NAMESPACE "NiagaraStackItemGroup"


void SNiagaraStackItemGroup::Construct(const FArguments& InArgs, UNiagaraStackItemGroup& InGroup, UNiagaraStackViewModel* InStackViewModel)
{
	Group = &InGroup;
	StackEntryItem = Group;
	StackViewModel = InStackViewModel;

	ChildSlot
	[
		// Name
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.TextStyle(FNiagaraEditorWidgetsStyle::Get(), "NiagaraEditor.Stack.GroupText")
			.ToolTipText_UObject(Group, &UNiagaraStackEntry::GetTooltipText)
			.Text_UObject(Group, &UNiagaraStackEntry::GetDisplayName)
			.HighlightText_UObject(InStackViewModel, &UNiagaraStackViewModel::GetCurrentSearchText)
			.ColorAndOpacity(this, &SNiagaraStackItemGroup::GetTextColorForSearch)
		]
		// Stack issues icon
		+ SHorizontalBox::Slot()
		.Padding(2, 0, 0, 0)
		.AutoWidth()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		[
			SNew(SNiagaraStackErrorButton)
			.IssueSeverity_UObject(Group, &UNiagaraStackItemGroup::GetHighestStackIssueSeverity)
			.ErrorTooltip(this, &SNiagaraStackItemGroup::GetErrorButtonTooltipText)
			.Visibility(this, &SNiagaraStackItemGroup::GetStackIssuesWarningVisibility)
			.OnButtonClicked(this, &SNiagaraStackItemGroup::ExpandEntry)
		]
		// Delete group button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Visibility(this, &SNiagaraStackItemGroup::GetDeleteButtonVisibility)
			.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
			.IsFocusable(false)
			.ForegroundColor(FNiagaraEditorWidgetsStyle::Get().GetColor("NiagaraEditor.Stack.ForegroundColor"))
			.ToolTipText(LOCTEXT("DeleteGroupToolTip", "Delete this group"))
			.OnClicked(this, &SNiagaraStackItemGroup::DeleteClicked)
			.Content()
			[
				SNew(STextBlock)
				.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.10"))
				.Text(FText::FromString(FString(TEXT("\xf1f8"))))
			]
		]
		// Add button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		[
			ConstructAddButton()
		]
	];
}

TSharedRef<SWidget> SNiagaraStackItemGroup::ConstructAddButton()
{
	AddActionButton.Reset();
	INiagaraStackItemGroupAddUtilities* AddUtilities = Group->GetAddUtilities();
	if (AddUtilities != nullptr)
	{
		if (AddUtilities->GetAddMode() == INiagaraStackItemGroupAddUtilities::AddFromAction)
		{
			return SAssignNew(AddActionButton, SComboButton)
				.Visibility(this, &SNiagaraStackItemGroup::GetAddButtonVisibility)
				.ButtonStyle(FNiagaraEditorWidgetsStyle::Get(), "NiagaraEditor.Stack.AddButton")
				.ToolTipText(this, &SNiagaraStackItemGroup::GetAddButtonToolTipText)
				.HasDownArrow(false)
				.OnGetMenuContent(this, &SNiagaraStackItemGroup::GetAddMenu)
				.ContentPadding(0)
				.MenuPlacement(MenuPlacement_BelowRightAnchor)
				.ButtonContent()
				[
					SNew(SBox)
					.WidthOverride(TextIconSize * 2)
					.HeightOverride(TextIconSize)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.TextStyle(FEditorStyle::Get(), "NormalText.Important")
						.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.10"))
						.Text(FText::FromString(FString(TEXT("\xf067"))) /*fa-plus*/)
					]
				];
		}
		else if (AddUtilities->GetAddMode() == INiagaraStackItemGroupAddUtilities::AddDirectly)
		{
			return SNew(SButton)
				.Visibility(this, &SNiagaraStackItemGroup::GetAddButtonVisibility)
				.ButtonStyle(FNiagaraEditorWidgetsStyle::Get(), "NiagaraEditor.Stack.AddButton")
				.ToolTipText(this, &SNiagaraStackItemGroup::GetAddButtonToolTipText)
				.ContentPadding(0)
				.OnClicked(this, &SNiagaraStackItemGroup::AddDirectlyButtonClicked)
				.Content()
				[
					SNew(SBox)
					.WidthOverride(TextIconSize * 2)
					.HeightOverride(TextIconSize)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.TextStyle(FEditorStyle::Get(), "NormalText.Important")
						.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.10"))
						.Text(FText::FromString(FString(TEXT("\xf067"))) /*fa-plus*/)
					]
				];
		}
	}
	return SNullWidget::NullWidget;
}

EVisibility SNiagaraStackItemGroup::GetDeleteButtonVisibility() const
{
	if (Group->CanDelete())
	{
		return EVisibility::Visible;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

EVisibility SNiagaraStackItemGroup::GetAddButtonVisibility() const
{
	if (Group->GetAddUtilities() != nullptr)
	{
		return EVisibility::Visible;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

FText SNiagaraStackItemGroup::GetAddButtonToolTipText() const
{
	if (Group->GetAddUtilities() != nullptr)
	{
		return FText::Format(LOCTEXT("AddToGroupFormat", "Add a new {0} to this group."), Group->GetAddUtilities()->GetAddItemName());
	}
	else
	{
		return FText();
	}
}

FReply SNiagaraStackItemGroup::AddDirectlyButtonClicked()
{
	Group->GetAddUtilities()->AddItemDirectly();
	return FReply::Handled();
}

FReply SNiagaraStackItemGroup::DeleteClicked()
{
	Group->Delete();
	return FReply::Handled();
}

TSharedRef<SWidget> SNiagaraStackItemGroup::GetAddMenu()
{
	TSharedRef<SNiagaraStackItemGroupAddMenu> AddMenu = SNew(SNiagaraStackItemGroupAddMenu, Group->GetAddUtilities(), INDEX_NONE);
	AddActionButton->SetMenuContentWidgetToFocus(AddMenu->GetFilterTextBox()->AsShared());
	return AddMenu;
}

EVisibility SNiagaraStackItemGroup::GetStackIssuesWarningVisibility() const
{
	return  Group->GetRecursiveStackIssuesCount() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SNiagaraStackItemGroup::GetErrorButtonTooltipText() const
{
	return FText::Format(LOCTEXT("GroupIssuesTooltip", "This group contains items that have a total of {0} issues, click to expand."), Group->GetRecursiveStackIssuesCount());
}


#undef LOCTEXT_NAMESPACE