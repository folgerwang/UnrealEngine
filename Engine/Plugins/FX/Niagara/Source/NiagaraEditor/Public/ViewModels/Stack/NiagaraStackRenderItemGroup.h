// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackItemGroup.h"
#include "NiagaraStackRenderItemGroup.generated.h"

class UNiagaraRendererProperties;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackRenderItemGroup : public UNiagaraStackItemGroup
{
	GENERATED_BODY()

public:
	void Initialize(FRequiredEntryData InRequiredEntryData);

protected:
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

private:
	void ItemAdded(UNiagaraRendererProperties* AddedRenderer);
	void ChildModifiedGroupItems();

private:
	TSharedPtr<INiagaraStackItemGroupAddUtilities> AddUtilities;
};
