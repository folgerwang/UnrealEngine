// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SNiagaraStackEntryWidget.h"
#include "Layout/Visibility.h"

class UNiagaraStackEventHandlerPropertiesItem;
class UNiagaraStackViewModel;

class SNiagaraStackEventHandlerPropertiesItem: public SNiagaraStackEntryWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraStackEventHandlerPropertiesItem) { }
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UNiagaraStackEventHandlerPropertiesItem& InEventHandlerPropertiesItem, UNiagaraStackViewModel* InStackViewModel);

private:
	EVisibility GetResetToBaseButtonVisibility() const;

	FReply ResetToBaseButtonClicked();

private:
	UNiagaraStackEventHandlerPropertiesItem* EventHandlerPropertiesItem;
};