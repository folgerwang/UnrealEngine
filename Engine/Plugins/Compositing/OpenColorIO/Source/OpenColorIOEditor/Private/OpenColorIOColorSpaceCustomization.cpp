// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OpenColorIOColorSpaceCustomization.h"

#include "Containers/StringConv.h"
#include "DetailWidgetRow.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SComboButton.h"

#define LOCTEXT_NAMESPACE "OpenColorIOColorSpaceCustomization"

void FOpenColorIOColorSpaceCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	//Reset internals
	ConfigurationFileProperty.Reset();
	CachedConfigFile.reset();
	LoadedFilePath = FFilePath();
	
	ColorSpaceProperty = InPropertyHandle;

	if (ColorSpaceProperty->GetNumPerObjectValues() == 1 && ColorSpaceProperty->IsValidHandle())
	{
		UProperty* Property = ColorSpaceProperty->GetProperty();
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

			//Verify if we're in an array before accessing the property directly. Need to go deeper for array properties.
			TSharedPtr<IPropertyHandle> ParentArrayHandle = InPropertyHandle->GetParentHandle();
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
			else
			{
				if (ColorSpaceProperty->HasMetaData(NAME_ConfigFile))
				{
					const FString& ConfigFileVariableName = ColorSpaceProperty->GetMetaData(NAME_ConfigFile);
					TSharedPtr<IPropertyHandle> ClassHandle = ColorSpaceProperty->GetParentHandle();
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


bool FOpenColorIOColorSpaceCustomization::LoadConfigurationFile(const FFilePath& InFilePath)
{
#if !PLATFORM_EXCEPTIONS_DISABLED
	try
#endif
	{
		FString FullPath;
		if (!FPaths::IsRelative(InFilePath.FilePath))
		{
			FullPath = InFilePath.FilePath;
		}
		else
		{
			const FString AbsoluteGameDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
			FullPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(AbsoluteGameDir, InFilePath.FilePath));
		}

		CachedConfigFile = OCIO_NAMESPACE::Config::CreateFromFile(StringCast<ANSICHAR>(*FullPath).Get());
		if (!CachedConfigFile)
		{
			return false;
		}
	}
#if !PLATFORM_EXCEPTIONS_DISABLED
	catch (OCIO_NAMESPACE::Exception&)
	{
		return false;
	}
#endif

	return true;
}

void FOpenColorIOColorSpaceCustomization::ProcessColorSpaceForMenuGeneration(FMenuBuilder& InMenuBuilder, const int32 InMenuDepth, const FString& InPreviousFamilyHierarchy, const FOpenColorIOColorSpace& InColorSpace, TArray<FString>& InOutExistingMenuFilter)
{
	const FString NextFamilyName = InColorSpace.GetFamilyNameAtDepth(InMenuDepth);
	if (!NextFamilyName.IsEmpty())
	{
		if (!InOutExistingMenuFilter.Contains(NextFamilyName))
		{
			//Only add the previous family and delimiter if there was one. First family doesn't need it.
			const FString PreviousHierarchyToAdd = !InPreviousFamilyHierarchy.IsEmpty() ? InPreviousFamilyHierarchy + FOpenColorIOColorSpace::FamilyDelimiter : TEXT("");
			const FString NewHierarchy = PreviousHierarchyToAdd + NextFamilyName;;
			const int32 NextMenuDepth = InMenuDepth + 1;
			InMenuBuilder.AddSubMenu(
				FText::FromString(NextFamilyName),
				LOCTEXT("OpensFamilySubMenu", "ColorSpace Family Sub Menu"),
				FNewMenuDelegate::CreateRaw(this, &FOpenColorIOColorSpaceCustomization::PopulateSubMenu, NextMenuDepth, NewHierarchy)
			);

			InOutExistingMenuFilter.Add(NextFamilyName);
		}
	}
	else
	{

		AddMenuEntry(InMenuBuilder, InColorSpace);
	}
}

void FOpenColorIOColorSpaceCustomization::PopulateSubMenu(FMenuBuilder& InMenuBuilder, int32 InMenuDepth, FString InPreviousFamilyHierarchy)
{
	//Submenus should always be at a certain depth level
	check(InMenuDepth > 0);

	// To keep track of submenus that were already added
	TArray<FString> ExistingSubMenus;

	const int32 ColorSpaceCount = CachedConfigFile->getNumColorSpaces();
	for (int32 i = 0; i < ColorSpaceCount; ++i)
	{
		const char* ColorSpaceName = CachedConfigFile->getColorSpaceNameByIndex(i);
		OCIO_NAMESPACE::ConstColorSpaceRcPtr LibColorSpace = CachedConfigFile->getColorSpace(ColorSpaceName);
		if (!LibColorSpace)
		{
			continue;
		}

		FOpenColorIOColorSpace ColorSpace;
		ColorSpace.ColorSpaceIndex = i;
		ColorSpace.ColorSpaceName = StringCast<TCHAR>(ColorSpaceName).Get();
		ColorSpace.FamilyName = StringCast<TCHAR>(LibColorSpace->getFamily()).Get();
		
		//Filter out color spaces that don't belong to this hierarchy
		if (InPreviousFamilyHierarchy.IsEmpty() || ColorSpace.FamilyName.Contains(InPreviousFamilyHierarchy))
		{
			ProcessColorSpaceForMenuGeneration(InMenuBuilder, InMenuDepth, InPreviousFamilyHierarchy, ColorSpace, ExistingSubMenus);
		}
	}
}

void FOpenColorIOColorSpaceCustomization::AddMenuEntry(FMenuBuilder& InMenuBuilder, const FOpenColorIOColorSpace& InColorSpace)
{
	InMenuBuilder.AddMenuEntry(
		FText::FromString(InColorSpace.ToString()),
		FText::FromString(InColorSpace.ToString()),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=]
			{
				if (UStructProperty* StructProperty = Cast<UStructProperty>(ColorSpaceProperty->GetProperty()))
				{
					TArray<void*> RawData;
					ColorSpaceProperty->AccessRawData(RawData);
					FOpenColorIOColorSpace* PreviousColorSpaceValue = reinterpret_cast<FOpenColorIOColorSpace*>(RawData[0]);

					FString TextValue;
					StructProperty->Struct->ExportText(TextValue, &InColorSpace, PreviousColorSpaceValue, nullptr, EPropertyPortFlags::PPF_None, nullptr);
					ensure(ColorSpaceProperty->SetValueFromFormattedString(TextValue, EPropertyValueSetFlags::DefaultFlags) == FPropertyAccess::Result::Success);
				}
			}
			),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=]
			{
				TArray<void*> RawData;
				ColorSpaceProperty->AccessRawData(RawData);
				FOpenColorIOColorSpace* ColorSpaceValue = reinterpret_cast<FOpenColorIOColorSpace*>(RawData[0]);
				return *ColorSpaceValue == InColorSpace;
			})
		),
		NAME_None,
		EUserInterfaceActionType::RadioButton
		);
}

TSharedRef<SWidget> FOpenColorIOColorSpaceCustomization::HandleSourceComboButtonMenuContent()
{
	bool bValidConfiguration = false;
	if (ConfigurationFileProperty.IsValid())
	{
		TArray<void*> RawData;
		ConfigurationFileProperty->AccessRawData(RawData);

		check(RawData.Num() == 1);
		FFilePath* ConfigFilePath = reinterpret_cast<FFilePath*>(RawData[0]);
		check(ConfigFilePath);

		if (!ConfigFilePath->FilePath.IsEmpty() && ConfigFilePath->FilePath != LoadedFilePath.FilePath)
		{
			bValidConfiguration = LoadConfigurationFile(*ConfigFilePath);
		}
	}

	// Generate menu
	FMenuBuilder MenuBuilder(true, nullptr);
	TArray<FString> ExistingSubMenus;

	MenuBuilder.BeginSection("AllColorSpaces", LOCTEXT("AllColorSpacesSection", "ColorSpaces"));
	{
		if (bValidConfiguration)
		{
			const int32 ColorSpaceCount = CachedConfigFile->getNumColorSpaces();
			for (int32 i = 0; i < ColorSpaceCount; ++i)
			{
				const char* ColorSpaceName = CachedConfigFile->getColorSpaceNameByIndex(i);
				OCIO_NAMESPACE::ConstColorSpaceRcPtr LibColorSpace = CachedConfigFile->getColorSpace(ColorSpaceName);
				if (!LibColorSpace)
				{
					continue;
				}

				FOpenColorIOColorSpace ColorSpace;
				ColorSpace.ColorSpaceIndex = i;
				ColorSpace.ColorSpaceName = StringCast<TCHAR>(ColorSpaceName).Get();
				ColorSpace.FamilyName = StringCast<TCHAR>(LibColorSpace->getFamily()).Get();

				//Top level menus have no preceding hierarchy.
				const int32 CurrentMenuDepth = 0;
				const FString PreviousHierarchy = FString();
				ProcessColorSpaceForMenuGeneration(MenuBuilder, CurrentMenuDepth, PreviousHierarchy, ColorSpace, ExistingSubMenus);
			}

			if (ColorSpaceCount <= 0)
			{
				MenuBuilder.AddWidget(SNullWidget::NullWidget, LOCTEXT("NoColorSpaceFound", "No color space found"), false, false);
			}
		}
		else
		{
			MenuBuilder.AddWidget(SNullWidget::NullWidget, LOCTEXT("InvalidConfigurationFile", "Invalid configuration file"), false, false);
		}
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
