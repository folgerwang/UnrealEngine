// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AjaMediaPortCustomization.h"

#include "AjaMediaFinder.h"
#include "DetailWidgetRow.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SComboButton.h"

#define LOCTEXT_NAMESPACE "AjaMediaPortCustomization"

void FAjaMediaPortCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	MediaPortProperty = InPropertyHandle;
	if (MediaPortProperty->GetNumPerObjectValues() == 1 && MediaPortProperty->IsValidHandle())
	{
		UProperty* Property = MediaPortProperty->GetProperty();
		check(Property && Cast<UStructProperty>(Property) && Cast<UStructProperty>(Property)->Struct && Cast<UStructProperty>(Property)->Struct->IsChildOf(FAjaMediaPort::StaticStruct()));

		TArray<void*> RawData;
		MediaPortProperty->AccessRawData(RawData);

		check(RawData.Num() == 1);
		FAjaMediaPort* MediaPortValue = reinterpret_cast<FAjaMediaPort*>(RawData[0]);

		check(MediaPortValue);

		TSharedPtr<IPropertyUtilities> PropertyUtils = CustomizationUtils.GetPropertyUtilities();

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
				.OnGetMenuContent(this, &FAjaMediaPortCustomization::HandleSourceComboButtonMenuContent)
				.ContentPadding(FMargin(4.0, 2.0))
			]
		].IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([=] { return !InPropertyHandle->IsEditConst() && PropertyUtils->IsPropertyEditingEnabled(); })));
	}
}

void FAjaMediaPortCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

TSharedRef<SWidget> FAjaMediaPortCustomization::HandleSourceComboButtonMenuContent() const
{
	// fetch found Aja sources
	TArray<FAjaMediaPort> OutSources;
	if (!UAjaMediaFinder::GetSources(OutSources))
	{
		return SNullWidget::NullWidget;
	}

	// generate menu
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.BeginSection("AllSources", LOCTEXT("AllSourcesSection", "All Sources"));
	{
		bool SourceAdded = false;

		for (const FAjaMediaPort& Source : OutSources)
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
						FAjaMediaPort* MediaPortValue = reinterpret_cast<FAjaMediaPort*>(RawData[0]);
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
