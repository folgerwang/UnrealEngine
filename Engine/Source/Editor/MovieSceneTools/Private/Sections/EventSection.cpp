// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Sections/EventSection.h"
#include "Rendering/DrawElements.h"
#include "SequencerSectionPainter.h"
#include "ISectionLayoutBuilder.h"
#include "EditorStyleSet.h"
#include "CommonMovieSceneTools.h"
#include "Sections/MovieSceneEventSection.h"
#include "MovieSceneTrack.h"
#include "SequencerTimeSliderController.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "EditorStyleSet.h"

int32 FEventSection::OnPaintSection( FSequencerSectionPainter& Painter ) const
{
	int32 LayerId = Painter.PaintSectionBackground();
	UMovieSceneEventSection* EventSection = Cast<UMovieSceneEventSection>( WeakSection.Get() );
	if (!EventSection)
	{
		return LayerId;
	}

	// Draw event names for keys on sections that are on selected tracks
	TSharedPtr<ISequencer> SequencerPtr = Sequencer.Pin();
	TArray<UMovieSceneTrack*> SelectedTracks;
	SequencerPtr->GetSelectedTracks(SelectedTracks);

	bool bSelected = false;
	for (auto SelectedTrack : SelectedTracks)
	{
		if (SelectedTrack->GetAllSections().Contains(EventSection))
		{
			bSelected = true;
			break;
		}
	}

	if (!bSelected)
	{
		return LayerId;
	}

	const FTimeToPixel& TimeToPixelConverter = Painter.GetTimeConverter();

	const ESlateDrawEffect DrawEffects = Painter.bParentEnabled
		? ESlateDrawEffect::None
		: ESlateDrawEffect::DisabledEffect;

	for (int32 KeyIndex = 0; KeyIndex < EventSection->GetEventData().GetKeyTimes().Num(); ++KeyIndex)
	{
		FFrameNumber EventTime = EventSection->GetEventData().GetKeyTimes()[KeyIndex];
		FEventPayload EventData = EventSection->GetEventData().GetKeyValues()[KeyIndex];

		if (!EventSection->GetRange().Contains(EventTime))
		{
			continue;
		}

		FString EventString = EventData.EventName.ToString();
		if (EventString.IsEmpty())
		{
			continue;
		}

		const float Time = TimeToPixelConverter.FrameToPixel(EventTime); 

		const FSlateFontInfo SmallLayoutFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
		const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		FVector2D TextSize = FontMeasureService->Measure(EventString, SmallLayoutFont);

		// Flip the text position if getting near the end of the view range
		static const float TextOffsetPx = 10.f;
		bool  bDrawLeft = (Painter.SectionGeometry.Size.X - Time) < (TextSize.X + 22.f) - TextOffsetPx;
		float TextPosition = bDrawLeft ? Time - TextSize.X - TextOffsetPx : Time + TextOffsetPx;
		//handle mirrored labels
		const float MajorTickHeight = 4.0f; 
		FVector2D TextOffset(TextPosition, Painter.SectionGeometry.Size.Y - (MajorTickHeight + TextSize.Y));

		const FLinearColor DrawColor = FEditorStyle::GetSlateColor("SelectionColor").GetColor(FWidgetStyle());
		const FVector2D BoxPadding = FVector2D(4.0f, 2.0f);

		// draw event string
		FSlateDrawElement::MakeBox(
			Painter.DrawElements,
			LayerId + 5,
			Painter.SectionGeometry.ToPaintGeometry(TextOffset - BoxPadding, TextSize + 2.0f * BoxPadding),
			FEditorStyle::GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			FLinearColor::Black.CopyWithNewOpacity(0.5f)
		);

		FSlateDrawElement::MakeText(
			Painter.DrawElements,
			LayerId + 6,
			Painter.SectionGeometry.ToPaintGeometry(TextOffset, TextSize),
			EventString,
			SmallLayoutFont,
			DrawEffects,
			DrawColor
		);
	}

	return LayerId + 1;
}
