// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Input/Reply.h"
#include "Widgets/Views/STableRow.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "DataTableEditorUtils.h"

class FDataTableEditor;

/**
 * A widget to represent a row in a Data Table Editor widget. This widget allows us to do things like right-click
 * and take actions on a particular row of a Data Table.
 */
class SDataTableListViewRowName : public STableRow<FDataTableEditorRowListViewDataPtr>
{
public:

	SLATE_BEGIN_ARGS(SDataTableListViewRowName){}
		/** The owning object. This allows us access to the actual data table being edited as well as some other API functions. */
		SLATE_ARGUMENT(TSharedPtr<FDataTableEditor>, DataTableEditor)
		/** The row we're working with to allow us to get naming information. */
		SLATE_ARGUMENT(FDataTableEditorRowListViewDataPtr, RowDataPtr)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
private:

	void OnSearchForReferences();
	FDataTableEditorRowListViewDataPtr RowDataPtr;
	TWeakPtr<FDataTableEditor> DataTableEditor;
};