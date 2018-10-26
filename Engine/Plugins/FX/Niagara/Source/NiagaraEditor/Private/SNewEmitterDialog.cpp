// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SNewEmitterDialog.h"
#include "NiagaraEmitter.h"
#include "NiagaraEditorStyle.h"
#include "SNiagaraTemplateAssetPicker.h"
#include "SNiagaraNewAssetDialog.h"

#include "AssetData.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"

typedef SItemSelector<FText, FAssetData> SNiagaraAssetItemSelector;

#define LOCTEXT_NAMESPACE "SNewEmitterDialog"

void SNewEmitterDialog::Construct(const FArguments& InArgs)
{
	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.SelectionMode = ESelectionMode::SingleToggle;
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
	AssetPickerConfig.Filter.ClassNames.Add(UNiagaraEmitter::StaticClass()->GetFName());
	AssetPickerConfig.GetCurrentSelectionDelegates.Add(&GetSelectedEmitterAssetsFromPicker);
	AssetPickerConfig.OnAssetsActivated.BindSP(this, &SNewEmitterDialog::OnEmitterAssetsActivated);

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	TSharedRef<SWidget> AssetPicker = ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig);

	SNiagaraNewAssetDialog::Construct(SNiagaraNewAssetDialog::FArguments(), UNiagaraEmitter::StaticClass()->GetFName(), LOCTEXT("AssetTypeName", "emitter"),
		{
			SNiagaraNewAssetDialog::FNiagaraNewAssetDialogOption(
				LOCTEXT("CreateFromTemplateLabel", "Create a new emitter from an emitter template"),
				LOCTEXT("TemplatesPickerHeader", "Select a Template Emitter"),
				SNiagaraNewAssetDialog::FOnGetSelectedAssetsFromPicker::CreateSP(this, &SNewEmitterDialog::GetSelectedEmitterTemplateAssets),
				SAssignNew(TemplateAssetPicker, SNiagaraTemplateAssetPicker, UNiagaraEmitter::StaticClass())
				.OnTemplateAssetActivated(this, &SNewEmitterDialog::OnTemplateAssetActivated)),
			SNiagaraNewAssetDialog::FNiagaraNewAssetDialogOption(
				LOCTEXT("CreateFromOtherEmitterLabel", "Copy an existing emitter from your project content"),
				LOCTEXT("ProjectEmitterPickerHeader", "Select a Project Emitter"),
				SNiagaraNewAssetDialog::FOnGetSelectedAssetsFromPicker::CreateSP(this, &SNewEmitterDialog::GetSelectedProjectEmiterAssets),
				AssetPicker),
			SNiagaraNewAssetDialog::FNiagaraNewAssetDialogOption(
				LOCTEXT("CreateEmptyLabel", "Create an empty emitter with no modules or renderers (Advanced)"),
				LOCTEXT("EmptyLabel", "Empty Emitter"),
				SNiagaraNewAssetDialog::FOnGetSelectedAssetsFromPicker(),
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NoOptionsLabel", "No Options"))
				])
		});
}

TOptional<FAssetData> SNewEmitterDialog::GetSelectedEmitterAsset()
{
	const TArray<FAssetData>& SelectedEmitterAssets = GetSelectedAssets();
	if (SelectedEmitterAssets.Num() > 0)
	{
		return TOptional<FAssetData>(SelectedEmitterAssets[0]);
	}
	return TOptional<FAssetData>();
}

TArray<FAssetData> SNewEmitterDialog::GetSelectedEmitterTemplateAssets()
{
	return TemplateAssetPicker->GetSelectedAssets();
}

TArray<FAssetData> SNewEmitterDialog::GetSelectedProjectEmiterAssets()
{
	return GetSelectedEmitterAssetsFromPicker.Execute();
}

void SNewEmitterDialog::OnTemplateAssetActivated(const FAssetData& ActivatedTemplateAsset)
{
	ConfirmSelection();
}

void SNewEmitterDialog::OnEmitterAssetsActivated(const TArray<FAssetData>& ActivatedAssets, EAssetTypeActivationMethod::Type ActivationMethod)
{
	if ((ActivationMethod == EAssetTypeActivationMethod::DoubleClicked || ActivationMethod == EAssetTypeActivationMethod::Opened) && ActivatedAssets.Num() == 1)
	{
		ConfirmSelection();
	}
}

#undef LOCTEXT_NAMESPACE