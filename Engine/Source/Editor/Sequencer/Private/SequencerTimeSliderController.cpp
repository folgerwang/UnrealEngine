// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SequencerTimeSliderController.h"
#include "Fonts/SlateFontInfo.h"
#include "Rendering/DrawElements.h"
#include "Misc/Paths.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "SequencerCommonHelpers.h"
#include "SequencerSettings.h"
#include "Misc/QualifiedFrameTime.h"
#include "MovieSceneTimeHelpers.h"
#include "CommonFrameRates.h"
#include "Sequencer.h"
#include "SequencerDisplayNode.h"

#define LOCTEXT_NAMESPACE "TimeSlider"

namespace ScrubConstants
{
	/** The minimum amount of pixels between each major ticks on the widget */
	const int32 MinPixelsPerDisplayTick = 12;

	/**The smallest number of units between between major tick marks */
	const float MinDisplayTickSpacing = 0.001f;

	/**The fraction of the current view range to scroll per unit delta  */
	const float ScrollPanFraction = 0.1f;
}

FSequencerTimeSliderController::FSequencerTimeSliderController( const FTimeSliderArgs& InArgs, TWeakPtr<FSequencer> InWeakSequencer )
	: WeakSequencer(InWeakSequencer)
	, TimeSliderArgs( InArgs )
	, DistanceDragged( 0.0f )
	, MouseDragType( DRAG_NONE )
	, bPanning( false )
{
	ScrubFillBrush              = FEditorStyle::GetBrush( TEXT( "Sequencer.Timeline.ScrubFill" ) );
	FrameBlockScrubHandleUpBrush   = FEditorStyle::GetBrush( TEXT( "Sequencer.Timeline.FrameBlockScrubHandleUp" ) ); 
	FrameBlockScrubHandleDownBrush = FEditorStyle::GetBrush( TEXT( "Sequencer.Timeline.FrameBlockScrubHandleDown" ) );
	VanillaScrubHandleUpBrush      = FEditorStyle::GetBrush( TEXT( "Sequencer.Timeline.VanillaScrubHandleUp" ) ); 
	VanillaScrubHandleDownBrush    = FEditorStyle::GetBrush( TEXT( "Sequencer.Timeline.VanillaScrubHandleDown" ) );
	ContextMenuSuppression = 0;
}

FFrameTime FSequencerTimeSliderController::ComputeScrubTimeFromMouse(const FGeometry& Geometry, FVector2D ScreenSpacePosition, FScrubRangeToScreen RangeToScreen) const
{
	FVector2D           CursorPos     = Geometry.AbsoluteToLocal( ScreenSpacePosition );
	double              MouseSeconds  = RangeToScreen.LocalXToInput( CursorPos.X );
	FFrameTime          ScrubTime     = MouseSeconds * GetTickResolution();

	if ( TimeSliderArgs.Settings->GetIsSnapEnabled() )
	{
		if (TimeSliderArgs.Settings->GetSnapPlayTimeToInterval())
		{
			// Set the style of the scrub handle
			TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
			if (Sequencer.IsValid() && Sequencer->GetScrubStyle() == ESequencerScrubberStyle::FrameBlock)
			{
				// Floor to the display frame
				ScrubTime = ConvertFrameTime(ConvertFrameTime(ScrubTime, GetTickResolution(), GetDisplayRate()).FloorToFrame(), GetDisplayRate(), GetTickResolution());
			}
			else
			{
				// Snap (round) to display rate
				ScrubTime = FFrameRate::Snap(ScrubTime, GetTickResolution(), GetDisplayRate());
			}
		}

		if (TimeSliderArgs.Settings->GetSnapPlayTimeToKeys())
		{
			// SnapTimeToNearestKey will return ScrubTime unmodified if there is no key within range.
			ScrubTime = SnapTimeToNearestKey(RangeToScreen, CursorPos.X, ScrubTime);
		}
	}

	if (TimeSliderArgs.Settings->ShouldKeepCursorInPlayRangeWhileScrubbing())
	{
		ScrubTime = MovieScene::ClampToDiscreteRange(ScrubTime, TimeSliderArgs.PlaybackRange.Get());
	}

	return ScrubTime;
}

FFrameTime FSequencerTimeSliderController::ComputeFrameTimeFromMouse(const FGeometry& Geometry, FVector2D ScreenSpacePosition, FScrubRangeToScreen RangeToScreen, bool CheckSnapping) const
{
	FVector2D CursorPos  = Geometry.AbsoluteToLocal( ScreenSpacePosition );
	double    MouseValue = RangeToScreen.LocalXToInput( CursorPos.X );

	if (CheckSnapping && TimeSliderArgs.Settings->GetIsSnapEnabled())
	{
		FFrameNumber        SnappedFrameNumber = (MouseValue * GetDisplayRate()).FloorToFrame();
		FQualifiedFrameTime RoundedPlayFrame   = FQualifiedFrameTime(SnappedFrameNumber, GetDisplayRate());
		return RoundedPlayFrame.ConvertTo(GetTickResolution());
	}
	else
	{
		return MouseValue * GetTickResolution();
	}
}

FSequencerTimeSliderController::FScrubberMetrics FSequencerTimeSliderController::GetHitTestScrubPixelMetrics(const FScrubRangeToScreen& RangeToScreen) const
{
	static const float DragToleranceSlateUnits = 2.f, MouseTolerance = 2.f;
	return GetScrubPixelMetrics(FQualifiedFrameTime(TimeSliderArgs.ScrubPosition.Get(), GetTickResolution()), RangeToScreen, DragToleranceSlateUnits + MouseTolerance);
}

FSequencerTimeSliderController::FScrubberMetrics FSequencerTimeSliderController::GetScrubPixelMetrics(const FQualifiedFrameTime& ScrubTime, const FScrubRangeToScreen& RangeToScreen, float DilationPixels) const
{
	FFrameRate DisplayRate = GetDisplayRate();

	FScrubberMetrics Metrics;

	static float MinScrubSize = 14.f;

	const FFrameNumber Frame = ScrubTime.ConvertTo(DisplayRate).FloorToFrame();

	float FrameStartPixel = RangeToScreen.InputToLocalX(  Frame    / DisplayRate );
	float FrameEndPixel   = RangeToScreen.InputToLocalX( (Frame+1) / DisplayRate ) - 1;

	{
		float RoundedStartPixel = FMath::RoundToInt(FrameStartPixel);
		FrameEndPixel -= (FrameStartPixel - RoundedStartPixel);

		FrameStartPixel = RoundedStartPixel;
		FrameEndPixel   = FMath::Max(FrameEndPixel, FrameStartPixel + 1);
	}

	// Store off the pixel width of the frame
	Metrics.FrameExtentsPx = TRange<float>(FrameStartPixel, FrameEndPixel);

	// Set the style of the scrub handle
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	Metrics.Style = Sequencer.IsValid() ? Sequencer->GetScrubStyle() : ESequencerScrubberStyle::Vanilla;

	// Always draw the extents on the section area for frame block styles
	Metrics.bDrawExtents = Metrics.Style == ESequencerScrubberStyle::FrameBlock;

	// If it's vanilla style or too small to show the frame width, set that up
	if (Metrics.Style == ESequencerScrubberStyle::Vanilla || FrameEndPixel - FrameStartPixel < MinScrubSize)
	{
		Metrics.Style = ESequencerScrubberStyle::Vanilla;

		float ScrubPixel = RangeToScreen.InputToLocalX(ScrubTime.AsSeconds());
		Metrics.HandleRangePx = TRange<float>(ScrubPixel - MinScrubSize*.5f, ScrubPixel + MinScrubSize*.5f);
	}
	else
	{
		Metrics.HandleRangePx = Metrics.FrameExtentsPx;
	}

	return Metrics;
}

struct FSequencerTimeSliderController::FDrawTickArgs
{
	/** Geometry of the area */
	FGeometry AllottedGeometry;
	/** Culling rect of the area */
	FSlateRect CullingRect;
	/** Color of each tick */
	FLinearColor TickColor;
	/** Offset in Y where to start the tick */
	float TickOffset;
	/** Height in of major ticks */
	float MajorTickHeight;
	/** Start layer for elements */
	int32 StartLayer;
	/** Draw effects to apply */
	ESlateDrawEffect DrawEffects;
	/** Whether or not to only draw major ticks */
	bool bOnlyDrawMajorTicks;
	/** Whether or not to mirror labels */
	bool bMirrorLabels;
	
};

void FSequencerTimeSliderController::DrawTicks( FSlateWindowElementList& OutDrawElements, const TRange<double>& ViewRange, const FScrubRangeToScreen& RangeToScreen, FDrawTickArgs& InArgs ) const
{
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return;
	}

	FFrameRate     TickResolution  = GetTickResolution();
	FFrameRate     DisplayRate     = GetDisplayRate();
	FPaintGeometry PaintGeometry   = InArgs.AllottedGeometry.ToPaintGeometry();
	FSlateFontInfo SmallLayoutFont = FCoreStyle::GetDefaultFontStyle("Regular", 8);

	double MajorGridStep  = 0.0;
	int32  MinorDivisions = 0;
	if (!Sequencer->GetGridMetrics(InArgs.AllottedGeometry.Size.X, MajorGridStep, MinorDivisions))
	{
		return;
	}

	if (InArgs.bOnlyDrawMajorTicks)
	{
		MinorDivisions = 0;
	}

	TArray<FVector2D> LinePoints;
	LinePoints.SetNumUninitialized(2);

	const bool bAntiAliasLines = false;

	const double FirstMajorLine = FMath::FloorToDouble(ViewRange.GetLowerBoundValue() / MajorGridStep) * MajorGridStep;
	const double LastMajorLine  = FMath::CeilToDouble(ViewRange.GetUpperBoundValue() / MajorGridStep) * MajorGridStep;

	const float  FlooredScrubPx  = RangeToScreen.InputToLocalX(ConvertFrameTime(TimeSliderArgs.ScrubPosition.Get(), TickResolution, GetDisplayRate()).FloorToFrame() / DisplayRate);

	for (double CurrentMajorLine = FirstMajorLine; CurrentMajorLine < LastMajorLine; CurrentMajorLine += MajorGridStep)
	{
		float MajorLinePx = RangeToScreen.InputToLocalX( CurrentMajorLine );

		LinePoints[0] = FVector2D( MajorLinePx, InArgs.TickOffset );
		LinePoints[1] = FVector2D( MajorLinePx, InArgs.TickOffset + InArgs.MajorTickHeight );

		// Draw each tick mark
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			InArgs.StartLayer,
			PaintGeometry,
			LinePoints,
			InArgs.DrawEffects,
			InArgs.TickColor,
			bAntiAliasLines
			);

		if (!InArgs.bOnlyDrawMajorTicks && !FMath::IsNearlyEqual(MajorLinePx, FlooredScrubPx, 3.f))
		{
			FString FrameString = TimeSliderArgs.NumericTypeInterface->ToString((CurrentMajorLine * TickResolution).RoundToFrame().Value);

			// Space the text between the tick mark but slightly above
			FVector2D TextOffset( MajorLinePx + 5.f, InArgs.bMirrorLabels ? 1.f : FMath::Abs( InArgs.AllottedGeometry.Size.Y - (InArgs.MajorTickHeight+3.f) ) );
			FSlateDrawElement::MakeText(
				OutDrawElements,
				InArgs.StartLayer+1, 
				InArgs.AllottedGeometry.ToPaintGeometry( TextOffset, InArgs.AllottedGeometry.Size ), 
				FrameString, 
				SmallLayoutFont,
				InArgs.DrawEffects,
				InArgs.TickColor*0.65f 
			);
		}

		for (int32 Step = 1; Step < MinorDivisions; ++Step)
		{
			// Compute the size of each tick mark.  If we are half way between to visible values display a slightly larger tick mark
			const float MinorTickHeight = ( (MinorDivisions % 2 == 0) && (Step % (MinorDivisions/2)) == 0 ) ? 6.0f : 2.0f;
			const float MinorLinePx = RangeToScreen.InputToLocalX( CurrentMajorLine + Step*MajorGridStep/MinorDivisions );

			LinePoints[0] = FVector2D(MinorLinePx, InArgs.bMirrorLabels ? 0.0f : FMath::Abs( InArgs.AllottedGeometry.Size.Y - MinorTickHeight ) );
			LinePoints[1] = FVector2D(MinorLinePx, LinePoints[0].Y + MinorTickHeight);

			// Draw each sub mark
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				InArgs.StartLayer,
				PaintGeometry,
				LinePoints,
				InArgs.DrawEffects,
				InArgs.TickColor,
				bAntiAliasLines
			);
		}
	}
}

int32 FSequencerTimeSliderController::DrawMarkedFrames( const FGeometry& AllottedGeometry, const FScrubRangeToScreen& RangeToScreen, FSlateWindowElementList& OutDrawElements, int32 LayerId, const ESlateDrawEffect& DrawEffects ) const
{
	TSet<FFrameNumber> MarkedFrames = TimeSliderArgs.MarkedFrames.Get();
	if (MarkedFrames.Num() < 1)
	{
		return LayerId;
	}

	for (FFrameNumber TickFrame : MarkedFrames)
	{
		double Seconds = TickFrame / GetTickResolution();

		const float  LinePos = RangeToScreen.InputToLocalX( Seconds );
		TArray<FVector2D> LinePoints;
		LinePoints.AddUninitialized(2);
		LinePoints[0] = FVector2D( LinePos, 0.0f );
		LinePoints[1] = FVector2D( LinePos, FMath::FloorToFloat( AllottedGeometry.Size.Y ) );

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId+1,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			DrawEffects,
			FLinearColor(0.f, 1.f, 1.f, 0.4f),
			false
			);
	}

	return LayerId + 1;
}

int32 FSequencerTimeSliderController::OnPaintTimeSlider( bool bMirrorLabels, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return LayerId;
	}

	const bool bEnabled = bParentEnabled;
	const ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	TRange<double> LocalViewRange      = TimeSliderArgs.ViewRange.Get();
	const float    LocalViewRangeMin   = LocalViewRange.GetLowerBoundValue();
	const float    LocalViewRangeMax   = LocalViewRange.GetUpperBoundValue();
	const float    LocalSequenceLength = LocalViewRangeMax-LocalViewRangeMin;
	
	FVector2D Scale = FVector2D(1.0f,1.0f);
	if ( LocalSequenceLength > 0)
	{
		FScrubRangeToScreen RangeToScreen( LocalViewRange, AllottedGeometry.Size );

		// draw tick marks
		const float MajorTickHeight = 9.0f;
	
		FDrawTickArgs Args;
		{
			Args.AllottedGeometry = AllottedGeometry;
			Args.bMirrorLabels = bMirrorLabels;
			Args.bOnlyDrawMajorTicks = false;
			Args.TickColor = FLinearColor::White;
			Args.CullingRect = MyCullingRect;
			Args.DrawEffects = DrawEffects;
			Args.StartLayer = LayerId;
			Args.TickOffset = bMirrorLabels ? 0.0f : FMath::Abs( AllottedGeometry.Size.Y - MajorTickHeight );
			Args.MajorTickHeight = MajorTickHeight;
		}

		DrawTicks( OutDrawElements, LocalViewRange, RangeToScreen, Args );

		// draw playback & selection range
		FPaintPlaybackRangeArgs PlaybackRangeArgs(
			bMirrorLabels ? FEditorStyle::GetBrush("Sequencer.Timeline.PlayRange_Bottom_L") : FEditorStyle::GetBrush("Sequencer.Timeline.PlayRange_Top_L"),
			bMirrorLabels ? FEditorStyle::GetBrush("Sequencer.Timeline.PlayRange_Bottom_R") : FEditorStyle::GetBrush("Sequencer.Timeline.PlayRange_Top_R"),
			6.f
		);

		LayerId = DrawPlaybackRange(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, RangeToScreen, PlaybackRangeArgs);
		LayerId = DrawSubSequenceRange(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, RangeToScreen, PlaybackRangeArgs);

		PlaybackRangeArgs.SolidFillOpacity = 0.05f;
		LayerId = DrawSelectionRange(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, RangeToScreen, PlaybackRangeArgs);

		// Draw the scrub handle
		FQualifiedFrameTime ScrubPosition = FQualifiedFrameTime(TimeSliderArgs.ScrubPosition.Get(), GetTickResolution());
		FScrubberMetrics    ScrubMetrics  = GetScrubPixelMetrics(ScrubPosition, RangeToScreen);
		const float         HandleStart   = ScrubMetrics.HandleRangePx.GetLowerBoundValue();
		const float         HandleEnd     = ScrubMetrics.HandleRangePx.GetUpperBoundValue();

		const int32 ArrowLayer = LayerId + 2;
		FPaintGeometry MyGeometry =	AllottedGeometry.ToPaintGeometry( FVector2D( HandleStart, 0 ), FVector2D( HandleEnd - HandleStart, AllottedGeometry.Size.Y ) );
		FLinearColor ScrubColor = InWidgetStyle.GetColorAndOpacityTint();
		{
			// @todo Sequencer this color should be specified in the style
			ScrubColor.A = ScrubColor.A * 0.75f;
			ScrubColor.B *= 0.1f;
			ScrubColor.G *= 0.2f;
		}

		const FSlateBrush* Brush = ScrubMetrics.Style == ESequencerScrubberStyle::Vanilla
			? ( bMirrorLabels ? VanillaScrubHandleUpBrush    : VanillaScrubHandleDownBrush )
			: ( bMirrorLabels ? FrameBlockScrubHandleUpBrush : FrameBlockScrubHandleDownBrush );

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			ArrowLayer,
			MyGeometry,
			Brush,
			DrawEffects,
			ScrubColor
		);

		LayerId = DrawMarkedFrames(AllottedGeometry, RangeToScreen, OutDrawElements, LayerId, DrawEffects);

		{
			// Draw the current time next to the scrub handle
			FString FrameString;

			FrameString = TimeSliderArgs.NumericTypeInterface->ToString(TimeSliderArgs.ScrubPosition.Get().GetFrame().Value);

			FSlateFontInfo SmallLayoutFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);

			const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			FVector2D TextSize = FontMeasureService->Measure(FrameString, SmallLayoutFont);

			// Flip the text position if getting near the end of the view range
			static const float TextOffsetPx  = 2.f;
			bool  bDrawLeft    = (AllottedGeometry.Size.X - HandleEnd) < (TextSize.X + 14.f) - TextOffsetPx;
			float TextPosition = bDrawLeft ? HandleStart - TextSize.X - TextOffsetPx : HandleEnd + TextOffsetPx;

			FVector2D TextOffset( TextPosition, Args.bMirrorLabels ? Args.AllottedGeometry.Size.Y - TextSize.Y : 0.f );

			FSlateDrawElement::MakeText(
				OutDrawElements,
				Args.StartLayer+1, 
				Args.AllottedGeometry.ToPaintGeometry( TextOffset, TextSize ), 
				FrameString, 
				SmallLayoutFont,
				Args.DrawEffects,
				Args.TickColor 
			);
		}
		
		if (MouseDragType == DRAG_SETTING_RANGE)
		{
			FFrameRate Resolution = GetTickResolution();
			FFrameTime MouseDownTime[2];

			FScrubRangeToScreen MouseDownRange(TimeSliderArgs.ViewRange.Get(), MouseDownGeometry.Size);
			MouseDownTime[0] = ComputeFrameTimeFromMouse(MouseDownGeometry, MouseDownPosition[0], MouseDownRange);
			MouseDownTime[1] = ComputeFrameTimeFromMouse(MouseDownGeometry, MouseDownPosition[1], MouseDownRange);

			float      MouseStartPosX = RangeToScreen.InputToLocalX(MouseDownTime[0] / Resolution);
			float      MouseEndPosX   = RangeToScreen.InputToLocalX(MouseDownTime[1] / Resolution);

			float RangePosX = MouseStartPosX < MouseEndPosX ? MouseStartPosX : MouseEndPosX;
			float RangeSizeX = FMath::Abs(MouseStartPosX - MouseEndPosX);

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId+1,
				AllottedGeometry.ToPaintGeometry( FVector2D(RangePosX, 0.f), FVector2D(RangeSizeX, AllottedGeometry.Size.Y) ),
				bMirrorLabels ? VanillaScrubHandleDownBrush : VanillaScrubHandleUpBrush,
				DrawEffects,
				MouseStartPosX < MouseEndPosX ? FLinearColor(0.5f, 0.5f, 0.5f) : FLinearColor(0.25f, 0.3f, 0.3f)
			);
		}

		return ArrowLayer;
	}

	return LayerId;
}


int32 FSequencerTimeSliderController::DrawSelectionRange(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FScrubRangeToScreen& RangeToScreen, const FPaintPlaybackRangeArgs& Args) const
{
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return LayerId;
	}

	TRange<double> SelectionRange = TimeSliderArgs.SelectionRange.Get() / GetTickResolution();

	if (!SelectionRange.IsEmpty())
	{
		const float SelectionRangeL = RangeToScreen.InputToLocalX(SelectionRange.GetLowerBoundValue()) - 1;
		const float SelectionRangeR = RangeToScreen.InputToLocalX(SelectionRange.GetUpperBoundValue()) + 1;
		const auto DrawColor = FEditorStyle::GetSlateColor("SelectionColor").GetColor(FWidgetStyle());

		if (Args.SolidFillOpacity > 0.f)
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 1,
				AllottedGeometry.ToPaintGeometry(FVector2D(SelectionRangeL, 0.f), FVector2D(SelectionRangeR - SelectionRangeL, AllottedGeometry.Size.Y)),
				FEditorStyle::GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				DrawColor.CopyWithNewOpacity(Args.SolidFillOpacity)
			);
		}

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(FVector2D(SelectionRangeL, 0.f), FVector2D(Args.BrushWidth, AllottedGeometry.Size.Y)),
			Args.StartBrush,
			ESlateDrawEffect::None,
			DrawColor
		);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(FVector2D(SelectionRangeR - Args.BrushWidth, 0.f), FVector2D(Args.BrushWidth, AllottedGeometry.Size.Y)),
			Args.EndBrush,
			ESlateDrawEffect::None,
			DrawColor
		);
	}

	return LayerId + 1;
}


int32 FSequencerTimeSliderController::DrawPlaybackRange(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FScrubRangeToScreen& RangeToScreen, const FPaintPlaybackRangeArgs& Args) const
{
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return LayerId;
	}

	if (!TimeSliderArgs.PlaybackRange.IsSet())
	{
		return LayerId;
	}

	const uint8 OpacityBlend = TimeSliderArgs.SubSequenceRange.Get().IsSet() ? 128 : 255;

	TRange<FFrameNumber> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();
	FFrameRate TickResolution = GetTickResolution();
	const float PlaybackRangeL = RangeToScreen.InputToLocalX(MovieScene::DiscreteInclusiveLower(PlaybackRange) / TickResolution);
	const float PlaybackRangeR = RangeToScreen.InputToLocalX(MovieScene::DiscreteExclusiveUpper(PlaybackRange) / TickResolution) - 1;

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2D(PlaybackRangeL, 0.f), FVector2D(Args.BrushWidth, AllottedGeometry.Size.Y)),
		Args.StartBrush,
		ESlateDrawEffect::None,
		FColor(32, 128, 32, OpacityBlend)	// 120, 75, 50 (HSV)
	);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2D(PlaybackRangeR - Args.BrushWidth, 0.f), FVector2D(Args.BrushWidth, AllottedGeometry.Size.Y)),
		Args.EndBrush,
		ESlateDrawEffect::None,
		FColor(128, 32, 32, OpacityBlend)	// 0, 75, 50 (HSV)
	);

	// Black tint for excluded regions
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2D(0.f, 0.f), FVector2D(PlaybackRangeL, AllottedGeometry.Size.Y)),
		FEditorStyle::GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		FLinearColor::Black.CopyWithNewOpacity(0.3f * OpacityBlend / 255.f)
	);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2D(PlaybackRangeR, 0.f), FVector2D(AllottedGeometry.Size.X - PlaybackRangeR, AllottedGeometry.Size.Y)),
		FEditorStyle::GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		FLinearColor::Black.CopyWithNewOpacity(0.3f * OpacityBlend / 255.f)
	);

	return LayerId + 1;
}

int32 FSequencerTimeSliderController::DrawSubSequenceRange(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FScrubRangeToScreen& RangeToScreen, const FPaintPlaybackRangeArgs& Args) const
{
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return LayerId;
	}

	TOptional<TRange<FFrameNumber>> RangeValue;
	RangeValue = TimeSliderArgs.SubSequenceRange.Get(RangeValue);

	if (!RangeValue.IsSet() || RangeValue->IsEmpty())
	{
		return LayerId;
	}

	const FFrameRate   Resolution = GetTickResolution();
	const FFrameNumber LowerFrame = MovieScene::DiscreteInclusiveLower(RangeValue.GetValue());
	const FFrameNumber UpperFrame = MovieScene::DiscreteExclusiveUpper(RangeValue.GetValue());

	const float SubSequenceRangeL = RangeToScreen.InputToLocalX(LowerFrame / Resolution) - 1;
	const float SubSequenceRangeR = RangeToScreen.InputToLocalX(UpperFrame / Resolution) + 1;

	static const FSlateBrush* LineBrushL(FEditorStyle::GetBrush("Sequencer.Timeline.PlayRange_L"));
	static const FSlateBrush* LineBrushR(FEditorStyle::GetBrush("Sequencer.Timeline.PlayRange_R"));

	FColor GreenTint(32, 128, 32);	// 120, 75, 50 (HSV)
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2D(SubSequenceRangeL, 0.f), FVector2D(Args.BrushWidth, AllottedGeometry.Size.Y)),
		LineBrushL,
		ESlateDrawEffect::None,
		GreenTint
	);

	FColor RedTint(128, 32, 32);	// 0, 75, 50 (HSV)
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2D(SubSequenceRangeR - Args.BrushWidth, 0.f), FVector2D(Args.BrushWidth, AllottedGeometry.Size.Y)),
		LineBrushR,
		ESlateDrawEffect::None,
		RedTint
	);

	// Black tint for excluded regions
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2D(0.f, 0.f), FVector2D(SubSequenceRangeL, AllottedGeometry.Size.Y)),
		FEditorStyle::GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		FLinearColor::Black.CopyWithNewOpacity(0.3f)
	);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2D(SubSequenceRangeR, 0.f), FVector2D(AllottedGeometry.Size.X - SubSequenceRangeR, AllottedGeometry.Size.Y)),
		FEditorStyle::GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		FLinearColor::Black.CopyWithNewOpacity(0.3f)
	);

	// Hash applied to the left and right of the sequence bounds
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2D(SubSequenceRangeL - 16.f, 0.f), FVector2D(16.f, AllottedGeometry.Size.Y)),
		FEditorStyle::GetBrush("Sequencer.Timeline.SubSequenceRangeHashL"),
		ESlateDrawEffect::None,
		GreenTint
	);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2D(SubSequenceRangeR, 0.f), FVector2D(16.f, AllottedGeometry.Size.Y)),
		FEditorStyle::GetBrush("Sequencer.Timeline.SubSequenceRangeHashR"),
		ESlateDrawEffect::None,
		RedTint
	);

	return LayerId + 1;
}

FReply FSequencerTimeSliderController::OnMouseButtonDown( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	DistanceDragged = 0;
	MouseDownPosition[0] = MouseDownPosition[1] = MouseEvent.GetScreenSpacePosition();
	MouseDownGeometry = MyGeometry;
	return FReply::Unhandled();
}

FReply FSequencerTimeSliderController::OnMouseButtonUp( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	bool bHandleLeftMouseButton  = MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton  && WidgetOwner.HasMouseCapture();
	bool bHandleRightMouseButton = MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && WidgetOwner.HasMouseCapture() && TimeSliderArgs.AllowZoom ;

	FScrubRangeToScreen RangeToScreen = FScrubRangeToScreen(TimeSliderArgs.ViewRange.Get(), MyGeometry.Size);
	FFrameTime          MouseTime     = ComputeFrameTimeFromMouse(MyGeometry, MouseEvent.GetScreenSpacePosition(), RangeToScreen);

	if ( bHandleRightMouseButton )
	{
		if (!bPanning)
		{
			// Open a context menu if allowed
			if (ContextMenuSuppression == 0 && TimeSliderArgs.PlaybackRange.IsSet())
			{
				TSharedRef<SWidget> MenuContent = OpenSetPlaybackRangeMenu(MouseTime.FrameNumber);
				FSlateApplication::Get().PushMenu(
					WidgetOwner.AsShared(),
					MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath(),
					MenuContent,
					MouseEvent.GetScreenSpacePosition(),
					FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu )
					);

				return FReply::Handled().SetUserFocus(MenuContent, EFocusCause::SetDirectly).ReleaseMouseCapture();
			}

			// return unhandled in case our parent wants to use our right mouse button to open a context menu
			if (DistanceDragged == 0.f)
			{
				return FReply::Unhandled().ReleaseMouseCapture();
			}
		}
		
		bPanning = false;
		
		return FReply::Handled().ReleaseMouseCapture();
	}
	else if ( bHandleLeftMouseButton )
	{
		if (MouseDragType == DRAG_PLAYBACK_START)
		{
			TimeSliderArgs.OnPlaybackRangeEndDrag.ExecuteIfBound();
		}
		else if (MouseDragType == DRAG_PLAYBACK_END)
		{
			TimeSliderArgs.OnPlaybackRangeEndDrag.ExecuteIfBound();
		}
		else if (MouseDragType == DRAG_SELECTION_START)
		{
			TimeSliderArgs.OnSelectionRangeEndDrag.ExecuteIfBound();
		}
		else if (MouseDragType == DRAG_SELECTION_END)
		{
			TimeSliderArgs.OnSelectionRangeEndDrag.ExecuteIfBound();
		}
		else if (MouseDragType == DRAG_SETTING_RANGE)
		{
			// Zooming
			FFrameTime MouseDownStart = ComputeFrameTimeFromMouse(MyGeometry, MouseDownPosition[0], RangeToScreen);

			const bool bCanZoomIn  = MouseTime > MouseDownStart;
			const bool bCanZoomOut = ViewRangeStack.Num() > 0;
			if (bCanZoomIn || bCanZoomOut)
			{
				TRange<double> ViewRange = TimeSliderArgs.ViewRange.Get();
				if (!bCanZoomIn)
				{
					ViewRange = ViewRangeStack.Pop();
				}

				if (bCanZoomIn)
				{
					// push the current value onto the stack
					ViewRangeStack.Add(ViewRange);

					ViewRange = TRange<double>(MouseDownStart.FrameNumber / GetTickResolution(), MouseTime.FrameNumber / GetTickResolution());
				}
				
				TimeSliderArgs.OnViewRangeChanged.ExecuteIfBound(ViewRange, EViewRangeInterpolation::Immediate);
				if( !TimeSliderArgs.ViewRange.IsBound() )
				{
					// The output is not bound to a delegate so we'll manage the value ourselves
					TimeSliderArgs.ViewRange.Set(ViewRange);
				}
			}
		}
		else
		{
			TimeSliderArgs.OnEndScrubberMovement.ExecuteIfBound();

			FFrameTime ScrubTime = MouseTime;
			FVector2D CursorPos  = MouseEvent.GetScreenSpacePosition();

			if (MouseDragType == DRAG_SCRUBBING_TIME)
			{
				ScrubTime = ComputeScrubTimeFromMouse(MyGeometry, CursorPos, RangeToScreen);
			}
			else if (TimeSliderArgs.Settings->GetSnapPlayTimeToKeys())
			{
				ScrubTime = SnapTimeToNearestKey(RangeToScreen, CursorPos.X, ScrubTime);
			}

			CommitScrubPosition( ScrubTime, /*bIsScrubbing=*/false );
		}

		MouseDragType = DRAG_NONE;
		DistanceDragged = 0.f;

		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}


FReply FSequencerTimeSliderController::OnMouseMove( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return FReply::Unhandled();
	}

	bool bHandleLeftMouseButton  = MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton  );
	bool bHandleRightMouseButton = MouseEvent.IsMouseButtonDown( EKeys::RightMouseButton ) && TimeSliderArgs.AllowZoom;

	if (bHandleRightMouseButton)
	{
		if (!bPanning)
		{
			DistanceDragged += FMath::Abs( MouseEvent.GetCursorDelta().X );
			if ( DistanceDragged > FSlateApplication::Get().GetDragTriggerDistance() )
			{
				bPanning = true;
			}
		}
		else
		{
			TRange<double> LocalViewRange = TimeSliderArgs.ViewRange.Get();
			double LocalViewRangeMin = LocalViewRange.GetLowerBoundValue();
			double LocalViewRangeMax = LocalViewRange.GetUpperBoundValue();

			FScrubRangeToScreen ScaleInfo( LocalViewRange, MyGeometry.Size );
			FVector2D ScreenDelta = MouseEvent.GetCursorDelta();
			FVector2D InputDelta;
			InputDelta.X = ScreenDelta.X/ScaleInfo.PixelsPerInput;

			double NewViewOutputMin = LocalViewRangeMin - InputDelta.X;
			double NewViewOutputMax = LocalViewRangeMax - InputDelta.X;

			ClampViewRange(NewViewOutputMin, NewViewOutputMax);
			SetViewRange(NewViewOutputMin, NewViewOutputMax, EViewRangeInterpolation::Immediate);
		}
	}
	else if (bHandleLeftMouseButton)
	{
		TRange<double> LocalViewRange = TimeSliderArgs.ViewRange.Get();
		FScrubRangeToScreen RangeToScreen(LocalViewRange, MyGeometry.Size);
		DistanceDragged += FMath::Abs( MouseEvent.GetCursorDelta().X );

		if ( MouseDragType == DRAG_NONE )
		{
			if ( DistanceDragged > FSlateApplication::Get().GetDragTriggerDistance() )
			{
				FFrameTime MouseDownFree = ComputeFrameTimeFromMouse(MyGeometry, MouseDownPosition[0], RangeToScreen, false);

				const FFrameRate TickResolution     = GetTickResolution();
				const bool       bLockedPlayRange   = TimeSliderArgs.IsPlaybackRangeLocked.Get();
				const float      MouseDownPixel     = RangeToScreen.InputToLocalX(MouseDownFree / TickResolution);
				const bool       bHitScrubber       = GetHitTestScrubPixelMetrics(RangeToScreen).HandleRangePx.Contains(MouseDownPixel);

				TRange<double>   SelectionRange   = TimeSliderArgs.SelectionRange.Get() / TickResolution;
				TRange<double>   PlaybackRange    = TimeSliderArgs.PlaybackRange.Get()  / TickResolution;

				// Disable selection range test if it's empty so that the playback range scrubbing gets priority
				if (!SelectionRange.IsEmpty() && !bHitScrubber && HitTestRangeEnd(RangeToScreen, SelectionRange, MouseDownPixel))
				{
					// selection range end scrubber
					MouseDragType = DRAG_SELECTION_END;
					TimeSliderArgs.OnSelectionRangeBeginDrag.ExecuteIfBound();
				}
				else if (!SelectionRange.IsEmpty() && !bHitScrubber && HitTestRangeStart(RangeToScreen, SelectionRange, MouseDownPixel))
				{
					// selection range start scrubber
					MouseDragType = DRAG_SELECTION_START;
					TimeSliderArgs.OnSelectionRangeBeginDrag.ExecuteIfBound();
				}
				else if (!bLockedPlayRange && !bHitScrubber && HitTestRangeEnd(RangeToScreen, PlaybackRange, MouseDownPixel))
				{
					// playback range end scrubber
					MouseDragType = DRAG_PLAYBACK_END;
					TimeSliderArgs.OnPlaybackRangeBeginDrag.ExecuteIfBound();
				}
				else if (!bLockedPlayRange && !bHitScrubber && HitTestRangeStart(RangeToScreen, PlaybackRange, MouseDownPixel))
				{
					// playback range start scrubber
					MouseDragType = DRAG_PLAYBACK_START;
					TimeSliderArgs.OnPlaybackRangeBeginDrag.ExecuteIfBound();
				}
				else if (FSlateApplication::Get().GetModifierKeys().AreModifersDown(EModifierKey::Control))
				{
					MouseDragType = DRAG_SETTING_RANGE;
				}
				else
				{
					MouseDragType = DRAG_SCRUBBING_TIME;
					TimeSliderArgs.OnBeginScrubberMovement.ExecuteIfBound();
				}
			}
		}
		else
		{
			FFrameTime MouseTime = ComputeFrameTimeFromMouse(MyGeometry, MouseEvent.GetScreenSpacePosition(), RangeToScreen);
			FFrameTime ScrubTime = ComputeScrubTimeFromMouse(MyGeometry, MouseEvent.GetScreenSpacePosition(), RangeToScreen);

			// Set the start range time?
			if (MouseDragType == DRAG_PLAYBACK_START)
			{
				SetPlaybackRangeStart(MouseTime.FrameNumber);
			}
			// Set the end range time?
			else if(MouseDragType == DRAG_PLAYBACK_END)
			{
				SetPlaybackRangeEnd(MouseTime.FrameNumber);
			}
			else if (MouseDragType == DRAG_SELECTION_START)
			{
				SetSelectionRangeStart(MouseTime.FrameNumber);
			}
			// Set the end range time?
			else if(MouseDragType == DRAG_SELECTION_END)
			{
				SetSelectionRangeEnd(MouseTime.FrameNumber);
			}
			else if (MouseDragType == DRAG_SCRUBBING_TIME)
			{
				// Delegate responsibility for clamping to the current viewrange to the client
				CommitScrubPosition( ScrubTime, /*bIsScrubbing=*/true );
			}
			else if (MouseDragType == DRAG_SETTING_RANGE)
			{
				MouseDownPosition[1] = MouseEvent.GetScreenSpacePosition();
			}
		}
	}

	if ( DistanceDragged != 0.f && (bHandleLeftMouseButton || bHandleRightMouseButton) )
	{
		return FReply::Handled().CaptureMouse(WidgetOwner.AsShared());
	}


	return FReply::Handled();
}


void FSequencerTimeSliderController::CommitScrubPosition( FFrameTime NewValue, bool bIsScrubbing )
{
	// Manage the scrub position ourselves if its not bound to a delegate
	if ( !TimeSliderArgs.ScrubPosition.IsBound() )
	{
		TimeSliderArgs.ScrubPosition.Set( NewValue );
	}

	TimeSliderArgs.OnScrubPositionChanged.ExecuteIfBound( NewValue, bIsScrubbing );
}

FReply FSequencerTimeSliderController::OnMouseWheel( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	TOptional<TRange<float>> NewTargetRange;

	if ( TimeSliderArgs.AllowZoom && MouseEvent.IsControlDown() )
	{
		float MouseFractionX = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X / MyGeometry.GetLocalSize().X;

		// If zooming on the current time, adjust mouse fractionX
		if (TimeSliderArgs.Settings->GetZoomPosition() == ESequencerZoomPosition::SZP_CurrentTime)
		{
			const double ScrubPosition = TimeSliderArgs.ScrubPosition.Get() / GetTickResolution();
			if (TimeSliderArgs.ViewRange.Get().Contains(ScrubPosition))
			{
				FScrubRangeToScreen RangeToScreen(TimeSliderArgs.ViewRange.Get(), MyGeometry.Size);
				float TimePosition = RangeToScreen.InputToLocalX(ScrubPosition);
				MouseFractionX = TimePosition / MyGeometry.GetLocalSize().X;
			}
		}

		const float ZoomDelta = -0.2f * MouseEvent.GetWheelDelta();
		if (ZoomByDelta(ZoomDelta, MouseFractionX))
		{
			return FReply::Handled();
		}
	}
	else if (MouseEvent.IsShiftDown())
	{
		PanByDelta(-MouseEvent.GetWheelDelta());
		return FReply::Handled();
	}
	
	return FReply::Unhandled();
}

FCursorReply FSequencerTimeSliderController::OnCursorQuery( TSharedRef<const SWidget> WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
{
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return FCursorReply::Unhandled();
	}

	FScrubRangeToScreen RangeToScreen(TimeSliderArgs.ViewRange.Get(), MyGeometry.Size);

	const FFrameRate TickResolution   = GetTickResolution();
	const bool       bLockedPlayRange = TimeSliderArgs.IsPlaybackRangeLocked.Get();
	const float      HitTestPixel     = MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition()).X;
	const bool       bHitScrubber     = GetHitTestScrubPixelMetrics(RangeToScreen).HandleRangePx.Contains(HitTestPixel);

	TRange<double>   SelectionRange   = TimeSliderArgs.SelectionRange.Get() / TickResolution;
	TRange<double>   PlaybackRange    = TimeSliderArgs.PlaybackRange.Get()  / TickResolution;

	if (MouseDragType == DRAG_SCRUBBING_TIME)
	{
		return FCursorReply::Unhandled();
	}

	// Use L/R resize cursor if we're dragging or hovering a playback range bound
	if ((MouseDragType == DRAG_PLAYBACK_END) ||
		(MouseDragType == DRAG_PLAYBACK_START) ||
		(MouseDragType == DRAG_SELECTION_START) ||
		(MouseDragType == DRAG_SELECTION_END) ||
		(!bLockedPlayRange         && !bHitScrubber && HitTestRangeStart(RangeToScreen, PlaybackRange,  HitTestPixel)) ||
		(!bLockedPlayRange         && !bHitScrubber && HitTestRangeEnd(  RangeToScreen, PlaybackRange,  HitTestPixel)) ||
		(!SelectionRange.IsEmpty() && !bHitScrubber && HitTestRangeStart(RangeToScreen, SelectionRange, HitTestPixel)) ||
		(!SelectionRange.IsEmpty() && !bHitScrubber && HitTestRangeEnd(  RangeToScreen, SelectionRange, HitTestPixel)))
	{
		return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
	}

	return FCursorReply::Unhandled();
}

int32 FSequencerTimeSliderController::OnPaintSectionView( const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bEnabled, const FPaintSectionAreaViewArgs& Args ) const
{
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return LayerId;
	}

	const ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	TRange<double> LocalViewRange = TimeSliderArgs.ViewRange.Get();
	FScrubRangeToScreen RangeToScreen( LocalViewRange, AllottedGeometry.Size );

	if (Args.PlaybackRangeArgs.IsSet())
	{
		FPaintPlaybackRangeArgs PaintArgs = Args.PlaybackRangeArgs.GetValue();
		LayerId = DrawPlaybackRange(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, RangeToScreen, PaintArgs);
		LayerId = DrawSubSequenceRange(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, RangeToScreen, PaintArgs);
		PaintArgs.SolidFillOpacity = 0.f;
		LayerId = DrawSelectionRange(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, RangeToScreen, PaintArgs);
	}

	if( Args.bDisplayTickLines )
	{
		static FLinearColor TickColor(0.f, 0.f, 0.f, 0.3f);

		// Draw major tick lines in the section area
		FDrawTickArgs DrawTickArgs;
		{
			DrawTickArgs.AllottedGeometry = AllottedGeometry;
			DrawTickArgs.bMirrorLabels = false;
			DrawTickArgs.bOnlyDrawMajorTicks = true;
			DrawTickArgs.TickColor = TickColor;
			DrawTickArgs.CullingRect = MyCullingRect;
			DrawTickArgs.DrawEffects = DrawEffects;
			// Draw major ticks under sections
			DrawTickArgs.StartLayer = LayerId-1;
			// Draw the tick the entire height of the section area
			DrawTickArgs.TickOffset = 0.0f;
			DrawTickArgs.MajorTickHeight = AllottedGeometry.Size.Y;
		}

		DrawTicks( OutDrawElements, LocalViewRange, RangeToScreen, DrawTickArgs );
	}

	if ( Args.bDisplayMarkedFrames )
	{
		LayerId = DrawMarkedFrames(AllottedGeometry, RangeToScreen, OutDrawElements, LayerId, DrawEffects);
	}

	if( Args.bDisplayScrubPosition )
	{
		FQualifiedFrameTime ScrubPosition = FQualifiedFrameTime(TimeSliderArgs.ScrubPosition.Get(), GetTickResolution());
		FScrubberMetrics    ScrubMetrics = GetScrubPixelMetrics(ScrubPosition, RangeToScreen);

		if (ScrubMetrics.bDrawExtents)
		{
			// Draw a box for the scrub position
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 1,
				AllottedGeometry.ToPaintGeometry(FVector2D(ScrubMetrics.FrameExtentsPx.GetLowerBoundValue(), 0.0f), FVector2D(ScrubMetrics.FrameExtentsPx.Size<float>(), AllottedGeometry.Size.Y)),
				ScrubFillBrush,
				DrawEffects,
				FLinearColor::White.CopyWithNewOpacity(0.5f)
			);
		}

			// Draw a line for the scrub position
			TArray<FVector2D> LinePoints;
			{
			float LinePos = RangeToScreen.InputToLocalX(ScrubPosition.AsSeconds());

				LinePoints.AddUninitialized(2);
			LinePoints[0] = FVector2D( LinePos, 0.0f );
			LinePoints[1] = FVector2D( LinePos, FMath::FloorToFloat( AllottedGeometry.Size.Y ) );
			}

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId+1,
			AllottedGeometry.ToPaintGeometry(),
				LinePoints,
				DrawEffects,
				FLinearColor(1.f, 1.f, 1.f, .5f),
				false
			);
		}

	return LayerId;
}

TSharedRef<SWidget> FSequencerTimeSliderController::OpenSetPlaybackRangeMenu(FFrameNumber FrameNumber)
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

	FText CurrentTimeText;
	CurrentTimeText = FText::FromString(TimeSliderArgs.NumericTypeInterface->ToString(FrameNumber.Value));
	

	TRange<FFrameNumber> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();

	MenuBuilder.BeginSection("SequencerPlaybackRangeMenu", FText::Format(LOCTEXT("PlaybackRangeTextFormat", "Playback Range ({0}):"), CurrentTimeText));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetPlaybackStart", "Set Start Time"),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=]{ SetPlaybackRangeStart(FrameNumber); }),
				FCanExecuteAction::CreateLambda([=]{ return !TimeSliderArgs.IsPlaybackRangeLocked.Get() && FrameNumber < MovieScene::DiscreteExclusiveUpper(PlaybackRange); })
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetPlaybackEnd", "Set End Time"),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=]{ SetPlaybackRangeEnd(FrameNumber); }),
				FCanExecuteAction::CreateLambda([=]{ return !TimeSliderArgs.IsPlaybackRangeLocked.Get() && FrameNumber >= MovieScene::DiscreteInclusiveLower(PlaybackRange); })
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ToggleLocked", "Locked"),
			LOCTEXT("ToggleLockedTooltip", "Lock/Unlock the playback range"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=] { TimeSliderArgs.OnTogglePlaybackRangeLocked.ExecuteIfBound(); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([=] { return TimeSliderArgs.IsPlaybackRangeLocked.Get(); })
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	}
	MenuBuilder.EndSection(); // SequencerPlaybackRangeMenu

	TRange<FFrameNumber> SelectionRange = TimeSliderArgs.SelectionRange.Get();
	MenuBuilder.BeginSection("SequencerSelectionRangeMenu", FText::Format(LOCTEXT("SelectionRangeTextFormat", "Selection Range ({0}):"), CurrentTimeText));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetSelectionStart", "Set Selection Start"),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=]{ SetSelectionRangeStart(FrameNumber); }),
				FCanExecuteAction::CreateLambda([=]{ return SelectionRange.IsEmpty() || FrameNumber < MovieScene::DiscreteExclusiveUpper(SelectionRange); })
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetSelectionEnd", "Set Selection End"),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=]{ SetSelectionRangeEnd(FrameNumber); }),
				FCanExecuteAction::CreateLambda([=]{ return SelectionRange.IsEmpty() || FrameNumber >= MovieScene::DiscreteInclusiveLower(SelectionRange); })
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ClearSelectionRange", "Clear Selection Range"),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=]{ TimeSliderArgs.OnSelectionRangeChanged.ExecuteIfBound(TRange<FFrameNumber>::Empty()); }),
				FCanExecuteAction::CreateLambda([=]{ return !SelectionRange.IsEmpty(); })
			)
		);
	}
	MenuBuilder.EndSection(); // SequencerPlaybackRangeMenu

	MenuBuilder.BeginSection("SequencerMarkMenu", FText::Format(LOCTEXT("MarkTextFormat", "Mark ({0}):"), CurrentTimeText));
	{
		FFrameNumber DisplayFrameNumber = GetDisplayRate().AsFrameNumber(FrameNumber / GetTickResolution());

		const TSet<FFrameNumber>& MarkedFrames = TimeSliderArgs.MarkedFrames.Get();
		bool HasMarkAtFrame = MarkedFrames.Contains(FrameNumber);
		if (!HasMarkAtFrame)
		{
			MenuBuilder.AddMenuEntry( 
				LOCTEXT("AddMark", "Add Mark"),
				FText(),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda( [=]{ AddMarkAtFrame(FrameNumber); }))
			);
		}
		else 
		{
			MenuBuilder.AddMenuEntry( 
				LOCTEXT("ClearMark", "Clear Mark"),
				FText(),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([=]{ ClearMarkAtFrame(FrameNumber); }))
			);
		}

		MenuBuilder.AddMenuEntry( 
			LOCTEXT("Clear All Marks", "Clear All Marks"),
			FText(),
			FSlateIcon(),

			FUIAction(
				FExecuteAction::CreateLambda([=]{ ClearAllMarks(); }),
				FCanExecuteAction::CreateLambda([=]{ return (MarkedFrames.Num() > 0); })
			)
		);
	}
	MenuBuilder.EndSection(); // SequencerMarkMenu

	return MenuBuilder.MakeWidget();
}

void FSequencerTimeSliderController::ClampViewRange(double& NewRangeMin, double& NewRangeMax)
{
	bool bNeedsClampSet = false;
	double NewClampRangeMin = TimeSliderArgs.ClampRange.Get().GetLowerBoundValue();
	if ( NewRangeMin < TimeSliderArgs.ClampRange.Get().GetLowerBoundValue() )
	{
		NewClampRangeMin = NewRangeMin;
		bNeedsClampSet = true;
	}

	double NewClampRangeMax = TimeSliderArgs.ClampRange.Get().GetUpperBoundValue();
	if ( NewRangeMax > TimeSliderArgs.ClampRange.Get().GetUpperBoundValue() )
	{
		NewClampRangeMax = NewRangeMax;
		bNeedsClampSet = true;
	}

	if (bNeedsClampSet)
	{
		SetClampRange(NewClampRangeMin, NewClampRangeMax);
	}
}

void FSequencerTimeSliderController::SetViewRange( double NewRangeMin, double NewRangeMax, EViewRangeInterpolation Interpolation )
{
	// Clamp to a minimum size to avoid zero-sized or negative visible ranges
	double MinVisibleTimeRange = FFrameNumber(1) / GetTickResolution();
	TRange<double> ExistingViewRange  = TimeSliderArgs.ViewRange.Get();
	TRange<double> ExistingClampRange = TimeSliderArgs.ClampRange.Get();

	if (NewRangeMax == ExistingViewRange.GetUpperBoundValue())
	{
		if (NewRangeMin > NewRangeMax - MinVisibleTimeRange)
		{
			NewRangeMin = NewRangeMax - MinVisibleTimeRange;
		}
	}
	else if (NewRangeMax < NewRangeMin + MinVisibleTimeRange)
	{
		NewRangeMax = NewRangeMin + MinVisibleTimeRange;
	}

	// Clamp to the clamp range
	const TRange<double> NewRange = TRange<double>::Intersection(TRange<double>(NewRangeMin, NewRangeMax), ExistingClampRange);
	TimeSliderArgs.OnViewRangeChanged.ExecuteIfBound( NewRange, Interpolation );

	if( !TimeSliderArgs.ViewRange.IsBound() )
	{
		// The  output is not bound to a delegate so we'll manage the value ourselves (no animation)
		TimeSliderArgs.ViewRange.Set( NewRange );
	}
}

void FSequencerTimeSliderController::SetClampRange( double NewRangeMin, double NewRangeMax )
{
	const TRange<double> NewRange(NewRangeMin, NewRangeMax);

	TimeSliderArgs.OnClampRangeChanged.ExecuteIfBound(NewRange);

	if( !TimeSliderArgs.ClampRange.IsBound() )
	{	
		// The  output is not bound to a delegate so we'll manage the value ourselves (no animation)
		TimeSliderArgs.ClampRange.Set(NewRange);
	}
}

void FSequencerTimeSliderController::SetPlayRange( FFrameNumber RangeStart, int32 RangeDuration )
{
	check(RangeDuration >= 0);

	const TRange<FFrameNumber> NewRange(RangeStart, RangeStart + RangeDuration);

	TimeSliderArgs.OnPlaybackRangeChanged.ExecuteIfBound(NewRange);

	if( !TimeSliderArgs.PlaybackRange.IsBound() )
	{
		// The  output is not bound to a delegate so we'll manage the value ourselves (no animation)
		TimeSliderArgs.PlaybackRange.Set(NewRange);
	}
}

bool FSequencerTimeSliderController::ZoomByDelta( float InDelta, float MousePositionFraction )
{
	TRange<double> LocalViewRange = TimeSliderArgs.ViewRange.Get().GetAnimationTarget();
	double LocalViewRangeMax = LocalViewRange.GetUpperBoundValue();
	double LocalViewRangeMin = LocalViewRange.GetLowerBoundValue();
	const double OutputViewSize = LocalViewRangeMax - LocalViewRangeMin;
	const double OutputChange = OutputViewSize * InDelta;

	double NewViewOutputMin = LocalViewRangeMin - (OutputChange * MousePositionFraction);
	double NewViewOutputMax = LocalViewRangeMax + (OutputChange * (1.f - MousePositionFraction));

	if( NewViewOutputMin < NewViewOutputMax )
	{
		ClampViewRange(NewViewOutputMin, NewViewOutputMax);
		SetViewRange(NewViewOutputMin, NewViewOutputMax, EViewRangeInterpolation::Animated);
		return true;
	}

	return false;
}

void FSequencerTimeSliderController::PanByDelta( float InDelta )
{
	TRange<double> LocalViewRange = TimeSliderArgs.ViewRange.Get().GetAnimationTarget();

	double CurrentMin = LocalViewRange.GetLowerBoundValue();
	double CurrentMax = LocalViewRange.GetUpperBoundValue();

	// Adjust the delta to be a percentage of the current range
	InDelta *= ScrubConstants::ScrollPanFraction * (CurrentMax - CurrentMin);

	double NewViewOutputMin = CurrentMin + InDelta;
	double NewViewOutputMax = CurrentMax + InDelta;

	ClampViewRange(NewViewOutputMin, NewViewOutputMax);
	SetViewRange(NewViewOutputMin, NewViewOutputMax, EViewRangeInterpolation::Animated);
}


bool FSequencerTimeSliderController::HitTestRangeStart(const FScrubRangeToScreen& RangeToScreen, const TRange<double>& Range, float HitPixel) const
{
	static float BrushSizeInStateUnits = 6.f, DragToleranceSlateUnits = 2.f, MouseTolerance = 2.f;
	const float  RangeStartPixel = RangeToScreen.InputToLocalX(Range.GetLowerBoundValue());

	// Hit test against the brush region to the right of the playback start position, +/- DragToleranceSlateUnits
	return HitPixel >= RangeStartPixel - MouseTolerance - DragToleranceSlateUnits &&
		HitPixel <= RangeStartPixel + MouseTolerance + BrushSizeInStateUnits + DragToleranceSlateUnits;
}

bool FSequencerTimeSliderController::HitTestRangeEnd(const FScrubRangeToScreen& RangeToScreen, const TRange<double>& Range, float HitPixel) const
{
	static float BrushSizeInStateUnits = 6.f, DragToleranceSlateUnits = 2.f, MouseTolerance = 2.f;
	const float  RangeEndPixel = RangeToScreen.InputToLocalX(Range.GetUpperBoundValue());

	// Hit test against the brush region to the left of the playback end position, +/- DragToleranceSlateUnits
	return HitPixel >= RangeEndPixel - MouseTolerance - BrushSizeInStateUnits - DragToleranceSlateUnits &&
		HitPixel <= RangeEndPixel + MouseTolerance + DragToleranceSlateUnits;
}

FFrameTime FSequencerTimeSliderController::SnapTimeToNearestKey(const FScrubRangeToScreen& RangeToScreen, float CursorPos, FFrameTime InTime) const
{
	if (!WeakSequencer.IsValid())
	{
		return InTime;
	}

	if (TimeSliderArgs.OnGetNearestKey.IsBound())
	{
		// If there are any tracks selected we'll find the nearest key only on that track. If there are no keys selected,
		// we will try to find the nearest keys on all tracks. This mirrors the behavior of the Jump to Next Keyframe commands.
		const TSet< TSharedRef<FSequencerDisplayNode> >& SelectedNodes = WeakSequencer.Pin()->GetSelection().GetSelectedOutlinerNodes();
		const bool bSearchAllTracks = SelectedNodes.Num() == 0;

		FFrameNumber NearestKey = TimeSliderArgs.OnGetNearestKey.Execute(InTime, bSearchAllTracks);

		float LocalKeyPos = RangeToScreen.InputToLocalX( NearestKey / GetTickResolution() );
		static float MouseTolerance = 20.f;

		if (FMath::IsNearlyEqual(LocalKeyPos, CursorPos, MouseTolerance))
		{
			return NearestKey;
		}
	}

	return InTime;
}

void FSequencerTimeSliderController::SetPlaybackRangeStart(FFrameNumber NewStart)
{
	TRange<FFrameNumber> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();

	if (NewStart <= MovieScene::DiscreteExclusiveUpper(PlaybackRange))
	{
		TimeSliderArgs.OnPlaybackRangeChanged.ExecuteIfBound(TRange<FFrameNumber>(NewStart, PlaybackRange.GetUpperBound()));
	}
}

void FSequencerTimeSliderController::SetPlaybackRangeEnd(FFrameNumber NewEnd)
{
	TRange<FFrameNumber> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();

	if (NewEnd >= MovieScene::DiscreteInclusiveLower(PlaybackRange))
	{
		TimeSliderArgs.OnPlaybackRangeChanged.ExecuteIfBound(TRange<FFrameNumber>(PlaybackRange.GetLowerBound(), TRangeBound<FFrameNumber>::Exclusive(NewEnd)));
	}
}

void FSequencerTimeSliderController::SetSelectionRangeStart(FFrameNumber NewStart)
{
	TRange<FFrameNumber> SelectionRange = TimeSliderArgs.SelectionRange.Get();

	if (SelectionRange.IsEmpty())
	{
		TimeSliderArgs.OnSelectionRangeChanged.ExecuteIfBound(TRange<FFrameNumber>(NewStart, NewStart + 1));
	}
	else if (NewStart <= MovieScene::DiscreteExclusiveUpper(SelectionRange))
	{
		TimeSliderArgs.OnSelectionRangeChanged.ExecuteIfBound(TRange<FFrameNumber>(NewStart, SelectionRange.GetUpperBound()));
	}
}

void FSequencerTimeSliderController::SetSelectionRangeEnd(FFrameNumber NewEnd)
{
	TRange<FFrameNumber> SelectionRange = TimeSliderArgs.SelectionRange.Get();

	if (SelectionRange.IsEmpty())
	{
		TimeSliderArgs.OnSelectionRangeChanged.ExecuteIfBound(TRange<FFrameNumber>(NewEnd - 1, NewEnd));
	}
	else if (NewEnd >= MovieScene::DiscreteInclusiveLower(SelectionRange))
	{
		TimeSliderArgs.OnSelectionRangeChanged.ExecuteIfBound(TRange<FFrameNumber>(SelectionRange.GetLowerBound(), NewEnd));
	}
}

void FSequencerTimeSliderController::AddMarkAtFrame(FFrameNumber FrameNumber)
{
	TimeSliderArgs.OnMarkedFrameChanged.ExecuteIfBound(FrameNumber, true);
}

void FSequencerTimeSliderController::ClearMarkAtFrame(FFrameNumber FrameNumber)
{
	TimeSliderArgs.OnMarkedFrameChanged.ExecuteIfBound(FrameNumber, false);
}

void FSequencerTimeSliderController::ClearAllMarks()
{
	TimeSliderArgs.OnClearAllMarkedFrames.ExecuteIfBound();
}
	

#undef LOCTEXT_NAMESPACE
