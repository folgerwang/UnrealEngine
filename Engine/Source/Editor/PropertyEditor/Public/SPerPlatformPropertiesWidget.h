// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/CoreStyle.h"
#include "Framework/SlateDelegates.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "PlatformInfo.h"


/**
* SPerPlatformPropertiesWidget
*/
class SPerPlatformPropertiesWidget : public SCompoundWidget
{
public:
	typedef typename TSlateDelegates<FName>::FOnGenerateWidget FOnGenerateWidget;

	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnPlatformAction, FName);

	SLATE_BEGIN_ARGS(SPerPlatformPropertiesWidget)
	: _OnGenerateWidget()
	{}

	SLATE_EVENT(FOnGenerateWidget, OnGenerateWidget)
	SLATE_EVENT(FOnPlatformAction, OnAddPlatform)
	SLATE_EVENT(FOnPlatformAction, OnRemovePlatform)

	SLATE_ATTRIBUTE(TArray<FName>, PlatformOverrideNames)

	SLATE_END_ARGS()

	/**
	* Construct this widget
	*
	* @param	InArgs	The declaration data for this widget
	*/
	void Construct(const typename SPerPlatformPropertiesWidget::FArguments& InArgs)
	{
		this->OnGenerateWidget = InArgs._OnGenerateWidget;
		this->OnAddPlatform = InArgs._OnAddPlatform;
		this->OnRemovePlatform = InArgs._OnRemovePlatform;
		this->PlatformOverrideNames = InArgs._PlatformOverrideNames;

		ConstructChildren();

		// this widget has to check platform count change from outside to ensure the widget represents latest update
		RegisterActiveTimer(FMath::RandRange(2.f, 5.f), FWidgetActiveTimerDelegate::CreateSP(this, &SPerPlatformPropertiesWidget::CheckPlatformCount));
	}

	void ConstructChildren()
	{
		TSharedPtr<SHorizontalBox> HorizontalBox;
		TSharedPtr<SVerticalBox> VerticalBox;

		TArray<FName> PlatformOverrides = PlatformOverrideNames.Get();
		LastPlatformOverrideNames = PlatformOverrides.Num();
		ChildSlot
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SAssignNew(HorizontalBox, SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(VerticalBox, SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Visibility(PlatformOverrides.Num() ? EVisibility::Visible : EVisibility::Collapsed)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(NSLOCTEXT("SPerPlatformPropertiesWidget", "DefaultPlatform", "Default"))
				]
			]
		];

		if (OnGenerateWidget.IsBound())
		{
			// Default control
			VerticalBox->AddSlot()
			.AutoHeight()
			.VAlign(EVerticalAlignment::VAlign_Bottom)
			[
				OnGenerateWidget.Execute(NAME_None)
			];

			// Add Platform menu
			bool bAddedMenuItem = false;
			FMenuBuilder AddPlatformMenuBuilder(true, nullptr, nullptr, true);
			{
				for (FName PlatformName : PlatformInfo::GetAllPlatformGroupNames())
				{
					if (!PlatformOverrides.Contains(PlatformName))
					{
						const FText MenuText = FText::Format(NSLOCTEXT("SPerPlatformPropertiesWidget", "AddOverrideFor", "Add Override for {0}"), FText::AsCultureInvariant(PlatformName.ToString()));
						AddPlatformMenuBuilder.AddMenuEntry(
							MenuText,
							MenuText,
							FSlateIcon(FEditorStyle::GetStyleSetName(), "PerPlatformWidget.AddPlatform"),
							FUIAction(FExecuteAction::CreateSP(this, &SPerPlatformPropertiesWidget::AddPlatform, PlatformName))
						);

						bAddedMenuItem = true;
					}
				}
			}

			if (bAddedMenuItem)
			{
				HorizontalBox->AddSlot()
				.AutoWidth()
				[
					SNew(SComboButton)
					.VAlign(EVerticalAlignment::VAlign_Bottom)
					.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
					.ContentPadding(2)
					.ForegroundColor(FSlateColor::UseForeground())
					.HasDownArrow(true)
					.MenuContent()
					[
						AddPlatformMenuBuilder.MakeWidget()
					]
				];
			}

			for (FName PlatformName : PlatformOverrides)
			{
				HorizontalBox->AddSlot()
				.Padding(FMargin(5.0f, 0.0f, 0.0f, 0.0f))
				.AutoWidth()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(EVerticalAlignment::VAlign_Bottom)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(STextBlock)
							.Font(IDetailLayoutBuilder::GetDetailFont())
							.Text(FText::FromName(PlatformName))
						]

						+ SHorizontalBox::Slot()
						.HAlign(EHorizontalAlignment::HAlign_Right)
						.AutoWidth()
						[
							SNew(SButton)
							.ContentPadding(FMargin(1.0f, 0.0f, 1.0f, 0.0f))
							.ButtonStyle(FEditorStyle::Get(), "NoBorder")
							.OnClicked(this, &SPerPlatformPropertiesWidget::RemovePlatform, PlatformName)
							.ToolTipText(FText::Format(NSLOCTEXT("SPerPlatformPropertiesWidget", "RemoveOverrideFor", "Remove Override for {0}"), FText::AsCultureInvariant(PlatformName.ToString())))
							.ForegroundColor(FSlateColor::UseForeground())
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							.Content()
							[
								SNew(SImage)
								.Image(FEditorStyle::GetBrush("Icons.Cross"))
							]
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						OnGenerateWidget.Execute(PlatformName)
					]
				];
			}
		}
		else
		{
			VerticalBox->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("SPerPlatformPropertiesWidget", "OnGenerateWidgetWarning", "No OnGenerateWidget() Provided"))
				.ColorAndOpacity(FLinearColor::Red)
			];
		}
	}

protected:
	void AddPlatform(FName PlatformName)
	{
		if (OnAddPlatform.IsBound() && OnAddPlatform.Execute(PlatformName))
		{
			ConstructChildren();
			Invalidate(EInvalidateWidget::LayoutAndVolatility);
		}
	}

	FReply RemovePlatform(FName PlatformName)
	{
		if (OnRemovePlatform.IsBound() && OnRemovePlatform.Execute(PlatformName))
		{
			ConstructChildren();
			Invalidate(EInvalidateWidget::LayoutAndVolatility);
		}
		return FReply::Handled();
	}

	EActiveTimerReturnType CheckPlatformCount(double InCurrentTime, float InDeltaSeconds)
	{
		// @fixme: the platform count is fixed and locally cached
		// so if you change this outside of editor, this widget won't update
		// this timer is the one checking to see if platform count has changed
		// if so, it will reconstruct
		TArray<FName> PlatformOverrides = PlatformOverrideNames.Get();
		if (LastPlatformOverrideNames != PlatformOverrides.Num())
		{
			ConstructChildren();
		}

		return EActiveTimerReturnType::Continue;
	}

	FOnGenerateWidget OnGenerateWidget;
	FOnPlatformAction OnAddPlatform;
	FOnPlatformAction OnRemovePlatform;
	TAttribute<TArray<FName>> PlatformOverrideNames;
	int32 LastPlatformOverrideNames;
};

