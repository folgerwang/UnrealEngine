// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackItemGroup.h"
#include "NiagaraEmitter.h"
#include "NiagaraStackEventHandlerGroup.generated.h"

class FNiagaraEmitterViewModel;

UCLASS()
/** Container for one or more NiagaraStackEventScriptItemGroups, allowing multiple event handlers per script.*/
class NIAGARAEDITOR_API UNiagaraStackEventHandlerGroup : public UNiagaraStackItemGroup
{
	GENERATED_BODY()

public:
	DECLARE_DELEGATE(FOnItemAdded);

public:
	void Initialize(FRequiredEntryData InRequiredEntryData);

	void SetOnItemAdded(FOnItemAdded InOnItemAdded);

private:
	void ItemAddedFromUtilties(FNiagaraEventScriptProperties AddedEventHandler);

private:
	TSharedPtr<INiagaraStackItemGroupAddUtilities> AddUtilities;
	FOnItemAdded ItemAddedDelegate;
};
