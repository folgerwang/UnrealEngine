// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Sections/BoolPropertySection.h"
#include "Rendering/DrawElements.h"
#include "SequencerSectionPainter.h"
#include "ISectionLayoutBuilder.h"
#include "EditorStyleSet.h"
#include "CommonMovieSceneTools.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Channels/MovieSceneChannelProxy.h"


int32 FBoolPropertySection::OnPaintSection( FSequencerSectionPainter& Painter ) const
{
	// custom drawing for bool curves
	UMovieSceneBoolSection* BoolSection = Cast<UMovieSceneBoolSection>( WeakSection.Get() );

	TArray<FFrameTime> SectionSwitchTimes;

	const FTimeToPixel& TimeConverter = Painter.GetTimeConverter();

	// Add the start time
	const FFrameNumber StartTime = TimeConverter.PixelToFrame(0.f).FloorToFrame();
	const FFrameNumber EndTime   = TimeConverter.PixelToFrame(Painter.SectionGeometry.GetLocalSize().X).CeilToFrame();
	
	SectionSwitchTimes.Add(StartTime);

	int32 LayerId = Painter.PaintSectionBackground();

	FMovieSceneBoolChannel* BoolChannel = BoolSection->GetChannelProxy().GetChannel<FMovieSceneBoolChannel>(0);
	if (!BoolChannel)
	{
		return LayerId;
	}

	for ( FFrameNumber Time : BoolChannel->GetTimes() )
	{
		if ( Time > StartTime && Time < EndTime )
		{
			SectionSwitchTimes.Add( Time );
		}
	}

	SectionSwitchTimes.Add(EndTime);

	const ESlateDrawEffect DrawEffects = Painter.bParentEnabled
		? ESlateDrawEffect::None
		: ESlateDrawEffect::DisabledEffect;

	static const int32 Height = 5;
	const float VerticalOffset = Painter.SectionGeometry.GetLocalSize().Y * .5f - Height * .5f;

	const FSlateBrush* BoolOverlayBrush = FEditorStyle::GetBrush("Sequencer.Section.StripeOverlay");

	for ( int32 i = 0; i < SectionSwitchTimes.Num() - 1; ++i )
	{
		FFrameTime ThisTime = SectionSwitchTimes[i];

		bool ValueAtTime = false;
		BoolChannel->Evaluate(ThisTime, ValueAtTime);

		const FColor Color = ValueAtTime ? FColor(0, 255, 0, 125) : FColor(255, 0, 0, 125);
		
		FVector2D StartPos(TimeConverter.FrameToPixel(ThisTime), VerticalOffset);
		FVector2D Size(TimeConverter.FrameToPixel(SectionSwitchTimes[i+1]) - StartPos.X, Height);

		FSlateDrawElement::MakeBox(
			Painter.DrawElements,
			LayerId + 1,
			Painter.SectionGeometry.ToPaintGeometry(StartPos, Size),
			BoolOverlayBrush,
			DrawEffects,
			Color
			);
	}

	return LayerId + 1;
}
