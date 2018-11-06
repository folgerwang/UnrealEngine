// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "Input/DragAndDrop.h"
#include "Input/Reply.h"
#include "NiagaraStackSpacer.generated.h"

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackSpacer : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	void Initialize(FRequiredEntryData InRequiredEntryData, FName SpacerKey = NAME_None, float InSpacerScale = 1.0f, EStackRowStyle InRowStyle = UNiagaraStackEntry::EStackRowStyle::None);

	//~ UNiagaraStackEntry interface
	virtual FText GetDisplayName() const override;
	virtual bool GetCanExpand() const override;
	virtual EStackRowStyle GetStackRowStyle() const override;

	FName GetSpacerKey() const;

	float GetSpacerScale() const;

	//~ DragDropHandling
	virtual FReply OnStackSpacerDrop(TSharedPtr<FDragDropOperation> DragDropOperation);
	virtual bool OnStackSpacerAllowDrop(TSharedPtr<FDragDropOperation> DragDropOperation);

private:
	FName SpacerKey;
	float SpacerScale;
	EStackRowStyle RowStyle;
};