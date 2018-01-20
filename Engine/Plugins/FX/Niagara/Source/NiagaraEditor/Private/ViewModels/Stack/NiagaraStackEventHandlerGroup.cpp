// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackEventHandlerGroup.h"
#include "ViewModels/Stack/NiagaraStackRendererItem.h"
#include "ViewModels/Stack/NiagaraStackEventScriptItemGroup.h"
#include "NiagaraEmitterViewModel.h"
#include "NiagaraEmitter.h"
#include "ViewModels/Stack/NiagaraStackAddEventScriptItem.h"
#include "NiagaraGraph.h"
#include "NiagaraScriptSource.h"

#define LOCTEXT_NAMESPACE "NiagaraStackEventHandlerGroup"



FText UNiagaraStackEventHandlerGroup::GetDisplayName() const
{
	return DisplayName;
}

void UNiagaraStackEventHandlerGroup::SetDisplayName(FText InDisplayName)
{
	DisplayName = InDisplayName;
}

void UNiagaraStackEventHandlerGroup::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren)
{
	if (AddScriptItem == nullptr)
	{
		AddScriptItem = NewObject<UNiagaraStackAddEventScriptItem>(this);
		AddScriptItem->Initialize(GetSystemViewModel(), GetEmitterViewModel(), GetStackEditorData());
		AddScriptItem->SetOnItemAdded(UNiagaraStackAddEventScriptItem::FOnItemAdded::CreateUObject(this, &UNiagaraStackEventHandlerGroup::ChildModifiedGroupItems));
	}

	NewChildren.Add(AddScriptItem);
}

void UNiagaraStackEventHandlerGroup::SetOnItemAdded(FOnItemAdded InOnItemAdded)
{
	ItemAddedDelegate = InOnItemAdded;
}

bool UNiagaraStackEventHandlerGroup::GetShouldShowInStack() const
{
	return true;
}

void UNiagaraStackEventHandlerGroup::ChildModifiedGroupItems()
{
	ItemAddedDelegate.ExecuteIfBound();
}

#undef LOCTEXT_NAMESPACE
