// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/SExpanderArrow.h"
#include "ViewModels/Stack/NiagaraStackFunctionInput.h"
#include "SGraphActionMenu.h"

class UNiagaraStackFunctionInput;
class UNiagaraScript;
class SNiagaraParameterEditor;
class SBox;
class IStructureDetailsView;
class SComboButton;

class SNiagaraStackFunctionInputValue: public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnColumnWidthChanged, float)
public:
	SLATE_BEGIN_ARGS(SNiagaraStackFunctionInputValue) { }
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UNiagaraStackFunctionInput* InFunctionInput);

private:

	bool GetInputEnabled() const;

	EVisibility GetValueWidgetVisibility(UNiagaraStackFunctionInput::EValueMode ValidMode) const;

	TSharedRef<SWidget> ConstructLocalValueStructWidget();

	void OnInputValueChanged();

	void ParameterBeginValueChange();

	void ParameterEndValueChange();

	void ParameterValueChanged(TWeakPtr<SNiagaraParameterEditor> ParameterEditor);

	void ParameterPropertyValueChanged(const FPropertyChangedEvent& PropertyChangedEvent);

	FText GetLinkedValueHandleText() const;

	FText GetDataValueText() const;

	FText GetDynamicValueText() const;

	FText GetExpressionValueText() const;

	void OnExpressionTextCommitted(const FText& Name, ETextCommit::Type CommitInfo);

	FText GetInvalidValueText() const;

	FText GetInvalidValueToolTipText() const;

	FReply DynamicInputTextDoubleClicked();

	class SNiagaraFunctionInputActionMenuExpander: public SExpanderArrow
	{
		SLATE_BEGIN_ARGS(SNiagaraFunctionInputActionMenuExpander) {}
			SLATE_ATTRIBUTE(float, IndentAmount)
		SLATE_END_ARGS()

	public:
		void Construct(const FArguments& InArgs, const FCustomExpanderData& ActionMenuData)
		{
			OwnerRowPtr = ActionMenuData.TableRow;
			IndentAmount = InArgs._IndentAmount;
			if (!ActionMenuData.RowAction.IsValid())
			{
				SExpanderArrow::FArguments SuperArgs;
				SuperArgs._IndentAmount = InArgs._IndentAmount;

				SExpanderArrow::Construct(SuperArgs, ActionMenuData.TableRow);
			}
			else
			{
				ChildSlot
				.Padding(TAttribute<FMargin>(this, &SNiagaraFunctionInputActionMenuExpander::GetCustomIndentPadding))
				[
					SNew(SBox)
				];
			}
		}

	private:
		FMargin GetCustomIndentPadding() const
		{
			return SExpanderArrow::GetExpanderPadding();
		}
	};

	static TSharedRef<SExpanderArrow> CreateCustomNiagaraFunctionInputActionExpander(const FCustomExpanderData& ActionMenuData);

	TSharedRef<SWidget> OnGetAvailableHandleMenu();

	void OnActionSelected(const TArray<TSharedPtr<FEdGraphSchemaAction>>& SelectedActions, ESelectInfo::Type InSelectionType);

	void CollectAllActions(FGraphActionListBuilderBase& OutAllActions);

	void SetToLocalValue();

	void DynamicInputScriptSelected(UNiagaraScript* DynamicInputScript);

	void CustomExpressionSelected();

	void ParameterHandleSelected(FNiagaraParameterHandle Handle);

	EVisibility GetResetButtonVisibility() const;

	FReply ResetButtonPressed() const;

	EVisibility GetResetToBaseButtonVisibility() const;

	FReply ResetToBaseButtonPressed() const;

	EVisibility GetInputIconVisibility() const;

	FText GetInputIconText() const;

	FText GetInputIconToolTip() const;

	FSlateColor GetInputIconColor() const;

	FReply OnFunctionInputDrop(TSharedPtr<FDragDropOperation> DragDropOperation);

	bool OnFunctionInputAllowDrop(TSharedPtr<FDragDropOperation> DragDropOperation);

private:
	UNiagaraStackFunctionInput* FunctionInput;

	TSharedPtr<FStructOnScope> DisplayedLocalValueStruct;

	TSharedPtr<SBox> LocalValueStructContainer;
	TSharedPtr<SNiagaraParameterEditor> LocalValueStructParameterEditor;
	TSharedPtr<IStructureDetailsView> LocalValueStructDetailsView;
	TSharedPtr<SComboButton> SetFunctionInputButton;
};
