// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "EdGraph/EdGraphSchema.h"
#include "ViewModels/Stack/NiagaraStackAddModuleItem.h"
#include "NiagaraStackAddEventScriptItem.generated.h"

class FNiagaraEmitterViewModel;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackAddEventScriptItem : public UNiagaraStackAddModuleItem
{
	GENERATED_BODY()

public:
	UNiagaraStackAddEventScriptItem();

	//~ UNiagaraStackEntry interface
	virtual FText GetDisplayName() const override;

	virtual ENiagaraScriptUsage GetOutputUsage() const override;

protected:
	//~ UNiagaraStackAddModuleItem interface
	virtual UNiagaraNodeOutput* GetOrCreateOutputNode() override;
	virtual FText GetInsertTransactionText() const override;

private:
	void AddEventScript();

};
