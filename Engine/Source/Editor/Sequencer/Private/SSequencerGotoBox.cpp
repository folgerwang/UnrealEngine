// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SSequencerGotoBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Framework/Application/SlateApplication.h"
#include "EditorStyleSet.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSpacer.h"

#define LOCTEXT_NAMESPACE "Sequencer"


void SSequencerGotoBox::Construct(const FArguments& InArgs, const TSharedRef<FSequencer>& InSequencer, USequencerSettings& InSettings, const TSharedRef<INumericTypeInterface<double>>& InNumericTypeInterface)
{
	SequencerPtr = InSequencer;
	Settings = &InSettings;
	NumericTypeInterface = InNumericTypeInterface;

	const FDockTabStyle* GenericTabStyle = &FCoreStyle::Get().GetWidgetStyle<FDockTabStyle>("Docking.Tab");
	const FButtonStyle* const CloseButtonStyle = &GenericTabStyle->CloseButtonStyle;

	ChildSlot
	[
		SAssignNew(Border, SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			.Padding(6.0f)
			.Visibility(EVisibility::Collapsed)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
							.Text(LOCTEXT("GotoLabel", "Go to:"))
					]

				 + SHorizontalBox::Slot()
				 	.Padding(6.0f, 0.0f, 0.0f, 0.0f)
				 	.AutoWidth()
				 	[
				 		 SAssignNew(EntryBox, SNumericEntryBox<double>)
				 		 	.MinDesiredValueWidth(64.0f)
				 		 	.OnValueCommitted(this, &SSequencerGotoBox::HandleEntryBoxValueCommitted)
				 		 	.TypeInterface(NumericTypeInterface)
				 		 	.Value_Lambda([this](){ return SequencerPtr.Pin()->GetLocalTime().Time.GetFrame().Value; })
				 	]

				+ SHorizontalBox::Slot()
				.Padding(3.0f, 0.0f, 0.0f, 0.0f)
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle( CloseButtonStyle )
					.OnClicked( this, &SSequencerGotoBox::OnCloseButtonClicked )
					.ContentPadding( 0 )
					[
						SNew(SSpacer)
						.Size( CloseButtonStyle->Normal.ImageSize )
					]
				]
			]
	];
}


void SSequencerGotoBox::ToggleVisibility()
{
	FSlateApplication& SlateApplication = FSlateApplication::Get();

	if (Border->GetVisibility() == EVisibility::Visible)
	{
		SlateApplication.SetAllUserFocus(LastFocusedWidget.Pin(), EFocusCause::Navigation);
		Border->SetVisibility(EVisibility::Collapsed);
	}
	else
	{
		Border->SetVisibility(EVisibility::Visible);
		LastFocusedWidget = SlateApplication.GetUserFocusedWidget(0);
		SlateApplication.SetAllUserFocus(EntryBox, EFocusCause::Navigation);
	}
}


void SSequencerGotoBox::HandleEntryBoxValueCommitted(double Value, ETextCommit::Type CommitType)
{
	if (CommitType != ETextCommit::OnEnter)
	{
		return;
	}

	ToggleVisibility();

	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();

	// scroll view range if new value is not visible. 
	const FAnimatedRange ViewRange = Sequencer->GetViewRange();

	// View range is in seconds, so we need to convert from frame numbers back into seconds.
	FFrameTime ValueAsFrameTime = FFrameTime::FromDecimal(Value);
	double ValueAsSeconds = Sequencer->GetFocusedFrameResolution().AsSeconds(ValueAsFrameTime);

	if (!ViewRange.Contains(ValueAsSeconds))
	{
		const double HalfViewWidth = 0.5 * ViewRange.Size<double>();
		const TRange<double> NewRange = TRange<double>(ValueAsSeconds - HalfViewWidth, ValueAsSeconds + HalfViewWidth);
		Sequencer->SetViewRange(NewRange);
	}
	Sequencer->SetLocalTimeDirectly(ValueAsFrameTime);
}

FReply SSequencerGotoBox::OnCloseButtonClicked()
{
	ToggleVisibility();

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
