// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "BlackmagicMediaPortCustomization.h"

#include "BlackmagicMediaFinder.h"
#include "DetailWidgetRow.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SComboButton.h"

#define LOCTEXT_NAMESPACE "BlackmagicMediaPortCustomization"

void FBlackmagicMediaPortCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	MediaPortProperty = InPropertyHandle;
	if (MediaPortProperty->GetNumPerObjectValues() == 1 && MediaPortProperty->IsValidHandle())
	{
		UProperty* Property = MediaPortProperty->GetProperty();
		check(Property && Cast<UStructProperty>(Property) && Cast<UStructProperty>(Property)->Struct && Cast<UStructProperty>(Property)->Struct->IsChildOf(FBlackmagicMediaPort::StaticStruct()));

		TArray<void*> RawData;
		MediaPortProperty->AccessRawData(RawData);

		check(RawData.Num() == 1);
		FBlackmagicMediaPort* MediaPortValue = reinterpret_cast<FBlackmagicMediaPort*>(RawData[0]);

		check(MediaPortValue);

		HeaderRow
		.NameContent()
		[
			InPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MaxDesiredWidth(512)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda([=] { return FText::FromString(MediaPortValue->ToUrl()); })))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			.VAlign(VAlign_Center)
			[
				SNew(SComboButton)
				.OnGetMenuContent(this, &FBlackmagicMediaPortCustomization::HandleSourceComboButtonMenuContent)
				.ContentPadding(FMargin(4.0, 2.0))
			]
		];
	}
}

void FBlackmagicMediaPortCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

TSharedRef<SWidget> FBlackmagicMediaPortCustomization::HandleSourceComboButtonMenuContent() const
{
	// fetch found sources
	TArray<FBlackmagicMediaPort> OutSources;
	if (!UBlackmagicMediaFinder::GetSources(OutSources))
	{
		return SNullWidget::NullWidget;
	}

	// generate menu
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.BeginSection("AllSources", LOCTEXT("AllSourcesSection", "All Sources"));
	{
		bool SourceAdded = false;

		for (const FBlackmagicMediaPort& Source : OutSources)
		{
			const TSharedPtr<IPropertyHandle> ValueProperty = MediaPortProperty;
			const FString Url = Source.ToUrl();

			MenuBuilder.AddMenuEntry(
				FText::FromString(Source.ToString()),
				FText::FromString(Url),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([=] {

						TArray<void*> RawData;
						MediaPortProperty->AccessRawData(RawData);

						check(RawData.Num() == 1);
						MediaPortProperty->NotifyPreChange();
						FBlackmagicMediaPort* MediaPortValue = reinterpret_cast<FBlackmagicMediaPort*>(RawData[0]);
						*MediaPortValue = Source;
						MediaPortProperty->NotifyPostChange();
						MediaPortProperty->NotifyFinishedChangingProperties();
					}),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([=] {
						FString CurrentValue;
						return ((ValueProperty->GetValue(CurrentValue) == FPropertyAccess::Success) && CurrentValue == Url);
					})
				),
				NAME_None,
				EUserInterfaceActionType::RadioButton
				);

			SourceAdded = true;
		}

		if (!SourceAdded)
		{
			MenuBuilder.AddWidget(SNullWidget::NullWidget, LOCTEXT("NoSourcesFound", "No sources found"), false, false);
		}
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
