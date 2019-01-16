// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OpenColorIOColorSpaceCustomization.h"

#include "Containers/StringConv.h"
#include "DetailWidgetRow.h"
#include "Engine/EngineTypes.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IPropertyUtilities.h"
#include "OpenColorIOColorSpace.h"
#include "OpenColorIO/OpenColorIO.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SComboButton.h"

#define LOCTEXT_NAMESPACE "OpenColorIOColorSpaceCustomization"

void FOpenColorIOColorSpaceCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	ColorSpaceProperty = InPropertyHandle;
	ConfigurationFileProperty.Reset();

	if (ColorSpaceProperty->GetNumPerObjectValues() == 1 && ColorSpaceProperty->IsValidHandle())
	{
		UProperty* Property = ColorSpaceProperty->GetProperty();
		TSharedPtr<IPropertyHandle> ParentArrayHandle = InPropertyHandle->GetParentHandle();
		check(Property && Cast<UStructProperty>(Property) && Cast<UStructProperty>(Property)->Struct && Cast<UStructProperty>(Property)->Struct->IsChildOf(FOpenColorIOColorSpace::StaticStruct()));

		TArray<void*> RawData;
		ColorSpaceProperty->AccessRawData(RawData);

		check(RawData.Num() == 1);
		FOpenColorIOColorSpace* ColorSpaceValue = reinterpret_cast<FOpenColorIOColorSpace*>(RawData[0]);

		check(ColorSpaceValue);
		TSharedPtr<IPropertyUtilities> PropertyUtils = CustomizationUtils.GetPropertyUtilities();

		// Get the ConfigurationFile to read color space from
		{
			static FName NAME_ConfigFile = TEXT("OCIOConfigFile");
			TSharedPtr<IPropertyHandleArray> ParentHandleAsArray = ParentArrayHandle->AsArray();
			if (ParentHandleAsArray.IsValid())
			{
				//Struct is in an array, parent class is access through more layers
				if (ParentArrayHandle->GetProperty()->HasMetaData(NAME_ConfigFile))
				{
					const FString& ConfigFileVariableName = ParentArrayHandle->GetProperty()->GetMetaData(NAME_ConfigFile);
					TSharedPtr<IPropertyHandle> ClassHandle = ParentArrayHandle->GetParentHandle()->GetParentHandle();
					if (ClassHandle.IsValid() && !ConfigFileVariableName.IsEmpty())
					{
						ConfigurationFileProperty = ClassHandle->GetChildHandle(*ConfigFileVariableName);
					}
				}
			}
		}


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
					.Text(MakeAttributeLambda([=] { return FText::FromString(ColorSpaceValue->ToString()); }))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(SComboButton)
					.OnGetMenuContent(this, &FOpenColorIOColorSpaceCustomization::HandleSourceComboButtonMenuContent)
					.ContentPadding(FMargin(4.0, 2.0))
				]
			].IsEnabled(MakeAttributeLambda([=] { return !InPropertyHandle->IsEditConst() && PropertyUtils->IsPropertyEditingEnabled(); }));
	}
}

void FOpenColorIOColorSpaceCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

TSharedRef<SWidget> FOpenColorIOColorSpaceCustomization::HandleSourceComboButtonMenuContent() const
{
	OCIO_NAMESPACE::ConstConfigRcPtr ConfigFile;
	if (ConfigurationFileProperty.IsValid())
	{
		TArray<void*> RawData;
		ConfigurationFileProperty->AccessRawData(RawData);

		check(RawData.Num() == 1);
		FFilePath* ConfigFilePath = reinterpret_cast<FFilePath*>(RawData[0]);
		check(ConfigFilePath);

		try
		{
			ConfigFile = OCIO_NAMESPACE::Config::CreateFromFile(StringCast<ANSICHAR>(*ConfigFilePath->FilePath).Get());
			if (!ConfigFile)
			{
				return SNullWidget::NullWidget;
			}
		}
		catch (OCIO_NAMESPACE::Exception& )
		{
			return SNullWidget::NullWidget;
		}
	}
	else
	{
		return SNullWidget::NullWidget;
	}

	// Fetch ColorSpaces from current Config file
	const int32 ColorSpaceCount = ConfigFile->getNumColorSpaces();
	if (ColorSpaceCount <= 0)
	{
		return SNullWidget::NullWidget;
	}

	// Generate menu
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.BeginSection("AllColorSpaces", LOCTEXT("AllColorSpacesSection", "All ColorSpaces"));
	{
		bool ColorSpaceAdded = false;

		for (int32 i = 0; i < ColorSpaceCount; ++i)
		{
			const char* ColorSpaceName = ConfigFile->getColorSpaceNameByIndex(i);
			OCIO_NAMESPACE::ConstColorSpaceRcPtr LibColorSpace = ConfigFile->getColorSpace(ColorSpaceName);
			if (!LibColorSpace)
			{
				continue;
			}

			const char* FamilyName = LibColorSpace->getFamily();

			FOpenColorIOColorSpace ColorSpace;
			ColorSpace.ColorSpaceIndex = i;
			ColorSpace.ColorSpaceName = StringCast<TCHAR>(ColorSpaceName).Get();
			ColorSpace.FamilyName = StringCast<TCHAR>(FamilyName).Get();
			//if (!ColorSpace.FamilyName.IsEmpty())
			//{
			//	MenuBuilder.
			//	MenuBuilder.AddSubMenu( LOCTEXT("SubMenu", "Sub Menu"), LOCTEXT("OpensASubmenu", "Opens a submenu"), FNewMenuDelegate::CreateStatic( &FMenus::FillSubMenuEntries ) );
			//}

			const TSharedPtr<IPropertyHandle> ValueProperty = ColorSpaceProperty;

			MenuBuilder.AddMenuEntry(
				FText::FromString(ColorSpace.ToString()),
				FText::FromString(ColorSpace.ToString()),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([=] {
						if (UStructProperty* StructProperty = Cast<UStructProperty>(ColorSpaceProperty->GetProperty()))
						{
							TArray<void*> RawData;
							ColorSpaceProperty->AccessRawData(RawData);
							FOpenColorIOColorSpace* PreviousColorSpaceValue = reinterpret_cast<FOpenColorIOColorSpace*>(RawData[0]);

							FString TextValue;
							StructProperty->Struct->ExportText(TextValue, &ColorSpace, PreviousColorSpaceValue, nullptr, EPropertyPortFlags::PPF_None, nullptr);
							ensure(ColorSpaceProperty->SetValueFromFormattedString(TextValue, EPropertyValueSetFlags::DefaultFlags) == FPropertyAccess::Result::Success);
						}
					}),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([=] {
						TArray<void*> RawData;
						ColorSpaceProperty->AccessRawData(RawData);
						FOpenColorIOColorSpace* ColorSpaceValue = reinterpret_cast<FOpenColorIOColorSpace*>(RawData[0]);
						return *ColorSpaceValue == ColorSpace;
					})
				),
				NAME_None,
				EUserInterfaceActionType::RadioButton
				);

			ColorSpaceAdded = true;
		}

		if (!ColorSpaceAdded)
		{
			MenuBuilder.AddWidget(SNullWidget::NullWidget, LOCTEXT("NoColorSpaceFound", "No color spaces found"), false, false);
		}
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
