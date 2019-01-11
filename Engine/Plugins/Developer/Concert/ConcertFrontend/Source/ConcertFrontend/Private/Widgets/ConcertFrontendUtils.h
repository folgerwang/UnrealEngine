// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConcertUIExtension.h"
#include "EditorStyleSet.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "EditorFontGlyphs.h"
#include "IConcertUICoreModule.h"

namespace ConcertFrontendUtils
{
	static const FName ButtonIconSyle = TEXT("FontAwesome.10");
	static const float MinDesiredWidthForBtnAndIcon = 29.f;
	static const FName ButtonStyleNames[(int32)EConcertUIStyle::NUM] = {
		TEXT("FlatButton"),
		TEXT("FlatButton.Primary"),
		TEXT("FlatButton.Info"),
		TEXT("FlatButton.Success"),
		TEXT("FlatButton.Warning"),
		TEXT("FlatButton.Danger"),
	};

	struct FConcertBrowserIconsDefinition
	{
		FConcertBrowserIconsDefinition()
			: IsEnabled(true)
			, Visibility(EVisibility::Visible)
			, Glyph(FEditorFontGlyphs::Question)
			, ToolTipText()
			, Style(EConcertUIStyle::Normal)
		{
		}

		TAttribute<bool> IsEnabled;
		TAttribute<EVisibility> Visibility;
		TAttribute<FText> Glyph;
		TAttribute<FText> ToolTipText;
		EConcertUIStyle Style;
	};

	FORCEINLINE bool ShowSessionConnectionUI()
	{
		return !(IS_PROGRAM);
	}

	inline TSharedRef<SWidget> CreateDisplayName(const TAttribute<FText>& InDisplayName)
	{
		return SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("NoBorder"))
			.ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.75f))
			.Padding(FMargin(6.0f, 4.0f))
			[
				SNew(STextBlock)
				.Font(FEditorStyle::GetFontStyle("BoldFont"))
				.Text(InDisplayName)
			];
	}

	FORCEINLINE FSlateColor GetIconColor(EConcertUIStyle ConcertStyle)
	{
		return FEditorStyle::Get().GetWidgetStyle<FButtonStyle>(ButtonStyleNames[(int32)ConcertStyle]).Normal.TintColor;
	}

	inline TSharedRef<SButton> CreateButton(const FConcertUIButtonDefinition& InDef)
	{
		const FButtonStyle* ButtonStyle = &FEditorStyle::Get().GetWidgetStyle<FButtonStyle>(ButtonStyleNames[(int32)InDef.Style]);
		check(ButtonStyle);
		const float ButtonContentWidthPadding = 6.f;
		const float PaddingCompensation = (ButtonStyle->NormalPadding.Left + ButtonStyle->NormalPadding.Right + ButtonContentWidthPadding * 2);

		return SNew(SButton)
			.ToolTipText(InDef.ToolTipText)
			.ButtonStyle(ButtonStyle)
			.ForegroundColor(FLinearColor::White)
			.ContentPadding(FMargin(ButtonContentWidthPadding, 2.f))
			.IsEnabled(InDef.IsEnabled)
			.Visibility(InDef.Visibility)
			.OnClicked(InDef.OnClicked)
			[
				SNew(SBox)
				.MinDesiredWidth(MinDesiredWidthForBtnAndIcon - PaddingCompensation)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Font(FEditorStyle::Get().GetFontStyle(ButtonIconSyle))
					.Text(InDef.Text)
					.Justification(ETextJustify::Center)
				]
			];
	}

	inline void AppendButtons(TSharedRef<SHorizontalBox> InHorizBox, TArrayView<const FConcertUIButtonDefinition> InDefs)
	{
		for (const FConcertUIButtonDefinition& Def : InDefs)
		{
			InHorizBox->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(1.0f))
				[
					CreateButton(Def)
				];
		}
	}

	inline TSharedRef<SWidget> CreateIcon(const FConcertBrowserIconsDefinition& InDef, TSharedRef<SHorizontalBox> InHorizBox)
	{
		return SNew(SBox)
			.VAlign(VAlign_Fill)
			.MinDesiredWidth(MinDesiredWidthForBtnAndIcon)
			.HAlign(HAlign_Center)
			.ToolTipText(InDef.ToolTipText)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("NoBorder"))
				.ColorAndOpacity(GetIconColor(InDef.Style).GetSpecifiedColor())
				[
					SNew(STextBlock)
					.Font(FEditorStyle::Get().GetFontStyle(ButtonIconSyle))
					.Text(InDef.Glyph)
					.Visibility(InDef.Visibility)
					.IsEnabled(InDef.IsEnabled)
					.Justification(ETextJustify::Center)
				]
			];
	}

	inline void AppendIcons(TSharedRef<SHorizontalBox> InHorizBox, TArrayView<const FConcertBrowserIconsDefinition> InDefs)
	{
		for (const FConcertBrowserIconsDefinition& Def : InDefs)
		{
			InHorizBox->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(1.0f))
				[
					CreateIcon(Def, InHorizBox)
				];
		}
	}

	template <typename ItemType, typename PredFactoryType>
	inline void SyncArraysByPredicate(TArray<TSharedPtr<ItemType>>& InOutArray, TArray<TSharedPtr<ItemType>>&& InNewArray, const PredFactoryType& InPredFactory)
	{
		if (InOutArray.Num() == 0)
		{
			// Empty array - can just move
			InOutArray = MoveTempIfPossible(InNewArray);
		}
		else
		{
			// Add or update the existing entries
			for (TSharedPtr<ItemType>& NewItem : InNewArray)
			{
				TSharedPtr<ItemType>* ExistingItemPtr = InOutArray.FindByPredicate(InPredFactory(NewItem));
				if (ExistingItemPtr)
				{
					**ExistingItemPtr = *NewItem;
				}
				else
				{
					InOutArray.Add(NewItem);
				}
			}
			// Remove entries that are no longer needed
			for (auto ExistingItemIt = InOutArray.CreateIterator(); ExistingItemIt; ++ExistingItemIt)
			{
				TSharedPtr<ItemType>* NewItemPtr = InNewArray.FindByPredicate(InPredFactory(*ExistingItemIt));
				if (!NewItemPtr)
				{
					ExistingItemIt.RemoveCurrent();
					continue;
				}
			}
		}
	}

	template <typename ItemType>
	inline TArray<TSharedPtr<ItemType>> DeepCopyArray(const TArray<TSharedPtr<ItemType>>& InArray)
	{
		TArray<TSharedPtr<ItemType>> ArrayCopy;
		{
			ArrayCopy.Reserve(InArray.Num());
			for (const TSharedPtr<ItemType>& Item : InArray)
			{
				ArrayCopy.Add(MakeShared<ItemType>(*Item));
			}
		}
		return ArrayCopy;
	}

	template <typename ItemType>
	inline TArray<TSharedPtr<ItemType>> DeepCopyArrayAndClearSource(TArray<TSharedPtr<ItemType>>& InOutArray)
	{
		TArray<TSharedPtr<ItemType>> ArrayCopy = DeepCopyArray(InOutArray);
		InOutArray.Reset();
		return ArrayCopy;
	}
};
