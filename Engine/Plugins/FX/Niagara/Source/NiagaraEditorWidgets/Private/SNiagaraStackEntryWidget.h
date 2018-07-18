// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class UNiagaraStackViewModel;
class UNiagaraStackEntry;

class SNiagaraStackEntryWidget : public SCompoundWidget
{
public:
	FSlateColor GetTextColorForSearch() const;
	FReply ExpandEntry();
	
protected:
	bool IsCurrentSearchMatch() const;
	
protected:
	UNiagaraStackViewModel * StackViewModel;
	
	UNiagaraStackEntry * StackEntryItem;
};