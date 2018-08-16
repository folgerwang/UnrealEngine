// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "STimecodeProvider.h"

#include "EditorStyleSet.h"
#include "Engine/Engine.h"
#include "Misc/App.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/STimecode.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Text/STextBlock.h"

void STimecodeProvider::Construct(const FArguments& InArgs)
{
	OverrideTimecodeProvider = InArgs._OverrideTimecodeProvider;

	TSharedRef<SWidget> StateWidget = InArgs._DisplaySynchronizationState ? SNew(SOverlay)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SThrobber)
			.Animate(SThrobber::VerticalAndOpacity)
			.NumPieces(1)
			.Visibility(this, &STimecodeProvider::HandleThrobberVisibility)
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.ColorAndOpacity(this, &STimecodeProvider::HandleIconColorAndOpacity)
			.Image(this, &STimecodeProvider::HandleIconImage)
			.Visibility(this, &STimecodeProvider::HandleImageVisibility)
		]
		: SNullWidget::NullWidget;

		TSharedRef<SWidget> FrameRateWidget = InArgs._DisplayFrameRate ? SNew(STextBlock)
			.Text(MakeAttributeLambda([this]
			{
				UTimecodeProvider* TimecodeProviderPtr = GetTimecodeProvider();
				if (TimecodeProviderPtr)
				{
					return TimecodeProviderPtr->GetFrameRate().ToPrettyText();
				}
				return GEngine->DefaultTimecodeFrameRate.ToPrettyText();
			}))
			.Font(InArgs._TimecodeProviderFont)
			.ColorAndOpacity(InArgs._TimecodeProviderColor)
		: SNullWidget::NullWidget;

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(0)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				StateWidget
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(MakeAttributeLambda([this]
				{
					UTimecodeProvider* TimecodeProviderPtr = GetTimecodeProvider();
					if (TimecodeProviderPtr)
					{
						return FText::FromName(TimecodeProviderPtr->GetFName());
					}
					return FText::FromString(TEXT("[System Clock]"));
				}))
				.Font(InArgs._TimecodeProviderFont)
				.ColorAndOpacity(InArgs._TimecodeProviderColor)
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			.FillWidth(1.f)
			[
				FrameRateWidget
			]
		]
		+ SVerticalBox::Slot()
		.Padding(0, -4, 0, 0)
		.AutoHeight()
		[
			SNew(STimecode)
			.Timecode(MakeAttributeLambda([this]
				{
					UTimecodeProvider* OverrideTimecodeProviderPtr = OverrideTimecodeProvider.Get().Get();
					if (OverrideTimecodeProviderPtr)
					{
						return OverrideTimecodeProviderPtr->GetTimecode();
					}

					// If we use the Engine's TimecodeProvider, get the timecode for the current frame
					return FApp::GetTimecode();
				}))
			.TimecodeFont(InArgs._TimecodeFont)
			.TimecodeColor(InArgs._TimecodeColor)
		]
	];
}

UTimecodeProvider* STimecodeProvider::GetTimecodeProvider() const
{
	UTimecodeProvider* TimecodeProviderPtr = OverrideTimecodeProvider.Get().Get();
	if (!TimecodeProviderPtr)
	{
		TimecodeProviderPtr = GEngine->GetTimecodeProvider();
	}

	return TimecodeProviderPtr;
}

FSlateColor STimecodeProvider::HandleIconColorAndOpacity() const
{
	FSlateColor Result = FSlateColor::UseForeground();
	UTimecodeProvider* TimecodeProviderPtr = GetTimecodeProvider();
	if (TimecodeProviderPtr)
	{
		ETimecodeProviderSynchronizationState State = TimecodeProviderPtr->GetSynchronizationState();
		switch (State)
		{
		case ETimecodeProviderSynchronizationState::Closed:
		case ETimecodeProviderSynchronizationState::Error:
			Result = FLinearColor::Red;
			break;
		case ETimecodeProviderSynchronizationState::Synchronized:
			Result = FLinearColor::Green;
			break;
		case ETimecodeProviderSynchronizationState::Synchronizing:
			Result = FLinearColor::Yellow;
			break;
		}
	}
	return Result;
}

const FSlateBrush* STimecodeProvider::HandleIconImage() const
{
	const FSlateBrush* Result = nullptr;

	UTimecodeProvider* TimecodeProviderPtr = GetTimecodeProvider();
	if (TimecodeProviderPtr)
	{
		ETimecodeProviderSynchronizationState State = TimecodeProviderPtr->GetSynchronizationState();
		switch (State)
		{
		case ETimecodeProviderSynchronizationState::Error:
			Result = FEditorStyle::GetBrush("Icons.Error");
			break;
		case ETimecodeProviderSynchronizationState::Closed:
			Result = FEditorStyle::GetBrush("Icons.Cross");
			break;
		case ETimecodeProviderSynchronizationState::Synchronized:
			Result = FEditorStyle::GetBrush("Symbols.Check");
			break;
		}
	}
	else
	{
		Result = FEditorStyle::GetBrush("Symbols.Check");
	}
	return Result;
}

EVisibility STimecodeProvider::HandleImageVisibility() const
{
	return (HandleThrobberVisibility() == EVisibility::Hidden) ? EVisibility::Visible : EVisibility::Hidden;
}

EVisibility STimecodeProvider::HandleThrobberVisibility() const
{
	UTimecodeProvider* TimecodeProviderPtr = GetTimecodeProvider();
	if (TimecodeProviderPtr)
	{
		ETimecodeProviderSynchronizationState State = TimecodeProviderPtr->GetSynchronizationState();
		if (State == ETimecodeProviderSynchronizationState::Synchronizing)
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Hidden;
}
