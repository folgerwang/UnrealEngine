// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SNiagaraNewAssetDialog.h"
#include "NiagaraEmitter.h"
#include "NiagaraEditorStyle.h"
#include "SNiagaraTemplateAssetPicker.h"
#include "NiagaraEditorSettings.h"

#include "AssetData.h"

#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "EditorStyleSet.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"

typedef SItemSelector<FText, FAssetData> SNiagaraAssetItemSelector;

#define LOCTEXT_NAMESPACE "SNiagaraNewAssetDialog"

void SNiagaraNewAssetDialog::Construct(const FArguments& InArgs, FName InSaveConfigKey, FText AssetTypeDisplayName, TArray<FNiagaraNewAssetDialogOption> InOptions)
{
	bUserConfirmedSelection = false;

	SaveConfigKey = InSaveConfigKey;
	FNiagaraNewAssetDialogConfig DialogConfig = GetDefault<UNiagaraEditorSettings>()->GetNewAssetDailogConfig(SaveConfigKey);
	SelectedOptionIndex = DialogConfig.SelectedOptionIndex;

	Options = InOptions;

	SetOnWindowClosed(FOnWindowClosed::CreateSP(this, &SNiagaraNewAssetDialog::OnWindowClosed));
	
	TSharedPtr<SVerticalBox> OptionsBox;
	TSharedPtr<SOverlay> AssetPickerOverlay;

	TSharedRef<SVerticalBox> RootBox =
		SNew(SVerticalBox)

		// Options label
		+ SVerticalBox::Slot()
		.Padding(0, 7, 0, 0)
		.AutoHeight()
		[
			SNew(SBox)
			.Padding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
			[
				SNew(STextBlock)
				.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.NewAssetDialog.HeaderText")
				.Text(LOCTEXT("OptionsLabel", "Select an Option"))
			]
		]

		// Creation mode radio buttons.
		+ SVerticalBox::Slot()
		.Padding(0, 5, 0, 5)
		.AutoHeight()
		[
			SNew(SBox)
			.Padding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
			[
					
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(FMargin(7))
				[
					SAssignNew(OptionsBox, SVerticalBox)
				]
			]
		]

		// Asset pickers label
		+ SVerticalBox::Slot()
		.Padding(0, 5, 0, 0)
		.AutoHeight()
		[
			SNew(SBox)
			.Padding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
			[
				SNew(STextBlock)
				.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.NewAssetDialog.HeaderText")
				.Text(this, &SNiagaraNewAssetDialog::GetAssetPickersLabelText)
			]
		]

		// Asset pickers
		+ SVerticalBox::Slot()
		.Padding(0, 5, 0, 5)
		[
			SNew(SBox)
			.Padding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(FMargin(7))
				[
					SAssignNew(AssetPickerOverlay, SOverlay)
				]
			]
		]

		// OK/Cancel buttons
		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(0, 5, 0, 5)
		[
			SNew(SUniformGridPanel)
			.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
			.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
			.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
			+SUniformGridPanel::Slot(0, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				.Text(LOCTEXT("OK", "OK"))
				.OnClicked(this, &SNiagaraNewAssetDialog::OnOkButtonClicked)
				.IsEnabled(this, &SNiagaraNewAssetDialog::IsOkButtonEnabled)
			]
			+SUniformGridPanel::Slot(1, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				.Text(LOCTEXT("Cancel", "Cancel"))
				.OnClicked(this, &SNiagaraNewAssetDialog::OnCancelButtonClicked)
			]
		];

	int32 OptionIndex = 0;
	for (FNiagaraNewAssetDialogOption& Option : Options)
	{
		OptionsBox->AddSlot()
			.Padding(0, 0, 0, OptionIndex < Options.Num() - 1 ? 7 : 0)
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.NewAssetDialog.SubBorder"))
				.BorderBackgroundColor(this, &SNiagaraNewAssetDialog::GetOptionBorderColor, OptionIndex)
				[
					SNew(SCheckBox)
					.Style(FCoreStyle::Get(), "RadioButton")
					.CheckBoxContentUsesAutoWidth(false)
					.IsChecked(this, &SNiagaraNewAssetDialog::GetOptionCheckBoxState, OptionIndex)
					.OnCheckStateChanged(this, &SNiagaraNewAssetDialog::OptionCheckBoxStateChanged, OptionIndex)
					.Content()
					[
						SNew(STextBlock)
						.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.NewAssetDialog.OptionText")
						.ColorAndOpacity(this, &SNiagaraNewAssetDialog::GetOptionTextColor, OptionIndex)
						.Text(Option.OptionText)
						.AutoWrapText(true)
					]
				]
			];

		AssetPickerOverlay->AddSlot()
			[
				SNew(SBox)
				.Visibility(this, &SNiagaraNewAssetDialog::GetAssetPickerVisibility, OptionIndex)
				[
					Option.AssetPicker
				]
			];

		OptionIndex++;
	}

	SWindow::Construct(SWindow::FArguments()
		.Title(FText::Format(LOCTEXT("NewEmitterDialogTitle", "Pick a starting point for your {0}"), AssetTypeDisplayName))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(DialogConfig.WindowSize)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			RootBox
		]);
}

bool SNiagaraNewAssetDialog::GetUserConfirmedSelection() const
{
	return bUserConfirmedSelection;
}

const TArray<FAssetData>& SNiagaraNewAssetDialog::GetSelectedAssets() const
{
	return SelectedAssets;
}

void SNiagaraNewAssetDialog::ConfirmSelection()
{
	const FNiagaraNewAssetDialogOption& SelectedOption = Options[SelectedOptionIndex];
	if (SelectedOption.OnGetSelectedAssetsFromPicker.IsBound())
	{
		SelectedOption.OnGetSelectedAssetsFromPicker.Execute(SelectedAssets);
		ensureMsgf(SelectedAssets.Num() > 0, TEXT("No assets selected when dialog was confirmed."));
	}
	bUserConfirmedSelection = true;
	RequestDestroyWindow();
}

void SNiagaraNewAssetDialog::OnWindowClosed(const TSharedRef<SWindow>& Window)
{
	SaveConfig();
}

FSlateColor SNiagaraNewAssetDialog::GetOptionBorderColor(int32 OptionIndex) const
{
	return SelectedOptionIndex == OptionIndex
		? FNiagaraEditorStyle::Get().GetColor("NiagaraEditor.NewAssetDialog.ActiveOptionBorderColor")
		: FSlateColor(FLinearColor::Transparent);
}

FSlateColor SNiagaraNewAssetDialog::GetOptionTextColor(int32 OptionIndex) const
{
	return SelectedOptionIndex == OptionIndex
		? FSlateColor(FLinearColor::White)
		: FSlateColor::UseForeground();
}

ECheckBoxState SNiagaraNewAssetDialog::GetOptionCheckBoxState(int32 OptionIndex) const
{
	return SelectedOptionIndex == OptionIndex ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SNiagaraNewAssetDialog::OptionCheckBoxStateChanged(ECheckBoxState InCheckBoxState, int32 OptionIndex)
{
	if (InCheckBoxState == ECheckBoxState::Checked)
	{
		SelectedOptionIndex = OptionIndex;
	}
}

FText SNiagaraNewAssetDialog::GetAssetPickersLabelText() const
{
	return Options[SelectedOptionIndex].AssetPickerHeader;
}

EVisibility SNiagaraNewAssetDialog::GetAssetPickerVisibility(int32 OptionIndex) const
{
	return  SelectedOptionIndex == OptionIndex ? EVisibility::Visible : EVisibility::Collapsed;
}

bool SNiagaraNewAssetDialog::IsOkButtonEnabled() const
{
	const FNiagaraNewAssetDialogOption& SelectedOption = Options[SelectedOptionIndex];
	if (SelectedOption.OnGetSelectedAssetsFromPicker.IsBound())
	{
		TArray<FAssetData> TempSelectedAssets;
		SelectedOption.OnGetSelectedAssetsFromPicker.Execute(TempSelectedAssets);
		return TempSelectedAssets.Num() != 0;
	}
	else
	{
		return true;
	}
}

FReply SNiagaraNewAssetDialog::OnOkButtonClicked()
{
	ConfirmSelection();
	return FReply::Handled();
}

FReply SNiagaraNewAssetDialog::OnCancelButtonClicked()
{
	bUserConfirmedSelection = false;
	SelectedAssets.Empty();

	RequestDestroyWindow();
	return FReply::Handled();
}

void SNiagaraNewAssetDialog::SaveConfig()
{
	FNiagaraNewAssetDialogConfig Config;
	Config.SelectedOptionIndex = SelectedOptionIndex;
	Config.WindowSize = GetClientSizeInScreen() / GetDPIScaleFactor();

	GetMutableDefault<UNiagaraEditorSettings>()->SetNewAssetDialogConfig(SaveConfigKey, Config);
}

#undef LOCTEXT_NAMESPACE