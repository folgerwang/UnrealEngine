// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackItem.h"
#include "NiagaraStackPropertyRow.generated.h"

class IDetailTreeNode;
class UNiagaraNode;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackPropertyRow : public UNiagaraStackItemContent
{
	GENERATED_BODY()
		
public:
	void Initialize(FRequiredEntryData InRequiredEntryData, TSharedRef<IDetailTreeNode> InDetailTreeNode, FString InOwnerStackItemEditorDataKey, FString InOwnerStackEditorDataKey, UNiagaraNode* InOwningNiagaraNode);

	TSharedRef<IDetailTreeNode> GetDetailTreeNode() const;

	virtual bool GetIsEnabled() const override;

protected:
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

	virtual void GetAdditionalSearchItemsInternal(TArray<FStackSearchItem>& SearchItems) const override;

private:
	TSharedPtr<IDetailTreeNode> DetailTreeNode;
	UNiagaraNode* OwningNiagaraNode;
	EStackRowStyle RowStyle;
};