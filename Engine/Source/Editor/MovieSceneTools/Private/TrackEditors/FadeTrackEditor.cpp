// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/FadeTrackEditor.h"
#include "Rendering/DrawElements.h"
#include "SequencerSectionPainter.h"
#include "EditorStyleSet.h"
#include "Tracks/MovieSceneFadeTrack.h"
#include "Sections/MovieSceneFadeSection.h"
#include "ISequencerSection.h"
#include "CommonMovieSceneTools.h"

#define LOCTEXT_NAMESPACE "FFadeTrackEditor"

/**
 * Class for fade sections handles drawing of fade gradient
 */
class FFadeSection
	: public FSequencerSection
{
public:

	/** Constructor. */
	FFadeSection(UMovieSceneSection& InSectionObject) : FSequencerSection(InSectionObject) {}

public:

	virtual int32 OnPaintSection( FSequencerSectionPainter& Painter ) const override
	{
		int32 LayerId = Painter.PaintSectionBackground();

		const ESlateDrawEffect DrawEffects = Painter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

		FVector2D GradientSize = FVector2D( Painter.SectionGeometry.Size.X - 2.f, Painter.SectionGeometry.Size.Y - 3.0f );
		FPaintGeometry PaintGeometry = Painter.SectionGeometry.ToPaintGeometry( FVector2D( 1.f, 3.f ), GradientSize );

		const UMovieSceneFadeSection* FadeSection = Cast<const UMovieSceneFadeSection>( WeakSection.Get() );

		FTimeToPixel TimeConverter    = Painter.GetTimeConverter();
		FFrameRate   TickResolution   = TimeConverter.GetTickResolution();

		const double StartTimeSeconds = TimeConverter.PixelToSeconds(1.f);
		const double EndTimeSeconds   = TimeConverter.PixelToSeconds(Painter.SectionGeometry.GetLocalSize().X-2.f);
		const double TimeThreshold    = FMath::Max(0.0001, TimeConverter.PixelToSeconds(5) - TimeConverter.PixelToSeconds(0));
		const double DurationSeconds  = EndTimeSeconds - StartTimeSeconds;

		TArray<TTuple<double, double>> CurvePoints;
		FadeSection->GetChannel().PopulateCurvePoints(StartTimeSeconds, EndTimeSeconds, TimeThreshold, 0.1f, TickResolution, CurvePoints);

		TArray<FSlateGradientStop> GradientStops;
		for (TTuple<double, double> Vector : CurvePoints)
		{
			GradientStops.Add( FSlateGradientStop(
				FVector2D( (Vector.Get<0>() - StartTimeSeconds) / DurationSeconds * Painter.SectionGeometry.Size.X, 0 ),
				FadeSection->FadeColor.CopyWithNewOpacity(Vector.Get<1>()) )
			);
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

		return LayerId + 1;
	}
};


/* FFadeTrackEditor static functions
 *****************************************************************************/

TSharedRef<ISequencerTrackEditor> FFadeTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FFadeTrackEditor(InSequencer));
}


/* FFadeTrackEditor structors
 *****************************************************************************/

FFadeTrackEditor::FFadeTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FFloatPropertyTrackEditor(InSequencer)
{ }

/* ISequencerTrackEditor interface
 *****************************************************************************/

TSharedRef<ISequencerSection> FFadeTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	return MakeShareable(new FFadeSection(SectionObject));
}

void FFadeTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddFadeTrack", "Fade Track"),
		LOCTEXT("AddFadeTrackTooltip", "Adds a new track that controls the fade of the sequence."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.Tracks.Fade"),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FFadeTrackEditor::HandleAddFadeTrackMenuEntryExecute),
			FCanExecuteAction::CreateRaw(this, &FFadeTrackEditor::HandleAddFadeTrackMenuEntryCanExecute)
		)
	);
}

bool FFadeTrackEditor::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	return (InSequence != nullptr) && (InSequence->GetClass()->GetName() == TEXT("LevelSequence"));
}

bool FFadeTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return (Type == UMovieSceneFadeTrack::StaticClass());
}

const FSlateBrush* FFadeTrackEditor::GetIconBrush() const
{
	return FEditorStyle::GetBrush("Sequencer.Tracks.Fade");
}


/* FFadeTrackEditor callbacks
 *****************************************************************************/

void FFadeTrackEditor::HandleAddFadeTrackMenuEntryExecute()
{
	UMovieScene* MovieScene = GetFocusedMovieScene();

	if (MovieScene == nullptr)
	{
		return;
	}

	UMovieSceneTrack* FadeTrack = MovieScene->FindMasterTrack<UMovieSceneFadeTrack>();

	if (FadeTrack != nullptr)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddFadeTrack_Transaction", "Add Fade Track"));

	MovieScene->Modify();

	FadeTrack = FindOrCreateMasterTrack<UMovieSceneFadeTrack>().Track;
	check(FadeTrack);

	UMovieSceneSection* NewSection = FadeTrack->CreateNewSection();
	check(NewSection);

	FadeTrack->AddSection(*NewSection);
	if (GetSequencer().IsValid())
	{
		GetSequencer()->OnAddTrack(FadeTrack);
	}
	GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
}

bool FFadeTrackEditor::HandleAddFadeTrackMenuEntryCanExecute() const
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
	
	return ((FocusedMovieScene != nullptr) && (FocusedMovieScene->FindMasterTrack<UMovieSceneFadeTrack>() == nullptr));
}

#undef LOCTEXT_NAMESPACE
