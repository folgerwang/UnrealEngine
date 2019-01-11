// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OpenColorIOColorSpaceConversionCustomization.h"

#include "Containers/StringConv.h"
#include "DetailWidgetRow.h"
#include "Engine/EngineTypes.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "IPropertyUtilities.h"
#include "OpenColorIOColorSpace.h"
#include "OpenColorIOConfiguration.h"
#include "PropertyHandle.h"
#include "SResetToDefaultMenu.h"
#include "Widgets/Input/SComboButton.h"

#define LOCTEXT_NAMESPACE "OpenColorIOColorSpaceConversionCustomization"

void FOpenColorIOColorSpaceConversionCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	ColorConversionProperty = InPropertyHandle;

	if (ColorConversionProperty.IsValid())
	{
		UProperty* Property = ColorConversionProperty->GetProperty();
		check(Property && Cast<UStructProperty>(Property) && Cast<UStructProperty>(Property)->Struct && Cast<UStructProperty>(Property)->Struct->IsChildOf(FOpenColorIOColorConversionSettings::StaticStruct()));

		if (ColorConversionProperty->GetNumPerObjectValues() == 1 && ColorConversionProperty->IsValidHandle())
		{
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
						.Text(MakeAttributeLambda([=]
						{
							TArray<void*> RawData;
							ColorConversionProperty->AccessRawData(RawData);
							FOpenColorIOColorConversionSettings* Conversion = reinterpret_cast<FOpenColorIOColorConversionSettings*>(RawData[0]);
							if (Conversion != nullptr)
							{
								return FText::FromString(*Conversion->ToString());
							}
							else
							{
								return FText::FromString(TEXT("<Invalid Conversion>"));
							}

						}))
					]
				].IsEnabled(MakeAttributeLambda([=] { return !InPropertyHandle->IsEditConst() && PropertyUtils->IsPropertyEditingEnabled(); }));
		}
	}
}

void FOpenColorIOColorSpaceConversionCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TArray<void*> RawData;
	ColorConversionProperty->AccessRawData(RawData);
	FOpenColorIOColorConversionSettings* ColorSpaceValue = reinterpret_cast<FOpenColorIOColorConversionSettings*>(RawData[0]);

	TSharedPtr<IPropertyUtilities> PropertyUtils = StructCustomizationUtils.GetPropertyUtilities();

	uint32 NumberOfChild;
	if (InStructPropertyHandle->GetNumChildren(NumberOfChild) == FPropertyAccess::Success)
	{
		for (uint32 Index = 0; Index < NumberOfChild; ++Index)
		{
			TSharedRef<IPropertyHandle> ChildHandle = InStructPropertyHandle->GetChildHandle(Index).ToSharedRef();
	
			//Create custom rows for source and destination color space of the conversion. Since the struct is hooked to an OCIOConfiguration
			//We use it to populate the available color spaces instead of using a raw configuration file.
			if (ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FOpenColorIOColorConversionSettings, SourceColorSpace))
			{
				SourceColorSpaceProperty = ChildHandle;

				FDetailWidgetRow& ColorSpaceWidget = StructBuilder.AddCustomRow(FText::FromName(ChildHandle->GetProperty()->GetFName()));
				AddColorSpaceRow(ColorSpaceWidget, ChildHandle, StructCustomizationUtils);
			}
			else if (ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FOpenColorIOColorConversionSettings, DestinationColorSpace))
			{
				DestinationColorSpaceProperty = ChildHandle;

				FDetailWidgetRow& ColorSpaceWidget = StructBuilder.AddCustomRow(FText::FromName(ChildHandle->GetProperty()->GetFName()));
				AddColorSpaceRow(ColorSpaceWidget, ChildHandle, StructCustomizationUtils);
			}
			else
			{
				StructBuilder.AddProperty(ChildHandle).IsEnabled(true).ShowPropertyButtons(false);
			}
		}
	}
	InStructPropertyHandle->MarkHiddenByCustomization();
}

void FOpenColorIOColorSpaceConversionCustomization::AddColorSpaceRow(FDetailWidgetRow& InWidgetRow, TSharedRef<IPropertyHandle> InChildHandle, IPropertyTypeCustomizationUtils& InCustomizationUtils) const
{
	TSharedPtr<IPropertyUtilities> PropertyUtils = InCustomizationUtils.GetPropertyUtilities();
	TSharedPtr<SResetToDefaultMenu> ResetToDefaultMenu;

	InWidgetRow
		.NameContent()
		[
			InChildHandle->CreatePropertyNameWidget()
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
				.Text(MakeAttributeLambda([=]
				{
					TArray<void*> RawData;
					InChildHandle->AccessRawData(RawData);
					FOpenColorIOColorSpace* ColorSpace = reinterpret_cast<FOpenColorIOColorSpace*>(RawData[0]);
					if (ColorSpace != nullptr)
					{
						return FText::FromString(*ColorSpace->ToString());
					}
					else
					{
						return FText::FromString(TEXT("<Invalid>"));
					}
				}))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			.VAlign(VAlign_Center)
			[
				SNew(SComboButton)
				.OnGetMenuContent_Lambda([=]() { return HandleColorSpaceComboButtonMenuContent(InChildHandle); })
				.ContentPadding(FMargin(4.0, 2.0))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SAssignNew(ResetToDefaultMenu, SResetToDefaultMenu)
			]
		].IsEnabled(MakeAttributeLambda([=] { return !InChildHandle->IsEditConst() && PropertyUtils->IsPropertyEditingEnabled(); }));
	
		ResetToDefaultMenu->AddProperty(InChildHandle);
}

TSharedRef<SWidget> FOpenColorIOColorSpaceConversionCustomization::HandleColorSpaceComboButtonMenuContent(TSharedPtr<IPropertyHandle> InPropertyHandle) const
{
	TArray<void*> RawData;
	ColorConversionProperty->AccessRawData(RawData);
	FOpenColorIOColorConversionSettings* ColorSpaceConversion = reinterpret_cast<FOpenColorIOColorConversionSettings*>(RawData[0]);

	if (ColorSpaceConversion && ColorSpaceConversion->ConfigurationSource)
	{
		FOpenColorIOColorSpace RestrictedColorSpace;
		if (InPropertyHandle == SourceColorSpaceProperty)
		{
			RestrictedColorSpace = ColorSpaceConversion->DestinationColorSpace;
		}
		else
		{
			RestrictedColorSpace = ColorSpaceConversion->SourceColorSpace;
		}

		// generate menu
		FMenuBuilder MenuBuilder(true, nullptr);

		MenuBuilder.BeginSection("AvailableColorSpaces", LOCTEXT("AvailableCoorSpaces", "Available Color Spaces"));
		{
			bool ColorSpaceAdded = false;

			for (int32 i = 0; i < ColorSpaceConversion->ConfigurationSource->DesiredColorSpaces.Num(); ++i)
			{
				const FOpenColorIOColorSpace& ColorSpace = ColorSpaceConversion->ConfigurationSource->DesiredColorSpaces[i];
				if (ColorSpace == RestrictedColorSpace || !ColorSpace.IsValid())
				{
					continue;
				}

				MenuBuilder.AddMenuEntry
				(
					FText::FromString(ColorSpace.ToString()),
					FText::FromString(ColorSpace.ToString()),
					FSlateIcon(),
					FUIAction
					(
						FExecuteAction::CreateLambda([=] 
						{
							if (UStructProperty* StructProperty = Cast<UStructProperty>(InPropertyHandle->GetProperty()))
							{
								TArray<void*> RawData;
								InPropertyHandle->AccessRawData(RawData);
								FOpenColorIOColorSpace* PreviousColorSpaceValue = reinterpret_cast<FOpenColorIOColorSpace*>(RawData[0]);

								FString TextValue;
								StructProperty->Struct->ExportText(TextValue, &ColorSpace, PreviousColorSpaceValue, nullptr, EPropertyPortFlags::PPF_None, nullptr);
								ensure(InPropertyHandle->SetValueFromFormattedString(TextValue, EPropertyValueSetFlags::DefaultFlags) == FPropertyAccess::Result::Success);
							}
						}),
						FCanExecuteAction(),
						FIsActionChecked::CreateLambda([=] 
						{
							TArray<void*> RawData;
							SourceColorSpaceProperty->AccessRawData(RawData);
							FOpenColorIOColorSpace* PreviousColorSpaceValue = reinterpret_cast<FOpenColorIOColorSpace*>(RawData[0]);
							return *PreviousColorSpaceValue == ColorSpace;
						})
					),
					NAME_None,
					EUserInterfaceActionType::RadioButton
				);

				ColorSpaceAdded = true;
			}

			if (!ColorSpaceAdded)
			{
				MenuBuilder.AddWidget(SNullWidget::NullWidget, LOCTEXT("NoColorSpaceFound", "No available color spaces"), false, false);
			}
		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

	return SNullWidget::NullWidget;
}

#undef LOCTEXT_NAMESPACE
