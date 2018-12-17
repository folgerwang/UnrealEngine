// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SNiagaraStackEntryWidget.h"

class UNiagaraStackParameterStoreEntry;
class UNiagaraStackViewModel;
class SInlineEditableTextBlock;

class SNiagaraStackParameterStoreEntryName: public SNiagaraStackEntryWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnColumnWidthChanged, float);

public:
	SLATE_BEGIN_ARGS(SNiagaraStackParameterStoreEntryName) { }
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UNiagaraStackParameterStoreEntry* InStackEntry, UNiagaraStackViewModel* InStackViewModel);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	bool GetIsNameReadOnly() const;

	bool GetIsNameWidgetSelected() const;

	bool VerifyNameTextChanged(const FText& NewText, FText& OutErrorMessage);

	void OnNameTextCommitted(const FText& InText, ETextCommit::Type InCommitType);

private:
	UNiagaraStackParameterStoreEntry* StackEntry;

	TSharedPtr<SInlineEditableTextBlock> NameTextBlock;
};