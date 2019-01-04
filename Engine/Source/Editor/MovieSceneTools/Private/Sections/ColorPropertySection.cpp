// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Sections/ColorPropertySection.h"
#include "Rendering/DrawElements.h"
#include "Sections/MovieSceneColorSection.h"
#include "SequencerSectionPainter.h"
#include "ISectionLayoutBuilder.h"
#include "EditorStyleSet.h"
#include "CommonMovieSceneTools.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "ISequencer.h"
#include "Channels/MovieSceneChannelProxy.h"

FColorPropertySection::FColorPropertySection(UMovieSceneSection& InSectionObject, const FGuid& InObjectBindingID, TWeakPtr<ISequencer> InSequencer)
	: FSequencerSection(InSectionObject)
	, ObjectBindingID(InObjectBindingID)
	, WeakSequencer(InSequencer)
{
	UMovieScenePropertyTrack* PropertyTrack = InSectionObject.GetTypedOuter<UMovieScenePropertyTrack>();
	if (PropertyTrack)
	{
		PropertyBindings = FTrackInstancePropertyBindings(PropertyTrack->GetPropertyName(), PropertyTrack->GetPropertyPath());
	}
}

int32 FColorPropertySection::OnPaintSection( FSequencerSectionPainter& Painter ) const
{
	int32 LayerId = Painter.PaintSectionBackground();

	const ESlateDrawEffect DrawEffects = Painter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	const UMovieSceneColorSection* ColorSection = Cast<const UMovieSceneColorSection>( WeakSection.Get() );

	const FTimeToPixel& TimeConverter = Painter.GetTimeConverter();

	const float StartTime       = TimeConverter.PixelToSeconds(0.f);
	const float EndTime         = TimeConverter.PixelToSeconds(Painter.SectionGeometry.GetLocalSize().X);
	const float SectionDuration = EndTime - StartTime;

	FVector2D GradientSize = FVector2D( Painter.SectionGeometry.Size.X - 2.f, (Painter.SectionGeometry.Size.Y / 4) - 3.0f );
	if ( GradientSize.X >= 1.f )
	{
		FPaintGeometry PaintGeometry = Painter.SectionGeometry.ToPaintGeometry( FVector2D( 1.f, 1.f ), GradientSize );

		// If we are showing a background pattern and the colors is transparent, draw a checker pattern
		FSlateDrawElement::MakeBox(
			Painter.DrawElements,
			LayerId,
			PaintGeometry,
			FEditorStyle::GetBrush( "Checker" ),
			DrawEffects);

		TArray< TTuple<float, FLinearColor> > ColorKeys;
		ConsolidateColorCurves( ColorKeys, ColorSection, TimeConverter );

		TArray<FSlateGradientStop> GradientStops;

		for (const TTuple<float, FLinearColor>& ColorStop : ColorKeys)
		{
			const float Time = ColorStop.Get<0>();

			// HACK: The color is converted to SRgb and then reinterpreted as linear here because gradients are converted to FColor
			// without the SRgb conversion before being passed to the renderer for some reason.
			const FLinearColor Color = ColorStop.Get<1>().ToFColor( true ).ReinterpretAsLinear();

			float TimeFraction = (Time - StartTime) / SectionDuration;
			GradientStops.Add( FSlateGradientStop( FVector2D( TimeFraction * Painter.SectionGeometry.Size.X, 0 ), Color ) );
		}

		if ( GradientStops.Num() > 0 )
		{
			FSlateDrawElement::MakeGradient(
				Painter.DrawElements,
				Painter.LayerId + 1,
				PaintGeometry,
				GradientStops,
				Orient_Vertical,
				DrawEffects
				);
		}
	}

	return LayerId + 1;
}


void FColorPropertySection::ConsolidateColorCurves( TArray< TTuple<float, FLinearColor> >& OutColorKeys, const UMovieSceneColorSection* InColorSection, const FTimeToPixel& TimeConverter ) const
{
	FLinearColor DefaultColor = GetPropertyValueAsLinearColor();

	UMovieSceneSection* Section = WeakSection.Get();
	if (Section)
	{
		// @todo Sequencer Optimize - This could all get cached, instead of recalculating everything every OnPaint

		TArrayView<FMovieSceneFloatChannel*> FloatChannels = Section->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();

		// Gather all the channels with keys
		TArray<TArrayView<const FFrameNumber>, TInlineAllocator<4>> ChannelTimes;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			if (FloatChannels[Index]->GetTimes().Num())
			{
				ChannelTimes.Add(FloatChannels[Index]->GetTimes());
			}
		}

		// Keep adding color stops for similar times until we have nothing left
		while ( ChannelTimes.Num() )
		{
			// Find the earliest time from the remaining channels
			FFrameNumber Time = TNumericLimits<int32>::Max();
			for (const TArrayView<const FFrameNumber>& Channel : ChannelTimes)
			{
				Time = FMath::Min(Time, Channel[0]);
			}

			// Slice the channels until we no longer match the next time
			for (TArrayView<const FFrameNumber>& Channel : ChannelTimes)
			{
				int32 SliceIndex = 0;
				while (SliceIndex < Channel.Num() && Time == Channel[SliceIndex])
				{
					++SliceIndex;
				}

				if (SliceIndex > 0)
				{
					int32 NewNum = Channel.Num() - SliceIndex;
					Channel = NewNum > 0 ? Channel.Slice(SliceIndex, NewNum) : TArrayView<const FFrameNumber>();
				}
			}

			// Remove empty channels with no keys left
			for (int32 Index = ChannelTimes.Num()-1; Index >= 0; --Index)
			{
				if (ChannelTimes[Index].Num() == 0)
				{
					ChannelTimes.RemoveAt(Index, 1, false);
				}
			}

			FLinearColor ColorAtTime = DefaultColor;
			FloatChannels[0]->Evaluate(Time, ColorAtTime.R);
			FloatChannels[1]->Evaluate(Time, ColorAtTime.G);
			FloatChannels[2]->Evaluate(Time, ColorAtTime.B);
			FloatChannels[3]->Evaluate(Time, ColorAtTime.A);

			OutColorKeys.Add(MakeTuple(float(Time / TimeConverter.GetTickResolution()), ColorAtTime));
		}
	}

	// Enforce at least one key for the default value
	if (OutColorKeys.Num() == 0)
	{
		OutColorKeys.Add(MakeTuple(0.f, DefaultColor));
	}
}

FLinearColor FColorPropertySection::GetPropertyValueAsLinearColor() const
{
	UMovieSceneSection* Section = WeakSection.Get();
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();

	FLinearColor LinearColor = FLinearColor::Black;

	if (Section && Sequencer.IsValid())
	{
		// Find the first object bound to this object binding ID, and apply each channel's external value to the color if possible
		for (TWeakObjectPtr<> WeakObject : Sequencer->FindObjectsInCurrentSequence(ObjectBindingID))
		{
			if (UObject* Object = WeakObject.Get())
			{
				// Access the editor data for the float channels which define how to extract the property value from the object
				TArrayView<const TMovieSceneExternalValue<float>> ExternalValues = Section->GetChannelProxy().GetAllExtendedEditorData<FMovieSceneFloatChannel>();

				FTrackInstancePropertyBindings* BindingsPtr = PropertyBindings.IsSet() ? &PropertyBindings.GetValue() : nullptr;

				if (ExternalValues[0].OnGetExternalValue)
				{
					LinearColor.R = ExternalValues[0].OnGetExternalValue(*Object, BindingsPtr).Get(0.f);
				}
				if (ExternalValues[1].OnGetExternalValue)
				{
					LinearColor.G = ExternalValues[1].OnGetExternalValue(*Object, BindingsPtr).Get(0.f);
				}
				if (ExternalValues[2].OnGetExternalValue)
				{
					LinearColor.B = ExternalValues[2].OnGetExternalValue(*Object, BindingsPtr).Get(0.f);
				}
				if (ExternalValues[3].OnGetExternalValue)
				{
					LinearColor.A = ExternalValues[3].OnGetExternalValue(*Object, BindingsPtr).Get(0.f);
				}

				break;
			}
		}
	}

	return LinearColor;
}
