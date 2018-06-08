// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackSpacer.h"

void UNiagaraStackSpacer::Initialize(FRequiredEntryData InRequiredEntryData, FName InSpacerKey, float InSpacerScale, EStackRowStyle InRowStyle)
{
	Super::Initialize(InRequiredEntryData, FString());
	SpacerKey = InSpacerKey;
	SpacerScale = InSpacerScale;
	RowStyle = InRowStyle;
}

FText UNiagaraStackSpacer::GetDisplayName() const
{
	return FText();
}

bool UNiagaraStackSpacer::GetCanExpand() const
{
	return false;
}

UNiagaraStackEntry::EStackRowStyle UNiagaraStackSpacer::GetStackRowStyle() const
{
	return RowStyle;
}

FName UNiagaraStackSpacer::GetSpacerKey() const
{
	return SpacerKey;
}

float UNiagaraStackSpacer::GetSpacerScale() const
{
	return SpacerScale;
}