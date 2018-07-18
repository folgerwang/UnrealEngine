// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SNiagaraStackEntryWidget.h"
#include "ViewModels/Stack/NiagaraStackViewModel.h"
#include "NiagaraEditorWidgetsStyle.h"

#define	LOCTEXT_NAMESPACE "SNiagaraStackEntryWidget"

FSlateColor SNiagaraStackEntryWidget::GetTextColorForSearch() const
{
	if (IsCurrentSearchMatch())
	{
		return FNiagaraEditorWidgetsStyle::Get().GetColor("NiagaraEditor.Stack.SearchHighlightColor");
	} 
	
	return FSlateColor::UseForeground();
}

bool SNiagaraStackEntryWidget::IsCurrentSearchMatch() const
{
	auto FocusedEntry = StackViewModel->GetCurrentFocusedEntry();
	return StackEntryItem != nullptr && FocusedEntry == StackEntryItem;
}

FReply SNiagaraStackEntryWidget::ExpandEntry()
{
	StackEntryItem->SetIsExpanded(true);
	StackEntryItem->OnStructureChanged().Broadcast();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
