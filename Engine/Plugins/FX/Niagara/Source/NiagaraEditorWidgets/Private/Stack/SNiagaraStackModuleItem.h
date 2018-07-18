// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SNiagaraStackEntryWidget.h"
#include "Styling/SlateTypes.h"
#include "Layout/Visibility.h"

class UNiagaraStackModuleItem;
class UNiagaraStackViewModel;

class SNiagaraStackModuleItem : public SNiagaraStackEntryWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraStackModuleItem) { }
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UNiagaraStackModuleItem& InModuleItem, UNiagaraStackViewModel* InStackViewModel);

	void SetEnabled(bool bInIsEnabled);

	bool CheckEnabledStatus(bool bIsEnabled);

	void FillRowContextMenu(class FMenuBuilder& MenuBuilder);

	//~ SWidget interface
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;

private:
	ECheckBoxState GetCheckState() const;

	void OnCheckStateChanged(ECheckBoxState InCheckState);

	EVisibility GetEditButtonVisibility() const;

	EVisibility GetRaiseActionMenuVisibility() const;

	EVisibility GetRefreshVisibility() const;

	FReply DeleteClicked();
	
	TSharedRef<SWidget> RaiseActionMenuClicked();

	bool CanRaiseActionMenu() const;

	FReply RefreshClicked();

	void InsertModuleAbove();

	void InsertModuleBelow();

	void ShowInsertModuleMenu(int32 InsertIndex);

	FReply OnModuleItemDrop(TSharedPtr<class FDragDropOperation> DragDropOperation);

	bool OnModuleItemAllowDrop(TSharedPtr<class FDragDropOperation> DragDropOperation);

	EVisibility GetStackIssuesWarningVisibility() const;

	FText GetErrorButtonTooltipText() const;

private:
	UNiagaraStackModuleItem* ModuleItem;
};