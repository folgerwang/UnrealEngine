// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "FrameRateCustomization.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SFrameRatePicker.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "FrameRate.h"
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
	];
}


FFrameRate FFrameRateCustomization::GetFirstFrameRate() const
{
	TArray<const void*> RawData;
	StructPropertyHandle->AccessRawData(RawData);

	for (const void* RawPtr : RawData)
	{
		return *reinterpret_cast<const FFrameRate*>(RawPtr);
	}

	return FFrameRate();
}

void FFrameRateCustomization::SetFrameRate(FFrameRate NewFrameRate)
{
	GEditor->BeginTransaction(FText::Format(LOCTEXT("EditProperty", "Edit {0}"), StructPropertyHandle->GetPropertyDisplayName()));

	StructPropertyHandle->NotifyPreChange();

	TArray<void*> RawData;
	StructPropertyHandle->AccessRawData(RawData);

	for (void* RawPtr : RawData)
	{
		*reinterpret_cast<FFrameRate*>(RawPtr) = NewFrameRate;
	}

	StructPropertyHandle->NotifyPostChange();
	StructPropertyHandle->NotifyFinishedChangingProperties();

	GEditor->EndTransaction();
}

bool FFrameRateCustomization::HasMultipleValues() const
{
	TArray<const void*> RawData;
	StructPropertyHandle->AccessRawData(RawData);

	TOptional<FFrameRate> CompareAgainst;
	for (const void* RawPtr : RawData)
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

	return false;
}

#undef LOCTEXT_NAMESPACE