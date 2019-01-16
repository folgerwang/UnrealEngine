// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "NiagaraStackModuleItemLinkedInputCollection.generated.h"

class UNiagaraNodeFunctionCall;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackModuleItemLinkedInputCollection : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	UNiagaraStackModuleItemLinkedInputCollection();

	void Initialize(FRequiredEntryData InRequiredEntryData, UNiagaraNodeFunctionCall& InFunctionCallNode);

	//~ UNiagaraStackEntry interface
	virtual FText GetDisplayName() const override;
	virtual bool IsExpandedByDefault() const override;
	virtual bool GetIsEnabled() const override;
	virtual EStackRowStyle GetStackRowStyle() const override;
	virtual bool GetShouldShowInStack() const override;

protected:
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

private:
	UNiagaraNodeFunctionCall* FunctionCallNode;
};
