// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SGraphTitleBarAddNewBookmark.h"
#include "BlueprintEditor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EditorStyleSet.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#define LOCTEXT_NAMESPACE "SGraphTitleBarAddNewBookmark"

void SGraphTitleBarAddNewBookmark::Construct(const FArguments& InArgs)
{
	EditorContextPtr = InArgs._EditorPtr;

	SComboButton::FArguments Args;
	Args.ButtonContent()
	[
		SNew(STextBlock)
		.TextStyle(FEditorStyle::Get(), "GraphBookmarkButtonText")
		.Text(this, &SGraphTitleBarAddNewBookmark::GetAddButtonGlyph)
	]
	.MenuContent()
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
		.Padding(4)
		[
			SNew(SBox)
			.MinDesiredWidth(300)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(8)
				.FillHeight(1.f)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SNew(STextBlock)
					.Text(this, &SGraphTitleBarAddNewBookmark::GetPopupTitleText)
					.Font(FEditorStyle::GetFontStyle("StandardDialog.LargeFont"))
				]
				+ SVerticalBox::Slot()
				.Padding(8, 4, 8, 8)
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(6)
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("BookmarkNameFieldLabel", "Name:"))
					]
					+ SHorizontalBox::Slot()
					.Padding(1)
					.FillWidth(1.f)
					[
						SAssignNew(NameEntryWidget, SEditableTextBox)
						.SelectAllTextWhenFocused(true)
						.OnTextCommitted(this, &SGraphTitleBarAddNewBookmark::OnNameTextCommitted)
						.Text(this, &SGraphTitleBarAddNewBookmark::GetDefaultNameText)
					]
				]
				+ SVerticalBox::Slot()
				.Padding(8, 4, 4, 8)
				.AutoHeight()
				.VAlign(VAlign_Bottom)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("RemoveButtonLabel", "Remove"))
						.TextStyle(FEditorStyle::Get(), "FlatButton.DefaultTextStyle")
						.ButtonStyle(FEditorStyle::Get(), "FlatButton.Default")
						.Visibility_Lambda([this]() -> EVisibility
						{
							return CurrentViewBookmarkId.IsValid() ? EVisibility::Visible : EVisibility::Collapsed;
						})
						.OnClicked(this, &SGraphTitleBarAddNewBookmark::OnRemoveButtonClicked)
					]
					+SHorizontalBox::Slot()
					.FillWidth(1.f)
					.HAlign(HAlign_Right)
					[
						SNew(SUniformGridPanel)
						.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
						.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
						.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
						+ SUniformGridPanel::Slot(0, 0)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.Text(LOCTEXT("CancelButtonLabel", "Cancel"))
							.TextStyle(FEditorStyle::Get(), "FlatButton.DefaultTextStyle")
							.ButtonStyle(FEditorStyle::Get(), "FlatButton.Default")
							.OnClicked_Lambda([this]() -> FReply
							{
								SetIsOpen(false);
								return FReply::Handled();
							})
						]
						+ SUniformGridPanel::Slot(1, 0)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.Text(this, &SGraphTitleBarAddNewBookmark::GetAddButtonLabel)
							.TextStyle(FEditorStyle::Get(), "FlatButton.DefaultTextStyle")
							.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
							.OnClicked(this, &SGraphTitleBarAddNewBookmark::OnAddButtonClicked)
						]
					]
				]
			]
		]
	]
	.HasDownArrow(false)
	.ComboButtonStyle(FEditorStyle::Get(), "ComboButton")
	.ButtonStyle(FEditorStyle::Get(), "GraphBookmarkButton")
	.ForegroundColor(FLinearColor(1, 1, 1, 0.5))
	.ToolTipText(LOCTEXT("AddBookmarkButtonToolTip", "Bookmark Current Location"))
	.OnComboBoxOpened(this, &SGraphTitleBarAddNewBookmark::OnComboBoxOpened);

	SetMenuContentWidgetToFocus(NameEntryWidget);

	SComboButton::Construct(Args);
}

FText SGraphTitleBarAddNewBookmark::GetPopupTitleText() const
{
	if (CurrentViewBookmarkId.IsValid())
	{
		return LOCTEXT("EditBookmarkPopupTitle", "Edit Bookmark");
	}
	
	return LOCTEXT("NewBookmarkPopupTitle", "New Bookmark");
}

FText SGraphTitleBarAddNewBookmark::GetDefaultNameText() const
{
	FText ResultText = LOCTEXT("DefaultBookmarkLabel", "New Bookmark");
	TSharedPtr<FBlueprintEditor> EditorContext = EditorContextPtr.Pin();
	if (EditorContext.IsValid())
	{
		UBlueprint* Blueprint = EditorContext->GetBlueprintObj();
		check(Blueprint);

		if (CurrentViewBookmarkId.IsValid())
		{
			for (const FBPEditorBookmarkNode& BookmarkNode : Blueprint->BookmarkNodes)
			{
				if (BookmarkNode.NodeGuid == CurrentViewBookmarkId)
				{
					ResultText = BookmarkNode.DisplayName;
					break;
				}
			}
		}
		else
		{
			int32 i = 1;
			bool bNeedsCheck;

			do
			{
				bNeedsCheck = false;

				for (const FBPEditorBookmarkNode& BookmarkNode : Blueprint->BookmarkNodes)
				{
					if (ResultText.EqualTo(BookmarkNode.DisplayName))
					{
						const FEditedDocumentInfo* BookmarkInfo = Blueprint->Bookmarks.Find(BookmarkNode.NodeGuid);
						if (BookmarkInfo && BookmarkInfo->EditedObjectPath.ResolveObject() == EditorContext->GetFocusedGraph())
						{
							ResultText = FText::Format(LOCTEXT("DefaultBookmarkLabelWithIndex", "New Bookmark {0}"), ++i);

							bNeedsCheck = true;
							break;
						}
					}
				}
			} while (bNeedsCheck);
		}
	}

	return ResultText;
}

FText SGraphTitleBarAddNewBookmark::GetAddButtonGlyph() const
{
	TSharedPtr<FBlueprintEditor> EditorContext = EditorContextPtr.Pin();
	if (EditorContext.IsValid())
	{
		FGuid CurrentBookmarkId;
		EditorContext->GetViewBookmark(CurrentBookmarkId);
		if (CurrentBookmarkId.IsValid())
		{
			return FText::FromString(FString(TEXT("\xf005"))); /*fa-star*/
		}
	}

	return FText::FromString(FString(TEXT("\xf006"))); /*fa-star-o*/
}

FText SGraphTitleBarAddNewBookmark::GetAddButtonLabel() const
{
	if (CurrentViewBookmarkId.IsValid())
	{
		return LOCTEXT("RenameButtonLabel", "Rename");
	}
	
	return LOCTEXT("AddButtonLabel", "Add");
}

void SGraphTitleBarAddNewBookmark::OnComboBoxOpened()
{
	CurrentNameText = FText::GetEmpty();
	CurrentViewBookmarkId.Invalidate();

	TSharedPtr<FBlueprintEditor> EditorContext = EditorContextPtr.Pin();
	if (EditorContext.IsValid())
	{
		EditorContext->GetViewBookmark(CurrentViewBookmarkId);
	}
}

FReply SGraphTitleBarAddNewBookmark::OnAddButtonClicked()
{
	TSharedPtr<FBlueprintEditor> EditorContext = EditorContextPtr.Pin();
	if (EditorContext.IsValid())
	{
		if (CurrentViewBookmarkId.IsValid())
		{
			EditorContext->RenameBookmark(CurrentViewBookmarkId, CurrentNameText);
		}
		else if (UEdGraph* FocusedGraph = EditorContext->GetFocusedGraph())
		{
			FEditedDocumentInfo NewBookmarkInfo;
			NewBookmarkInfo.EditedObjectPath = FocusedGraph;
			EditorContext->GetViewLocation(NewBookmarkInfo.SavedViewOffset, NewBookmarkInfo.SavedZoomAmount);
			if (FBPEditorBookmarkNode* NewNode = EditorContext->AddBookmark(CurrentNameText, NewBookmarkInfo))
			{
				EditorContext->SetViewLocation(NewBookmarkInfo.SavedViewOffset, NewBookmarkInfo.SavedZoomAmount, NewNode->NodeGuid);
			}
		}
	}

	SetIsOpen(false);

	return FReply::Handled();
}

FReply SGraphTitleBarAddNewBookmark::OnRemoveButtonClicked()
{
	TSharedPtr<FBlueprintEditor> EditorContext = EditorContextPtr.Pin();
	if (EditorContext.IsValid() && CurrentViewBookmarkId.IsValid())
	{
		EditorContext->RemoveBookmark(CurrentViewBookmarkId);
	}

	SetIsOpen(false);

	return FReply::Handled();
}

void SGraphTitleBarAddNewBookmark::OnNameTextCommitted(const FText& InText, ETextCommit::Type CommitType)
{
	CurrentNameText = InText;

	if (CommitType == ETextCommit::OnEnter)
	{
		OnAddButtonClicked();
	}
}

#undef LOCTEXT_NAMESPACE