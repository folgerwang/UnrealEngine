// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TimecodeDetailsCustomization.h"
#include "Misc/Timecode.h"
#include "IDetailPropertyRow.h"
#include "IDetailChildrenBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "PropertyCustomizationHelpers.h"

#define LOCTEXT_NAMESPACE "Timecode"

void FTimecodeDetailsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TimecodeProperty = PropertyHandle;

	ChildBuilder.AddCustomRow(LOCTEXT("TimecodeLabel", "Timecode"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(PropertyHandle->GetPropertyDisplayName())
			.ToolTipText(LOCTEXT("TimecodeLabelTooltip", "Timecode"))
			.Font(CustomizationUtils.GetRegularFont())
		]
		.ValueContent()
		[
			SNew(SEditableTextBox)
			.Text(this, &FTimecodeDetailsCustomization::OnGetTimecodeText)
			.OnTextCommitted(this, &FTimecodeDetailsCustomization::OnTimecodeTextCommitted)
			.SelectAllTextWhenFocused(true)
			.RevertTextOnEscape(true)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];
}

FText FTimecodeDetailsCustomization::OnGetTimecodeText() const
{
	TArray<void*> RawData;
	TimecodeProperty->AccessRawData(RawData);

	FString CurrentValue = ((FTimecode*)RawData[0])->ToString();
	return FText::FromString(CurrentValue);
}

void FTimecodeDetailsCustomization::OnTimecodeTextCommitted(const FText& InText, ETextCommit::Type CommitInfo)
{
	TArray<void*> RawData;
	TimecodeProperty->AccessRawData(RawData);

	TArray<FString> Splits;
	InText.ToString().ParseIntoArray(Splits, TEXT(":"));

	if (Splits.Num() == 4)
	{
		((FTimecode*)RawData[0])->Hours = FCString::Atoi(*Splits[0]);
		((FTimecode*)RawData[0])->Minutes = FCString::Atoi(*Splits[1]);
		((FTimecode*)RawData[0])->Seconds = FCString::Atoi(*Splits[2]);
		((FTimecode*)RawData[0])->Frames = FCString::Atoi(*Splits[3]);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Unexpected timecode format. Expected 4 values, got %d"), Splits.Num());
	}
}

#undef LOCTEXT_NAMESPACE
