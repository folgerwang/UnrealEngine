// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AjaMediaModeCustomization.h"

#include "AjaMediaFinder.h"
#include "AjaMediaSettings.h"
#include "DetailWidgetRow.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "IPropertyUtilities.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"

#define LOCTEXT_NAMESPACE "AjaMediaPortCustomization"


void FAjaMediaModeCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	MediaModeProperty = InPropertyHandle;
	MediaPortProperty.Reset();
	OverrideProperty.Reset();
	ParentObject.Reset();

	if (MediaModeProperty->GetNumPerObjectValues() == 1 && MediaModeProperty->IsValidHandle())
	{
		UProperty* Property = MediaModeProperty->GetProperty();
		
		check(Property && Cast<UStructProperty>(Property) && Cast<UStructProperty>(Property)->Struct &&
				Cast<UStructProperty>(Property)->Struct->IsChildOf(FAjaMediaMode::StaticStruct()));


		// Check if it's an output. By default it's an Input
		{
			static FName NAME_CustomizeAsInput = TEXT("CustomizeAsInput");
			const FString& MetaDataValue = Property->GetMetaData(NAME_CustomizeAsInput);
			
			bOutput = Property->HasMetaData(NAME_CustomizeAsInput) && !MetaDataValue.IsEmpty() && !MetaDataValue.ToBool();
		}

		TSharedPtr<IPropertyHandle> ParentHandle = InPropertyHandle->GetParentHandle();
		if (ParentHandle.IsValid())
		{
			// Get the MediaPort to read from the same device
			{
				static FName NAME_MediaPort = TEXT("MediaPort");
				if (Property->HasMetaData(NAME_MediaPort))
				{
					const FString& MetaDataValue = Property->GetMetaData(NAME_MediaPort);
					if (!MetaDataValue.IsEmpty())
					{
						MediaPortProperty = ParentHandle->GetChildHandle(*MetaDataValue, false);
					}
				}
			}
			// Get the EditCondition to see if it's overriding project settings
			{
				bool bNegate = false;
				OverrideProperty = PropertyCustomizationHelpers::GetEditConditionProperty(Property, bNegate);
				if (OverrideProperty.IsValid())
				{
					TArray<UObject*> Objects;
					ParentHandle->GetOuterObjects(Objects);
					if (Objects.Num() == 1)
					{
						ParentObject = Objects[0];
					}
				}
			}
		}

		TArray<void*> RawData;
		MediaModeProperty->AccessRawData(RawData);

		check(RawData.Num() == 1);
		FAjaMediaMode* MediaModeValue = reinterpret_cast<FAjaMediaMode*>(RawData[0]);

		check(MediaModeValue);

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
					const FAjaMediaMode MediaMode = GetMediaModeValue(*MediaModeValue);
					return FText::FromString(MediaMode.ToString());
				}))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			.VAlign(VAlign_Center)
			[
				SNew(SComboButton)
				.OnGetMenuContent(this, &FAjaMediaModeCustomization::HandleSourceComboButtonMenuContent)
				.ContentPadding(FMargin(4.0, 2.0))
			]
		].IsEnabled(MakeAttributeLambda([=] { return !InPropertyHandle->IsEditConst() && PropertyUtils->IsPropertyEditingEnabled(); }));
	}
}

void FAjaMediaModeCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

TSharedRef<SWidget> FAjaMediaModeCustomization::HandleSourceComboButtonMenuContent() const
{
	int32 DeviceIndex = 0;
	if (MediaPortProperty.IsValid())
	{
		TArray<void*> RawData;
		MediaPortProperty->AccessRawData(RawData);

		check(RawData.Num() == 1);
		FAjaMediaPort* MediaPortValue = reinterpret_cast<FAjaMediaPort*>(RawData[0]);
		check(MediaPortValue);
		if (!MediaPortValue->IsValid())
		{
			return SNullWidget::NullWidget;
		}
		DeviceIndex = MediaPortValue->DeviceIndex;
	}

	// fetch found sources
	TArray<FAjaMediaMode> OutModes;
	if (!FAjaMediaFinder::GetModes(DeviceIndex, bOutput, OutModes))
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

		for (const FAjaMediaMode& Mode : OutModes)
		{
			const TSharedPtr<IPropertyHandle> ValueProperty = MediaModeProperty;
			const FString Url = Mode.ToString();

			MenuBuilder.AddMenuEntry(
				FText::FromString(Mode.ToString()),
				FText::FromString(Url),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([=] {
						if (UStructProperty* StructProperty = Cast<UStructProperty>(MediaModeProperty->GetProperty()))
						{
							TArray<void*> RawData;
							MediaModeProperty->AccessRawData(RawData);
							FAjaMediaMode* PreviousMediaModeValue = reinterpret_cast<FAjaMediaMode*>(RawData[0]);

							FString TextValue;
							StructProperty->Struct->ExportText(TextValue, &Mode, PreviousMediaModeValue, nullptr, EPropertyPortFlags::PPF_None, nullptr);
							ensure(MediaModeProperty->SetValueFromFormattedString(TextValue, EPropertyValueSetFlags::DefaultFlags) == FPropertyAccess::Result::Success);
						}
					}),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([=] {
						TArray<void*> RawData;
						MediaModeProperty->AccessRawData(RawData);
						FAjaMediaMode* MediaModeValue = reinterpret_cast<FAjaMediaMode*>(RawData[0]);
						return *MediaModeValue == Mode;
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

FAjaMediaMode FAjaMediaModeCustomization::GetMediaModeValue(const FAjaMediaMode& InMediaModeValue) const
{
	FAjaMediaMode CurrentMediaMode = InMediaModeValue;
	
	//If MediaMode has no EditCondition, it means it always overrides project setting.
	if (OverrideProperty.IsValid() && ParentObject.IsValid())
	{
		uint8* ValueAddr = OverrideProperty->ContainerPtrToValuePtr<uint8>(ParentObject.Get());
		const bool bIsOverriden = OverrideProperty->GetPropertyValue(ValueAddr);
		if (!bIsOverriden)
		{
			TArray<void*> RawData;
			MediaPortProperty->AccessRawData(RawData);

			check(RawData.Num() == 1);
			FAjaMediaPort* MediaPortValue = reinterpret_cast<FAjaMediaPort*>(RawData[0]);
			check(MediaPortValue);
			if (MediaPortValue->IsValid())
			{
				if (bOutput)
				{
					CurrentMediaMode = GetDefault<UAjaMediaSettings>()->GetOutputMediaMode(*MediaPortValue);
				}
				else
				{
					CurrentMediaMode = GetDefault<UAjaMediaSettings>()->GetInputMediaMode(*MediaPortValue);
				}
			}
		}
	}

	return CurrentMediaMode;
}

#undef LOCTEXT_NAMESPACE
