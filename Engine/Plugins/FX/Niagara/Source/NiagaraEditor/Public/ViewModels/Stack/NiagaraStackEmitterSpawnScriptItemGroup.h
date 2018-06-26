// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackScriptItemGroup.h"
#include "ViewModels/Stack/NiagaraStackItem.h"
#include "NiagaraStackEmitterSpawnScriptItemGroup.generated.h"

class FNiagaraEmitterViewModel;
class FNiagaraScriptViewModel;
class UNiagaraStackObject;
class UNiagaraStackSpacer;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackEmitterPropertiesItem : public UNiagaraStackItem
{
	GENERATED_BODY()

public:
	void Initialize(FRequiredEntryData InRequiredEntryData);

	virtual FText GetDisplayName() const override;

	bool CanResetToBase() const;

	void ResetToBase();

protected:
	virtual void FinalizeInternal() override;

	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

private:
	void EmitterPropertiesChanged();

private:
	mutable TOptional<bool> bCanResetToBase;

	TWeakObjectPtr<UNiagaraEmitter> Emitter;

	UPROPERTY()
	UNiagaraStackObject* EmitterObject;
};

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackEmitterSpawnScriptItemGroup : public UNiagaraStackScriptItemGroup
{
	GENERATED_BODY()

public:
	UNiagaraStackEmitterSpawnScriptItemGroup();

protected:
	void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

private:
	UPROPERTY()
	UNiagaraStackEmitterPropertiesItem* PropertiesItem;
};
