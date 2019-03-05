// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SLogWidget.h"
#include "Framework/Text/SlateTextRun.h"
#include "LiveCodingConsoleStyle.h"
#include "SlateOptMacros.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

#define LOCTEXT_NAMESPACE "LiveCoding"

//// FLogWidgetTextLayoutMarshaller ////

FLogWidgetTextLayoutMarshaller::FLogWidgetTextLayoutMarshaller()
	: TextLayout(nullptr)
{
	DefaultStyle = FTextBlockStyle()
		.SetFont( FCoreStyle::GetDefaultFontStyle( "Mono", 9 ) )
		.SetColorAndOpacity( FLinearColor::White )
		.SetSelectedBackgroundColor( FLinearColor(0.9f, 0.9f, 0.9f) );
}

FLogWidgetTextLayoutMarshaller::~FLogWidgetTextLayoutMarshaller()
{
}

void FLogWidgetTextLayoutMarshaller::SetText(const FString& SourceString, FTextLayout& TargetTextLayout)
{
	TextLayout = &TargetTextLayout;

	for(const TSharedRef<FString>& Line : Lines)
	{
		TextLayout->AddLine(FSlateTextLayout::FNewLineData(Line, TArray<TSharedRef<IRun>>()));
	}
}

void FLogWidgetTextLayoutMarshaller::GetText(FString& TargetString, const FTextLayout& SourceTextLayout)
{
	SourceTextLayout.GetAsText(TargetString);
}

void FLogWidgetTextLayoutMarshaller::Clear()
{
	Lines.Empty();
	MakeDirty();
}

void FLogWidgetTextLayoutMarshaller::AppendLine(const FSlateColor& Color, const FString& Line)
{
	TSharedRef<FString> NewLine = MakeShared<FString>(Line);
	Lines.Add(NewLine);

	if(TextLayout)
	{
		// Remove the "default" line that's added for an empty text box.
		if(Lines.Num() == 1)
		{
			TextLayout->ClearLines();
		}

		FTextBlockStyle Style = DefaultStyle;
		Style.ColorAndOpacity = Color;

		TArray<TSharedRef<IRun>> Runs;
		Runs.Add(FSlateTextRun::Create(FRunInfo(), NewLine, Style));
		TextLayout->AddLine(FSlateTextLayout::FNewLineData(NewLine, Runs));
	}
}

int32 FLogWidgetTextLayoutMarshaller::GetNumLines() const
{
	return Lines.Num();
}

//// SLogWidget ////

SLogWidget::SLogWidget()
	: bIsUserScrolled(false)
{
}

SLogWidget::~SLogWidget()
{
}

void SLogWidget::Construct(const FArguments& InArgs)
{
	MessagesTextMarshaller = MakeShared<FLogWidgetTextLayoutMarshaller>();

	ChildSlot
	[
		SNew(SBorder)
		[
			 SAssignNew(MessagesTextBox, SMultiLineEditableTextBox)
				.Style(FLiveCodingConsoleStyle::Get(), "Log.TextBox")
				.Marshaller(MessagesTextMarshaller)
				.IsReadOnly(true)
				.AlwaysShowScrollbars(true)
				.OnVScrollBarUserScrolled(this, &SLogWidget::OnScroll)
		]
	];

	RegisterActiveTimer(0.03f, FWidgetActiveTimerDelegate::CreateSP(this, &SLogWidget::OnTimerElapsed));
}

void SLogWidget::Clear()
{
	MessagesTextMarshaller->Clear();
}

void SLogWidget::ScrollToEnd()
{
	MessagesTextBox->ScrollTo(FTextLocation(MessagesTextMarshaller->GetNumLines() - 1));
	bIsUserScrolled = false;
}

void SLogWidget::AppendLine(const FSlateColor& Color, const FString& Text)
{
	FLine Line;
	Line.Color = Color;
	Line.Text = Text;

	FScopeLock Lock(&CriticalSection);
	QueuedLines.Add(MoveTemp(Line));
}

void SLogWidget::OnScroll(float ScrollOffset)
{
	bIsUserScrolled = ScrollOffset < 1.0 && !FMath::IsNearlyEqual(ScrollOffset, 1.0f);
}

EActiveTimerReturnType SLogWidget::OnTimerElapsed(double CurrentTime, float DeltaTime)
{
	FScopeLock Lock(&CriticalSection);
	for(const FLine& QueuedLine : QueuedLines)
	{
		MessagesTextMarshaller->AppendLine(QueuedLine.Color, QueuedLine.Text);
	}
	if(!bIsUserScrolled)
	{
		ScrollToEnd();
	}
	QueuedLines.Empty();
	return EActiveTimerReturnType::Continue;
}

#undef LOCTEXT_NAMESPACE

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
