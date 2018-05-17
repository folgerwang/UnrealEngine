// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackParameterStoreGroup.h"
#include "ViewModels/Stack/NiagaraStackModuleItem.h"
#include "NiagaraScriptViewModel.h"
#include "NiagaraScriptGraphViewModel.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeFunctionCall.h"
#include "EdGraphSchema_Niagara.h"
#include "ViewModels/Stack/NiagaraStackErrorItem.h"
#include "ViewModels/Stack/NiagaraStackParameterStoreEntry.h"
#include "Internationalization/Internationalization.h"
#include "ViewModels/Stack/NiagaraStackSpacer.h"
#include "NiagaraStackEditorData.h"
#include "ScopedTransaction.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"


#define LOCTEXT_NAMESPACE "UNiagaraStackParameterStoreGroup"

class FParameterStoreGroupAddAction : public INiagaraStackItemGroupAddAction
{
public:
	FParameterStoreGroupAddAction(FNiagaraVariable InNewParameterVariable)
		: NewParameterVariable(InNewParameterVariable)
	{
	}

	FNiagaraVariable GetNewParameterVariable() const
	{
		return NewParameterVariable;
	}

	virtual FText GetCategory() const override
	{
		return LOCTEXT("CreateNewParameterCategory", "Parameter Types");
	}

	virtual FText GetDisplayName() const override
	{
		return NewParameterVariable.GetType().GetNameText();
	}

	virtual FText GetDescription() const override
	{
		return FText::Format(LOCTEXT("AddParameterActionDescriptionFormat", "Create a new {0} parameter."), GetDisplayName());
	}

private:
	FNiagaraVariable NewParameterVariable;
};

class FParameterStoreGroupAddUtiliites : public FNiagaraStackItemGroupAddUtilities
{
public:
	FParameterStoreGroupAddUtiliites(UObject& InParameterStoreOwner, FNiagaraParameterStore& InParameterStore, UNiagaraStackEditorData& InStackEditorData)
		: FNiagaraStackItemGroupAddUtilities(LOCTEXT("ScriptGroupAddItemName", "Parameter"), EAddMode::AddFromAction, true, FOnItemAdded())
		, ParameterStoreOwner(InParameterStoreOwner)
		, ParameterStore(InParameterStore)
		, StackEditorData(InStackEditorData)
	{
	}

	virtual void AddItemDirectly() override { unimplemented(); }

	virtual void GenerateAddActions(TArray<TSharedRef<INiagaraStackItemGroupAddAction>>& OutAddActions) const override
	{
		TArray<FNiagaraTypeDefinition> AvailableTypes;
		FNiagaraStackGraphUtilities::GetNewParameterAvailableTypes(AvailableTypes);
		for (const FNiagaraTypeDefinition& AvailableType : AvailableTypes)
		{
			FNiagaraParameterHandle NewParameterHandle(FNiagaraParameterHandle::UserNamespace, *(TEXT("New") + AvailableType.GetName()));
			FNiagaraVariable NewParameterVariable(AvailableType, NewParameterHandle.GetParameterHandleString());
			OutAddActions.Add(MakeShared<FParameterStoreGroupAddAction>(NewParameterVariable));
		}
	}

	virtual void ExecuteAddAction(TSharedRef<INiagaraStackItemGroupAddAction> AddAction, int32 TargetIndex) override
	{
		TSharedRef<FParameterStoreGroupAddAction> ParameterAddAction = StaticCastSharedRef<FParameterStoreGroupAddAction>(AddAction);

		FScopedTransaction AddTransaction(LOCTEXT("AddParameter", "Add Parameter"));
		ParameterStoreOwner.Modify();

		FNiagaraVariable ParameterVariable = ParameterAddAction->GetNewParameterVariable();
		FNiagaraEditorUtilities::ResetVariableToDefaultValue(ParameterVariable);

		ParameterStore.AddParameter(ParameterVariable);
		StackEditorData.SetModuleInputIsRenamePending(ParameterVariable.GetName().ToString(), true);
	}

private:
	UObject& ParameterStoreOwner;
	FNiagaraParameterStore& ParameterStore;
	UNiagaraStackEditorData& StackEditorData;
};

void UNiagaraStackParameterStoreGroup::Initialize(
	FRequiredEntryData InRequiredEntryData,
	UObject* InOwner,
	FNiagaraParameterStore* InParameterStore)
{
	AddUtilities = MakeShared<FParameterStoreGroupAddUtiliites>(*InOwner, *InParameterStore, *InRequiredEntryData.StackEditorData);
	FText DisplayName = LOCTEXT("SystemExposedVariablesGroup", "System Exposed Parameters");
	FText ToolTip = LOCTEXT("SystemExposedVariablesGroupToolTip", "Displays the variables created in the User namespace. These variables are exposed to owning UComponents, blueprints, etc.");
	Super::Initialize(InRequiredEntryData, DisplayName, ToolTip, AddUtilities.Get());
	
	Owner = InOwner;
	ParameterStore = InParameterStore;
}

void UNiagaraStackParameterStoreGroup::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	if (ParameterStore != nullptr)
	{
		FName ParameterStoreItemSpacerKey = "ParameterStoreSpacer";
		UNiagaraStackSpacer* ParameterStoreItemSpacer = FindCurrentChildOfTypeByPredicate<UNiagaraStackSpacer>(CurrentChildren,
			[=](UNiagaraStackSpacer* CurrentParameterStoreItemSpacer) { return CurrentParameterStoreItemSpacer->GetSpacerKey() == ParameterStoreItemSpacerKey; });

		if (ParameterStoreItemSpacer == nullptr)
		{
			ParameterStoreItemSpacer = NewObject<UNiagaraStackSpacer>(this);
			ParameterStoreItemSpacer->Initialize(CreateDefaultChildRequiredData(), ParameterStoreItemSpacerKey, 1.4f);
		}

		NewChildren.Add(ParameterStoreItemSpacer);

		UNiagaraStackParameterStoreItem* ParameterStoreItem = FindCurrentChildOfTypeByPredicate<UNiagaraStackParameterStoreItem>(CurrentChildren,
			[=](UNiagaraStackParameterStoreItem* CurrentItem) { return true; });

		if (ParameterStoreItem == nullptr)
		{
			ParameterStoreItem = NewObject<UNiagaraStackParameterStoreItem>(this);
			ParameterStoreItem->Initialize(CreateDefaultChildRequiredData(), Owner.Get(), ParameterStore);
		}

		NewChildren.Add(ParameterStoreItem);
	}
	Super::RefreshChildrenInternal(CurrentChildren, NewChildren, NewIssues);
}

void UNiagaraStackParameterStoreItem::Initialize(
	FRequiredEntryData InRequiredEntryData,
	UObject* InOwner,
	FNiagaraParameterStore* InParameterStore)
{
	Super::Initialize(InRequiredEntryData, TEXT("ParameterStoreItem"));

	Owner = InOwner;
	ParameterStore = InParameterStore;
	ParameterStoreChangedHandle = ParameterStore->AddOnChangedHandler(
		FNiagaraParameterStore::FOnChanged::FDelegate::CreateUObject(this, &UNiagaraStackParameterStoreItem::ParameterStoreChanged));
}

FText UNiagaraStackParameterStoreItem::GetDisplayName() const
{
	return LOCTEXT("ParameterItemDisplayName", "Parameters");
}

void UNiagaraStackParameterStoreItem::BeginDestroy()
{
	if (Owner != nullptr)
	{
		ParameterStore->RemoveOnChangedHandler(ParameterStoreChangedHandle);
	}
	Super::BeginDestroy();
}

void UNiagaraStackParameterStoreItem::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	if (ParameterStore != nullptr && ParameterStore->GetNumParameters() > 0)
	{
		TArray<FNiagaraVariable> Variables;
		ParameterStore->GetParameters(Variables);

		for (FNiagaraVariable& Var : Variables)
		{
			UNiagaraStackParameterStoreEntry* ValueObjectEntry = FindCurrentChildOfTypeByPredicate<UNiagaraStackParameterStoreEntry>(CurrentChildren,
				[=](UNiagaraStackParameterStoreEntry* CurrentEntry) { return CurrentEntry->GetDisplayName().ToString() == Var.GetName().ToString(); });

			if (ValueObjectEntry == nullptr)
			{
				ValueObjectEntry = NewObject<UNiagaraStackParameterStoreEntry>(this);
				ValueObjectEntry->Initialize(CreateDefaultChildRequiredData(), Owner.Get(), ParameterStore, Var.GetName().ToString(), Var.GetType(), GetStackEditorDataKey());
			}

			NewChildren.Add(ValueObjectEntry);
		}
	}
	Super::RefreshChildrenInternal(CurrentChildren, NewChildren, NewIssues);
}

void UNiagaraStackParameterStoreItem::ParameterStoreChanged()
{
	if (IsValid())
	{
		RefreshChildren();
	}
}

#undef LOCTEXT_NAMESPACE
