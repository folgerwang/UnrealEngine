// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SNiagaraStackEntryWidget.h"
#include "Layout/Visibility.h"

class UNiagaraStackItemGroup;
class UNiagaraStackViewModel;

class SNiagaraStackItemGroup : public SNiagaraStackEntryWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraStackItemGroup) { }
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UNiagaraStackItemGroup& InGroup, UNiagaraStackViewModel* InStackViewModel);

private:
	TSharedRef<SWidget> ConstructAddButton();

	EVisibility GetDeleteButtonVisibility() const;

	EVisibility GetAddButtonVisibility() const;

	FText GetAddButtonToolTipText() const;

	FReply AddDirectlyButtonClicked();

	FReply DeleteClicked();

	TSharedRef<SWidget> GetAddMenu();

	EVisibility GetStackIssuesWarningVisibility() const;

	FText GetErrorButtonTooltipText() const;

private:
	UNiagaraStackItemGroup* Group;
	TSharedPtr<class SComboButton> AddActionButton; 
	const float TextIconSize = 16;
};
