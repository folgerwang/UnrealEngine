// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "QualifiedFrameTimeDetailsCustomization.h"
#include "IDetailPropertyRow.h"
#include "Misc/FrameNumber.h"
#include "IDetailChildrenBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "PropertyCustomizationHelpers.h"
#include "Misc/FrameRate.h"

#define LOCTEXT_NAMESPACE "TimeManagement.QualifiedFrameTime"

void FQualifiedFrameTimeDetailsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TMap<FName, TSharedRef<IPropertyHandle>> CustomizedProperties;

	uint32 NumChildren = 0;
	PropertyHandle->GetNumChildren(NumChildren);

	// Add child properties to UI and pick out the properties which need customization
	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
		CustomizedProperties.Add(ChildHandle->GetProperty()->GetFName(), ChildHandle);
	}

	TSharedRef<IPropertyHandle> FrameNumberProperty = CustomizedProperties.FindChecked(GET_MEMBER_NAME_CHECKED(FFrameNumber, Value));
	ChildBuilder.AddCustomRow(LOCTEXT("TimeLabel", "Time"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TimeLabel", "Time"))
			.ToolTipText(LOCTEXT("TimeLabelTooltip", "Time field which takes timecode, frames and time formats."))
			.Font(CustomizationUtils.GetRegularFont())
		]
		.ValueContent()
		[
			SNew(SEditableTextBox)
			.Text(this, &FQualifiedFrameTimeDetailsCustomization::OnGetTimeText)
			.OnTextCommitted(this, &FQualifiedFrameTimeDetailsCustomization::OnTimeTextCommitted)
			.SelectAllTextWhenFocused(true)
			.RevertTextOnEscape(true)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];
}

FText FQualifiedFrameTimeDetailsCustomization::OnGetTimeText() const
{
	FString Foo = TEXT("Time");
	return FText::FromString(Foo);
}

void FQualifiedFrameTimeDetailsCustomization::OnTimeTextCommitted(const FText& InText, ETextCommit::Type CommitInfo)
{
	// @todo sequencer-timecode unified time display

	// FSequencerTimeEvaluator Eval;
	// TValueOrError<FFrameTime, FExpressionError> TimecodeResult = Eval.EvaluateTimecode(*InText.ToString(), FFrameRate(30, 1));
	// bool bWasFrameText;
	// TValueOrError<FFrameTime, FExpressionError> FrameResult = Eval.EvaluateFrame(*InText.ToString(), FFrameRate(30, 1), /*Out*/ bWasFrameText);
	// bool bWasTimeText;
	// TValueOrError<FFrameTime, FExpressionError> TimeResult = Eval.EvaluateTime(*InText.ToString(), FFrameRate(30,1), /*Out*/ bWasTimeText);
	// 
	// if (TimecodeResult.IsValid())
	// {
	// 	UE_LOG(LogTemp, Log, TEXT("Timecode Result: %d"), TimecodeResult.GetValue().GetFrame().Value);
	// }
	// else if (TimeResult.IsValid() && !bWasFrameText)
	// {
	// 	UE_LOG(LogTemp, Log, TEXT("Time Result: %d"), TimeResult.GetValue().GetFrame().Value);
	// }
	// else if (FrameResult.IsValid() && !bWasTimeText)
	// {
	// 	UE_LOG(LogTemp, Log, TEXT("Frame Result: %d"), FrameResult.GetValue().GetFrame().Value);
	// }
	// else
	// {
	// 	UE_LOG(LogTemp, Log, TEXT("Unknown"));
	// }
}

#undef LOCTEXT_NAMESPACE
