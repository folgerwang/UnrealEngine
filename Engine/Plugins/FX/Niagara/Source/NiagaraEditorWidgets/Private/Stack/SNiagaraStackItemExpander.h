// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class UNiagaraStackAdvancedExpander;

class SNiagaraStackItemExpander: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraStackItemExpander) { }
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UNiagaraStackAdvancedExpander& InItemExpander);

private:
	const struct FSlateBrush* GetButtonBrush() const;

	FText GetToolTipText() const;

	FReply ExpandButtonClicked();

private:
	UNiagaraStackAdvancedExpander* ShowAdvancedExpander;
	FText ExpandedToolTipText;
	FText CollapsedToolTipText;
};