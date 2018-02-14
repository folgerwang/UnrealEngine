// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackParameterStoreGroup.h"
#include "ViewModels/Stack/NiagaraStackModuleItem.h"
#include "ViewModels/Stack/NiagaraStackAddScriptModuleItem.h"
#include "NiagaraScriptViewModel.h"
#include "NiagaraScriptGraphViewModel.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeFunctionCall.h"
#include "EdGraphSchema_Niagara.h"
#include "ViewModels/Stack/NiagaraStackErrorItem.h"
#include "Internationalization/Internationalization.h"
#include "ViewModels/Stack/NiagaraStackParameterStoreEntry.h"


#define LOCTEXT_NAMESPACE "UNiagaraStackParameterStoreGroup"


void UNiagaraStackParameterStoreGroup::Initialize(
	TSharedRef<FNiagaraSystemViewModel> InSystemViewModel,
	TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel,
	UNiagaraStackEditorData& InStackEditorData,
	TSharedRef<FNiagaraScriptViewModel> InScriptViewModel,
	UObject* InOwner,
	FNiagaraParameterStore* InParameterStore)
{
	Super::Initialize(InSystemViewModel, InEmitterViewModel, InStackEditorData);
	Owner = InOwner;
	ParameterStore = InParameterStore;
}

FText UNiagaraStackParameterStoreGroup::GetDisplayName() const
{
	return DisplayName;
}

void UNiagaraStackParameterStoreGroup::SetDisplayName(FText InDisplayName)
{
	DisplayName = InDisplayName;
}

void UNiagaraStackParameterStoreGroup::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren)
{
	if (ParameterStore != nullptr)
	{
		TArray<FNiagaraVariable> Variables;
		ParameterStore->GetParameters(Variables);

		for (FNiagaraVariable& Var : Variables)
		{
			UNiagaraStackParameterStoreEntry* ValueObjectEntry = NewObject<UNiagaraStackParameterStoreEntry>(this);
			ValueObjectEntry->Initialize(GetSystemViewModel(), GetEmitterViewModel(), GetStackEditorData(), Owner, ParameterStore, Var.GetName().ToString(), Var.GetType());
			ValueObjectEntry->SetItemIndentLevel(1);
			NewChildren.Add(ValueObjectEntry);
		}
	}
}

void UNiagaraStackParameterStoreGroup::ItemAdded()
{
	RefreshChildren();
}

void UNiagaraStackParameterStoreGroup::ChildModifiedGroupItems()
{
	RefreshChildren();
}


#undef LOCTEXT_NAMESPACE
