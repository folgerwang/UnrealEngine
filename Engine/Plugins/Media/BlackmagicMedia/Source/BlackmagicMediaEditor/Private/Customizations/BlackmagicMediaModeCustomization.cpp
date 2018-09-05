// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "BlackmagicMediaModeCustomization.h"

#include "BlackmagicMediaFinder.h"
#include "DetailWidgetRow.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SComboButton.h"

#define LOCTEXT_NAMESPACE "BlackmagicMediaPortCustomization"

FBlackmagicMediaModeCustomization::FBlackmagicMediaModeCustomization(bool InOutput)
	: bOutput(InOutput)
{
}


void FBlackmagicMediaModeCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	MediaModeProperty = InPropertyHandle;
	if (MediaModeProperty->GetNumPerObjectValues() == 1 && MediaModeProperty->IsValidHandle())
	{
		UProperty* Property = MediaModeProperty->GetProperty();

		check(Property && Cast<UStructProperty>(Property) && Cast<UStructProperty>(Property)->Struct &&
				(Cast<UStructProperty>(Property)->Struct->IsChildOf(FBlackmagicMediaModeInput::StaticStruct())
				|| Cast<UStructProperty>(Property)->Struct->IsChildOf(FBlackmagicMediaModeOutput::StaticStruct())));

		TArray<void*> RawData;
		MediaModeProperty->AccessRawData(RawData);

		check(RawData.Num() == 1);
		FBlackmagicMediaMode* MediaModeValue = reinterpret_cast<FBlackmagicMediaMode*>(RawData[0]);

		check(MediaModeValue);

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
				.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda([=] { return FText::FromString(MediaModeValue->ToUrl()); })))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			.VAlign(VAlign_Center)
			[
				SNew(SComboButton)
				.OnGetMenuContent(this, &FBlackmagicMediaModeCustomization::HandleSourceComboButtonMenuContent)
				.ContentPadding(FMargin(4.0, 2.0))
			]
		];
	}
}

void FBlackmagicMediaModeCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

TSharedRef<SWidget> FBlackmagicMediaModeCustomization::HandleSourceComboButtonMenuContent() const
{
	// fetch found sources
	TArray<FBlackmagicMediaMode> OutModes;
	if (!UBlackmagicMediaFinder::GetModes(OutModes, bOutput))
	{
		return SNullWidget::NullWidget;
	}

	// generate menu
	FMenuBuilder MenuBuilder(true, nullptr);

	const ANSICHAR* SectionName = bOutput ? "AllOutputModes" : "AllInputModes";
	TAttribute<FText> HeaderText = bOutput ? LOCTEXT("AllOutputModesSection", "Output Modes") : LOCTEXT("AllInputModesSection", "Input Modes");

	MenuBuilder.BeginSection(SectionName, HeaderText);
	{
		bool ModeAdded = false;

		for (const FBlackmagicMediaMode& Mode : OutModes)
		{
			const TSharedPtr<IPropertyHandle> ValueProperty = MediaModeProperty;
			const FString Url = Mode.ToUrl();

			MenuBuilder.AddMenuEntry(
				FText::FromString(Mode.ToString()),
				FText::FromString(Url),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([=] {

						TArray<void*> RawData;
						MediaModeProperty->AccessRawData(RawData);

						check(RawData.Num() == 1);
						MediaModeProperty->NotifyPreChange();
						FBlackmagicMediaMode* MediaModeValue = reinterpret_cast<FBlackmagicMediaMode*>(RawData[0]);
						*MediaModeValue = Mode;
						MediaModeProperty->NotifyPostChange();
						MediaModeProperty->NotifyFinishedChangingProperties();
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

			ModeAdded = true;
		}

		if (!ModeAdded)
		{
			MenuBuilder.AddWidget(SNullWidget::NullWidget, LOCTEXT("NoModesFound", "No display mode found"), false, false);
		}
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
