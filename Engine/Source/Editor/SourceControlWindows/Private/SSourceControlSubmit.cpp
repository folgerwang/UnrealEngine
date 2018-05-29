// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SSourceControlSubmit.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "ISourceControlProvider.h"
#include "ISourceControlModule.h"
#include "SourceControlHelpers.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SErrorText.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"


#if SOURCE_CONTROL_WITH_SLATE

#define LOCTEXT_NAMESPACE "SSourceControlSubmit"


namespace SSourceControlSubmitWidgetDefs
{
	const FName ColumnID_CheckBoxLabel("CheckBox");
	const FName ColumnID_IconLabel("Icon");
	const FName ColumnID_FileLabel("File");

	const float CheckBoxColumnWidth = 23.0f;
	const float IconColumnWidth = 21.0f;
}


FSubmitItem::FSubmitItem(const FSourceControlStateRef& InItem)
	: Item(InItem)
{
	CheckBoxState = ECheckBoxState::Checked;
	DisplayName = FText::FromString(Item->GetFilename());
}


void SSourceControlSubmitListRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	SourceControlSubmitWidgetPtr = InArgs._SourceControlSubmitWidget;
	Item = InArgs._Item;

	SMultiColumnTableRow<TSharedPtr<FSubmitItem>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}


TSharedRef<SWidget> SSourceControlSubmitListRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	// Create the widget for this item
	TSharedPtr<SSourceControlSubmitWidget> SourceControlSubmitWidget = SourceControlSubmitWidgetPtr.Pin();
	if (SourceControlSubmitWidget.IsValid())
	{
		return SourceControlSubmitWidget->GenerateWidgetForItemAndColumn(Item, ColumnName);
	}

	// Packages dialog no longer valid; return a valid, null widget.
	return SNullWidget::NullWidget;
}


void SSourceControlSubmitWidget::Construct(const FArguments& InArgs)
{
	ParentFrame = InArgs._ParentWindow.Get();
	SortByColumn = SSourceControlSubmitWidgetDefs::ColumnID_FileLabel;
	SortMode = EColumnSortMode::Ascending;

	for (const auto& Item : InArgs._Items.Get())
	{
		ListViewItems.Add(MakeShareable(new FSubmitItem(Item)));
	}

	TSharedRef<SHeaderRow> HeaderRowWidget = SNew(SHeaderRow);

	HeaderRowWidget->AddColumn(
		SHeaderRow::Column(SSourceControlSubmitWidgetDefs::ColumnID_CheckBoxLabel)
		[
			SNew(SCheckBox)
			.IsChecked(this, &SSourceControlSubmitWidget::GetToggleSelectedState)
			.OnCheckStateChanged(this, &SSourceControlSubmitWidget::OnToggleSelectedCheckBox)
		]
		.FixedWidth(SSourceControlSubmitWidgetDefs::CheckBoxColumnWidth)
	);

	HeaderRowWidget->AddColumn(
		SHeaderRow::Column(SSourceControlSubmitWidgetDefs::ColumnID_IconLabel)
		[
			SNew(SSpacer)
		]
		.SortMode(this, &SSourceControlSubmitWidget::GetColumnSortMode, SSourceControlSubmitWidgetDefs::ColumnID_IconLabel)
		.OnSort(this, &SSourceControlSubmitWidget::OnColumnSortModeChanged)
		.FixedWidth(SSourceControlSubmitWidgetDefs::IconColumnWidth)
	);

	HeaderRowWidget->AddColumn(
		SHeaderRow::Column(SSourceControlSubmitWidgetDefs::ColumnID_FileLabel)
		.DefaultLabel(LOCTEXT("FileColumnLabel", "File"))
		.SortMode(this, &SSourceControlSubmitWidget::GetColumnSortMode, SSourceControlSubmitWidgetDefs::ColumnID_FileLabel)
		.OnSort(this, &SSourceControlSubmitWidget::OnColumnSortModeChanged)
		.FillWidth(7.0f)
	);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew( STextBlock )
				.Text( NSLOCTEXT("SourceControl.SubmitPanel", "ChangeListDesc", "Changelist Description") )
			]
			+SVerticalBox::Slot()
			.FillHeight(.5f)
			.Padding(FMargin(5, 0, 5, 5))
			[
				SNew(SBox)
				.WidthOverride(520)
				[
					SAssignNew( ChangeListDescriptionTextCtrl, SMultiLineEditableTextBox )
					.SelectAllTextWhenFocused( true )
					.AutoWrapText( true )
				]
			]
			+SVerticalBox::Slot()
			.Padding(FMargin(5, 0))
			[
				SNew(SBorder)
				[
					SAssignNew(ListView, SListView<TSharedPtr<FSubmitItem>>)
					.ItemHeight(20)
					.ListItemsSource(&ListViewItems)
					.OnGenerateRow(this, &SSourceControlSubmitWidget::OnGenerateRowForList)
					.HeaderRow(HeaderRowWidget)
					.SelectionMode(ESelectionMode::None)
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(5, 5, 5, 0))
			[
				SNew( SBorder)
				.Visibility(this, &SSourceControlSubmitWidget::IsWarningPanelVisible)
				.Padding(5)
				[
					SNew( SErrorText )
					.ErrorText( NSLOCTEXT("SourceControl.SubmitPanel", "ChangeListDescWarning", "Changelist description is required to submit") )
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SWrapBox)
				.UseAllottedWidth(true)
				+SWrapBox::Slot()
				.Padding(0.0f, 0.0f, 16.0f, 0.0f)
				[
					SNew(SCheckBox)
					.OnCheckStateChanged( this, &SSourceControlSubmitWidget::OnCheckStateChanged_KeepCheckedOut)
					.IsChecked( this, &SSourceControlSubmitWidget::GetKeepCheckedOut )
					.IsEnabled( this, &SSourceControlSubmitWidget::CanCheckOut )
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("SourceControl.SubmitPanel", "KeepCheckedOut", "Keep Files Checked Out") )
					]
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(0.0f,0.0f,0.0f,5.0f)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+SUniformGridPanel::Slot(0,0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.IsEnabled(this, &SSourceControlSubmitWidget::IsOKEnabled)
					.Text( NSLOCTEXT("SourceControl.SubmitPanel", "OKButton", "Submit") )
					.OnClicked(this, &SSourceControlSubmitWidget::OKClicked)
				]
				+SUniformGridPanel::Slot(1,0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.Text( NSLOCTEXT("SourceControl.SubmitPanel", "CancelButton", "Cancel") )
					.OnClicked(this, &SSourceControlSubmitWidget::CancelClicked)
				]
			]
		]
	];

	RequestSort();

	DialogResult = ESubmitResults::SUBMIT_CANCELED;
	KeepCheckedOut = ECheckBoxState::Unchecked;

	ParentFrame.Pin()->SetWidgetToFocusOnActivate(ChangeListDescriptionTextCtrl);
}

FReply SSourceControlSubmitWidget::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
   // Pressing escape returns as if the user clicked cancel
   if ( InKeyEvent.GetKey() == EKeys::Escape )
   {
      return CancelClicked();
   }

   return FReply::Unhandled();
}

TSharedRef<SWidget> SSourceControlSubmitWidget::GenerateWidgetForItemAndColumn(TSharedPtr<FSubmitItem> Item, const FName ColumnID) const
{
	check(Item.IsValid());

	const FMargin RowPadding(3, 0, 0, 0);

	TSharedPtr<SWidget> ItemContentWidget;

	if (ColumnID == SSourceControlSubmitWidgetDefs::ColumnID_CheckBoxLabel)
	{
		ItemContentWidget = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(RowPadding)
			[
				SNew(SCheckBox)
				.IsChecked(Item.Get(), &FSubmitItem::GetCheckBoxState)
				.OnCheckStateChanged(Item.Get(), &FSubmitItem::SetCheckBoxState)
			];
	}
	else if (ColumnID == SSourceControlSubmitWidgetDefs::ColumnID_IconLabel)
	{
		ItemContentWidget = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush(Item->GetIconName()))
				.ToolTipText(Item->GetIconTooltip())
			];
	}
	else if (ColumnID == SSourceControlSubmitWidgetDefs::ColumnID_FileLabel)
	{
		ItemContentWidget = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(RowPadding)
			[
				SNew(STextBlock)
				.Text(Item->GetDisplayName())
			];
	}

	return ItemContentWidget.ToSharedRef();
}


ECheckBoxState SSourceControlSubmitWidget::GetToggleSelectedState() const
{
	// Default to a Checked state
	ECheckBoxState PendingState = ECheckBoxState::Checked;

	// Iterate through the list of selected items
	for (const auto& Item : ListViewItems)
	{
		if (Item->GetCheckBoxState() == ECheckBoxState::Unchecked)
		{
			// If any item in the list is Unchecked, then represent the entire set of highlighted items as Unchecked,
			// so that the first (user) toggle of ToggleSelectedCheckBox consistently Checks all items
			PendingState = ECheckBoxState::Unchecked;
			break;
		}
	}

	return PendingState;
}


void SSourceControlSubmitWidget::OnToggleSelectedCheckBox(ECheckBoxState InNewState)
{
	for (const auto& Item : ListViewItems)
	{
		Item->SetCheckBoxState(InNewState);
	}

	ListView->RequestListRefresh();
}


void SSourceControlSubmitWidget::FillChangeListDescription(FChangeListDescription& OutDesc)
{
	OutDesc.Description = ChangeListDescriptionTextCtrl->GetText();
	OutDesc.FilesForAdd.Empty();
	OutDesc.FilesForSubmit.Empty();

	for (const auto& Item : ListViewItems)
	{
		if (Item->GetCheckBoxState() == ECheckBoxState::Checked)
		{
			if (Item->CanCheckIn())
			{
				OutDesc.FilesForSubmit.Add(Item->GetFilename());
			}
			else if (Item->NeedsAdding())
			{
				OutDesc.FilesForAdd.Add(Item->GetFilename());
			}
		}
	}
}


bool SSourceControlSubmitWidget::WantToKeepCheckedOut()
{
	return KeepCheckedOut == ECheckBoxState::Checked ? true : false;
}


FReply SSourceControlSubmitWidget::OKClicked()
{
	DialogResult = ESubmitResults::SUBMIT_ACCEPTED;
	ParentFrame.Pin()->RequestDestroyWindow();

	return FReply::Handled();
}


FReply SSourceControlSubmitWidget::CancelClicked()
{
	DialogResult = ESubmitResults::SUBMIT_CANCELED;
	ParentFrame.Pin()->RequestDestroyWindow();

	return FReply::Handled();
}


bool SSourceControlSubmitWidget::IsOKEnabled() const
{
	return !ChangeListDescriptionTextCtrl->GetText().IsEmpty();
}


EVisibility SSourceControlSubmitWidget::IsWarningPanelVisible() const
{
	return IsOKEnabled()? EVisibility::Hidden : EVisibility::Visible;
}


void SSourceControlSubmitWidget::OnCheckStateChanged_KeepCheckedOut(ECheckBoxState InState)
{
	KeepCheckedOut = InState;
}


ECheckBoxState SSourceControlSubmitWidget::GetKeepCheckedOut() const
{
	return KeepCheckedOut;
}


bool SSourceControlSubmitWidget::CanCheckOut() const
{
	const ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	return SourceControlProvider.UsesCheckout();
}


TSharedRef<ITableRow> SSourceControlSubmitWidget::OnGenerateRowForList(TSharedPtr<FSubmitItem> SubmitItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<ITableRow> Row =
	SNew(SSourceControlSubmitListRow, OwnerTable)
		.SourceControlSubmitWidget(SharedThis(this))
		.Item(SubmitItem)
		.IsEnabled(SubmitItem->IsEnabled());

	return Row;
}


EColumnSortMode::Type SSourceControlSubmitWidget::GetColumnSortMode(const FName ColumnId) const
{
	if (SortByColumn != ColumnId)
	{
		return EColumnSortMode::None;
	}

	return SortMode;
}


void SSourceControlSubmitWidget::OnColumnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type InSortMode)
{
	SortByColumn = ColumnId;
	SortMode = InSortMode;

	RequestSort();
}


void SSourceControlSubmitWidget::RequestSort()
{
	// Sort the list of root items
	SortTree();

	ListView->RequestListRefresh();
}


void SSourceControlSubmitWidget::SortTree()
{
	if (SortByColumn == SSourceControlSubmitWidgetDefs::ColumnID_FileLabel)
	{
		if (SortMode == EColumnSortMode::Ascending)
		{
			ListViewItems.Sort([](const TSharedPtr<FSubmitItem>& A, const TSharedPtr<FSubmitItem>& B) {
				return A->GetDisplayName().ToString() < B->GetDisplayName().ToString(); });
		}
		else if (SortMode == EColumnSortMode::Descending)
		{
			ListViewItems.Sort([](const TSharedPtr<FSubmitItem>& A, const TSharedPtr<FSubmitItem>& B) {
				return A->GetDisplayName().ToString() >= B->GetDisplayName().ToString(); });
		}
	}
	else if (SortByColumn == SSourceControlSubmitWidgetDefs::ColumnID_IconLabel)
	{
		if (SortMode == EColumnSortMode::Ascending)
		{
			ListViewItems.Sort([](const TSharedPtr<FSubmitItem>& A, const TSharedPtr<FSubmitItem>& B) {
				return A->GetIconName().ToString() < B->GetIconName().ToString(); });
		}
		else if (SortMode == EColumnSortMode::Descending)
		{
			ListViewItems.Sort([](const TSharedPtr<FSubmitItem>& A, const TSharedPtr<FSubmitItem>& B) {
				return A->GetIconName().ToString() >= B->GetIconName().ToString(); });
		}
	}
}


#undef LOCTEXT_NAMESPACE

#endif // SOURCE_CONTROL_WITH_SLATE
