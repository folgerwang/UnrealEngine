// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SNiagaraTemplateAssetPicker.h"
#include "NiagaraEmitter.h"
#include "NiagaraEditorStyle.h"

#include "AssetData.h"
#include "AssetRegistryModule.h"
#include "Modules/ModuleManager.h"
#include "AssetThumbnail.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "SNiagaraAssetSelector"

void SNiagaraTemplateAssetPicker::Construct(const FArguments& InArgs, UClass* AssetClass)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> EmitterAssets;
	AssetRegistryModule.Get().GetAssetsByClass(AssetClass->GetFName(), EmitterAssets);

	NiagaraPluginCategory = LOCTEXT("NiagaraCategory", "Engine (Niagara Plugin)");
	ProjectCategory = LOCTEXT("ProjectCategory", "Project");

	AssetThumbnailPool = MakeShareable(new FAssetThumbnailPool(24));

	OnTemplateAssetActivated = InArgs._OnTemplateAssetActivated;
		
	TArray<FAssetData> EmittersToShow;
	for (const FAssetData& EmitterAsset : EmitterAssets)
	{
		bool bShowEmitter = false;
		EmitterAsset.GetTagValue("bIsTemplateAsset", bShowEmitter);
		if (bShowEmitter)
		{
			EmittersToShow.Add(EmitterAsset);
		}
	}

	ChildSlot
	[
		SAssignNew(ItemSelector, SNiagaraAssetItemSelector)
		.Items(EmittersToShow)
		.OnGetCategoriesForItem(this, &SNiagaraTemplateAssetPicker::OnGetCategoriesForItem)
		.OnCompareCategoriesForEquality(this, &SNiagaraTemplateAssetPicker::OnCompareCategoriesForEquality)
		.OnCompareCategoriesForSorting(this, &SNiagaraTemplateAssetPicker::OnCompareCategoriesForSorting)
		.OnCompareItemsForSorting(this, &SNiagaraTemplateAssetPicker::OnCompareItemsForSorting)
		.OnDoesItemMatchFilterText(this, &SNiagaraTemplateAssetPicker::OnDoesItemMatchFilterText)
		.OnGenerateWidgetForCategory(this, &SNiagaraTemplateAssetPicker::OnGenerateWidgetForCategory)
		.OnGenerateWidgetForItem(this, &SNiagaraTemplateAssetPicker::OnGenerateWidgetForItem)
		.OnItemActivated(this, &SNiagaraTemplateAssetPicker::OnItemActivated)
	];
}

void SNiagaraTemplateAssetPicker::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	AssetThumbnailPool->Tick(InDeltaTime);
}

TArray<FAssetData> SNiagaraTemplateAssetPicker::GetSelectedAssets() const
{
	return ItemSelector->GetSelectedItems();
}

TArray<FText> SNiagaraTemplateAssetPicker::OnGetCategoriesForItem(const FAssetData& Item)
{
	TArray<FText> Categories;
	TArray<FString> AssetPathParts;
	Item.ObjectPath.ToString().ParseIntoArray(AssetPathParts, TEXT("/"));
	if (AssetPathParts.Num() > 0)
	{
		if (AssetPathParts[0] == TEXT("Niagara"))
		{
			Categories.Add(LOCTEXT("NiagaraCategory", "Engine (Niagara Plugin)"));
		}
		else if(AssetPathParts[0] == TEXT("Game"))
		{
			Categories.Add(LOCTEXT("ProjectCategory", "Project"));
		}
		else
		{
			Categories.Add(FText::Format(LOCTEXT("OtherPluginFormat", "Plugin - {0}"), FText::FromString(AssetPathParts[0])));
		}
	}
	return Categories;
}

bool SNiagaraTemplateAssetPicker::OnCompareCategoriesForEquality(const FText& CategoryA, const FText& CategoryB) const
{
	return CategoryA.CompareTo(CategoryB) == 0;
}

bool SNiagaraTemplateAssetPicker::OnCompareCategoriesForSorting(const FText& CategoryA, const FText& CategoryB) const
{
	int32 CompareResult = CategoryA.CompareTo(CategoryB);
	if (CompareResult != 0)
	{
		// Project first
		if (CategoryA.CompareTo(ProjectCategory) == 0)
		{
			return true;
		}
		if (CategoryB.CompareTo(ProjectCategory) == 0)
		{
			return false;
		}

		// Niagara plugin second.
		if (CategoryA.CompareTo(NiagaraPluginCategory) == 0)
		{
			return true;
		}
		if (CategoryB.CompareTo(NiagaraPluginCategory) == 0)
		{
			return false;
		}
	}
	// Otherwise just return the actual result.
	return CompareResult == -1;
}

bool SNiagaraTemplateAssetPicker::OnCompareItemsForSorting(const FAssetData& ItemA, const FAssetData& ItemB) const
{
	return ItemA.AssetName.ToString().Compare(ItemB.AssetName.ToString()) == -1;
}

bool SNiagaraTemplateAssetPicker::OnDoesItemMatchFilterText(const FText& FilterText, const FAssetData& Item)
{
	return Item.AssetName.ToString().Contains(FilterText.ToString());
}

TSharedRef<SWidget> SNiagaraTemplateAssetPicker::OnGenerateWidgetForCategory(const FText& Category)
{
	return SNew(SBox)
		.Padding(FMargin(5, 5, 5, 3))
		[
			SNew(STextBlock)
			.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.AssetPickerAssetCategoryText")
			.Text(Category)
		];
}

const int32 ThumbnailSize = 72;

TSharedRef<SWidget> SNiagaraTemplateAssetPicker::OnGenerateWidgetForItem(const FAssetData& Item)
{
	TSharedRef<FAssetThumbnail> AssetThumbnail = MakeShared<FAssetThumbnail>(Item, ThumbnailSize, ThumbnailSize, AssetThumbnailPool);
	FAssetThumbnailConfig ThumbnailConfig;
	ThumbnailConfig.bAllowFadeIn = false;

	FText AssetDescription;
	Item.GetTagValue("TemplateAssetDescription", AssetDescription);

	return
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5, 3, 5, 5)
		[
			SNew(STextBlock)
			.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.AssetPickerAssetNameText")
			.Text(FText::FromString(FName::NameToDisplayString(Item.AssetName.ToString(), false)))
		]
		+ SVerticalBox::Slot()
		.Padding(5, 0, 5, 5)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 10, 0)
			[
				SNew(SBox)
				.WidthOverride(ThumbnailSize)
				.HeightOverride(ThumbnailSize)
				[
					AssetThumbnail->MakeThumbnailWidget(ThumbnailConfig)
				]
			]
			+ SHorizontalBox::Slot()
			[
				SNew(STextBlock)
				.Text(AssetDescription)
				.AutoWrapText(true)
			]
		];
}

void SNiagaraTemplateAssetPicker::OnItemActivated(const FAssetData& Item)
{
	OnTemplateAssetActivated.ExecuteIfBound(Item);
}

#undef LOCTEXT_NAMESPACE
