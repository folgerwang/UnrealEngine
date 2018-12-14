// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "STimeRange.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "EditorStyleSet.h"
#include "Misc/QualifiedFrameTime.h"
#include "MovieSceneTimeHelpers.h"

#define LOCTEXT_NAMESPACE "STimeRange"

void STimeRange::Construct( const STimeRange::FArguments& InArgs, TSharedRef<ITimeSliderController> InTimeSliderController, TSharedRef<INumericTypeInterface<double>> NumericTypeInterface )
{
	TimeSliderController = InTimeSliderController;

	TSharedRef<SWidget> WorkingRangeStart = SNullWidget::NullWidget, WorkingRangeEnd = SNullWidget::NullWidget;
	if (InArgs._ShowWorkingRange)
	{
		WorkingRangeStart = SNew(SSpinBox<double>)
		.Value(this, &STimeRange::WorkingStartTime)
		.ToolTipText(LOCTEXT("WorkingRangeStart", "Working Range Start"))
		.OnValueCommitted(this, &STimeRange::OnWorkingStartTimeCommitted)
		.OnValueChanged(this, &STimeRange::OnWorkingStartTimeChanged)
		.MinValue(TOptional<double>())
		.MaxValue(this, &STimeRange::MaxWorkingStartTime)
		.Style(&FEditorStyle::Get().GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
		.TypeInterface(NumericTypeInterface)
		.ClearKeyboardFocusOnCommit(true)
		.Delta(this, &STimeRange::GetSpinboxDelta)
		.LinearDeltaSensitivity(25);

		WorkingRangeEnd = SNew(SSpinBox<double>)
		.Value(this, &STimeRange::WorkingEndTime)
		.ToolTipText(LOCTEXT("WorkingRangeEnd", "Working Range End"))
		.OnValueCommitted( this, &STimeRange::OnWorkingEndTimeCommitted )
		.OnValueChanged( this, &STimeRange::OnWorkingEndTimeChanged )
		.MinValue(this, &STimeRange::MinWorkingEndTime)
		.MaxValue(TOptional<double>())
		.Style(&FEditorStyle::Get().GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
		.TypeInterface(NumericTypeInterface)
		.ClearKeyboardFocusOnCommit(true)
		.Delta(this, &STimeRange::GetSpinboxDelta)
		.LinearDeltaSensitivity(25);
	}

	TSharedRef<SWidget> ViewRangeStart = SNullWidget::NullWidget, ViewRangeEnd = SNullWidget::NullWidget;
	if (InArgs._ShowViewRange)
	{
		ViewRangeStart = SNew(SSpinBox<double>)
		.Value(this, &STimeRange::ViewStartTime)
		.ToolTipText(LOCTEXT("ViewStartTimeTooltip", "View Range Start Time"))
		.OnValueCommitted( this, &STimeRange::OnViewStartTimeCommitted )
		.OnValueChanged( this, &STimeRange::OnViewStartTimeChanged )
		.MinValue(TOptional<double>())
		.MaxValue(this, &STimeRange::MaxViewStartTime)
		.Style(&FEditorStyle::Get().GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
		.TypeInterface(NumericTypeInterface)
		.ClearKeyboardFocusOnCommit(true)
		.Delta(this, &STimeRange::GetSpinboxDelta)
		.LinearDeltaSensitivity(25);


		ViewRangeEnd = SNew(SSpinBox<double>)
		.Value(this, &STimeRange::ViewEndTime)
		.ToolTipText(LOCTEXT("ViewEndTimeTooltip", "View Range End Time"))
		.OnValueCommitted( this, &STimeRange::OnViewEndTimeCommitted )
		.OnValueChanged( this, &STimeRange::OnViewEndTimeChanged )
		.MinValue(this, &STimeRange::MinViewEndTime)
		.MaxValue(TOptional<double>())
		.Style(&FEditorStyle::Get().GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
		.TypeInterface(NumericTypeInterface)
		.ClearKeyboardFocusOnCommit(true)
		.Delta(this, &STimeRange::GetSpinboxDelta)
		.LinearDeltaSensitivity(25);
	}

	TSharedRef<SWidget> PlaybackRangeStart = SNullWidget::NullWidget, PlaybackRangeEnd = SNullWidget::NullWidget;
	{
		PlaybackRangeStart = SNew(SSpinBox<double>)
		.Value(this, &STimeRange::PlayStartTime)
		.ToolTipText(LOCTEXT("PlayStartTimeTooltip", "Playback Range Start Time"))
		.OnValueCommitted(this, &STimeRange::OnPlayStartTimeCommitted)
		.OnValueChanged(this, &STimeRange::OnPlayStartTimeChanged)
		.MinValue(TOptional<double>())
		.MaxValue(TOptional<double>())
		.Style(&FEditorStyle::Get().GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
		.TypeInterface(NumericTypeInterface)
		.ClearKeyboardFocusOnCommit(true)
		.Delta(this, &STimeRange::GetSpinboxDelta)
		.LinearDeltaSensitivity(25);


		PlaybackRangeEnd = SNew(SSpinBox<double>)
		.Value(this, &STimeRange::PlayEndTime)
		.ToolTipText(LOCTEXT("PlayEndTimeTooltip", "Playback Range Stop Time"))
		.OnValueCommitted( this, &STimeRange::OnPlayEndTimeCommitted )
		.OnValueChanged( this, &STimeRange::OnPlayEndTimeChanged )
		.MinValue(TOptional<double>())
		.MaxValue(TOptional<double>())
		.Style(&FEditorStyle::Get().GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
		.TypeInterface(NumericTypeInterface)
		.ClearKeyboardFocusOnCommit(true)
		.Delta(this, &STimeRange::GetSpinboxDelta)
		.LinearDeltaSensitivity(25);
	}

	this->ChildSlot
	.HAlign(HAlign_Fill)
	[
		SNew(SHorizontalBox)

		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(2.f)
		[
			SNew(SBox)
			.Visibility(InArgs._ShowWorkingRange ? EVisibility::Visible : EVisibility::Collapsed)
			.MinDesiredWidth(64)
			.HAlign(HAlign_Center)
			[
				WorkingRangeStart
			]
		]

		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(2.f)
		[
			SNew(SBox)
			.MinDesiredWidth(64)
			.HAlign(HAlign_Center)
			.Visibility(InArgs._ShowPlaybackRange ? EVisibility::Visible : EVisibility::Collapsed)
			[
				SNew(SBorder)
				.Padding(0)
				.BorderImage(nullptr)
				.ForegroundColor(FLinearColor::Green)
				[
					PlaybackRangeStart
				]
			]
		]

		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(2.f)
		[
			SNew(SBox)
			.Visibility(InArgs._ShowViewRange ? EVisibility::Visible : EVisibility::Collapsed)
			.MinDesiredWidth(64)
			.HAlign(HAlign_Center)
			[
				ViewRangeStart
			]
		]

		+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(2.0f, 4.0f)
			.VAlign(VAlign_Center)
			[
				InArgs._CenterContent.Widget
			]
		
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(2.f)
		[
			SNew(SBox)
			.Visibility(InArgs._ShowViewRange ? EVisibility::Visible : EVisibility::Collapsed)
			.MinDesiredWidth(64)
			.HAlign(HAlign_Center)
			[
				ViewRangeEnd
			]
		]

		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(2.f)
		[
			SNew(SBox)
			.MinDesiredWidth(64)
			.HAlign(HAlign_Center)
			.Visibility(InArgs._ShowPlaybackRange ? EVisibility::Visible : EVisibility::Collapsed)
			[
				SNew(SBorder)
				.Padding(0)
				.BorderImage(nullptr)
				.ForegroundColor(FLinearColor::Red)
				[
					PlaybackRangeEnd
				]
			]
		]

		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(2.f)
		[
			SNew(SBox)
			.MinDesiredWidth(64)
			.HAlign(HAlign_Center)
			.Visibility(InArgs._ShowWorkingRange ? EVisibility::Visible : EVisibility::Collapsed)
			[
				WorkingRangeEnd
			]
		]
	];
}

double STimeRange::WorkingStartTime() const
{
	FFrameRate Rate = TimeSliderController->GetTickResolution();
	FFrameTime Time = TimeSliderController->GetClampRange().GetLowerBoundValue() * Rate;
	return Time.GetFrame().Value;
}

double STimeRange::WorkingEndTime() const
{
	FFrameRate Rate = TimeSliderController->GetTickResolution();
	FFrameTime Time = TimeSliderController->GetClampRange().GetUpperBoundValue() * Rate;
	return Time.GetFrame().Value;
}

double STimeRange::ViewStartTime() const
{
	FFrameRate TickResolution = TimeSliderController->GetTickResolution();

	// View range is in seconds so we convert it to tick resolution
	FFrameTime Time = TimeSliderController->GetViewRange().GetLowerBoundValue() * TickResolution;
	return Time.GetFrame().Value;
}

double STimeRange::ViewEndTime() const
{
	FFrameRate TickResolution = TimeSliderController->GetTickResolution();

	// View range is in seconds so we convert it to tick resolution
	FFrameTime Time = TimeSliderController->GetViewRange().GetUpperBoundValue() * TickResolution;
	return Time.GetFrame().Value;
}

double STimeRange::GetSpinboxDelta() const
{
	return TimeSliderController->GetTickResolution().AsDecimal() * TimeSliderController->GetDisplayRate().AsInterval();
}

double STimeRange::PlayStartTime() const
{
	FFrameNumber LowerBound = MovieScene::DiscreteInclusiveLower(TimeSliderController->GetPlayRange());
	return LowerBound.Value; 
}

double STimeRange::PlayEndTime() const
{
	FFrameNumber UpperBound = MovieScene::DiscreteExclusiveUpper(TimeSliderController->GetPlayRange());
	return UpperBound.Value;
}

TOptional<double> STimeRange::MaxViewStartTime() const
{
	return ViewEndTime();
}

TOptional<double> STimeRange::MinViewEndTime() const
{
	return ViewStartTime();
}

TOptional<double> STimeRange::MinPlayStartTime() const
{
	return WorkingStartTime();
}

TOptional<double> STimeRange::MaxPlayStartTime() const
{
	return PlayEndTime();
}

TOptional<double> STimeRange::MinPlayEndTime() const
{
	return PlayStartTime();
}

TOptional<double> STimeRange::MaxPlayEndTime() const
{
	return WorkingEndTime();
}

TOptional<double> STimeRange::MaxWorkingStartTime() const
{
	return ViewEndTime();
}

TOptional<double> STimeRange::MinWorkingEndTime() const
{
	return ViewStartTime();
}

void STimeRange::OnWorkingStartTimeCommitted(double NewValue, ETextCommit::Type InTextCommit)
{
	OnWorkingStartTimeChanged(NewValue);
}

void STimeRange::OnWorkingEndTimeCommitted(double NewValue, ETextCommit::Type InTextCommit)
{
	OnWorkingEndTimeChanged(NewValue);
}

void STimeRange::OnViewStartTimeCommitted(double NewValue, ETextCommit::Type InTextCommit)
{
	OnViewStartTimeChanged(NewValue);
}

void STimeRange::OnViewEndTimeCommitted(double NewValue, ETextCommit::Type InTextCommit)
{
	OnViewEndTimeChanged(NewValue);
}

void STimeRange::OnPlayStartTimeCommitted(double NewValue, ETextCommit::Type InTextCommit)
{
	OnPlayStartTimeChanged(NewValue);
}

void STimeRange::OnPlayEndTimeCommitted(double NewValue, ETextCommit::Type InTextCommit)
{
	OnPlayEndTimeChanged(NewValue);
}

void STimeRange::OnWorkingStartTimeChanged(double NewValue)
{
	FFrameRate TickResolution = TimeSliderController->GetTickResolution();
	double Time = TickResolution.AsSeconds(FFrameTime::FromDecimal(NewValue));

	// Clamp range is in seconds
	TimeSliderController->SetClampRange(Time, TimeSliderController->GetClampRange().GetUpperBoundValue());

	if (Time > TimeSliderController->GetViewRange().GetLowerBoundValue())
	{
		TimeSliderController->SetViewRange(Time, TimeSliderController->GetViewRange().GetUpperBoundValue(), EViewRangeInterpolation::Immediate);
	}
}

void STimeRange::OnWorkingEndTimeChanged(double NewValue)
{
	FFrameRate TickResolution = TimeSliderController->GetTickResolution();
	double Time = TickResolution.AsSeconds(FFrameTime::FromDecimal(NewValue));

	// Clamp range is in seconds
	TimeSliderController->SetClampRange(TimeSliderController->GetClampRange().GetLowerBoundValue(), Time);

	if (Time < TimeSliderController->GetViewRange().GetUpperBoundValue())
	{
		TimeSliderController->SetViewRange(TimeSliderController->GetViewRange().GetLowerBoundValue(), Time, EViewRangeInterpolation::Immediate);
	}
}

void STimeRange::OnViewStartTimeChanged(double NewValue)
{
	FFrameRate TickResolution = TimeSliderController->GetTickResolution();
	double Time = TickResolution.AsSeconds(FFrameTime::FromDecimal(NewValue));

	if (Time < TimeSliderController.Get()->GetClampRange().GetLowerBoundValue())
	{
		TimeSliderController->SetClampRange(Time, TimeSliderController->GetClampRange().GetUpperBoundValue());
	}

	TimeSliderController->SetViewRange(Time, TimeSliderController->GetViewRange().GetUpperBoundValue(), EViewRangeInterpolation::Immediate);
}

void STimeRange::OnViewEndTimeChanged(double NewValue)
{
	FFrameRate TickResolution = TimeSliderController->GetTickResolution();
	double Time = TickResolution.AsSeconds(FFrameTime::FromDecimal(NewValue));

	if (Time > TimeSliderController->GetClampRange().GetUpperBoundValue())
	{
		TimeSliderController->SetClampRange(TimeSliderController->GetClampRange().GetLowerBoundValue(), Time);
	}

	TimeSliderController->SetViewRange(TimeSliderController->GetViewRange().GetLowerBoundValue(), Time, EViewRangeInterpolation::Immediate);
}

void STimeRange::OnPlayStartTimeChanged(double NewValue)
{
	// We can't use the UI control to clamp the value to a Min/Max due to needing an unlimited-range spinbox for
	// UI adjustment to work in reasonable deltas, so instead we clamp it here.
	NewValue = FMath::Clamp(NewValue, MinPlayStartTime().GetValue(), MaxPlayStartTime().GetValue());

	FFrameRate TickResolution = TimeSliderController->GetTickResolution();
	FFrameTime Time = FFrameTime::FromDecimal(NewValue);
	double     TimeInSeconds = TickResolution.AsSeconds(Time);

	if (TimeInSeconds < TimeSliderController.Get()->GetClampRange().GetLowerBoundValue())
	{
		TimeSliderController->SetClampRange(TimeInSeconds, TimeSliderController->GetClampRange().GetLowerBoundValue());
	}

	FFrameNumber PlayDuration = MovieScene::DiscreteExclusiveUpper(TimeSliderController->GetPlayRange()) - Time.FrameNumber;
	TimeSliderController->SetPlayRange(Time.FrameNumber, PlayDuration.Value);
}

void STimeRange::OnPlayEndTimeChanged(double NewValue)
{
	// We can't use the UI control to clamp the value to a Min/Max due to needing an unlimited-range spinbox for
	// UI adjustment to work in reasonable deltas, so instead we clamp it here.
	NewValue = FMath::Clamp(NewValue, MinPlayEndTime().GetValue(), MaxPlayEndTime().GetValue());

	FFrameRate TickResolution = TimeSliderController->GetTickResolution();
	FFrameTime Time = FFrameTime::FromDecimal(NewValue);
	double     TimeInSeconds = TickResolution.AsSeconds(Time);

	if (TimeInSeconds > TimeSliderController->GetClampRange().GetUpperBoundValue())
	{
		TimeSliderController->SetClampRange(TimeSliderController->GetClampRange().GetLowerBoundValue(), TimeInSeconds);
	}

	FFrameNumber StartFrame   = MovieScene::DiscreteInclusiveLower(TimeSliderController->GetPlayRange());
	int32        PlayDuration = (Time.FrameNumber - StartFrame).Value;

	TimeSliderController->SetPlayRange(StartFrame, PlayDuration);
}

#undef LOCTEXT_NAMESPACE
