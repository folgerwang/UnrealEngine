// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorUndoClient.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STreeView.h"
#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "Widgets/Input/SSearchBox.h"


class UNiagaraStackViewModel;
class SNiagaraStackTableRow;
class SSearchBox;
class FReply;

class SNiagaraStack : public SCompoundWidget, public FEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS(SNiagaraStack)
	{}

	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UNiagaraStackViewModel* InStackViewModel);

private:
	struct FRowWidgets
	{
		FRowWidgets(TSharedRef<SWidget> InNameWidget, TSharedRef<SWidget> InValueWidget)
			: NameWidget(InNameWidget)
			, ValueWidget(InValueWidget)
		{
		}

		FRowWidgets(TSharedRef<SWidget> InWholeRowWidget)
			: NameWidget(InWholeRowWidget)
		{
		}

		TSharedRef<SWidget> NameWidget;
		TSharedPtr<SWidget> ValueWidget;
	};

	void PrimeTreeExpansion();

	TSharedRef<ITableRow> OnGenerateRowForStackItem(UNiagaraStackEntry* Item, const TSharedRef<STableViewBase>& OwnerTable);

	TSharedRef<SNiagaraStackTableRow> ConstructContainerForItem(UNiagaraStackEntry* Item);

	FRowWidgets ConstructNameAndValueWidgetsForItem(UNiagaraStackEntry* Item, TSharedRef<SNiagaraStackTableRow> Container);
	
	void OnGetChildren(UNiagaraStackEntry* Item, TArray<UNiagaraStackEntry*>& Children);

	void StackTreeScrolled(double ScrollValue);

	float GetNameColumnWidth() const;
	float GetContentColumnWidth() const;

	void OnNameColumnWidthChanged(float Width);
	void OnContentColumnWidthChanged(float Width);
	void StackStructureChanged();

	EVisibility GetVisibilityForItem(UNiagaraStackEntry* Item) const;

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void ConstructHeaderWidget();
	FSlateColor GetPinColor() const;
	FReply PinButtonPressed();

	// ~stack search stuff
	void OnSearchTextChanged(const FText& SearchText);
	FReply ScrollToNextMatch();
	FReply ScrollToPreviousMatch();
	TOptional<SSearchBox::FSearchResultData> GetSearchResultData() const;
	bool GetIsSearching() const;
	void OnSearchBoxTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);	
	void OnSearchBoxSearch(SSearchBox::SearchDirection Direction);
	FSlateColor GetTextColorForItem(UNiagaraStackEntry* Item) const;
	void AddSearchScrollOffset(int NumberOfSteps);
	void OnStackSearchComplete();
	EActiveTimerReturnType TriggerExpandSearchResults(double InCurrentTime, float InDeltaTime);
	void ExpandSearchResults();
	bool IsEntryFocusedInSearch(UNiagaraStackEntry* Entry) const;
	
	// Inline menu commands
	void OpenSourceEmitter();
	bool CanOpenSourceEmitter() const;
	void SetEmitterEnabled(bool bIsEnabled);
	bool CheckEmitterEnabledStatus(bool bIsEnabled);
	void ShowEmitterInContentBrowser();
	void NavigateTo(UNiagaraStackEntry* Item);
	void CollapseAll();

	TSharedRef<SWidget> GetViewOptionsMenu() const;

	// Drag/Drop
	FReply OnRowDragDetected(const FGeometry& InGeometry, const FPointerEvent& InPointerEvent, UNiagaraStackEntry* InStackEntry);

	TOptional<EItemDropZone> OnRowCanAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, UNiagaraStackEntry* InTargetEntry);

	FReply OnRowAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, UNiagaraStackEntry* InTargetEntry);

private:
	UNiagaraStackViewModel* StackViewModel;

	TSharedPtr<STreeView<UNiagaraStackEntry*>> StackTree;

	float NameColumnWidth;

	float ContentColumnWidth;
	
	TSharedPtr<SWidget> HeaderWidget;

	FLinearColor PinIsPinnedColor;
	
	FLinearColor PinIsUnpinnedColor;

	FLinearColor CurrentPinColor;
	
	// ~ search stuff
	TSharedPtr<SSearchBox> SearchBox;
	TSharedPtr<FActiveTimerHandle> SearchExpandTimer;
	static const FText OccurencesFormat;
	bool bNeedsJumpToNextOccurence;
};
