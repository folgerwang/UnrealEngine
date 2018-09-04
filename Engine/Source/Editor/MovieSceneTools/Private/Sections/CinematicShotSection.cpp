// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Sections/CinematicShotSection.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "Rendering/DrawElements.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ScopedTransaction.h"
#include "MovieSceneTrack.h"
#include "MovieScene.h"
#include "TrackEditors/CinematicShotTrackEditor.h"
#include "SequencerSectionPainter.h"
#include "EditorStyleSet.h"
#include "MovieSceneToolHelpers.h"
#include "MovieSceneTimeHelpers.h"
#include "Toolkits/AssetEditorManager.h"


#define LOCTEXT_NAMESPACE "FCinematicShotSection"


/* FCinematicShotSection structors
 *****************************************************************************/

FCinematicShotSection::FCinematicSectionCache::FCinematicSectionCache(UMovieSceneCinematicShotSection* Section)
	: InnerFrameRate(1, 1)
	, InnerFrameOffset(0)
	, SectionStartFrame(0)
	, TimeScale(1.f)
{
	if (Section)
	{
		UMovieSceneSequence* InnerSequence = Section->GetSequence();
		if (InnerSequence)
		{
			InnerFrameRate = InnerSequence->GetMovieScene()->GetTickResolution();
		}

		InnerFrameOffset = Section->Parameters.GetStartFrameOffset();
		SectionStartFrame = Section->HasStartFrame() ? Section->GetInclusiveStartFrame() : 0;
		TimeScale = Section->Parameters.TimeScale;
	}
}


FCinematicShotSection::FCinematicShotSection(TSharedPtr<ISequencer> InSequencer, TSharedPtr<FTrackEditorThumbnailPool> InThumbnailPool, UMovieSceneSection& InSection, TSharedPtr<FCinematicShotTrackEditor> InCinematicShotTrackEditor)
	: FViewportThumbnailSection(InSequencer, InThumbnailPool, InSection)
	, SectionObject(*CastChecked<UMovieSceneCinematicShotSection>(&InSection))
	, Sequencer(InSequencer)
	, CinematicShotTrackEditor(InCinematicShotTrackEditor)
	, InitialStartOffsetDuringResize(0)
	, InitialStartTimeDuringResize(0)
	, ThumbnailCacheData(&SectionObject)
{
	AdditionalDrawEffect = ESlateDrawEffect::NoGamma;
}


FCinematicShotSection::~FCinematicShotSection()
{
}

float FCinematicShotSection::GetSectionHeight() const
{
	return FViewportThumbnailSection::GetSectionHeight() + 2*9.f;
}

FMargin FCinematicShotSection::GetContentPadding() const
{
	return FMargin(8.f, 15.f);
}

void FCinematicShotSection::SetSingleTime(double GlobalTime)
{
	double ReferenceOffsetSeconds = SectionObject.HasStartFrame() ? SectionObject.GetInclusiveStartFrame() / SectionObject.GetTypedOuter<UMovieScene>()->GetTickResolution() : 0;
	SectionObject.SetThumbnailReferenceOffset(GlobalTime - ReferenceOffsetSeconds);
}

void FCinematicShotSection::BeginResizeSection()
{
	InitialStartOffsetDuringResize = SectionObject.Parameters.GetStartFrameOffset();
	InitialStartTimeDuringResize = SectionObject.HasStartFrame() ? SectionObject.GetInclusiveStartFrame() : 0;
}

void FCinematicShotSection::ResizeSection(ESequencerSectionResizeMode ResizeMode, FFrameNumber ResizeTime)
{
	UMovieSceneSequence* InnerSequence = SectionObject.GetSequence();

	// Adjust the start offset when resizing from the beginning
	if (ResizeMode == SSRM_LeadingEdge && InnerSequence)
	{
		const FFrameRate    OuterFrameRate   = SectionObject.GetTypedOuter<UMovieScene>()->GetTickResolution();
		const FFrameRate    InnerFrameRate   = InnerSequence->GetMovieScene()->GetTickResolution();
		const FFrameNumber  ResizeDifference = ResizeTime - InitialStartTimeDuringResize;
		const FFrameTime    InnerFrameTime   = ConvertFrameTime(ResizeDifference, OuterFrameRate, InnerFrameRate);
		const int32         NewStartOffset   = FFrameTime::FromDecimal(InnerFrameTime.AsDecimal() * SectionObject.Parameters.TimeScale).FrameNumber.Value;

		// Ensure start offset is not less than 0
		SectionObject.Parameters.SetStartFrameOffset(FMath::Max(NewStartOffset, 0));
	}

	FViewportThumbnailSection::ResizeSection(ResizeMode, ResizeTime);
}

void FCinematicShotSection::BeginSlipSection()
{
	BeginResizeSection();
}

void FCinematicShotSection::SlipSection(double SlipTime)
{
	UMovieSceneSequence* InnerSequence = SectionObject.GetSequence();

	// Adjust the start offset when resizing from the beginning
	if (InnerSequence)
	{
		const FFrameRate    OuterFrameRate   = SectionObject.GetTypedOuter<UMovieScene>()->GetTickResolution();
		const FFrameRate    InnerFrameRate   = InnerSequence->GetMovieScene()->GetTickResolution();
		const double        ResizeDifference = SlipTime - InitialStartTimeDuringResize / OuterFrameRate;
		const int32         NewStartOffset   = ( (ResizeDifference * SectionObject.Parameters.TimeScale) * InnerFrameRate ).FrameNumber.Value;

		// Ensure start offset is not less than 0
		SectionObject.Parameters.SetStartFrameOffset(FMath::Max(NewStartOffset, 0));
	}

	FViewportThumbnailSection::SlipSection(SlipTime);
}

void FCinematicShotSection::Tick(const FGeometry& AllottedGeometry, const FGeometry& ClippedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	// Set cached data
	FCinematicSectionCache NewCacheData(&SectionObject);
	if (NewCacheData != ThumbnailCacheData)
	{
		ThumbnailCache.ForceRedraw();
	}
	ThumbnailCacheData = NewCacheData;

	// Update single reference frame settings
	if (GetDefault<UMovieSceneUserThumbnailSettings>()->bDrawSingleThumbnails && SectionObject.HasStartFrame())
	{
		double ReferenceTime = SectionObject.GetInclusiveStartFrame() / SectionObject.GetTypedOuter<UMovieScene>()->GetTickResolution() + SectionObject.GetThumbnailReferenceOffset();
		ThumbnailCache.SetSingleReferenceFrame(ReferenceTime);
	}
	else
	{
		ThumbnailCache.SetSingleReferenceFrame(TOptional<double>());
	}

	FViewportThumbnailSection::Tick(AllottedGeometry, ClippedGeometry, InCurrentTime, InDeltaTime);
}

int32 FCinematicShotSection::OnPaintSection(FSequencerSectionPainter& InPainter) const
{
	static const FSlateBrush* FilmBorder = FEditorStyle::GetBrush("Sequencer.Section.FilmBorder");

	InPainter.LayerId = InPainter.PaintSectionBackground();

	FVector2D LocalSectionSize = InPainter.SectionGeometry.GetLocalSize();

	FSlateDrawElement::MakeBox(
		InPainter.DrawElements,
		InPainter.LayerId++,
		InPainter.SectionGeometry.ToPaintGeometry(FVector2D(LocalSectionSize.X-2.f, 7.f), FSlateLayoutTransform(FVector2D(1.f, 4.f))),
		FilmBorder,
		InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect
	);

	FSlateDrawElement::MakeBox(
		InPainter.DrawElements,
		InPainter.LayerId++,
		InPainter.SectionGeometry.ToPaintGeometry(FVector2D(LocalSectionSize.X-2.f, 7.f), FSlateLayoutTransform(FVector2D(1.f, LocalSectionSize.Y - 11.f))),
		FilmBorder,
		InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect
	);

	TRange<FFrameNumber> SectionRange = SectionObject.GetRange();
	if (SectionRange.GetLowerBound().IsOpen() || SectionRange.GetUpperBound().IsOpen())
	{
		return InPainter.LayerId;
	}

	const FFrameNumber SectionStartFrame = SectionObject.GetInclusiveStartFrame();
	const FFrameNumber SectionEndFrame   = SectionObject.GetExclusiveEndFrame();
	const int32        SectionSize       = MovieScene::DiscreteSize(SectionRange);

	if (SectionSize <= 0)
	{
		return InPainter.LayerId;
	}

	FViewportThumbnailSection::OnPaintSection(InPainter);

	UMovieSceneSequence* InnerSequence = SectionObject.GetSequence();
	if (!InnerSequence)
	{
		return InPainter.LayerId;
	}

	const float PixelsPerFrame = InPainter.SectionGeometry.Size.X / float(SectionSize);

	UMovieScene*         MovieScene    = InnerSequence->GetMovieScene();
	TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();

	FMovieSceneSequenceTransform InnerToOuterTransform = SectionObject.OuterToInnerTransform().Inverse();

	const FFrameNumber PlaybackStart = (MovieScene::DiscreteInclusiveLower(PlaybackRange) * InnerToOuterTransform).FloorToFrame();
	if (SectionRange.Contains(PlaybackStart))
	{
		const int32 StartOffset = (PlaybackStart-SectionStartFrame).Value;
		// add dark tint for left out-of-bounds
		FSlateDrawElement::MakeBox(
			InPainter.DrawElements,
			InPainter.LayerId++,
			InPainter.SectionGeometry.ToPaintGeometry(
				FVector2D(0.0f, 0.f),
				FVector2D(StartOffset * PixelsPerFrame, InPainter.SectionGeometry.Size.Y)
			),
			FEditorStyle::GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			FLinearColor::Black.CopyWithNewOpacity(0.5f)
		);

		// add green line for playback start
		FSlateDrawElement::MakeBox(
			InPainter.DrawElements,
			InPainter.LayerId++,
			InPainter.SectionGeometry.ToPaintGeometry(
				FVector2D(StartOffset * PixelsPerFrame, 0.f),
				FVector2D(1.0f, InPainter.SectionGeometry.Size.Y)
			),
			FEditorStyle::GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			FColor(32, 128, 32)	// 120, 75, 50 (HSV)
		);
	}

	const FFrameNumber PlaybackEnd = (MovieScene::DiscreteExclusiveUpper(PlaybackRange) * InnerToOuterTransform).FloorToFrame();
	if (SectionRange.Contains(PlaybackEnd))
	{
		// add dark tint for right out-of-bounds
		const int32 EndOffset = (PlaybackEnd-SectionStartFrame).Value;
		FSlateDrawElement::MakeBox(
			InPainter.DrawElements,
			InPainter.LayerId++,
			InPainter.SectionGeometry.ToPaintGeometry(
				FVector2D(EndOffset * PixelsPerFrame, 0.f),
				FVector2D((SectionSize - EndOffset) * PixelsPerFrame, InPainter.SectionGeometry.Size.Y)
			),
			FEditorStyle::GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			FLinearColor::Black.CopyWithNewOpacity(0.5f)
		);


		// add red line for playback end
		FSlateDrawElement::MakeBox(
			InPainter.DrawElements,
			InPainter.LayerId++,
			InPainter.SectionGeometry.ToPaintGeometry(
				FVector2D(EndOffset * PixelsPerFrame, 0.f),
				FVector2D(1.0f, InPainter.SectionGeometry.Size.Y)
			),
			FEditorStyle::GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			FColor(128, 32, 32)	// 0, 75, 50 (HSV)
		);
	}

	return InPainter.LayerId;
}

void FCinematicShotSection::BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding)
{
	FViewportThumbnailSection::BuildSectionContextMenu(MenuBuilder, ObjectBinding);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("ShotMenuText", "Shot"));
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("TakesMenu", "Takes"),
			LOCTEXT("TakesMenuTooltip", "Shot takes"),
			FNewMenuDelegate::CreateLambda([=](FMenuBuilder& InMenuBuilder){ AddTakesMenu(InMenuBuilder); }));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("NewTake", "New Take"),
			FText::Format(LOCTEXT("NewTakeTooltip", "Create a new take for {0}"), FText::FromString(SectionObject.GetShotDisplayName())),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(CinematicShotTrackEditor.Pin().ToSharedRef(), &FCinematicShotTrackEditor::NewTake, &SectionObject))
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("InsertNewShot", "Insert Shot"),
			LOCTEXT("InsertNewShotTooltip", "Insert a new shot at the current time"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(CinematicShotTrackEditor.Pin().ToSharedRef(), &FCinematicShotTrackEditor::InsertShot))
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("DuplicateShot", "Duplicate Shot"),
			FText::Format(LOCTEXT("DuplicateShotTooltip", "Duplicate {0} to create a new shot"), FText::FromString(SectionObject.GetShotDisplayName())),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(CinematicShotTrackEditor.Pin().ToSharedRef(), &FCinematicShotTrackEditor::DuplicateShot, &SectionObject))
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("RenderShot", "Render Shot"),
			FText::Format(LOCTEXT("RenderShotTooltip", "Render shot movie"), FText::FromString(SectionObject.GetShotDisplayName())),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(CinematicShotTrackEditor.Pin().ToSharedRef(), &FCinematicShotTrackEditor::RenderShot, &SectionObject))
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("RenameShot", "Rename Shot"),
			FText::Format(LOCTEXT("RenameShotTooltip", "Rename {0}"), FText::FromString(SectionObject.GetShotDisplayName())),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FCinematicShotSection::EnterRename))
		);
	}
	MenuBuilder.EndSection();
}

void FCinematicShotSection::AddTakesMenu(FMenuBuilder& MenuBuilder)
{
	TArray<uint32> TakeNumbers;
	uint32 CurrentTakeNumber = INDEX_NONE;
	MovieSceneToolHelpers::GatherTakes(&SectionObject, TakeNumbers, CurrentTakeNumber);

	for (auto TakeNumber : TakeNumbers)
	{
		MenuBuilder.AddMenuEntry(
			FText::Format(LOCTEXT("TakeNumber", "Take {0}"), FText::AsNumber(TakeNumber)),
			FText::Format(LOCTEXT("TakeNumberTooltip", "Switch to take {0}"), FText::AsNumber(TakeNumber)),
			TakeNumber == CurrentTakeNumber ? FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.Star") : FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.Empty"),
			FUIAction(FExecuteAction::CreateSP(CinematicShotTrackEditor.Pin().ToSharedRef(), &FCinematicShotTrackEditor::SwitchTake, TakeNumber))
		);
	}
}

/* FCinematicShotSection callbacks
 *****************************************************************************/

FText FCinematicShotSection::HandleThumbnailTextBlockText() const
{
	return FText::FromString(SectionObject.GetShotDisplayName());
}


void FCinematicShotSection::HandleThumbnailTextBlockTextCommitted(const FText& NewShotName, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter && !HandleThumbnailTextBlockText().EqualTo(NewShotName))
	{
		SectionObject.Modify();

		const FScopedTransaction Transaction(LOCTEXT("SetShotName", "Set Shot Name"));
	
		SectionObject.SetShotDisplayName(NewShotName.ToString());
	}
}

FReply FCinematicShotSection::OnSectionDoubleClicked(const FGeometry& SectionGeometry, const FPointerEvent& MouseEvent)
{
	if( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		if (SectionObject.GetSequence())
		{
			if (MouseEvent.IsControlDown())
			{
				FAssetEditorManager::Get().OpenEditorForAsset(SectionObject.GetSequence());
			}
			else
			{
				Sequencer.Pin()->FocusSequenceInstance(SectionObject);
			}
		}
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
