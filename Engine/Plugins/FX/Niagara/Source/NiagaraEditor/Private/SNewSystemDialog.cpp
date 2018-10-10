// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SNewSystemDialog.h"
#include "NiagaraSystem.h"
#include "NiagaraEditorStyle.h"
#include "SNiagaraTemplateAssetPicker.h"

#include "AssetData.h"

#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWrapBox.h"
#include "EditorStyleSet.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"

typedef SItemSelector<FText, FAssetData> SNiagaraAssetItemSelector;

#define LOCTEXT_NAMESPACE "SNewSystemDialog"

void SNewSystemDialog::Construct(const FArguments& InArgs)
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FAssetPickerConfig SystemAssetPickerConfig;
	SystemAssetPickerConfig.SelectionMode = ESelectionMode::SingleToggle;
	SystemAssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
	SystemAssetPickerConfig.Filter.ClassNames.Add(UNiagaraSystem::StaticClass()->GetFName());
	SystemAssetPickerConfig.GetCurrentSelectionDelegates.Add(&GetSelectedSystemAssetsFromPicker);

	TSharedRef<SWidget> SystemAssetPicker = ContentBrowserModule.Get().CreateAssetPicker(SystemAssetPickerConfig);

	FAssetPickerConfig EmitterAssetPickerConfig;
	EmitterAssetPickerConfig.SelectionMode = ESelectionMode::Multi;
	EmitterAssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
	EmitterAssetPickerConfig.Filter.ClassNames.Add(UNiagaraEmitter::StaticClass()->GetFName());
	EmitterAssetPickerConfig.GetCurrentSelectionDelegates.Add(&GetSelectedEmitterAssetsFromPicker);

	TSharedRef<SWidget> EmitterAssetPicker = ContentBrowserModule.Get().CreateAssetPicker(EmitterAssetPickerConfig);

	SNiagaraNewAssetDialog::Construct(SNiagaraNewAssetDialog::FArguments(), UNiagaraSystem::StaticClass()->GetFName(), LOCTEXT("AssetTypeName", "system"),
		{
			SNiagaraNewAssetDialog::FNiagaraNewAssetDialogOption(
				LOCTEXT("CreateFromTemplateLabel", "Create a new system from a system template"),
				LOCTEXT("TemplateLabel", "Select a System Template"),
				SNiagaraNewAssetDialog::FOnGetSelectedAssetsFromPicker::CreateSP(this, &SNewSystemDialog::GetSelectedSystemTemplateAssets),
				SAssignNew(TemplateAssetPicker, SNiagaraTemplateAssetPicker, UNiagaraSystem::StaticClass())),
			SNiagaraNewAssetDialog::FNiagaraNewAssetDialogOption(
				LOCTEXT("CreateFromSelectedEmittersLabel", "Create a new system from a set of selected emitters"),
				LOCTEXT("ProjectEmittersLabel", "Select Emitters to Add"),
				SNiagaraNewAssetDialog::FOnGetSelectedAssetsFromPicker::CreateSP(this, &SNewSystemDialog::GetSelectedProjectEmiterAssets),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(0, 0, 0, 10)
				[
					EmitterAssetPicker
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.NewAssetDialog.SubHeaderText")
						.Text(LOCTEXT("SelectedEmittersLabel", "Emitters to Add:"))
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
						//.ButtonStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.NewAssetDialog.AddButton")
						.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
						.IsEnabled(this, &SNewSystemDialog::IsAddEmittersToSelectionButtonEnabled)
						.OnClicked(this, &SNewSystemDialog::AddEmittersToSelectionButtonClicked)
						.ToolTipText(LOCTEXT("AddSelectedEmitterToolTip", "Add the selected emitter to the collection\n of emitters to be added to the new system."))
						.Content()
						[
							SNew(SBox)
							.WidthOverride(32.0f)
							.HeightOverride(16.0f)
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.TextStyle(FEditorStyle::Get(), "NormalText.Important")
								.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.10"))
								.Text(FText::FromString(FString(TEXT("\xf067"))) /*fa-plus*/)
							]
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(SelectedEmitterBox, SWrapBox)
					.UseAllottedWidth(true)
				]),
			SNiagaraNewAssetDialog::FNiagaraNewAssetDialogOption(
				LOCTEXT("CreateFromOtherSystemLabel", "Copy an existing system from your project content"),
				LOCTEXT("ProjectSystemsLabel", "Select a Project System"),
				SNiagaraNewAssetDialog::FOnGetSelectedAssetsFromPicker::CreateSP(this, &SNewSystemDialog::GetSelectedProjectSystemAssets),
				SystemAssetPicker),
			SNiagaraNewAssetDialog::FNiagaraNewAssetDialogOption(
				LOCTEXT("CreateEmptyLabel", "Create an empty system with no emitters"),
				LOCTEXT("EmptyLabel", "Empty System"),
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

TOptional<FAssetData> SNewSystemDialog::GetSelectedSystemAsset() const
{
	const TArray<FAssetData>& AllSelectedAssets = GetSelectedAssets();
	TArray<FAssetData> SelectedSystemAssets;
	for (const FAssetData& SelectedAsset : AllSelectedAssets)
	{
		if (SelectedAsset.AssetClass == UNiagaraSystem::StaticClass()->GetFName())
		{
			SelectedSystemAssets.Add(SelectedAsset);
		}
	}
	if (SelectedSystemAssets.Num() == 1)
	{
		return TOptional<FAssetData>(SelectedSystemAssets[0]);
	}
	return TOptional<FAssetData>();
}

TArray<FAssetData> SNewSystemDialog::GetSelectedEmitterAssets() const
{
	const TArray<FAssetData>& AllSelectedAssets = GetSelectedAssets();
	TArray<FAssetData> ConfirmedSelectedEmitterAssets;
	for (const FAssetData& SelectedAsset : AllSelectedAssets)
	{
		if (SelectedAsset.AssetClass == UNiagaraEmitter::StaticClass()->GetFName())
		{
			ConfirmedSelectedEmitterAssets.Add(SelectedAsset);
		}
	}
	return ConfirmedSelectedEmitterAssets;
}

TArray<FAssetData> SNewSystemDialog::GetSelectedSystemTemplateAssets()
{
	return TemplateAssetPicker->GetSelectedAssets();
}

TArray<FAssetData> SNewSystemDialog::GetSelectedProjectSystemAssets()
{
	return GetSelectedSystemAssetsFromPicker.Execute();
}

TArray<FAssetData> SNewSystemDialog::GetSelectedProjectEmiterAssets()
{
	return SelectedEmitterAssets;
}

bool SNewSystemDialog::IsAddEmittersToSelectionButtonEnabled() const
{
	return GetSelectedEmitterAssetsFromPicker.Execute().Num() > 0;
}

FReply SNewSystemDialog::AddEmittersToSelectionButtonClicked()
{
	TArray<FAssetData> SelectedEmitterAssetsFromPicker = GetSelectedEmitterAssetsFromPicker.Execute();
	for (const FAssetData& SelectedEmitterAsset : SelectedEmitterAssetsFromPicker)
	{
		if (SelectedEmitterAssets.Contains(SelectedEmitterAsset) == false)
		{
			TSharedPtr<SWidget> SelectedEmitterWidget;
			SelectedEmitterBox->AddSlot()
				.Padding(FMargin(0, 0, 5, 0))
				[
					SNew(SBorder)
					.BorderImage(FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.NewAssetDialog.SubBorder"))
					.BorderBackgroundColor(FNiagaraEditorStyle::Get().GetColor("NiagaraEditor.NewAssetDialog.SubBorderColor"))
					[
						SAssignNew(SelectedEmitterWidget, SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(5, 0, 0, 0)
						[
							SNew(STextBlock)
							.Text(FText::FromName(SelectedEmitterAsset.AssetName))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(2, 0, 0, 0)
						[
							SNew(SButton)
							.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
							.OnClicked(this, &SNewSystemDialog::RemoveEmitterFromSelectionButtonClicked, SelectedEmitterAsset)
							.ToolTipText(LOCTEXT("RemoveSelectedEmitterToolTip", "Remove the selected emitter from the collection\n of emitters to be added to the new system."))
							[
								SNew(STextBlock)
								//.TextStyle(FEditorStyle::Get(), "NormalText.Important")
								.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.10"))
								.Text(FText::FromString(FString(TEXT("\xf057"))) /*times-circle*/)
								.ColorAndOpacity(FLinearColor(.8f, .2f, .2f, 1.0f))
							]
						]
					]
				];
			SelectedEmitterAssets.Add(SelectedEmitterAsset);
			SelectedEmitterAssetWidgets.Add(SelectedEmitterWidget);
		}
	}
	return FReply::Handled();
}

FReply SNewSystemDialog::RemoveEmitterFromSelectionButtonClicked(FAssetData EmitterAsset)
{
	int32 SelectionIndex = SelectedEmitterAssets.IndexOfByKey(EmitterAsset);
	if (SelectionIndex != INDEX_NONE)
	{
		SelectedEmitterAssets.RemoveAt(SelectionIndex);
		SelectedEmitterBox->RemoveSlot(SelectedEmitterAssetWidgets[SelectionIndex].ToSharedRef());
		SelectedEmitterAssetWidgets.RemoveAt(SelectionIndex);
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE