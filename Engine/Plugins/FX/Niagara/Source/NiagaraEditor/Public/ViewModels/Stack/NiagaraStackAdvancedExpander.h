// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "NiagaraStackAdvancedExpander.generated.h"

class UNiagaraNodeFunctionCall;
class UNiagaraStackFunctionInputCollection;
class UNiagaraStackModuleItemOutputCollection;
class UNiagaraNode;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackAdvancedExpander : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	DECLARE_DELEGATE(FOnToggleShowAdvanced);

public:
	void Initialize(
		FRequiredEntryData InRequiredEntryData,
		FString InOwnerStackItemEditorDataKey,
		UNiagaraNode* OwningNiagaraNode = nullptr);

	virtual bool GetCanExpand() const override;
	virtual bool GetIsEnabled() const override;
	virtual EStackRowStyle GetStackRowStyle() const override;

	void SetOnToggleShowAdvanced(FOnToggleShowAdvanced OnToggleShowAdvanced);

	bool GetShowAdvanced() const;

	void ToggleShowAdvanced();

private:
	FString OwnerStackItemEditorDataKey;

	UNiagaraNode* OwningNiagaraNode;

	FOnToggleShowAdvanced ToggleShowAdvancedDelegate;
};
