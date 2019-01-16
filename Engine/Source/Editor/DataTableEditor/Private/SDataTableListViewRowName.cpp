// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SDataTableListViewRowName.h"

#include "AssetData.h"
#include "DataTableEditor.h"
#include "DataTableRowUtlis.h"
#include "EditorStyleSet.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

void SDataTableListViewRowName::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	RowDataPtr = InArgs._RowDataPtr;
	DataTableEditor = InArgs._DataTableEditor;
	STableRow<FDataTableEditorRowListViewDataPtr>::Construct(
		typename STableRow<FDataTableEditorRowListViewDataPtr>::FArguments()
		.Style(FEditorStyle::Get(), "DataTableEditor.NameListViewRow")
		.Content()
		[
			SNew(SBox)
			.Padding(FMargin(4, 2, 4, 2))
			[
				SNew(SBox)
				.HeightOverride(RowDataPtr->DesiredRowHeight)
				[
					SNew(STextBlock)
					.ColorAndOpacity(DataTableEditor.Get(), &FDataTableEditor::GetRowTextColor, RowDataPtr->RowId)
					.Text(RowDataPtr->DisplayName)
					.HighlightText(DataTableEditor.Get(), &FDataTableEditor::GetFilterText)
				]
			]
		],
		InOwnerTableView
	);
}

FReply SDataTableListViewRowName::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && RowDataPtr.IsValid() && FEditorDelegates::OnOpenReferenceViewer.IsBound())
	{
		TSharedRef<SWidget> MenuWidget = FDataTableRowUtils::MakeRowActionsMenu(FExecuteAction::CreateSP(this, &SDataTableListViewRowName::OnSearchForReferences));
		
		FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();

		FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, MenuWidget, MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect::ContextMenu);

		return FReply::Handled();
	}

	return STableRow::OnMouseButtonUp(MyGeometry, MouseEvent);
}

void SDataTableListViewRowName::OnSearchForReferences()
{
	if (DataTableEditor.IsValid() && RowDataPtr.IsValid())
	{
		UDataTable* SourceDataTable = const_cast<UDataTable*>(DataTableEditor->GetDataTable());
		
		TArray<FAssetIdentifier> AssetIdentifiers;
		AssetIdentifiers.Add(FAssetIdentifier(SourceDataTable, RowDataPtr->RowId));

		FEditorDelegates::OnOpenReferenceViewer.Broadcast(AssetIdentifiers);
	}
}
