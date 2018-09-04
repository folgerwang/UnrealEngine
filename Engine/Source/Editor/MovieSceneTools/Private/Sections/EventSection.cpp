// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Sections/EventSection.h"
#include "Rendering/DrawElements.h"
#include "SequencerSectionPainter.h"
#include "ISectionLayoutBuilder.h"
#include "EditorStyleSet.h"
#include "CommonMovieSceneTools.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Sections/MovieSceneEventSection.h"
#include "Sections/MovieSceneEventTriggerSection.h"
#include "Sections/MovieSceneEventRepeaterSection.h"
#include "MovieSceneTrack.h"
#include "SequencerTimeSliderController.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "EditorStyleSet.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "MovieSceneSequence.h"
#include "EditorFontGlyphs.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "EventSection"

bool FEventSectionBase::IsSectionSelected() const
{
	TSharedPtr<ISequencer> SequencerPtr = Sequencer.Pin();

	TArray<UMovieSceneTrack*> SelectedTracks;
	SequencerPtr->GetSelectedTracks(SelectedTracks);

	UMovieSceneSection* Section = WeakSection.Get();
	UMovieSceneTrack*   Track   = Section ? CastChecked<UMovieSceneTrack>(Section->GetOuter()) : nullptr;
	return Track && SelectedTracks.Contains(Track);
}

void FEventSectionBase::PaintEventName(FSequencerSectionPainter& Painter, int32 LayerId, const FString& InEventString, float PixelPos, bool bIsEventValid) const
{
	static const int32   FontSize      = 10;
	static const float   BoxOffsetPx   = 10.f;
	static const TCHAR*  WarningString = TEXT("\xf071");

	const FSlateFontInfo FontAwesomeFont = FEditorStyle::Get().GetFontStyle("FontAwesome.10");
	const FSlateFontInfo SmallLayoutFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
	const FLinearColor   DrawColor       = FEditorStyle::GetSlateColor("SelectionColor").GetColor(FWidgetStyle());

	TSharedRef<FSlateFontMeasure> FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	// Setup the warning size. Static since it won't ever change
	static FVector2D WarningSize    = FontMeasureService->Measure(WarningString, FontAwesomeFont);
	const  FMargin   WarningPadding = (bIsEventValid || InEventString.Len() == 0) ? FMargin(0.f) : FMargin(0.f, 0.f, 4.f, 0.f);
	const  FMargin   BoxPadding     = FMargin(4.0f, 2.0f);

	const FVector2D  TextSize       = FontMeasureService->Measure(InEventString, SmallLayoutFont);
	const FVector2D  IconSize       = bIsEventValid ? FVector2D::ZeroVector : WarningSize;
	const FVector2D  PaddedIconSize = IconSize + WarningPadding.GetDesiredSize();
	const FVector2D  BoxSize        = FVector2D(TextSize.X + PaddedIconSize.X, FMath::Max(TextSize.Y, PaddedIconSize.Y )) + BoxPadding.GetDesiredSize();

	// Flip the text position if getting near the end of the view range
	bool  bDrawLeft    = (Painter.SectionGeometry.Size.X - PixelPos) < (BoxSize.X + 22.f) - BoxOffsetPx;
	float BoxPositionX = bDrawLeft ? PixelPos - BoxSize.X - BoxOffsetPx : PixelPos + BoxOffsetPx;

	FVector2D BoxOffset  = FVector2D(BoxPositionX,                    Painter.SectionGeometry.Size.Y*.5f - BoxSize.Y*.5f);
	FVector2D IconOffset = FVector2D(BoxPadding.Left,                 BoxSize.Y*.5f - IconSize.Y*.5f);
	FVector2D TextOffset = FVector2D(IconOffset.X + PaddedIconSize.X, BoxSize.Y*.5f - TextSize.Y*.5f);

	// Draw the background box
	FSlateDrawElement::MakeBox(
		Painter.DrawElements,
		LayerId + 1,
		Painter.SectionGeometry.ToPaintGeometry(BoxOffset, BoxSize),
		FEditorStyle::GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		FLinearColor::Black.CopyWithNewOpacity(0.5f)
	);

	if (!bIsEventValid)
	{
		// Draw a warning icon for unbound repeaters
		FSlateDrawElement::MakeText(
			Painter.DrawElements,
			LayerId + 2,
			Painter.SectionGeometry.ToPaintGeometry(BoxOffset + IconOffset, IconSize),
			WarningString,
			FontAwesomeFont,
			Painter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect,
			FEditorStyle::GetWidgetStyle<FTextBlockStyle>("Log.Warning").ColorAndOpacity.GetSpecifiedColor()
		);
	}

	FSlateDrawElement::MakeText(
		Painter.DrawElements,
		LayerId + 2,
		Painter.SectionGeometry.ToPaintGeometry(BoxOffset + TextOffset, TextSize),
		InEventString,
		SmallLayoutFont,
		Painter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect,
		DrawColor
	);
}

int32 FEventSection::OnPaintSection(FSequencerSectionPainter& Painter) const
{
	int32 LayerId = Painter.PaintSectionBackground();
	UMovieSceneEventSection* EventSection = Cast<UMovieSceneEventSection>( WeakSection.Get() );
	if (!EventSection || !IsSectionSelected())
	{
		return LayerId;
	}

	const FTimeToPixel& TimeToPixelConverter = Painter.GetTimeConverter();

	for (int32 KeyIndex = 0; KeyIndex < EventSection->GetEventData().GetKeyTimes().Num(); ++KeyIndex)
	{
		FFrameNumber EventTime = EventSection->GetEventData().GetKeyTimes()[KeyIndex];
		FEventPayload EventData = EventSection->GetEventData().GetKeyValues()[KeyIndex];

		if (EventSection->GetRange().Contains(EventTime))
		{
			FString EventString = EventData.EventName.ToString();
			if (!EventString.IsEmpty())
			{
				const float PixelPos = TimeToPixelConverter.FrameToPixel(EventTime);
				PaintEventName(Painter, LayerId, EventString, PixelPos);
			}
		}
	}

	return LayerId + 3;
}

int32 FEventTriggerSection::OnPaintSection(FSequencerSectionPainter& Painter) const
{
	int32 LayerId = Painter.PaintSectionBackground();

	UMovieSceneEventTriggerSection* EventTriggerSection = Cast<UMovieSceneEventTriggerSection>(WeakSection.Get());
	if (!EventTriggerSection || !IsSectionSelected())
	{
		return LayerId;
	}

	const FTimeToPixel& TimeToPixelConverter = Painter.GetTimeConverter();

	TMovieSceneChannelData<FMovieSceneEvent> EventData = EventTriggerSection->EventChannel.GetData();
	TArrayView<const FFrameNumber>     Times  = EventData.GetTimes();
	TArrayView<const FMovieSceneEvent> Events = EventData.GetValues();

	TRange<FFrameNumber> EventSectionRange = EventTriggerSection->GetRange();
	for (int32 KeyIndex = 0; KeyIndex < Times.Num(); ++KeyIndex)
	{
		FFrameNumber EventTime = Times[KeyIndex];
		UK2Node_FunctionEntry* FunctionEntry = Events[KeyIndex].GetFunctionEntry();

		if (EventSectionRange.Contains(EventTime))
		{
			FString EventString = FunctionEntry ? FunctionEntry->GetNodeTitle(ENodeTitleType::MenuTitle).ToString() : FString();
			bool bIsEventValid = FMovieSceneEvent::IsValidFunction(FunctionEntry);

			const float PixelPos = TimeToPixelConverter.FrameToPixel(EventTime);
			PaintEventName(Painter, LayerId, EventString, PixelPos, bIsEventValid);
		}
	}

	return LayerId + 3;
}

FReply FEventTriggerSection::OnKeyDoubleClicked(FKeyHandle KeyHandle)
{
	UMovieSceneEventTriggerSection* EventTriggerSection = Cast<UMovieSceneEventTriggerSection>( WeakSection.Get() );
	if (EventTriggerSection)
	{
		TMovieSceneChannelData<FMovieSceneEvent> ChannelData = EventTriggerSection->EventChannel.GetData();
		int32 EventIndex = ChannelData.GetIndex(KeyHandle);
		if (EventIndex != INDEX_NONE)
		{
			TArrayView<FMovieSceneEvent> Events = ChannelData.GetValues();
			FMovieSceneEvent& Event = Events[EventIndex];

			if (!Event.IsBoundToBlueprint())
			{
				FMovieSceneSequenceEditor* SequenceEditor = FMovieSceneSequenceEditor::Find(EventTriggerSection->GetTypedOuter<UMovieSceneSequence>());
				if (ensure(SequenceEditor))
				{
					FScopedTransaction Transaction(LOCTEXT("BindTriggerEvent", "Create Event Endpoint"));

					UK2Node_FunctionEntry* NewEndpoint = SequenceEditor->CreateEventEndpoint(EventTriggerSection->GetTypedOuter<UMovieSceneSequence>());
					if (NewEndpoint)
					{
						SequenceEditor->InitializeEndpointForTrack(EventTriggerSection->GetTypedOuter<UMovieSceneEventTrack>(), NewEndpoint);
						FMovieSceneSequenceEditor::BindEventToEndpoint(EventTriggerSection, &Event, NewEndpoint);
					}
				}
			}

			if (UK2Node_FunctionEntry* FunctionEntry = Event.GetFunctionEntry())
			{
				FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(FunctionEntry, false);
			}
		}
	}

	return FReply::Handled();
}

int32 FEventRepeaterSection::OnPaintSection(FSequencerSectionPainter& Painter) const
{
	int32 LayerId = Painter.PaintSectionBackground();

	UMovieSceneEventRepeaterSection* EventRepeaterSection = Cast<UMovieSceneEventRepeaterSection>(WeakSection.Get());
	if (EventRepeaterSection)
	{
		UK2Node_FunctionEntry* FunctionEntry = EventRepeaterSection->Event.GetFunctionEntry();

		float TextOffsetX = EventRepeaterSection->GetRange().GetLowerBound().IsClosed() ? FMath::Max(0.f, Painter.GetTimeConverter().FrameToPixel(EventRepeaterSection->GetRange().GetLowerBoundValue())) : 0.f;

		FString EventString = FunctionEntry ? FunctionEntry->GetNodeTitle(ENodeTitleType::MenuTitle).ToString() : FString();
		bool bIsEventValid = FMovieSceneEvent::IsValidFunction(FunctionEntry);
		PaintEventName(Painter, LayerId, EventString, TextOffsetX, bIsEventValid);
	}

	return LayerId + 1;
}

FReply FEventRepeaterSection::OnSectionDoubleClicked(const FGeometry& SectionGeometry, const FPointerEvent& MouseEvent)
{
	UMovieSceneEventRepeaterSection* EventRepeaterSection = Cast<UMovieSceneEventRepeaterSection>( WeakSection.Get() );
	if (EventRepeaterSection)
	{
		if (!EventRepeaterSection->Event.IsBoundToBlueprint())
		{
			FMovieSceneSequenceEditor* SequenceEditor = FMovieSceneSequenceEditor::Find(EventRepeaterSection->GetTypedOuter<UMovieSceneSequence>());
			if (ensure(SequenceEditor))
			{
				FScopedTransaction Transaction(LOCTEXT("BindRepeaterEvent", "Create Event Endpoint"));

				UK2Node_FunctionEntry* NewEndpoint = SequenceEditor->CreateEventEndpoint(EventRepeaterSection->GetTypedOuter<UMovieSceneSequence>());
				if (NewEndpoint)
				{
					SequenceEditor->InitializeEndpointForTrack(EventRepeaterSection->GetTypedOuter<UMovieSceneEventTrack>(), NewEndpoint);
					FMovieSceneSequenceEditor::BindEventToEndpoint(EventRepeaterSection, &EventRepeaterSection->Event, NewEndpoint);
				}
			}
		}

		if (UK2Node* FunctionEntry = EventRepeaterSection->Event.GetFunctionEntry())
		{
			FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(FunctionEntry, false);
		}
	}

	return FReply::Handled();
}


#undef LOCTEXT_NAMESPACE // "EventSection"