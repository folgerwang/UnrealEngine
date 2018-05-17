// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackItemGroup.h"
#include "NiagaraCommon.h"
#include "UObject/ObjectKey.h"
#include "NiagaraStackScriptItemGroup.generated.h"

class FNiagaraScriptViewModel;
class UNiagaraStackSpacer;
class UNiagaraNodeOutput;
class FScriptItemGroupAddUtilities;
class UNiagaraStackModuleItem;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackScriptItemGroup : public UNiagaraStackItemGroup
{
	GENERATED_BODY()

public:
	void Initialize(
		FRequiredEntryData InRequiredEntryData,
		FText InDisplayName,
		FText InToolTip,
		TSharedRef<FNiagaraScriptViewModel> InScriptViewModel,
		ENiagaraScriptUsage InScriptUsage,
		FGuid InScriptUsageId = FGuid());

	ENiagaraScriptUsage GetScriptUsage() const { return ScriptUsage; }
	FGuid GetScriptUsageId() const { return ScriptUsageId; }

protected:
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

	virtual TOptional<FDropResult> ChildRequestCanDropInternal(const UNiagaraStackEntry& TargetChild, const TArray<UNiagaraStackEntry*>& DraggedEntries) override;

	virtual TOptional<FDropResult> ChildRequestDropInternal(const UNiagaraStackEntry& TargetChild, const TArray<UNiagaraStackEntry*>& DraggedEntries) override;

private:
	void ItemAdded();

	void ChildModifiedGroupItems();

protected:
	TWeakPtr<FNiagaraScriptViewModel> ScriptViewModel;
	void RefreshIssues(TArray<FStackIssue>& NewIssues);

private:
	TSharedPtr<FScriptItemGroupAddUtilities> AddUtilities;

	ENiagaraScriptUsage ScriptUsage;

	FGuid ScriptUsageId;
	bool bIsValidForOutput;

	TMap<FObjectKey, UNiagaraStackModuleItem*> StackSpacerToModuleItemMap;
};
