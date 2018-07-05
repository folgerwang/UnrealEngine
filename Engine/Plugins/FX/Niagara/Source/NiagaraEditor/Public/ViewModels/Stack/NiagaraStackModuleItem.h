// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackItem.h"
#include "NiagaraTypes.h"
#include "NiagaraStackModuleItem.generated.h"

class UNiagaraNodeFunctionCall;
class UNiagaraStackFunctionInputCollection;
class UNiagaraStackModuleItemOutputCollection;
class INiagaraStackItemGroupAddUtilities;
struct FAssetData;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackModuleItem : public UNiagaraStackItem
{
	GENERATED_BODY()

public:
	UNiagaraStackModuleItem();

	const UNiagaraNodeFunctionCall& GetModuleNode() const;

	UNiagaraNodeFunctionCall& GetModuleNode();

	void Initialize(FRequiredEntryData InRequiredEntryData, INiagaraStackItemGroupAddUtilities* GroupAddUtilities, UNiagaraNodeFunctionCall& InFunctionCallNode);

	virtual FText GetDisplayName() const override;
	virtual FText GetTooltipText() const override;

	INiagaraStackItemGroupAddUtilities* GetGroupAddUtilities();

	bool CanMoveAndDelete() const;
	bool CanRefresh() const;
	void Refresh();

	virtual bool GetIsEnabled() const override;
	void SetIsEnabled(bool bInIsEnabled);

	void Delete();

	int32 GetModuleIndex();
	
	UObject* GetExternalAsset() const override;

	virtual bool CanDrag() const override;

	/** Gets the output node of this module. */
	class UNiagaraNodeOutput* GetOutputNode() const;

	void NotifyModuleMoved();

	bool CanAddInput(FNiagaraVariable InputParameter) const;

	void AddInput(FNiagaraVariable InputParameter);

protected:
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

private:
	bool FilterOutputCollection(const UNiagaraStackEntry& Child) const;
	void RefreshIssues(TArray<FStackIssue>& NewIssues);

private:
	void RefreshIsEnabled();

private:
	UNiagaraNodeOutput* OutputNode;
	UNiagaraNodeFunctionCall* FunctionCallNode;
	bool bCanMoveAndDelete;
	bool bIsEnabled;
	bool bCanRefresh;

	UPROPERTY()
	UNiagaraStackFunctionInputCollection* InputCollection;

	UPROPERTY()
	UNiagaraStackModuleItemOutputCollection* OutputCollection;

	INiagaraStackItemGroupAddUtilities* GroupAddUtilities;
};
