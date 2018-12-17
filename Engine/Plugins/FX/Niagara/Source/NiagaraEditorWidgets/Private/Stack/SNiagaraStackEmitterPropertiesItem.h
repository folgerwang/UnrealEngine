// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SNiagaraStackEntryWidget.h"
#include "Layout/Visibility.h"

class UNiagaraStackEmitterPropertiesItem;
class UNiagaraStackViewModel;

class SNiagaraStackEmitterPropertiesItem: public SNiagaraStackEntryWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraStackEmitterPropertiesItem) { }
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UNiagaraStackEmitterPropertiesItem& InEmitterPropertiesItem, UNiagaraStackViewModel* InStackViewModel);

private:
	EVisibility GetResetToBaseButtonVisibility() const;

	FReply ResetToBaseButtonClicked();

private:
	UNiagaraStackEmitterPropertiesItem* EmitterPropertiesItem;
};