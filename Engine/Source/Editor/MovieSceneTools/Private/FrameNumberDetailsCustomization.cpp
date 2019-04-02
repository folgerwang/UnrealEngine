// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "FrameNumberDetailsCustomization.h"
#include "IDetailPropertyRow.h"
#include "Misc/FrameNumber.h"
#include "IDetailChildrenBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "PropertyCustomizationHelpers.h"
#include "Misc/FrameRate.h"

#define LOCTEXT_NAMESPACE "TimeManagement.FrameNumber"

void FFrameNumberDetailsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Check for Min/Max metadata on the property itself and we'll apply it to the child when changed.
	const FString& MetaUIMinString = PropertyHandle->GetMetaData(TEXT("UIMin"));
	const FString& MetaUIMaxString = PropertyHandle->GetMetaData(TEXT("UIMax"));

	UIClampMin = TNumericLimits<int32>::Lowest();
	UIClampMax = TNumericLimits<int32>::Max();

	if (!MetaUIMinString.IsEmpty())
	{
		TTypeFromString<int32>::FromString(UIClampMin, *MetaUIMinString);
	}

	if (!MetaUIMaxString.IsEmpty())
	{
		TTypeFromString<int32>::FromString(UIClampMax, *MetaUIMaxString);
	}

	TMap<FName, TSharedRef<IPropertyHandle>> CustomizedProperties;

	uint32 NumChildren = 0;
	PropertyHandle->GetNumChildren(NumChildren);

	// Add child properties to UI and pick out the properties which need customization
	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
		CustomizedProperties.Add(ChildHandle->GetProperty()->GetFName(), ChildHandle);
	}

	FrameNumberProperty = CustomizedProperties.FindChecked(GET_MEMBER_NAME_CHECKED(FFrameNumber, Value));

	ChildBuilder.AddCustomRow(LOCTEXT("TimeLabel", "Time"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(PropertyHandle->GetPropertyDisplayName())
			.ToolTipText(LOCTEXT("TimeLabelTooltip", "Time field which takes timecode, frames and seconds formats."))
			.Font(CustomizationUtils.GetRegularFont())
		]
		.ValueContent()
		[
			SNew(SEditableTextBox)
			.Text(this, &FFrameNumberDetailsCustomization::OnGetTimeText)
			.OnTextCommitted(this, &FFrameNumberDetailsCustomization::OnTimeTextCommitted)
			.SelectAllTextWhenFocused(true)
			.ClearKeyboardFocusOnCommit(false)
			.RevertTextOnEscape(true)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];
}

FText FFrameNumberDetailsCustomization::OnGetTimeText() const
{
	int32 CurrentValue = 0.0;
	FrameNumberProperty->GetValue(CurrentValue);

	return FText::FromString(NumericTypeInterface->ToString(CurrentValue));
}

void FFrameNumberDetailsCustomization::OnTimeTextCommitted(const FText& InText, ETextCommit::Type CommitInfo)
{
	int32 ExistingValue = 0.0;
	FrameNumberProperty->GetValue(ExistingValue);
	TOptional<double> TickResolution = NumericTypeInterface->FromString(InText.ToString(), ExistingValue);
	if (TickResolution.IsSet())
	{
		double ClampedValue = FMath::Clamp(TickResolution.GetValue(), (double)UIClampMin, (double)UIClampMax);
		FrameNumberProperty->SetValue((int32)ClampedValue);
	}
}

#undef LOCTEXT_NAMESPACE
