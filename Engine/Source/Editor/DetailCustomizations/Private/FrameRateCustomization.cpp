// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "FrameRateCustomization.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SFrameRatePicker.h"
#include "IPropertyUtilities.h"
#include "IPropertyTypeCustomization.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "Misc/FrameRate.h"
#include "Editor.h"


#define LOCTEXT_NAMESPACE "FrameRateCustomization"


TSharedRef<IPropertyTypeCustomization> FFrameRateCustomization::MakeInstance()
{
	return MakeShareable(new FFrameRateCustomization);
}


void FFrameRateCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	StructPropertyHandle = InPropertyHandle;
}


void FFrameRateCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TSharedPtr<IPropertyUtilities> PropertyUtils = CustomizationUtils.GetPropertyUtilities();

	FDetailWidgetRow& CustomRow = ChildBuilder.AddCustomRow(StructPropertyHandle->GetPropertyDisplayName());

	CustomRow.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	];

	CustomRow.ValueContent()
	[
		SNew(SFrameRatePicker)
		.Font(CustomizationUtils.GetRegularFont())
		.HasMultipleValues(this, &FFrameRateCustomization::HasMultipleValues)
		.Value(this, &FFrameRateCustomization::GetFirstFrameRate)
		.OnValueChanged(this, &FFrameRateCustomization::SetFrameRate)
	].IsEnabled(MakeAttributeLambda([=] { return !InPropertyHandle->IsEditConst() && PropertyUtils->IsPropertyEditingEnabled(); }));
}


FFrameRate FFrameRateCustomization::GetFirstFrameRate() const
{
	TArray<const void*> RawData;
	StructPropertyHandle->AccessRawData(RawData);

	for (const void* RawPtr : RawData)
	{
		if (RawPtr)
		{
			return *reinterpret_cast<const FFrameRate*>(RawPtr);
		}
	}

	return FFrameRate();
}

void FFrameRateCustomization::SetFrameRate(FFrameRate NewFrameRate)
{
	if (UStructProperty* StructProperty = Cast<UStructProperty>(StructPropertyHandle->GetProperty()))
	{
		TArray<void*> RawData;
		StructPropertyHandle->AccessRawData(RawData);
		FFrameRate* PreviousFrameRate = reinterpret_cast<FFrameRate*>(RawData[0]);

		FString TextValue;
		StructProperty->Struct->ExportText(TextValue, &NewFrameRate, PreviousFrameRate, nullptr, EPropertyPortFlags::PPF_None, nullptr);
		ensure(StructPropertyHandle->SetValueFromFormattedString(TextValue, EPropertyValueSetFlags::DefaultFlags) == FPropertyAccess::Result::Success);
	}
}

bool FFrameRateCustomization::HasMultipleValues() const
{
	TArray<const void*> RawData;
	StructPropertyHandle->AccessRawData(RawData);

	TOptional<FFrameRate> CompareAgainst;
	for (const void* RawPtr : RawData)
	{
		if (RawPtr == nullptr)
		{
			if (CompareAgainst.IsSet())
			{
				return false;
			}
		}
		else
		{
			FFrameRate ThisRate = *reinterpret_cast<const FFrameRate*>(RawPtr);

			if (!CompareAgainst.IsSet())
			{
				CompareAgainst = ThisRate;
			}
			else if (ThisRate != CompareAgainst.GetValue())
			{
				return true;
			}
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE