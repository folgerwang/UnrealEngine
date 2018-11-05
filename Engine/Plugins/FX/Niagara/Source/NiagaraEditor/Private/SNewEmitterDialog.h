// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SNiagaraNewAssetDialog.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "AssetData.h"
#include "ContentBrowserDelegates.h"

class SNiagaraTemplateAssetPicker;

/** A modal dialog to collect information needed to create a new niagara system. */
class SNewEmitterDialog : public SNiagaraNewAssetDialog
{
public:
	SLATE_BEGIN_ARGS(SNewEmitterDialog)
	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	TOptional<FAssetData> GetSelectedEmitterAsset();

private:
	void GetSelectedEmitterTemplateAssets(TArray<FAssetData>& OutSelectedAssets);

	void GetSelectedProjectEmiterAssets(TArray<FAssetData>& OutSelectedAssets);

	void OnTemplateAssetActivated(const FAssetData& InActivatedTemplateAsset);

	void OnEmitterAssetsActivated(const TArray<FAssetData>& ActivatedAssets, EAssetTypeActivationMethod::Type ActivationMethod);

private:
	TSharedPtr<SNiagaraTemplateAssetPicker> TemplateAssetPicker;

	FGetCurrentSelectionDelegate GetSelectedEmitterAssetsFromPicker;

	FAssetData ActivatedTemplateAsset;

	FAssetData ActivatedProjectAsset;
};