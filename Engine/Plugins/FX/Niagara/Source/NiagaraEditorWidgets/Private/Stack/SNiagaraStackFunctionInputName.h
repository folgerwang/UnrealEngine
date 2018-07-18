// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SNiagaraStackEntryWidget.h"
#include "Layout/Visibility.h"
#include "Styling/SlateTypes.h"

class UNiagaraStackFunctionInput;
class SInlineEditableTextBlock;

class SNiagaraStackFunctionInputName: public SNiagaraStackEntryWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnColumnWidthChanged, float);

public:
	SLATE_BEGIN_ARGS(SNiagaraStackFunctionInputName) { }
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UNiagaraStackFunctionInput* InFunctionInput, UNiagaraStackViewModel* InStackViewModel);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	EVisibility GetEditConditionCheckBoxVisibility() const;

	ECheckBoxState GetEditConditionCheckState() const;

	void OnEditConditionCheckStateChanged(ECheckBoxState InCheckState);

	bool GetIsNameReadOnly() const;

	bool GetIsNameWidgetSelected() const;

	bool GetIsEnabled() const;

	void OnNameTextCommitted(const FText& InText, ETextCommit::Type InCommitType);

private:
	UNiagaraStackFunctionInput* FunctionInput;

	TSharedPtr<SInlineEditableTextBlock> NameTextBlock;
};