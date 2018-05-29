// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DynamicEntryBoxDetails.h"

#include "Editor/PropertyEditor/Public/DetailLayoutBuilder.h"
#include "Editor/PropertyEditor/Public/DetailCategoryBuilder.h"
#include "Components/DynamicEntryBox.h"
#include "PropertyCustomizationHelpers.h"

TSharedRef<IDetailCustomization> FDynamicEntryBoxDetails::MakeInstance()
{
	return MakeShareable(new FDynamicEntryBoxDetails());
}

void FDynamicEntryBoxDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailLayout.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() != 1)
	{
		return;
	}
	EntryBox = Cast<UDynamicEntryBox>(Objects[0].Get());
	if (!EntryBox.IsValid())
	{
		return;
	}

	IDetailCategoryBuilder& EntryLayoutCategory = DetailLayout.EditCategory(TEXT("EntryLayout"));

	const TAttribute<bool> CanEditAignmentAttribute(this, &FDynamicEntryBoxDetails::CanEditAlignment);
	EntryLayoutCategory.AddProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UDynamicEntryBox, EntryHorizontalAlignment)))
		.IsEnabled(CanEditAignmentAttribute);
	EntryLayoutCategory.AddProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UDynamicEntryBox, EntryVerticalAlignment)))
		.IsEnabled(CanEditAignmentAttribute);

	EntryLayoutCategory.AddProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UDynamicEntryBox, MaxElementSize)))
		.IsEnabled(TAttribute<bool>(this, &FDynamicEntryBoxDetails::CanEditMaxElementSize));
	EntryLayoutCategory.AddProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UDynamicEntryBox, EntrySpacing)))
		.IsEnabled(TAttribute<bool>(this, &FDynamicEntryBoxDetails::CanEditEntrySpacing));
	EntryLayoutCategory.AddProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UDynamicEntryBox, SpacingPattern)))
		.IsEnabled(TAttribute<bool>(this, &FDynamicEntryBoxDetails::CanEditSpacingPattern));

	AddEntryClassPicker(*EntryBox, EntryLayoutCategory, DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UDynamicEntryBox, EntryWidgetClass)));
}

bool FDynamicEntryBoxDetails::CanEditSpacingPattern() const
{
	return EntryBox->GetBoxType() == EDynamicBoxType::Overlay;
}

bool FDynamicEntryBoxDetails::CanEditEntrySpacing() const
{
	return EntryBox->SpacingPattern.Num() == 0;
}

bool FDynamicEntryBoxDetails::CanEditAlignment() const
{
	return EntryBox->GetBoxType() != EDynamicBoxType::Overlay || CanEditEntrySpacing();
}

bool FDynamicEntryBoxDetails::CanEditMaxElementSize() const
{
	const EDynamicBoxType BoxType = EntryBox->GetBoxType();
	return BoxType == EDynamicBoxType::Horizontal || BoxType == EDynamicBoxType::Vertical;
}
