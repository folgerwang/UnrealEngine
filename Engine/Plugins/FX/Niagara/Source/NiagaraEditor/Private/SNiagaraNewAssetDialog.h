// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "AssetData.h"
#include "IAssetTypeActions.h"

class SNiagaraTemplateAssetPicker;

/** A modal dialog to collect information needed to create a new niagara system. */
class SNiagaraNewAssetDialog : public SWindow
{
public:
	DECLARE_DELEGATE_OneParam(FOnGetSelectedAssetsFromPicker, TArray<FAssetData>& /* OutSelectedAssets */);

public:
	class FNiagaraNewAssetDialogOption
	{
	public:
		FText OptionText;
		FText AssetPickerHeader;
		TSharedRef<SWidget> AssetPicker;
		FOnGetSelectedAssetsFromPicker OnGetSelectedAssetsFromPicker;

		FNiagaraNewAssetDialogOption(FText InOptionText, FText InAssetPickerHeader, FOnGetSelectedAssetsFromPicker InOnGetSelectedAssetsFromPicker, TSharedRef<SWidget> InAssetPicker)
			: OptionText(InOptionText)
			, AssetPickerHeader(InAssetPickerHeader)
			, AssetPicker(InAssetPicker)
			, OnGetSelectedAssetsFromPicker(InOnGetSelectedAssetsFromPicker)
		{
		}
	};

public:
	SLATE_BEGIN_ARGS(SNiagaraNewAssetDialog)
	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, FName InSaveConfigKey, FText AssetTypeDisplayName, TArray<FNiagaraNewAssetDialogOption> InOptions);

	bool GetUserConfirmedSelection() const;

protected:
	const TArray<FAssetData>& GetSelectedAssets() const;

	void ConfirmSelection();

private:
	void OnWindowClosed(const TSharedRef<SWindow>& Window);

	FSlateColor GetOptionBorderColor(int32 OptionIndex) const;

	ECheckBoxState GetOptionCheckBoxState(int32 OptionIndex) const;

	void OptionCheckBoxStateChanged(ECheckBoxState InCheckBoxState, int32 OptionIndex);

	FSlateColor GetOptionTextColor(int32 OptionIndex) const;

	FText GetAssetPickersLabelText() const;

	EVisibility GetAssetPickerVisibility(int32 OptionIndex) const;

	bool IsOkButtonEnabled() const;

	FReply OnOkButtonClicked();

	FReply OnCancelButtonClicked();

	void SaveConfig();

private:
	FName SaveConfigKey;

	TArray<FNiagaraNewAssetDialogOption> Options;

	int32 SelectedOptionIndex;

	bool bUserConfirmedSelection;
	TArray<FAssetData> SelectedAssets;
};