// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Factories/DataTableFactory.h"
#include "Engine/DataTable.h"

#include "UObject/Class.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorStyleSet.h"
#include "Input/Reply.h"
#include "DataTableEditorUtils.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "DataTableFactory"

UDataTableFactory::UDataTableFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UDataTable::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

bool UDataTableFactory::ConfigureProperties()
{
	class FDataTableFactoryUI : public TSharedFromThis < FDataTableFactoryUI >
	{
		TSharedPtr<SWindow> PickerWindow;
		TSharedPtr<SComboBox<UScriptStruct*>> RowStructCombo;
		TSharedPtr<SButton> OkButton;
		UScriptStruct* ResultStruct;
	public:
		FDataTableFactoryUI() : ResultStruct(nullptr) {}

		TSharedRef<SWidget> MakeRowStructItemWidget(class UScriptStruct* InStruct) const
		{
			return SNew(STextBlock).Text(InStruct ? InStruct->GetDisplayNameText() : FText::GetEmpty());
		}

		FText GetSelectedRowOptionText() const
		{
			UScriptStruct* RowStruct = RowStructCombo.IsValid() ? RowStructCombo->GetSelectedItem() : nullptr;
			return RowStruct ? RowStruct->GetDisplayNameText() : FText::GetEmpty();
		}

		FReply OnCreate()
		{
			ResultStruct = RowStructCombo.IsValid() ? RowStructCombo->GetSelectedItem() : nullptr;
			if (PickerWindow.IsValid())
			{
				PickerWindow->RequestDestroyWindow();
			}
			return FReply::Handled();
		}

		FReply OnCancel()
		{
			ResultStruct = nullptr;
			if (PickerWindow.IsValid())
			{
				PickerWindow->RequestDestroyWindow();
			}
			return FReply::Handled();
		}

		bool IsAnyRowSelected() const
		{
			return  RowStructCombo.IsValid() && RowStructCombo->GetSelectedItem();
		}

		UScriptStruct* OpenStructSelector()
		{
			ResultStruct = nullptr;
			auto RowStructs = FDataTableEditorUtils::GetPossibleStructs();

			RowStructCombo = SNew(SComboBox<UScriptStruct*>)
				.OptionsSource(&RowStructs)
				.OnGenerateWidget(this, &FDataTableFactoryUI::MakeRowStructItemWidget)
				[
					SNew(STextBlock)
					.Text(this, &FDataTableFactoryUI::GetSelectedRowOptionText)
				];

			PickerWindow = SNew(SWindow)
				.Title(LOCTEXT("DataTableFactoryOptions", "Pick Row Structure"))
				.ClientSize(FVector2D(350, 100))
				.SupportsMinimize(false).SupportsMaximize(false)
				[
					SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
				.Padding(10)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
				.AutoHeight()
				[
					RowStructCombo.ToSharedRef()
				]
			+ SVerticalBox::Slot()
				.HAlign(HAlign_Right)
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(OkButton, SButton)
					.Text(LOCTEXT("OK", "OK"))
				.OnClicked(this, &FDataTableFactoryUI::OnCreate)
				]
			+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("Cancel", "Cancel"))
				.OnClicked(this, &FDataTableFactoryUI::OnCancel)
				]
				]
				]
				];

			OkButton->SetEnabled(
				TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FDataTableFactoryUI::IsAnyRowSelected)));

			GEditor->EditorAddModalWindow(PickerWindow.ToSharedRef());

			PickerWindow.Reset();
			RowStructCombo.Reset();

			return ResultStruct;
		}
	};


	TSharedRef<FDataTableFactoryUI> StructSelector = MakeShareable(new FDataTableFactoryUI());
	Struct = StructSelector->OpenStructSelector();

	return Struct != nullptr;
}

UObject* UDataTableFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UDataTable* DataTable = nullptr;
	if (Struct && ensure(SupportedClass == Class))
	{
		ensure(0 != (RF_Public & Flags));
		DataTable = MakeNewDataTable(InParent, Name, Flags);
		if (DataTable)
		{
			DataTable->RowStruct = Struct;
		}
	}
	return DataTable;
}

UDataTable* UDataTableFactory::MakeNewDataTable(UObject* InParent, FName Name, EObjectFlags Flags)
{
	return NewObject<UDataTable>(InParent, Name, Flags);
}

#undef LOCTEXT_NAMESPACE // "DataTableFactory"
