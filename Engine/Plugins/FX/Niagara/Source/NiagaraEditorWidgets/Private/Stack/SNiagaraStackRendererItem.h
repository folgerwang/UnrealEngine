// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SNiagaraStackEntryWidget.h"
#include "Styling/SlateTypes.h"
#include "Layout/Visibility.h"

class UNiagaraStackRendererItem;
class UNiagaraStackViewModel; 

class SNiagaraStackRendererItem: public SNiagaraStackEntryWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraStackRendererItem) { }
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UNiagaraStackRendererItem& InRendererItem, UNiagaraStackViewModel* InStackViewModel);
	
private:
	EVisibility GetDeleteButtonVisibility() const;

	FReply DeleteClicked();

	EVisibility GetResetToBaseButtonVisibility() const;

	FReply ResetToBaseButtonClicked();

	void OnCheckStateChanged(ECheckBoxState InCheckState);

	ECheckBoxState CheckEnabledStatus() const;

	EVisibility GetStackIssuesWarningVisibility() const;

	FText GetErrorButtonTooltipText() const;

private:
	UNiagaraStackRendererItem* RendererItem;
};