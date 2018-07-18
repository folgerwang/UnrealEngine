// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackFunctionInputBinder.h"

class UNiagaraScript;
class UNiagaraNodeFunctionCall;
struct FNiagaraVariableMetaData;

/** Users a condition string to bind to an input on a stack function and determines if the value of that input matches a specific value. */
class FNiagaraStackFunctionInputCondition
{
public:
	void Initialize(UNiagaraScript* InScript,
		TArray<UNiagaraScript*> InDependentScripts,
		FString InOwningEmitterUniqueName,
		UNiagaraNodeFunctionCall* InFunctionCallNode);

	void Refresh(const FString* Condition);

	bool IsValid() const;

	bool GetConditionIsEnabled() const;

	bool CanSetConditionIsEnabled() const;

	void SetConditionIsEnabled(bool bInIsEnabled);

	FName GetConditionInputName() const;

	FNiagaraTypeDefinition GetConditionInputType() const;

	FNiagaraVariableMetaData* GetConditionInputMetaData() const;

private:
	UNiagaraScript* Script;

	TArray<UNiagaraScript*> DependentScripts;

	FString OwningEmitterUniqueName;

	UNiagaraNodeFunctionCall* FunctionCallNode;

	FNiagaraStackFunctionInputBinder InputBinder;

	TArray<uint8> TargetValueData;
};