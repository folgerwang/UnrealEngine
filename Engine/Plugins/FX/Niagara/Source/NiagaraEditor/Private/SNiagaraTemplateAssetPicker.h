// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SItemSelector.h"
#include "AssetData.h"

typedef SItemSelector<FText, FAssetData> SNiagaraAssetItemSelector;

class FAssetThumbnailPool;

class SNiagaraTemplateAssetPicker : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnTemplateAssetActivated, const FAssetData&);

public:
	SLATE_BEGIN_ARGS(SNiagaraTemplateAssetPicker) {}
		SLATE_EVENT(FOnTemplateAssetActivated, OnTemplateAssetActivated);
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UClass* AssetClass);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	TArray<FAssetData> GetSelectedAssets() const;

private:
	TArray<FText> OnGetCategoriesForItem(const FAssetData& Item);

	bool OnCompareCategoriesForEquality(const FText& CategoryA, const FText& CategoryB) const;

	bool OnCompareCategoriesForSorting(const FText& CategoryA, const FText& CategoryB) const;

	bool OnCompareItemsForSorting(const FAssetData& ItemA, const FAssetData& ItemB) const;

	bool OnDoesItemMatchFilterText(const FText& FilterText, const FAssetData& Item);

	TSharedRef<SWidget> OnGenerateWidgetForCategory(const FText& Category);

	TSharedRef<SWidget> OnGenerateWidgetForItem(const FAssetData& Item);

	void OnItemActivated(const FAssetData& Item);

private:
	TSharedPtr<SNiagaraAssetItemSelector> ItemSelector;
	FText NiagaraPluginCategory;
	FText ProjectCategory;
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool;
	FOnTemplateAssetActivated OnTemplateAssetActivated;
};