// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Input/Reply.h"

class FNiagaraScriptViewModel;
class INiagaraParameterCollectionViewModel;

class FNiagaraScriptDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance(TWeakPtr<FNiagaraScriptViewModel> ScriptViewModel);
	
	FNiagaraScriptDetails(TSharedPtr<FNiagaraScriptViewModel> InScriptViewModel);

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder);

	FReply OnRefreshMetadata();

private:
	TSharedPtr<FNiagaraScriptViewModel> ScriptViewModel;

};

