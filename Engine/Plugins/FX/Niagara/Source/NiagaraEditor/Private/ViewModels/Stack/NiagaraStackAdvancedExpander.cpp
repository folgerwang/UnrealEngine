// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackAdvancedExpander.h"
#include "NiagaraStackEditorData.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "NiagaraNode.h"

void UNiagaraStackAdvancedExpander::Initialize(
	FRequiredEntryData InRequiredEntryData,
	FString InOwnerStackItemEditorDataKey,
	UNiagaraNode* InOwningNiagaraNode)
{
	Super::Initialize(InRequiredEntryData, FString());
	OwnerStackItemEditorDataKey = InOwnerStackItemEditorDataKey;
	OwningNiagaraNode = InOwningNiagaraNode;
}

bool UNiagaraStackAdvancedExpander::GetCanExpand() const
{
	return false;
}

bool UNiagaraStackAdvancedExpander::GetIsEnabled() const
{
	return OwningNiagaraNode == nullptr || OwningNiagaraNode->GetDesiredEnabledState() == ENodeEnabledState::Enabled;
}

UNiagaraStackEntry::EStackRowStyle UNiagaraStackAdvancedExpander::GetStackRowStyle() const
{
	return UNiagaraStackEntry::EStackRowStyle::ItemFooter;
}

void UNiagaraStackAdvancedExpander::SetOnToggleShowAdvanced(FOnToggleShowAdvanced OnExpandedChanged)
{
	ToggleShowAdvancedDelegate = OnExpandedChanged;
}

bool UNiagaraStackAdvancedExpander::GetShowAdvanced() const
{
	return GetStackEditorData().GetStackItemShowAdvanced(OwnerStackItemEditorDataKey, false);
}

void UNiagaraStackAdvancedExpander::ToggleShowAdvanced()
{
	ToggleShowAdvancedDelegate.ExecuteIfBound();
}
