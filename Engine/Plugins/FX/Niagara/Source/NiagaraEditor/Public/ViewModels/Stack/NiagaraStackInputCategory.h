// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackItem.h"
#include "NiagaraTypes.h"
#include "NiagaraStackInputCategory.generated.h"

class UNiagaraStackFunctionInput;
class UNiagaraNodeFunctionCall;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackInputCategory : public UNiagaraStackItemContent
{
	GENERATED_BODY() 

public:
	void Initialize(
		FRequiredEntryData InRequiredEntryData,
		UNiagaraNodeFunctionCall& InModuleNode,
		UNiagaraNodeFunctionCall& InInputFunctionCallNode, 
		FText InCategoryName,
		FString InOwnerStackItemEditorDataKey);
	
	const FText& GetCategoryName() const;

	void ResetInputs();

	void AddInput(FName InInputParameterHandle, FNiagaraTypeDefinition InInputType);

	//~ UNiagaraStackEntry interface
	virtual FText GetDisplayName() const override;
	virtual bool GetShouldShowInStack() const override;
	virtual EStackRowStyle GetStackRowStyle() const override;
	virtual bool GetIsEnabled() const override;

	void SetShouldShowInStack(bool bInShouldShowInStack);

protected:
	//~ UNiagaraStackEntry interface
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

private:
	bool FilterForVisibleCondition(const UNiagaraStackEntry& Child) const;
	bool FilterForIsInlineEditConditionToggle(const UNiagaraStackEntry& Child) const;

private:
	struct FInputParameterHandleAndType
	{
		FName ParameterHandle;
		FNiagaraTypeDefinition Type;
	};

	UNiagaraNodeFunctionCall* ModuleNode;
	UNiagaraNodeFunctionCall* InputFunctionCallNode;
	FText CategoryName;
	TArray<FInputParameterHandleAndType> Inputs;
	bool bShouldShowInStack;
};