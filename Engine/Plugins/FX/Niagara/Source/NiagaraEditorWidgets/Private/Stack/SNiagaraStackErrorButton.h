// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "Widgets/Input/SButton.h"

class UNiagaraStackViewModel;
class UNiagaraStackEntry;

class SNiagaraStackErrorButton : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraStackErrorButton) {}
		SLATE_ATTRIBUTE(EStackIssueSeverity, IssueSeverity)
		SLATE_ATTRIBUTE(FText, ErrorTooltip)
		SLATE_EVENT(FOnClicked, OnButtonClicked)
	SLATE_END_ARGS()
	virtual void Construct(const FArguments& InArgs);
	
private:
	TAttribute<EStackIssueSeverity> IssueSeverity;
	TAttribute<FText> ErrorTooltip;
	FOnClicked OnButtonClicked;
};