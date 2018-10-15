// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackSpacer.h"
#include "NiagaraStackScriptItemGroup.h"
#include "NiagaraStackModuleSpacer.generated.h"


UCLASS()
class NIAGARAEDITOR_API UNiagaraStackModuleSpacer : public UNiagaraStackSpacer
{
	GENERATED_BODY()

public:

	DECLARE_DELEGATE_TwoParams(FOnStackSpacerAcceptDrop, const UNiagaraStackModuleSpacer*, const FNiagaraVariable&)

	void Initialize(FRequiredEntryData InRequiredEntryData, ENiagaraScriptUsage InScriptUsage, FName SpacerKey = NAME_None, float InSpacerScale = 1.0f, EStackRowStyle InRowStyle = UNiagaraStackEntry::EStackRowStyle::None);

	//~ DragDropHandling
	virtual FReply OnStackSpacerDrop(TSharedPtr<FDragDropOperation> DragDropOperation) override; 
	virtual bool OnStackSpacerAllowDrop(TSharedPtr<FDragDropOperation> DragDropOperation) override;

	FOnStackSpacerAcceptDrop OnStackSpacerAcceptDrop;

private:
	ENiagaraScriptUsage ItemGroupScriptUsage;
}; 
