// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Stack/SNiagaraStackParameterStoreEntryValue.h"
#include "NiagaraEditorModule.h"
#include "NiagaraEditorWidgetsStyle.h"
#include "NiagaraEditorStyle.h"
#include "INiagaraEditorTypeUtilities.h"
#include "SNiagaraParameterEditor.h"
#include "ViewModels/Stack/NiagaraStackParameterStoreEntry.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "PropertyEditorModule.h"
#include "IStructureDetailsView.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "NiagaraStackParameterStoreEntryValue"

void SNiagaraStackParameterStoreEntryValue::Construct(const FArguments& InArgs, UNiagaraStackParameterStoreEntry* InStackEntry)
{
	StackEntry = InStackEntry;

	StackEntry->OnValueChanged().AddSP(this, &SNiagaraStackParameterStoreEntryValue::OnInputValueChanged);
	DisplayedValueStruct = StackEntry->GetValueStruct();

	FMargin ItemPadding = FMargin(0);
	ChildSlot
	[
		// Values
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(0, 0, 3, 0)
		[
			// Value Icon
			SNew(SBox)
			.WidthOverride(TextIconSize)
			[
				SNew(STextBlock)
				.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.10"))
				.Text(this, &SNiagaraStackParameterStoreEntryValue::GetInputIconText)
				.ToolTipText(this, &SNiagaraStackParameterStoreEntryValue::GetInputIconToolTip)
				.ColorAndOpacity(this, &SNiagaraStackParameterStoreEntryValue::GetInputIconColor)
			]
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		[
			// Assigned handle
			SNew(SVerticalBox)
			// Value struct
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(ValueStructContainer, SBox)
				[
					ConstructValueStructWidget()
				]
			]
		]

		// Handle drop-down button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(3, 0, 0, 0)
		[
			SNew(SComboButton)
			.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
			.ForegroundColor(FSlateColor::UseForeground())
			.OnGetMenuContent(this, &SNiagaraStackParameterStoreEntryValue::OnGetAvailableHandleMenu)
			.ContentPadding(FMargin(2))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
		]

		// Reset Button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(3, 0, 0, 0)
		[
			SNew(SButton)
			.IsFocusable(false)
			.ToolTipText(LOCTEXT("ResetToolTip", "Reset to the default value"))
			.ButtonStyle(FEditorStyle::Get(), "NoBorder")
			.ContentPadding(0)
			.Visibility(this, &SNiagaraStackParameterStoreEntryValue::GetResetButtonVisibility)
			.OnClicked(this, &SNiagaraStackParameterStoreEntryValue::ResetButtonPressed)
			.Content()
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
			.IsFocusable(false)
			.ForegroundColor(FSlateColor::UseForeground())
			.ToolTipText(LOCTEXT("DeleteToolTip", "Delete this parameter"))
			.Visibility(this, &SNiagaraStackParameterStoreEntryValue::GetDeleteButtonVisibility)
			.OnClicked(this, &SNiagaraStackParameterStoreEntryValue::DeleteClicked)
			.Content()
			[
				SNew(STextBlock)
				.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.10"))
				.Text(FText::FromString(FString(TEXT("\xf1f8"))))
			]
		]
	];
}

FReply SNiagaraStackParameterStoreEntryValue::DeleteClicked()
{
	// toast notification
	FNotificationInfo Info(FText::Format(LOCTEXT("NiagaraDeletedUserParameter", "System exposed parameter was deleted.\n{0}\n(All links to inner variables were invalidated in the process.)"), StackEntry->GetDisplayName()));
	Info.ExpireDuration = 5.0f;
	Info.bFireAndForget = true;
	Info.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Info"));
	FSlateNotificationManager::Get().AddNotification(Info);

	// Delete after the notification is posted to prevent the entry from becoming invalidated before generating the message.
	StackEntry->Delete();

	return FReply::Handled();
}

TSharedRef<SWidget> SNiagaraStackParameterStoreEntryValue::OnGetAvailableHandleMenu()
{
	// TODO: This will need to be adjusted based on the current stack being edited, i.e. system vs emitter vs particle.
	FMenuBuilder MenuBuilder(true, nullptr);
	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SNiagaraStackParameterStoreEntryValue::ConstructValueStructWidget()
{
	ValueStructParameterEditor.Reset();
	ValueStructDetailsView.Reset();
	if (DisplayedValueStruct.IsValid())
	{
		FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::GetModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
		TSharedPtr<INiagaraEditorTypeUtilities, ESPMode::ThreadSafe> TypeEditorUtilities = NiagaraEditorModule.GetTypeUtilities(StackEntry->GetInputType());
		if (TypeEditorUtilities.IsValid() && TypeEditorUtilities->CanCreateParameterEditor())
		{
			TSharedPtr<SNiagaraParameterEditor> ParameterEditor = TypeEditorUtilities->CreateParameterEditor(StackEntry->GetInputType());
			ParameterEditor->UpdateInternalValueFromStruct(DisplayedValueStruct.ToSharedRef());
			ParameterEditor->SetOnBeginValueChange(SNiagaraParameterEditor::FOnValueChange::CreateSP(
				this, &SNiagaraStackParameterStoreEntryValue::ParameterBeginValueChange));
			ParameterEditor->SetOnEndValueChange(SNiagaraParameterEditor::FOnValueChange::CreateSP(
				this, &SNiagaraStackParameterStoreEntryValue::ParameterEndValueChange));
			ParameterEditor->SetOnValueChanged(SNiagaraParameterEditor::FOnValueChange::CreateSP(
				this, &SNiagaraStackParameterStoreEntryValue::ParameterValueChanged, ParameterEditor.ToSharedRef()));

			ValueStructParameterEditor = ParameterEditor;

			return SNew(SBox)
				.HAlign(ParameterEditor->GetHorizontalAlignment())
				.VAlign(ParameterEditor->GetVerticalAlignment())
				[
					ParameterEditor.ToSharedRef()
				];
		}
		else
		{
			FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

			TSharedRef<IStructureDetailsView> StructureDetailsView = PropertyEditorModule.CreateStructureDetailView(
				FDetailsViewArgs(false, false, false, FDetailsViewArgs::HideNameArea, true),
				FStructureDetailsViewArgs(),
				nullptr);

			StructureDetailsView->SetStructureData(DisplayedValueStruct);
			StructureDetailsView->GetOnFinishedChangingPropertiesDelegate().AddSP(this, &SNiagaraStackParameterStoreEntryValue::ParameterPropertyValueChanged);

			ValueStructDetailsView = StructureDetailsView;
			return StructureDetailsView->GetWidget().ToSharedRef();
		}
	}
	else
	{
		return SNullWidget::NullWidget;
	}
}

void SNiagaraStackParameterStoreEntryValue::OnInputValueChanged()
{
	TSharedPtr<FStructOnScope> NewValueStruct = StackEntry->GetValueStruct();
	if (DisplayedValueStruct == NewValueStruct)
	{
		if (ValueStructParameterEditor.IsValid())
		{
			ValueStructParameterEditor->UpdateInternalValueFromStruct(DisplayedValueStruct.ToSharedRef());
		}
		if (ValueStructDetailsView.IsValid())
		{
			ValueStructDetailsView->SetStructureData(TSharedPtr<FStructOnScope>());
			ValueStructDetailsView->SetStructureData(DisplayedValueStruct);
		}
	}
	else
	{
		DisplayedValueStruct = NewValueStruct;
		ValueStructContainer->SetContent(ConstructValueStructWidget());
	}
}

void SNiagaraStackParameterStoreEntryValue::ParameterBeginValueChange()
{
	StackEntry->NotifyBeginValueChange();
}

void SNiagaraStackParameterStoreEntryValue::ParameterEndValueChange()
{
	StackEntry->NotifyEndValueChange();
}

void SNiagaraStackParameterStoreEntryValue::ParameterValueChanged(TSharedRef<SNiagaraParameterEditor> ParameterEditor)
{
	ParameterEditor->UpdateStructFromInternalValue(StackEntry->GetValueStruct().ToSharedRef());
	StackEntry->NotifyValueChanged();
}

void SNiagaraStackParameterStoreEntryValue::ParameterPropertyValueChanged(const FPropertyChangedEvent& PropertyChangedEvent)
{
	StackEntry->NotifyValueChanged();
}

EVisibility SNiagaraStackParameterStoreEntryValue::GetDeleteButtonVisibility() const
{
	return StackEntry->CanRenameInput() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SNiagaraStackParameterStoreEntryValue::GetReferenceVisibility() const
{
	return EVisibility::Collapsed;
}

EVisibility SNiagaraStackParameterStoreEntryValue::GetResetButtonVisibility() const
{
	return StackEntry->CanReset() ? EVisibility::Visible : EVisibility::Hidden;
}

FReply SNiagaraStackParameterStoreEntryValue::ResetButtonPressed() const
{
	StackEntry->Reset();
	return FReply::Handled();
}

FText SNiagaraStackParameterStoreEntryValue::GetInputIconText() const
{
	if (DisplayedValueStruct.IsValid())
	{
		return FText::FromString(FString(TEXT("\xf040") /* fa-pencil */));
	}
	else if (StackEntry->GetValueObject() != nullptr)
	{
		return FText::FromString(FString(TEXT("\xf1C0") /* fa-database */));
	}
	else
	{
		return FText();
	}
}

FText SNiagaraStackParameterStoreEntryValue::GetInputIconToolTip() const
{
	if (DisplayedValueStruct.IsValid())
	{
		return LOCTEXT("StructInputIconToolTip", "Local Value");
	}
	else if (StackEntry->GetValueObject() != nullptr)
	{
		return LOCTEXT("DataInterfaceInputIconToolTip", "Data Value");
	}
	else
	{
		return FText();
	}
}

FSlateColor SNiagaraStackParameterStoreEntryValue::GetInputIconColor() const
{
	if (DisplayedValueStruct.IsValid())
	{
		return FLinearColor(FColor::Orange);
	}
	else if (StackEntry->GetValueObject() != nullptr)
	{
		return FLinearColor(FColor::Yellow);
	}
	else
	{
		return FLinearColor(FColor::White);
	}
}

#undef LOCTEXT_NAMESPACE