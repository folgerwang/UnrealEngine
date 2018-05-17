// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Stack/SNiagaraStackFunctionInputName.h"
#include "NiagaraEditorWidgetsStyle.h"
#include "NiagaraEditorStyle.h"
#include "ViewModels/Stack/NiagaraStackViewModel.h"
#include "ViewModels/Stack/NiagaraStackFunctionInput.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SCheckBox.h"

void SNiagaraStackFunctionInputName::Construct(const FArguments& InArgs, UNiagaraStackFunctionInput* InFunctionInput, UNiagaraStackViewModel* InStackViewModel)
{
	FunctionInput = InFunctionInput;
	StackViewModel = InStackViewModel;
	StackEntryItem = InFunctionInput;

	ChildSlot
	[
		SNew(SHorizontalBox)
		// Edit condition checkbox
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, 3, 0)
		[
			SNew(SCheckBox)
			.Visibility(this, &SNiagaraStackFunctionInputName::GetEditConditionCheckBoxVisibility)
			.IsChecked(this, &SNiagaraStackFunctionInputName::GetEditConditionCheckState)
			.OnCheckStateChanged(this, &SNiagaraStackFunctionInputName::OnEditConditionCheckStateChanged)
		]
		// Name Label
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		[
			SAssignNew(NameTextBlock, SInlineEditableTextBlock)
			.Style(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterInlineEditableText")
			.Text_UObject(FunctionInput, &UNiagaraStackEntry::GetDisplayName)
			.IsReadOnly(this, &SNiagaraStackFunctionInputName::GetIsNameReadOnly)
			.IsEnabled(this, &SNiagaraStackFunctionInputName::GetIsEnabled)
			.IsSelected(this, &SNiagaraStackFunctionInputName::GetIsNameWidgetSelected)
			.OnTextCommitted(this, &SNiagaraStackFunctionInputName::OnNameTextCommitted)
			.HighlightText_UObject(InStackViewModel, &UNiagaraStackViewModel::GetCurrentSearchText)
			.ColorAndOpacity(this, &SNiagaraStackFunctionInputName::GetTextColorForSearch)
			.ToolTipText_UObject(FunctionInput, &UNiagaraStackFunctionInput::GetTooltipText, UNiagaraStackFunctionInput::EValueMode::Local)
		]
	];
}

void SNiagaraStackFunctionInputName::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (FunctionInput->GetIsRenamePending() && NameTextBlock.IsValid())
	{
		NameTextBlock->EnterEditingMode();
		FunctionInput->SetIsRenamePending(false);
	}
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

EVisibility SNiagaraStackFunctionInputName::GetEditConditionCheckBoxVisibility() const
{
	return FunctionInput->GetHasEditCondition() && FunctionInput->GetShowEditConditionInline() ? EVisibility::Visible : EVisibility::Collapsed;
}

ECheckBoxState SNiagaraStackFunctionInputName::GetEditConditionCheckState() const
{
	return FunctionInput->GetHasEditCondition() && FunctionInput->GetEditConditionEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SNiagaraStackFunctionInputName::OnEditConditionCheckStateChanged(ECheckBoxState InCheckState)
{
	FunctionInput->SetEditConditionEnabled(InCheckState == ECheckBoxState::Checked);
}

bool SNiagaraStackFunctionInputName::GetIsNameReadOnly() const
{
	return FunctionInput->CanRenameInput() == false;
}

bool SNiagaraStackFunctionInputName::GetIsNameWidgetSelected() const
{
	return true;
}

bool SNiagaraStackFunctionInputName::GetIsEnabled() const
{
	return FunctionInput->GetHasEditCondition() == false || FunctionInput->GetEditConditionEnabled();
}

void SNiagaraStackFunctionInputName::OnNameTextCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	FunctionInput->RenameInput(*InText.ToString());
}
